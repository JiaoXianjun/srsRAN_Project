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

#include "srsran/phy/lower/lower_phy_error_notifier.h"
#include "srsran/phy/support/resource_grid_context.h"

namespace srsran {

/// \brief Implements a generic physical layer error adapter.
///
/// Currently, the adapter only logs the error and the context.
class phy_error_adapter : public lower_phy_error_notifier
{
private:
  /// Adapter logger.
  srslog::basic_logger& logger;

public:
  /// Creates an adapter with the desired logging level.
  phy_error_adapter(std::string log_level) : logger(srslog::fetch_basic_logger("Low-PHY"))
  {
    logger.set_level(srslog::str_to_basic_level(log_level));
  }

  // See interface for documentation.
  void on_late_resource_grid(const resource_grid_context& context) override
  {
    logger.set_context(context.slot.sfn(), context.slot.slot_index());
    logger.warning(
        "Real-time failure in low-phy: Downlink data late for sector {} and slot {}.", context.sector, context.slot);
  }

  void on_overflow_resource_grid(const resource_grid_context& context) override
  {
    logger.set_context(context.slot.sfn(), context.slot.slot_index());
    logger.info("Real-time failure in low-phy: Downlink data overflow for sector {} and slot {}.",
                context.sector,
                context.slot);
  }

  // See interface for documentation.
  void on_prach_request_late(const prach_buffer_context& context) override
  {
    logger.set_context(context.slot.sfn(), context.slot.slot_index());
    logger.warning("Real-time failure in low-phy: PRACH request late for sector {}, slot {} and start symbol {}.",
                   context.sector,
                   context.slot,
                   context.start_symbol);
  }

  // See interface for documentation.
  void on_prach_request_overflow(const prach_buffer_context& context) override
  {
    logger.set_context(context.slot.sfn(), context.slot.slot_index());
    logger.warning("Real-time failure in low-phy: PRACH request overflow for sector {}, slot {} and start symbol {}.",
                   context.sector,
                   context.slot,
                   context.start_symbol);
  }

  // See interface for documentation.
  void on_puxch_request_late(const resource_grid_context& context) override
  {
    logger.set_context(context.slot.sfn(), context.slot.slot_index());
    logger.warning(
        "Real-time failure in low-phy: PUxCH request late for sector {}, slot {}.", context.sector, context.slot);
  }

  // See interface for documentation.
  void on_puxch_request_overflow(const resource_grid_context& context) override
  {
    logger.set_context(context.slot.sfn(), context.slot.slot_index());
    logger.warning(
        "Real-time failure in low-phy: PUxCH request overflow for sector {}, slot {}.", context.sector, context.slot);
  }
};

} // namespace srsran
