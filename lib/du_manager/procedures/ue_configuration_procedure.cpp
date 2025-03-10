/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "ue_configuration_procedure.h"
#include "../../ran/gnb_format.h"
#include "../converters/asn1_cell_group_config_helpers.h"
#include "../converters/scheduler_configuration_helpers.h"
#include "srsran/mac/mac_ue_configurator.h"
#include "srsran/rlc/rlc_factory.h"
#include "srsran/scheduler/config/logical_channel_config_factory.h"

using namespace srsran;
using namespace srsran::srs_du;

ue_configuration_procedure::ue_configuration_procedure(const f1ap_ue_context_update_request& request_,
                                                       du_ue_manager_repository&             ue_mng_,
                                                       const du_manager_params&              du_params_) :
  request(request_),
  ue_mng(ue_mng_),
  du_params(du_params_),
  ue(ue_mng.find_ue(request.ue_index)),
  proc_logger(logger, name(), request.ue_index, ue->rnti)
{
  srsran_assert(ue != nullptr, "ueId={} not found", request.ue_index);
}

void ue_configuration_procedure::operator()(coro_context<async_task<f1ap_ue_context_update_response>>& ctx)
{
  CORO_BEGIN(ctx);

  proc_logger.log_proc_started();

  prev_cell_group = ue->resources.value();
  if (ue->resources.update(ue->pcell_index, request).release_required) {
    proc_logger.log_proc_failure("Failed to allocate DU UE resources");
    CORO_EARLY_RETURN(make_ue_config_failure());
  }

  // > Update DU UE bearers.
  update_ue_context();

  // > Update MAC bearers.
  CORO_AWAIT(update_mac_mux_and_demux());

  // > Destroy old DU UE bearers that are now detached from remaining layers.
  clear_old_ue_context();

  proc_logger.log_proc_completed();

  CORO_RETURN(make_ue_config_response());
}

void ue_configuration_procedure::update_ue_context()
{
  // > Create DU UE SRB objects.
  for (srb_id_t srbid : request.srbs_to_setup) {
    lcid_t lcid = srb_id_to_lcid(srbid);
    auto   it   = std::find_if(ue->resources->rlc_bearers.begin(),
                           ue->resources->rlc_bearers.end(),
                           [lcid](const rlc_bearer_config& e) { return e.lcid == lcid; });
    srsran_assert(it != ue->resources->rlc_bearers.end(), "SRB should have been allocated at this point");
    du_ue_srb& srb = ue->bearers.add_srb(srbid, it->rlc_cfg);

    // >> Create RLC SRB entity.
    srb.rlc_bearer =
        create_rlc_entity(make_rlc_entity_creation_message(ue->ue_index, ue->pcell_index, srb, du_params.services));
  }

  // > Create F1-C bearers.
  f1ap_ue_configuration_request req{};
  req.ue_index = ue->ue_index;
  for (srb_id_t srb_id : request.srbs_to_setup) {
    du_ue_srb& bearer = ue->bearers.srbs()[srb_id];
    req.f1c_bearers_to_add.emplace_back();
    req.f1c_bearers_to_add.back().srb_id          = srb_id;
    req.f1c_bearers_to_add.back().rx_sdu_notifier = &bearer.connector.f1c_rx_sdu_notif;
  }
  f1ap_ue_configuration_response resp = du_params.f1ap.ue_mng.handle_ue_configuration_request(req);

  // > Connect newly created F1-C bearers with RLC SRBs, and RLC SRBs with MAC logical channel notifiers.
  for (f1c_bearer_addmodded& bearer_added : resp.f1c_bearers_added) {
    du_ue_srb& srb = ue->bearers.srbs()[bearer_added.srb_id];
    srb.connector.connect(
        req.ue_index, bearer_added.srb_id, *bearer_added.bearer, *srb.rlc_bearer, du_params.rlc.mac_ue_info_handler);
  }

  // > Move DU UE DRBs to be removed out of the UE bearer manager.
  // Note: This DRB pointer will remain valid and accessible from other layers until we update the latter.
  for (const drb_id_t& drb_to_rem : request.drbs_to_rem) {
    srsran_assert(std::any_of(ue->resources->rlc_bearers.begin(),
                              ue->resources->rlc_bearers.end(),
                              [&drb_to_rem](const rlc_bearer_config& e) { return e.drb_id == drb_to_rem; }),
                  "The bearer config should be created at this point");

    drbs_to_rem.push_back(ue->bearers.remove_drb(drb_to_rem));
  }

  // > Create DU UE DRB objects.
  for (const f1ap_drb_to_setup& drbtoadd : request.drbs_to_setup) {
    if (drbtoadd.uluptnl_info_list.empty()) {
      logger.warning("Failed to create DRB-Id={}. Cause: No UL UP TNL Info List provided.", drbtoadd.drb_id);
      continue;
    }
    if (ue->bearers.drbs().count(drbtoadd.drb_id) > 0) {
      logger.warning("Failed to Modify DRB-Id={}. Cause: DRB modifications not supported.", drbtoadd.drb_id);
      continue;
    }

    // Find the RLC configuration for this DRB.
    auto it = std::find_if(ue->resources->rlc_bearers.begin(),
                           ue->resources->rlc_bearers.end(),
                           [&drbtoadd](const rlc_bearer_config& e) { return e.drb_id == drbtoadd.drb_id; });
    srsran_assert(it != ue->resources->rlc_bearers.end(), "The bearer config should be created at this point");

    // Create DU DRB instance.
    std::unique_ptr<du_ue_drb> drb = create_drb(
        ue->ue_index, ue->pcell_index, drbtoadd.drb_id, it->lcid, it->rlc_cfg, drbtoadd.uluptnl_info_list, du_params);
    if (drb == nullptr) {
      logger.warning("Failed to create DRB-Id={}.", drbtoadd.drb_id);
      continue;
    }
    ue->bearers.add_drb(std::move(drb));
  }
}

void ue_configuration_procedure::clear_old_ue_context()
{
  drbs_to_rem.clear();
}

async_task<mac_ue_reconfiguration_response_message> ue_configuration_procedure::update_mac_mux_and_demux()
{
  // Create Request to MAC to reconfigure existing UE.
  mac_ue_reconfiguration_request_message mac_ue_reconf_req;
  mac_ue_reconf_req.ue_index           = request.ue_index;
  mac_ue_reconf_req.crnti              = ue->rnti;
  mac_ue_reconf_req.pcell_index        = ue->pcell_index;
  mac_ue_reconf_req.mac_cell_group_cfg = ue->resources->mcg_cfg;
  mac_ue_reconf_req.phy_cell_group_cfg = ue->resources->pcg_cfg;

  for (srb_id_t srbid : request.srbs_to_setup) {
    du_ue_srb& bearer = ue->bearers.srbs()[srbid];
    mac_ue_reconf_req.bearers_to_addmod.emplace_back();
    auto& lc_ch     = mac_ue_reconf_req.bearers_to_addmod.back();
    lc_ch.lcid      = bearer.lcid();
    lc_ch.ul_bearer = &bearer.connector.mac_rx_sdu_notifier;
    lc_ch.dl_bearer = &bearer.connector.mac_tx_sdu_notifier;
  }

  for (const std::unique_ptr<du_ue_drb>& drb : drbs_to_rem) {
    mac_ue_reconf_req.bearers_to_rem.push_back(drb->lcid);
  }

  for (const auto& drb : request.drbs_to_setup) {
    if (ue->bearers.drbs().count(drb.drb_id) == 0) {
      // The DRB failed to be setup. Carry on with other DRBs.
      continue;
    }
    du_ue_drb& bearer = *ue->bearers.drbs().at(drb.drb_id);
    mac_ue_reconf_req.bearers_to_addmod.emplace_back();
    auto& lc_ch     = mac_ue_reconf_req.bearers_to_addmod.back();
    lc_ch.lcid      = bearer.lcid;
    lc_ch.ul_bearer = &bearer.connector.mac_rx_sdu_notifier;
    lc_ch.dl_bearer = &bearer.connector.mac_tx_sdu_notifier;
  }

  // Create Scheduler UE Reconfig Request that will be embedded in the mac configuration request.
  mac_ue_reconf_req.sched_cfg = create_scheduler_ue_config_request(*ue);

  return du_params.mac.ue_cfg.handle_ue_reconfiguration_request(mac_ue_reconf_req);
}

f1ap_ue_context_update_response ue_configuration_procedure::make_ue_config_response()
{
  f1ap_ue_context_update_response resp;
  resp.result = true;

  // > Handle DRBs that were setup or failed to be setup.
  for (const f1ap_drb_to_setup& drb_req : request.drbs_to_setup) {
    if (ue->bearers.drbs().count(drb_req.drb_id) == 0) {
      resp.drbs_failed_to_setup.push_back(drb_req.drb_id);
      continue;
    }
    du_ue_drb& drb_added = *ue->bearers.drbs().at(drb_req.drb_id);

    resp.drbs_setup.emplace_back();
    f1ap_drb_setup& drb_setup   = resp.drbs_setup.back();
    drb_setup.drb_id            = drb_added.drb_id;
    drb_setup.dluptnl_info_list = drb_added.dluptnl_info_list;
  }

  // > Calculate ASN.1 CellGroupConfig to be sent in DU-to-CU container.
  asn1::rrc_nr::cell_group_cfg_s asn1_cell_group;
  calculate_cell_group_config_diff(asn1_cell_group, prev_cell_group, *ue->resources);
  {
    asn1::bit_ref     bref{resp.du_to_cu_rrc_container};
    asn1::SRSASN_CODE code = asn1_cell_group.pack(bref);
    srsran_assert(code == asn1::SRSASN_SUCCESS, "Invalid cellGroupConfig");
  }

  return resp;
}

f1ap_ue_context_update_response ue_configuration_procedure::make_ue_config_failure()
{
  f1ap_ue_context_update_response resp;
  resp.result = false;
  return resp;
}
