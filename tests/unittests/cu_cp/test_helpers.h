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

#include "du_processor_test_messages.h"
#include "lib/e1ap/common/e1ap_asn1_helpers.h"
#include "tests/unittests/e1ap/common/e1ap_cu_cp_test_messages.h"
#include "srsran/adt/variant.h"
#include "srsran/cu_cp/cu_cp.h"
#include "srsran/cu_cp/cu_cp_types.h"
#include "srsran/cu_cp/cu_up_processor.h"
#include "srsran/cu_cp/du_processor.h"
#include "srsran/support/async/async_task_loop.h"
#include "srsran/support/test_utils.h"
#include <list>

namespace srsran {
namespace srs_cu_cp {

/// \brief Generate DU-to-CU RRC Container with CellGroupConfig.
byte_buffer generate_container_with_cell_group_config();

/// \brief Generate RRC Container with RRC Setup Complete message.
byte_buffer generate_rrc_setup_complete();

/// \brief Generate a random gnb_cu_cp_ue_e1ap_id
gnb_cu_cp_ue_e1ap_id_t generate_random_gnb_cu_cp_ue_e1ap_id();

/// \brief Generate a random gnb_cu_up_ue_e1ap_id
gnb_cu_up_ue_e1ap_id_t generate_random_gnb_cu_up_ue_e1ap_id();

/// \brief Generate a random gnb_cu_ue_f1ap_id
gnb_cu_ue_f1ap_id_t generate_random_gnb_cu_ue_f1ap_id();

/// \brief Generate a random gnb_du_ue_f1ap_id
gnb_du_ue_f1ap_id_t generate_random_gnb_du_ue_f1ap_id();

struct dummy_du_processor_ue_task_scheduler : public du_processor_ue_task_scheduler {
public:
  dummy_du_processor_ue_task_scheduler(timer_manager& timers_, task_executor& exec_) : timer_db(timers_), exec(exec_) {}
  void schedule_async_task(ue_index_t ue_index, async_task<void>&& task) override
  {
    ctrl_loop.schedule(std::move(task));
  }
  unique_timer   make_unique_timer() override { return timer_db.create_unique_timer(exec); }
  timer_manager& get_timer_manager() override { return timer_db; }

  void tick_timer() { timer_db.tick(); }

private:
  async_task_sequencer ctrl_loop{16};
  timer_manager&       timer_db;
  task_executor&       exec;
};

struct dummy_du_processor_cu_cp_notifier : public du_processor_cu_cp_notifier {
public:
  explicit dummy_du_processor_cu_cp_notifier(cu_cp_du_handler* cu_cp_handler_ = nullptr) : cu_cp_handler(cu_cp_handler_)
  {
  }

  void attach_handler(cu_cp_du_handler* cu_cp_handler_) { cu_cp_handler = cu_cp_handler_; }

  void on_rrc_ue_created(du_index_t du_index, ue_index_t ue_index, rrc_ue_interface* rrc_ue) override
  {
    logger.info("Received a RRC UE creation notification");

    if (cu_cp_handler != nullptr) {
      cu_cp_handler->handle_rrc_ue_creation(du_index, ue_index, rrc_ue);
    }
  }

private:
  srslog::basic_logger& logger        = srslog::fetch_basic_logger("TEST");
  cu_cp_du_handler*     cu_cp_handler = nullptr;
};

// Stuct to configure Bearer Context Setup result content.
struct bearer_context_setup_outcome_t {
  bool                outcome = false;
  std::list<unsigned> pdu_sessions_success_list; // List of PDU session IDs that were successful to setup.
  std::list<unsigned> pdu_sessions_failed_list;
};

struct bearer_context_modification_outcome_t {
  bool outcome = false;
};

struct dummy_du_processor_e1ap_control_notifier : public du_processor_e1ap_control_notifier {
public:
  dummy_du_processor_e1ap_control_notifier() = default;

  void set_first_message_outcome(variant<bearer_context_setup_outcome_t, bearer_context_modification_outcome_t> outcome)
  {
    first_e1ap_message = outcome;
  }

  void set_second_message_outcome(bearer_context_modification_outcome_t outcome) { second_e1ap_message = outcome; }

  async_task<e1ap_bearer_context_setup_response>
  on_bearer_context_setup_request(const e1ap_bearer_context_setup_request& msg) override
  {
    logger.info("Received a new bearer context setup request");

    return launch_async([res = e1ap_bearer_context_setup_response{}, msg, this](
                            coro_context<async_task<e1ap_bearer_context_setup_response>>& ctx) mutable {
      CORO_BEGIN(ctx);

      const auto& result = variant_get<bearer_context_setup_outcome_t>(first_e1ap_message);
      res.success        = result.outcome;
      if (res.success) {
        for (const auto& id : result.pdu_sessions_success_list) {
          // add only the most relevant items
          e1ap_pdu_session_resource_setup_modification_item res_setup_item;
          res_setup_item.pdu_session_id = uint_to_pdu_session_id(id);

          // add a single DRB
          drb_id_t                   drb_id = drb_id_t::drb1;
          e1ap_drb_setup_item_ng_ran drb_item;
          drb_item.drb_id = drb_id;

          // add one UP transport item
          e1ap_up_params_item up_item;
          drb_item.ul_up_transport_params.push_back(up_item);
          res_setup_item.drb_setup_list_ng_ran.emplace(drb_id, drb_item);

          res.pdu_session_resource_setup_list.emplace(res_setup_item.pdu_session_id, res_setup_item);
        }

        for (const auto& id : result.pdu_sessions_failed_list) {
          e1ap_pdu_session_resource_failed_item res_failed_item;
          res_failed_item.pdu_session_id = uint_to_pdu_session_id(id);
          res.pdu_session_resource_failed_list.emplace(res_failed_item.pdu_session_id, res_failed_item);
        }
      }

      CORO_RETURN(res);
    });
  }

  async_task<e1ap_bearer_context_modification_response>
  on_bearer_context_modification_request(const e1ap_bearer_context_modification_request& msg) override
  {
    logger.info("Received a new bearer context modification request");

    return launch_async([res = e1ap_bearer_context_modification_response{},
                         this](coro_context<async_task<e1ap_bearer_context_modification_response>>& ctx) mutable {
      CORO_BEGIN(ctx);

      if (variant_holds_alternative<bearer_context_modification_outcome_t>(first_e1ap_message)) {
        // first E1AP message is already a bearer modification
        const auto& result = variant_get<bearer_context_modification_outcome_t>(first_e1ap_message);
        res.success        = result.outcome;
      } else {
        // second E1AP message is the bearer modification
        res.success = second_e1ap_message.outcome;
      }

      if (res.success) {
        // generate random ids to make sure its not only working for hardcoded values
        gnb_cu_cp_ue_e1ap_id_t cu_cp_ue_e1ap_id = generate_random_gnb_cu_cp_ue_e1ap_id();
        gnb_cu_up_ue_e1ap_id_t cu_up_ue_e1ap_id = generate_random_gnb_cu_up_ue_e1ap_id();

        res = generate_e1ap_bearer_context_modification_response(cu_cp_ue_e1ap_id, cu_up_ue_e1ap_id);
      }

      CORO_RETURN(res);
    });
  }

  virtual async_task<void> on_bearer_context_release_command(const e1ap_bearer_context_release_command& cmd) override
  {
    logger.info("Received a new bearer context release command");

    return launch_async([](coro_context<async_task<void>>& ctx) mutable {
      CORO_BEGIN(ctx);
      CORO_RETURN();
    });
  }

private:
  srslog::basic_logger& logger = srslog::fetch_basic_logger("TEST");
  variant<bearer_context_setup_outcome_t, bearer_context_modification_outcome_t> first_e1ap_message;
  bearer_context_modification_outcome_t                                          second_e1ap_message;
};

struct dummy_du_processor_ngap_control_notifier : public du_processor_ngap_control_notifier {
public:
  dummy_du_processor_ngap_control_notifier() = default;

  virtual void on_ue_context_release_request(const cu_cp_ue_context_release_request& msg) override
  {
    logger.info("Received a UE Context Release Request");
  }

private:
  srslog::basic_logger& logger = srslog::fetch_basic_logger("TEST");
};

struct dummy_du_processor_f1ap_ue_context_notifier : public du_processor_f1ap_ue_context_notifier {
public:
  dummy_du_processor_f1ap_ue_context_notifier() = default;

  void set_ue_context_setup_outcome(bool outcome) { ue_context_setup_outcome = outcome; }

  void set_ue_context_modification_outcome(bool outcome) { ue_context_modification_outcome = outcome; }

  async_task<f1ap_ue_context_setup_response>
  on_ue_context_setup_request(const f1ap_ue_context_setup_request& request) override
  {
    logger.info("Received a new UE context setup request");

    return launch_async([res = f1ap_ue_context_setup_response{},
                         this](coro_context<async_task<f1ap_ue_context_setup_response>>& ctx) mutable {
      CORO_BEGIN(ctx);

      res.success = ue_context_setup_outcome;

      CORO_RETURN(res);
    });
  }

  async_task<cu_cp_ue_context_modification_response>
  on_ue_context_modification_request(const cu_cp_ue_context_modification_request& request) override
  {
    logger.info("Received a new UE context modification request");

    return launch_async([res = cu_cp_ue_context_modification_response{},
                         this](coro_context<async_task<cu_cp_ue_context_modification_response>>& ctx) mutable {
      CORO_BEGIN(ctx);

      if (ue_context_modification_outcome) {
        // generate random ids to make sure its not only working for hardcoded values
        gnb_cu_ue_f1ap_id_t cu_ue_f1ap_id = generate_random_gnb_cu_ue_f1ap_id();
        gnb_du_ue_f1ap_id_t du_ue_f1ap_id = generate_random_gnb_du_ue_f1ap_id();

        res = generate_cu_cp_ue_context_modification_response(cu_ue_f1ap_id, du_ue_f1ap_id);
      } else {
        res.success = false;
      }

      CORO_RETURN(res);
    });
  }

  async_task<ue_index_t> on_ue_context_release_command(const f1ap_ue_context_release_command& msg) override
  {
    logger.info("Received a new UE context release command");

    return launch_async([msg](coro_context<async_task<ue_index_t>>& ctx) mutable {
      CORO_BEGIN(ctx);
      CORO_RETURN(msg.ue_index);
    });
  }

private:
  srslog::basic_logger& logger                          = srslog::fetch_basic_logger("TEST");
  bool                  ue_context_setup_outcome        = false;
  bool                  ue_context_modification_outcome = false;
};

struct dummy_du_processor_rrc_ue_control_message_notifier : public du_processor_rrc_ue_control_message_notifier {
public:
  dummy_du_processor_rrc_ue_control_message_notifier() = default;

  void set_rrc_reconfiguration_outcome(bool outcome) { rrc_reconfiguration_outcome = outcome; }

  void on_new_guami(const guami& msg) override { logger.info("Received a new GUAMI"); }

  async_task<bool> on_ue_capability_transfer_request(const cu_cp_ue_capability_transfer_request& msg) override
  {
    logger.info("Received a new UE capability transfer request");

    return launch_async([this](coro_context<async_task<bool>>& ctx) mutable {
      CORO_BEGIN(ctx);
      CORO_RETURN(ue_cap_transfer_outcome);
    });
  }

  async_task<bool> on_rrc_reconfiguration_request(const cu_cp_rrc_reconfiguration_procedure_request& msg) override
  {
    logger.info("Received a new RRC reconfiguration request");

    return launch_async([this](coro_context<async_task<bool>>& ctx) mutable {
      CORO_BEGIN(ctx);
      CORO_RETURN(rrc_reconfiguration_outcome);
    });
  }

  void on_rrc_ue_release() override { logger.info("Received a new RRC UE Release request"); }

private:
  srslog::basic_logger& logger                      = srslog::fetch_basic_logger("TEST");
  bool                  ue_cap_transfer_outcome     = true;
  bool                  rrc_reconfiguration_outcome = false;
};

struct dummy_du_processor_rrc_du_ue_notifier : public du_processor_rrc_du_ue_notifier {
public:
  dummy_du_processor_rrc_du_ue_notifier() = default;

  rrc_ue_interface* on_ue_creation_request(const rrc_ue_creation_message& msg) override
  {
    logger.info("Received a UE creation request");
    return nullptr;
  }

  void on_ue_context_release_command(ue_index_t ue_index) override { logger.info("Received a UE Release Command"); };

  /// Send RRC Release to all UEs connected to this DU.
  void on_release_ues() override { logger.info("Releasing all UEs"); };

private:
  srslog::basic_logger& logger = srslog::fetch_basic_logger("TEST");
};

struct dummy_cu_up_processor_cu_up_management_notifier : public cu_up_processor_cu_up_management_notifier {
public:
  dummy_cu_up_processor_cu_up_management_notifier() = default;

  void on_new_cu_up_connection() override { logger.info("Received a new CU-UP connection notification"); }

  void on_cu_up_remove_request_received(const cu_up_index_t cu_up_index) override
  {
    logger.info("Received a CU-UP remove request for cu_up_index={}", cu_up_index);
    last_cu_up_index_to_remove = cu_up_index;
  }

  cu_up_index_t last_cu_up_index_to_remove;

private:
  srslog::basic_logger& logger = srslog::fetch_basic_logger("TEST");
};

struct dummy_cu_up_processor_task_scheduler : public cu_up_processor_task_scheduler {
public:
  dummy_cu_up_processor_task_scheduler(timer_manager& timers_, task_executor& exec_) : timer_db(timers_), exec(exec_) {}

  void schedule_async_task(cu_up_index_t cu_up_index, async_task<void>&& task) override
  {
    ctrl_loop.schedule(std::move(task));
  }
  unique_timer   make_unique_timer() override { return timer_db.create_unique_timer(exec); }
  timer_manager& get_timer_manager() override { return timer_db; }

  void tick_timer() { timer_db.tick(); }

private:
  async_task_sequencer ctrl_loop{16};
  timer_manager&       timer_db;
  task_executor&       exec;
};

struct dummy_cu_up_processor_e1ap_control_notifier : public cu_up_processor_e1ap_control_notifier {
public:
  dummy_cu_up_processor_e1ap_control_notifier() = default;

  void set_cu_cp_e1_setup_outcome(bool outcome) { cu_cp_e1_setup_outcome = outcome; }

  async_task<cu_cp_e1_setup_response> on_cu_cp_e1_setup_request(const cu_cp_e1_setup_request& request) override
  {
    logger.info("Received a new CU-CP E1 setup request");

    return launch_async([this](coro_context<async_task<cu_cp_e1_setup_response>>& ctx) mutable {
      CORO_BEGIN(ctx);

      cu_cp_e1_setup_response res;
      res.success = cu_cp_e1_setup_outcome;
      if (cu_cp_e1_setup_outcome) {
        fill_e1ap_cu_cp_e1_setup_response(
            res, generate_cu_cp_e1_setup_respose(0).pdu.successful_outcome().value.gnb_cu_cp_e1_setup_resp());
      } else {
        fill_e1ap_cu_cp_e1_setup_response(
            res, generate_cu_cp_e1_setup_failure(0).pdu.unsuccessful_outcome().value.gnb_cu_cp_e1_setup_fail());
      }

      CORO_RETURN(res);
    });
  }

private:
  bool cu_cp_e1_setup_outcome = false;

  srslog::basic_logger& logger = srslog::fetch_basic_logger("TEST");
};

} // namespace srs_cu_cp
} // namespace srsran
