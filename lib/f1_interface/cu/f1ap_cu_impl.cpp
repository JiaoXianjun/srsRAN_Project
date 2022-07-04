/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "f1ap_cu_impl.h"
#include "../../ran/gnb_format.h"
#include "srsgnb/asn1/f1ap.h"
#include "srsgnb/support/async/event_signal.h"

using namespace srsgnb;
using namespace asn1::f1ap;
using namespace srs_cu_cp;

class srsgnb::srs_cu_cp::f1ap_cu_impl::f1ap_event_manager
{
public:
  /// F1 Context Release Complete
  // FIXME handle static return value (only returns f1ap_ue_context_release_complete)
  using f1ap_ue_context_release_outcome_t =
      expected<const ue_context_release_complete_s*, const ue_context_setup_fail_s*>;
  event_signal<f1ap_ue_context_release_outcome_t> f1ap_ue_context_release_complete;

  /// F1 UE context setup procedure outcome.
  using f1ap_ue_context_setup_outcome_t = expected<const ue_context_setup_resp_s*, const ue_context_setup_fail_s*>;
  event_signal<f1ap_ue_context_setup_outcome_t> f1ap_ue_context_setup_response;

  /// F1 UE Context Modification procedure outcome.
  using f1ap_ue_context_modification_outcome_t = expected<const ue_context_mod_resp_s*, const ue_context_mod_fail_s*>;
  event_signal<f1ap_ue_context_modification_outcome_t> f1ap_ue_context_modification_response_message;
};

f1ap_cu_impl::f1ap_cu_impl(f1c_message_notifier&            f1c_pdu_notifier_,
                           f1c_initiating_message_notifier& f1c_init_message_notifier_) :
  logger(srslog::fetch_basic_logger("CU-F1AP")),
  pdu_notifier(f1c_pdu_notifier_),
  init_message_notifier(f1c_init_message_notifier_)
{
}

// Note: For fwd declaration of member types, dtor cannot be trivial.
f1ap_cu_impl::~f1ap_cu_impl() {}

void f1ap_cu_impl::handle_f1ap_setup_response(const f1_setup_response_message& msg)
{
  // Pack message into PDU
  f1c_msg f1c_msg;
  if (msg.success) {
    // TODO send response
    logger.info("Transmitting F1SetupResponse message");

    f1c_msg.pdu.set_successful_outcome();
    f1c_msg.pdu.successful_outcome().load_info_obj(ASN1_F1AP_ID_F1_SETUP);
    f1c_msg.pdu.successful_outcome().value.f1_setup_resp() = msg.response;

    // set values handled by F1
    f1c_msg.pdu.successful_outcome().value.f1_setup_resp()->transaction_id.value = 99;
  } else {
    logger.info("Transmitting F1SetupFailure message");
    f1c_msg.pdu.set_unsuccessful_outcome();
    f1c_msg.pdu.unsuccessful_outcome().load_info_obj(ASN1_F1AP_ID_F1_SETUP);
    f1c_msg.pdu.unsuccessful_outcome().value.f1_setup_fail() = msg.failure;
    auto& setup_fail                                         = f1c_msg.pdu.unsuccessful_outcome().value.f1_setup_fail();

    // set values handled by F1
    setup_fail->transaction_id.value = 99;
    setup_fail->cause.value.set_radio_network();
    setup_fail->cause.value.radio_network() = asn1::f1ap::cause_radio_network_opts::options::no_radio_res_available;
  }

  // send response
  pdu_notifier.on_new_message(f1c_msg);
}

void f1ap_cu_impl::handle_dl_rrc_message_transfer(const f1ap_dl_rrc_msg& msg)
{
  logger.info("Transmitting DL RRC message");
  // Pack message into PDU
  f1c_msg f1c_msg;
  f1c_msg.pdu.set_init_msg();
  f1c_msg.pdu.init_msg().load_info_obj(ASN1_F1AP_ID_DLRRC_MSG_TRANSFER);
  f1c_msg.pdu.init_msg().value.dlrrc_msg_transfer() = msg.msg;

  if (logger.debug.enabled()) {
    asn1::json_writer js;
    f1c_msg.pdu.to_json(js);
    logger.debug("Containerized DL RRC message: {}", js.to_string());
  }

  // send DL RRC message
  pdu_notifier.on_new_message(f1c_msg);
}

async_task<f1ap_ue_context_setup_response_message>
f1ap_cu_impl::handle_ue_context_setup_request(const f1ap_ue_context_setup_request_message& request)
{
  f1ap_event_manager::f1ap_ue_context_setup_outcome_t f1_ue_ctx_setup_resp;

  // TODO: send msg

  return launch_async([this, f1_ue_ctx_setup_resp, res = f1ap_ue_context_setup_response_message{}, request](
                          coro_context<async_task<f1ap_ue_context_setup_response_message>>& ctx) mutable {
    CORO_BEGIN(ctx);

    CORO_AWAIT_VALUE(f1_ue_ctx_setup_resp, events->f1ap_ue_context_setup_response);

    if (f1_ue_ctx_setup_resp.has_value()) {
      logger.info("Received F1AP PDU with successful outcome.");
      res.msg     = *f1_ue_ctx_setup_resp.value();
      res.success = true;
    } else {
      logger.info("Received F1AP PDU with unsuccessful outcome.");
      res.success = false;
    }

    CORO_RETURN(res);
  });
}

async_task<f1ap_ue_context_release_complete_message>
f1ap_cu_impl::handle_ue_context_release(const f1ap_ue_context_release_command_message& msg)
{
  // TODO: send msg

  f1ap_event_manager::f1ap_ue_context_release_outcome_t f1ap_ue_ctxt_rel_complete;

  return launch_async([this, f1ap_ue_ctxt_rel_complete, res = f1ap_ue_context_release_complete_message{}, msg](
                          coro_context<async_task<f1ap_ue_context_release_complete_message>>& ctx) mutable {
    CORO_BEGIN(ctx);

    CORO_AWAIT_VALUE(f1ap_ue_ctxt_rel_complete, events->f1ap_ue_context_release_complete);
    res.msg = *f1ap_ue_ctxt_rel_complete.value();

    CORO_RETURN(res);
  });
}

async_task<f1ap_ue_context_modification_response_message>
f1ap_cu_impl::handle_ue_context_modification(const f1ap_ue_context_modification_request_message& request)
{
  f1ap_event_manager::f1ap_ue_context_modification_outcome_t f1ap_ue_ctx_mod_resp;

  // TODO: send msg

  return launch_async([this, f1ap_ue_ctx_mod_resp, res = f1ap_ue_context_modification_response_message{}, request](
                          coro_context<async_task<f1ap_ue_context_modification_response_message>>& ctx) mutable {
    CORO_BEGIN(ctx);

    CORO_AWAIT_VALUE(f1ap_ue_ctx_mod_resp, events->f1ap_ue_context_modification_response_message);

    if (f1ap_ue_ctx_mod_resp.has_value()) {
      logger.info("Received F1AP PDU with successful outcome.");
      res.response = *f1ap_ue_ctx_mod_resp.value();
      res.success  = true;
    } else {
      logger.info("Received F1AP PDU with unsuccessful outcome.");
      res.failure = *f1ap_ue_ctx_mod_resp.error();
      res.success = false;
    }

    CORO_RETURN(res);
  });
}

void f1ap_cu_impl::handle_message(const f1c_msg& msg)
{
  logger.info("Handling F1AP PDU of type {}", msg.pdu.type().to_string());

  switch (msg.pdu.type().value) {
    case asn1::f1ap::f1_ap_pdu_c::types_opts::init_msg:
      handle_initiating_message(msg.pdu.init_msg());
      break;
    default:
      logger.error("Invalid PDU type");
      break;
  }
}

void f1ap_cu_impl::handle_initiating_message(const asn1::f1ap::init_msg_s& msg)
{
  switch (msg.value.type().value) {
    case asn1::f1ap::f1_ap_elem_procs_o::init_msg_c::types_opts::options::f1_setup_request: {
      f1_setup_request_message req_msg = {};
      req_msg.request                  = msg.value.f1_setup_request();
      init_message_notifier.on_f1_setup_request_received(req_msg);
    } break;
    case asn1::f1ap::f1_ap_elem_procs_o::init_msg_c::types_opts::init_ulrrc_msg_transfer: {
      f1ap_initial_ul_rrc_msg ul_transfer = {};
      ul_transfer.msg                     = msg.value.init_ulrrc_msg_transfer();
      init_message_notifier.on_initial_ul_rrc_message_transfer_received(ul_transfer);
    } break;
    case asn1::f1ap::f1_ap_elem_procs_o::init_msg_c::types_opts::ulrrc_msg_transfer: {
      f1ap_ul_rrc_msg ul_transfer = {};
      ul_transfer.msg             = msg.value.ulrrc_msg_transfer();
      init_message_notifier.on_ul_rrc_message_transfer_received(ul_transfer);
    } break;
    default:
      logger.error("Initiating message of type {} is not supported", msg.value.type().to_string());
  }
}
