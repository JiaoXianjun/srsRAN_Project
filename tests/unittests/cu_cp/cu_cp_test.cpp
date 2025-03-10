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

#include "cu_cp_test_helpers.h"
#include <gtest/gtest.h>

using namespace srsran;
using namespace srs_cu_cp;
using namespace asn1::f1ap;

//////////////////////////////////////////////////////////////////////////////////////
/* Initial CU-CP routine manager with connected CU-UPs                              */
//////////////////////////////////////////////////////////////////////////////////////

/// Test the initail cu-cp routine
TEST_F(cu_cp_test, when_new_cu_ups_conneced_then_cu_up_e1_setup_request_send)
{
  // create CU-CP config
  cu_cp_configuration cfg;
  cfg.cu_cp_executor = &ctrl_worker;
  cfg.f1ap_notifier  = &f1ap_pdu_notifier;
  cfg.e1ap_notifier  = &e1ap_pdu_notifier;
  cfg.ngap_notifier  = &ngap_amf_notifier;

  cfg.ngap_config.ran_node_name = "srsgnb01";
  cfg.ngap_config.plmn          = "00101";
  cfg.ngap_config.tac           = 7;

  // create and start DUT
  auto dummy_cu_cp = std::make_unique<cu_cp>(std::move(cfg));
  dummy_cu_cp->handle_new_cu_up_connection();

  dummy_cu_cp->start();

  // check that CU-UP has been added
  ASSERT_EQ(dummy_cu_cp->get_nof_cu_ups(), 1U);
  ASSERT_EQ(e1ap_pdu_notifier.last_e1ap_msg.pdu.type(), asn1::e1ap::e1ap_pdu_c::types_opts::options::init_msg);
  ASSERT_EQ(e1ap_pdu_notifier.last_e1ap_msg.pdu.init_msg().value.type().value,
            asn1::e1ap::e1ap_elem_procs_o::init_msg_c::types_opts::gnb_cu_cp_e1_setup_request);
}

//////////////////////////////////////////////////////////////////////////////////////
/* DU connection handling                                                           */
//////////////////////////////////////////////////////////////////////////////////////

/// Test the DU connection
TEST_F(cu_cp_test, when_new_du_connection_then_du_added)
{
  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();

  // check that DU has been added
  ASSERT_EQ(cu_cp_obj->get_nof_dus(), 1U);
}

/// Test the DU removal
TEST_F(cu_cp_test, when_du_remove_request_received_then_du_removed)
{
  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();

  // Check that DU has been added
  ASSERT_EQ(cu_cp_obj->get_nof_dus(), 1U);

  // Remove DU
  cu_cp_obj->handle_du_remove_request(du_index_t::min);

  // Check that DU has been removed
  ASSERT_EQ(cu_cp_obj->get_nof_dus(), 0U);
}

/// Test exeeding the maximum number of connected DUs
TEST_F(cu_cp_test, when_max_nof_dus_connected_then_reject_new_connection)
{
  for (int it = du_index_to_uint(du_index_t::min); it < MAX_NOF_DUS; it++) {
    cu_cp_obj->handle_new_du_connection();
  }

  // Check that MAX_NOF_DUS are connected
  ASSERT_EQ(cu_cp_obj->get_nof_dus(), MAX_NOF_DUS);

  cu_cp_obj->handle_new_du_connection();

  // Check that MAX_NOF_DUS are connected
  ASSERT_EQ(cu_cp_obj->get_nof_dus(), MAX_NOF_DUS);
}

//////////////////////////////////////////////////////////////////////////////////////
/* CU-UP connection handling                                                        */
//////////////////////////////////////////////////////////////////////////////////////

/// Test the CU-UP connection
TEST_F(cu_cp_test, when_new_cu_up_connection_then_cu_up_added)
{
  // Connect CU-UP
  cu_cp_obj->handle_new_cu_up_connection();

  // check that CU-UP has been added
  ASSERT_EQ(cu_cp_obj->get_nof_cu_ups(), 1U);
}

/// Test the CU-UP removal
TEST_F(cu_cp_test, when_cu_up_remove_request_received_then_cu_up_removed)
{
  // Connect CU-UP
  cu_cp_obj->handle_new_cu_up_connection();

  // Check that CU-UP has been added
  ASSERT_EQ(cu_cp_obj->get_nof_cu_ups(), 1U);

  // Remove CU-UP
  // FIXME: This is scheduled but never run
  cu_cp_obj->handle_cu_up_remove_request(cu_up_index_t::min);

  // Check that CU-UP has been removed
  ASSERT_EQ(cu_cp_obj->get_nof_cu_ups(), 0U);
}

/// Test exeeding the maximum number of connected CU-UPs
TEST_F(cu_cp_test, when_max_nof_cu_ups_connected_then_reject_new_connection)
{
  for (int it = cu_up_index_to_uint(cu_up_index_t::min); it < MAX_NOF_CU_UPS; it++) {
    cu_cp_obj->handle_new_cu_up_connection();
  }

  // Check that MAX_NOF_CU_UPS are connected
  ASSERT_EQ(cu_cp_obj->get_nof_cu_ups(), MAX_NOF_CU_UPS);

  cu_cp_obj->handle_new_cu_up_connection();

  // Check that MAX_NOF_CU_UPS are connected
  ASSERT_EQ(cu_cp_obj->get_nof_cu_ups(), MAX_NOF_CU_UPS);
}

//////////////////////////////////////////////////////////////////////////////////////
/* AMF connection handling                                                          */
//////////////////////////////////////////////////////////////////////////////////////

TEST_F(cu_cp_test, when_ng_setup_response_received_then_amf_connected)
{
  // Connect AMF by injecting a ng_setup_response
  ngap_message ngap_msg = generate_ng_setup_response();
  cu_cp_obj->get_ngap_message_handler().handle_message(ngap_msg);

  ASSERT_TRUE(cu_cp_obj->amf_is_connected());
}

TEST_F(cu_cp_test, when_amf_connected_then_ue_added)
{
  // Connect AMF by injecting a ng_setup_response
  ngap_message ngap_msg = generate_ng_setup_response();
  cu_cp_obj->get_ngap_message_handler().handle_message(ngap_msg);

  ASSERT_TRUE(cu_cp_obj->amf_is_connected());

  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();
  // Connect CU-UP
  cu_cp_obj->handle_new_cu_up_connection();

  // Generate F1SetupRequest
  f1ap_message f1setup_msg = generate_f1_setup_request();

  // Pass message to CU-CP
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(f1setup_msg);

  // Inject Initial UL RRC message
  f1ap_message init_ul_rrc_msg = generate_init_ul_rrc_message_transfer(int_to_gnb_du_ue_f1ap_id(41255));
  test_logger.info("Injecting Initial UL RRC message");
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(init_ul_rrc_msg);

  // Inject UL RRC message containing RRC Setup Complete
  f1ap_message ul_rrc_msg = generate_ul_rrc_message_transfer(
      int_to_gnb_cu_ue_f1ap_id(0), int_to_gnb_du_ue_f1ap_id(41255), srb_id_t::srb1, generate_rrc_setup_complete());
  test_logger.info("Injecting UL RRC message (RRC Setup Complete)");
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(ul_rrc_msg);

  // check that UE has been added
  ASSERT_EQ(cu_cp_obj->get_nof_ues(), 1U);

  // check that the Initial UE Message was sent to the AMF
  ASSERT_EQ(ngap_amf_notifier.last_ngap_msg.pdu.type(), asn1::ngap::ngap_pdu_c::types_opts::options::init_msg);
  ASSERT_EQ(ngap_amf_notifier.last_ngap_msg.pdu.init_msg().value.type().value,
            asn1::ngap::ngap_elem_procs_o::init_msg_c::types_opts::init_ue_msg);
  ASSERT_EQ(ngap_amf_notifier.last_ngap_msg.pdu.init_msg().value.init_ue_msg()->ran_ue_ngap_id.value.value, 0);
}

TEST_F(cu_cp_test, when_amf_not_connected_then_ue_rejected)
{
  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();
  // Connect CU-UP
  cu_cp_obj->handle_new_cu_up_connection();

  // Generate F1SetupRequest
  f1ap_message f1setup_msg = generate_f1_setup_request();

  // Pass message to CU-CP
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(f1setup_msg);

  gnb_cu_ue_f1ap_id_t cu_ue_id = int_to_gnb_cu_ue_f1ap_id(0);
  gnb_du_ue_f1ap_id_t du_ue_id = int_to_gnb_du_ue_f1ap_id(41255);
  rnti_t              crnti    = to_rnti(0x4601);

  // Inject Initial UL RRC message
  f1ap_message init_ul_rrc_msg = generate_init_ul_rrc_message_transfer(du_ue_id, crnti);
  test_logger.info("Injecting Initial UL RRC message");
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(init_ul_rrc_msg);

  // Inject UE Context Release Complete message
  f1ap_message ue_context_release_complete_msg = generate_ue_context_release_complete(cu_ue_id, du_ue_id);
  test_logger.info("Injecting UE Context Release Complete message");
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(ue_context_release_complete_msg);

  // check that UE has not been added
  ASSERT_EQ(cu_cp_obj->get_nof_ues(), 0U);

  // check that the Initial UE Message was not send to the AMF
  ASSERT_NE(ngap_amf_notifier.last_ngap_msg.pdu.init_msg().value.type().value,
            asn1::ngap::ngap_elem_procs_o::init_msg_c::types_opts::init_ue_msg);
}

/// Test the f1 initial UL RRC message transfer procedure
TEST_F(cu_cp_test, when_amf_connection_drop_then_reject_ue)
{
  // Connect AMF by injecting a ng_setup_response
  ngap_message ngap_msg = generate_ng_setup_response();
  cu_cp_obj->get_ngap_message_handler().handle_message(ngap_msg);

  ASSERT_TRUE(cu_cp_obj->amf_is_connected());

  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();
  // Connect CU-UP
  cu_cp_obj->handle_new_cu_up_connection();

  // Generate F1SetupRequest
  f1ap_message f1setup_msg = generate_f1_setup_request();

  // Pass message to CU-CP
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(f1setup_msg);

  // Attach first UE (successful)
  {
    gnb_cu_ue_f1ap_id_t first_ue_cu_ue_id = int_to_gnb_cu_ue_f1ap_id(0);
    gnb_du_ue_f1ap_id_t first_ue_du_ue_id = int_to_gnb_du_ue_f1ap_id(41255);
    rnti_t              crnti             = to_rnti(0x4601);

    // Inject Initial UL RRC message
    f1ap_message init_ul_rrc_msg = generate_init_ul_rrc_message_transfer(first_ue_du_ue_id, crnti);
    test_logger.info("Injecting Initial UL RRC message");
    cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(init_ul_rrc_msg);

    // Inject UL RRC message containing RRC Setup Complete
    f1ap_message ul_rrc_msg = generate_ul_rrc_message_transfer(
        first_ue_cu_ue_id, first_ue_du_ue_id, srb_id_t::srb1, generate_rrc_setup_complete());
    test_logger.info("Injecting UL RRC message (RRC Setup Complete)");
    cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(ul_rrc_msg);

    // check that UE has been added
    ASSERT_EQ(cu_cp_obj->get_nof_ues(), 1U);
  }

  // Disconnect AMF
  cu_cp_obj->handle_amf_connection_drop();

  ASSERT_FALSE(cu_cp_obj->amf_is_connected());

  // Attach second UE (failure)
  {
    gnb_cu_ue_f1ap_id_t second_ue_cu_ue_id = int_to_gnb_cu_ue_f1ap_id(1);
    gnb_du_ue_f1ap_id_t second_ue_du_ue_id = int_to_gnb_du_ue_f1ap_id(41256);
    rnti_t              crnti              = to_rnti(0x4602);

    // Inject Initial UL RRC message
    f1ap_message init_ul_rrc_msg = generate_init_ul_rrc_message_transfer(second_ue_du_ue_id, crnti);
    test_logger.info("Injecting Initial UL RRC message");
    cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(init_ul_rrc_msg);

    // Inject UE Context Release Complete message
    f1ap_message ue_context_release_complete_msg =
        generate_ue_context_release_complete(second_ue_cu_ue_id, second_ue_du_ue_id);
    test_logger.info("Injecting UE Context Release Complete message");
    cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(ue_context_release_complete_msg);

    // The UE should not exists in the CU-CP
    ASSERT_EQ(cu_cp_obj->get_nof_ues(), 1U);

    // Check that UE has also been removed from F1AP
    ASSERT_EQ(cu_cp_obj->get_f1ap_statistics_handler(uint_to_du_index(0)).get_nof_ues(), 1);
  }
}

//////////////////////////////////////////////////////////////////////////////////////
/* Paging handling                                                                  */
//////////////////////////////////////////////////////////////////////////////////////

/// Test the handling of an paging message when a DU is not connected
TEST_F(cu_cp_test, when_du_connection_not_finished_then_paging_is_not_sent_to_du)
{
  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();

  // Generate Paging
  ngap_message paging_msg = generate_valid_minimal_paging_message();

  cu_cp_obj->get_ngap_message_handler().handle_message(paging_msg);

  ASSERT_FALSE(check_minimal_paging_result());
}

/// Test the handling of a valid Paging message with only mandatory values set
TEST_F(cu_cp_test, when_valid_paging_message_received_then_paging_is_sent_to_du)
{
  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();

  // Generate F1SetupRequest
  f1ap_message f1setup_msg = generate_f1_setup_request();
  // Pass message to CU-CP
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(f1setup_msg);

  // Generate Paging
  ngap_message paging_msg = generate_valid_minimal_paging_message();

  cu_cp_obj->get_ngap_message_handler().handle_message(paging_msg);

  ASSERT_TRUE(check_minimal_paging_result());
}

/// Test the handling of a valid Paging message with optional values set
TEST_F(cu_cp_test, when_valid_paging_message_with_optional_values_received_then_paging_is_sent_to_du)
{
  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();

  // Generate F1SetupRequest
  f1ap_message f1setup_msg = generate_f1_setup_request();
  // Pass message to CU-CP
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(f1setup_msg);

  // Generate Paging
  ngap_message paging_msg = generate_valid_paging_message();

  cu_cp_obj->get_ngap_message_handler().handle_message(paging_msg);

  ASSERT_TRUE(check_paging_result());
}

/// Test the handling of an invalid Paging message
TEST_F(cu_cp_test, when_no_du_for_tac_exists_then_paging_is_not_sent_to_du)
{
  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();

  // Generate F1SetupRequest
  f1ap_message f1setup_msg = generate_f1_setup_request();
  // Pass message to CU-CP
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(f1setup_msg);

  // Generate Paging with unknown tac
  ngap_message paging_msg = generate_valid_minimal_paging_message();
  paging_msg.pdu.init_msg().value.paging()->tai_list_for_paging.value[0].tai.tac.from_number(8);

  cu_cp_obj->get_ngap_message_handler().handle_message(paging_msg);

  ASSERT_FALSE(check_minimal_paging_result());
}

/// Test the handling of an invalid Paging message
TEST_F(cu_cp_test, when_assist_data_for_paging_for_unknown_tac_is_included_then_paging_is_not_sent_to_du)
{
  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();

  // Generate F1SetupRequest
  f1ap_message f1setup_msg = generate_f1_setup_request();
  // Pass message to CU-CP
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(f1setup_msg);

  // Generate Paging with unknown tac but assist data for paging
  ngap_message paging_msg = generate_valid_paging_message();
  paging_msg.pdu.init_msg().value.paging()->tai_list_for_paging.value[0].tai.tac.from_number(8);

  cu_cp_obj->get_ngap_message_handler().handle_message(paging_msg);

  ASSERT_FALSE(check_paging_result());
}

/// Test the handling of an invalid Paging message
TEST_F(cu_cp_test, when_invalid_paging_message_received_then_paging_is_not_sent_to_du)
{
  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();

  // Generate F1SetupRequest
  f1ap_message f1setup_msg = generate_f1_setup_request();
  // Pass message to CU-CP
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(f1setup_msg);

  // Generate Paging
  ngap_message paging_msg = generate_invalid_paging_message();

  cu_cp_obj->get_ngap_message_handler().handle_message(paging_msg);

  ASSERT_FALSE(check_paging_result());
}

//////////////////////////////////////////////////////////////////////////////////////
/* Inactivity Notification                                                          */
//////////////////////////////////////////////////////////////////////////////////////

/// Test the handling of a ue level inactivity notification
TEST_F(cu_cp_test, when_ue_level_inactivity_message_received_then_ue_context_release_request_is_sent)
{
  // Connect AMF by injecting a ng_setup_response
  ngap_message ngap_msg = generate_ng_setup_response();
  cu_cp_obj->get_ngap_message_handler().handle_message(ngap_msg);

  ASSERT_TRUE(cu_cp_obj->amf_is_connected());

  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();
  // Connect CU-UP
  cu_cp_obj->handle_new_cu_up_connection();

  // Generate F1SetupRequest
  f1ap_message f1setup_msg = generate_f1_setup_request();

  // Pass message to CU-CP
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(f1setup_msg);

  // Attach UE
  {
    gnb_cu_ue_f1ap_id_t cu_ue_id = int_to_gnb_cu_ue_f1ap_id(0);
    gnb_du_ue_f1ap_id_t du_ue_id = int_to_gnb_du_ue_f1ap_id(0);
    rnti_t              crnti    = to_rnti(0x4601);

    // Inject Initial UL RRC message
    f1ap_message init_ul_rrc_msg = generate_init_ul_rrc_message_transfer(du_ue_id, crnti);
    test_logger.info("Injecting Initial UL RRC message");
    cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(init_ul_rrc_msg);

    // Inject UL RRC message containing RRC Setup Complete
    f1ap_message ul_rrc_msg =
        generate_ul_rrc_message_transfer(cu_ue_id, du_ue_id, srb_id_t::srb1, generate_rrc_setup_complete());
    test_logger.info("Injecting UL RRC message (RRC Setup Complete)");
    cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(ul_rrc_msg);

    // check that UE has been added
    ASSERT_EQ(cu_cp_obj->get_nof_ues(), 1U);
  }

  cu_cp_inactivity_notification inactivity_notification;
  inactivity_notification.ue_index    = uint_to_ue_index(0);
  inactivity_notification.ue_inactive = true;

  cu_cp_obj->handle_bearer_context_inactivity_notification(inactivity_notification);

  // check that the UE Context Release Request was sent to the AMF
  ASSERT_EQ(ngap_amf_notifier.last_ngap_msg.pdu.type(), asn1::ngap::ngap_pdu_c::types_opts::options::init_msg);
  ASSERT_EQ(ngap_amf_notifier.last_ngap_msg.pdu.init_msg().value.type().value,
            asn1::ngap::ngap_elem_procs_o::init_msg_c::types_opts::ue_context_release_request);
  ASSERT_EQ(
      ngap_amf_notifier.last_ngap_msg.pdu.init_msg().value.ue_context_release_request()->cause.value.radio_network(),
      asn1::ngap::cause_radio_network_opts::options::user_inactivity);
}

/// Test the handling of an inactivity notification with unsupported activity level
TEST_F(cu_cp_test, when_unsupported_inactivity_message_received_then_ue_context_release_request_is_not_sent)
{
  // Connect AMF by injecting a ng_setup_response
  ngap_message ngap_msg = generate_ng_setup_response();
  cu_cp_obj->get_ngap_message_handler().handle_message(ngap_msg);

  ASSERT_TRUE(cu_cp_obj->amf_is_connected());

  // Connect DU (note that this creates a DU processor, but the DU is only connected after the F1Setup procedure)
  cu_cp_obj->handle_new_du_connection();
  // Connect CU-UP
  cu_cp_obj->handle_new_cu_up_connection();

  // Generate F1SetupRequest
  f1ap_message f1setup_msg = generate_f1_setup_request();

  // Pass message to CU-CP
  cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(f1setup_msg);

  // Attach UE
  {
    gnb_cu_ue_f1ap_id_t cu_ue_id = int_to_gnb_cu_ue_f1ap_id(0);
    gnb_du_ue_f1ap_id_t du_ue_id = int_to_gnb_du_ue_f1ap_id(0);
    rnti_t              crnti    = to_rnti(0x4601);

    // Inject Initial UL RRC message
    f1ap_message init_ul_rrc_msg = generate_init_ul_rrc_message_transfer(du_ue_id, crnti);
    test_logger.info("Injecting Initial UL RRC message");
    cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(init_ul_rrc_msg);

    // Inject UL RRC message containing RRC Setup Complete
    f1ap_message ul_rrc_msg =
        generate_ul_rrc_message_transfer(cu_ue_id, du_ue_id, srb_id_t::srb1, generate_rrc_setup_complete());
    test_logger.info("Injecting UL RRC message (RRC Setup Complete)");
    cu_cp_obj->get_f1ap_message_handler(uint_to_du_index(0)).handle_message(ul_rrc_msg);

    // check that UE has been added
    ASSERT_EQ(cu_cp_obj->get_nof_ues(), 1U);
  }

  cu_cp_inactivity_notification inactivity_notification;
  inactivity_notification.ue_index    = uint_to_ue_index(0);
  inactivity_notification.ue_inactive = false;

  cu_cp_obj->handle_bearer_context_inactivity_notification(inactivity_notification);

  // check that the UE Context Release Request was not sent to the AMF
  ASSERT_NE(ngap_amf_notifier.last_ngap_msg.pdu.init_msg().value.type().value,
            asn1::ngap::ngap_elem_procs_o::init_msg_c::types_opts::ue_context_release_request);
}