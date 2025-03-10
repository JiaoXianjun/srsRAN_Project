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

#pragma once

#include "srsran/cu_cp/cu_cp_types.h"
#include "srsran/ran/lcid.h"

namespace srsran {

namespace srs_cu_cp {

/// \brief List of all supported 5QIs and their corresponding PDCP/SDAP configs
struct drb_manager_cfg {
  std::map<five_qi_t, cu_cp_qos_config> five_qi_config; ///< Configuration for available 5QI.
};

/// Object to manage DRB configs, allocation, creation and deletion for UE.
class drb_manager
{
public:
  virtual ~drb_manager() = default;

  /// \brief Return list of DRBs to be added based on the PDU session resource setup message.
  virtual std::vector<drb_id_t> calculate_drb_to_add_list(const cu_cp_pdu_session_resource_setup_request& pdu) = 0;

  /// \brief Return PDPC config for a given DRB.
  virtual pdcp_config get_pdcp_config(drb_id_t drb_id) = 0;

  /// \brief Return SDAP config for a given DRB.
  virtual sdap_config_t get_sdap_config(drb_id_t drb_id) = 0;

  /// \brief Return list of QoS flow IDs mapped on a given DRB.
  virtual std::vector<qos_flow_id_t> get_mapped_qos_flows(drb_id_t drb_id) = 0;

  /// \brief Return list of QoS flow IDs of a given PDU session.
  virtual std::vector<qos_flow_id_t> get_mapped_qos_flows(pdu_session_id_t pdu_session_id) = 0;

  /// \brief Return list of DRB IDs of a given PDU session.
  virtual std::vector<drb_id_t> get_drbs(pdu_session_id_t pdu_session_id) = 0;

  /// \brief Return PDU session ID for a given DRB.
  virtual pdu_session_id_t get_pdu_session_id(drb_id_t drb_id) = 0;

  /// \brief Return slice information for a given DRB.
  virtual s_nssai_t get_s_nssai(drb_id_t drb_id) = 0;

  /// \brief Return number of DRBs.
  virtual size_t get_nof_drbs() = 0;

  /// \brief Returns the number of PDU sessions of the UE.
  virtual size_t get_nof_pdu_sessions() = 0;

  /// \brief Return whether a configuration for a 5QI exists.
  virtual bool valid_5qi(five_qi_t five_qi) = 0;
};

} // namespace srs_cu_cp

} // namespace srsran
