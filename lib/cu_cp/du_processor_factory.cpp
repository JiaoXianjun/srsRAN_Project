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

#include "srsran/cu_cp/du_processor_factory.h"
#include "du_processor_impl.h"

/// Notice this would be the only place were we include concrete class implementation files.

using namespace srsran;
using namespace srs_cu_cp;

std::unique_ptr<du_processor_interface>
srsran::srs_cu_cp::create_du_processor(const du_processor_config_t         du_processor_config_,
                                       du_processor_cu_cp_notifier&        cu_cp_notifier_,
                                       f1ap_du_management_notifier&        f1ap_du_mgmt_notifier_,
                                       f1ap_message_notifier&              f1ap_notifier_,
                                       du_processor_e1ap_control_notifier& e1ap_ctrl_notifier_,
                                       du_processor_ngap_control_notifier& ngap_ctrl_notifier_,
                                       rrc_ue_nas_notifier&                rrc_ue_nas_pdu_notifier_,
                                       rrc_ue_control_notifier&            rrc_ue_ngap_ctrl_notifier_,
                                       du_processor_ue_task_scheduler&     task_sched_,
                                       du_processor_ue_manager&            ue_manager_,
                                       task_executor&                      ctrl_exec_)
{
  auto du_processor = std::make_unique<du_processor_impl>(du_processor_config_,
                                                          cu_cp_notifier_,
                                                          f1ap_du_mgmt_notifier_,
                                                          f1ap_notifier_,
                                                          e1ap_ctrl_notifier_,
                                                          ngap_ctrl_notifier_,
                                                          rrc_ue_nas_pdu_notifier_,
                                                          rrc_ue_ngap_ctrl_notifier_,
                                                          task_sched_,
                                                          ue_manager_,
                                                          ctrl_exec_);
  return du_processor;
}
