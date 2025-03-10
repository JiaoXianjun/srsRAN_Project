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

#include "srsran/gateways/baseband/baseband_gateway.h"
#include "srsran/phy/lower/amplitude_controller/amplitude_controller_factories.h"
#include "srsran/phy/lower/lower_phy_error_notifier.h"
#include "srsran/phy/lower/lower_phy_rx_symbol_notifier.h"
#include "srsran/phy/lower/lower_phy_timing_notifier.h"
#include "srsran/phy/lower/modulation/ofdm_demodulator.h"
#include "srsran/phy/lower/modulation/ofdm_modulator.h"
#include "srsran/phy/lower/sampling_rate.h"
#include "srsran/phy/support/resource_grid_pool.h"
#include "srsran/ran/cyclic_prefix.h"
#include "srsran/ran/n_ta_offset.h"
#include "srsran/ran/subcarrier_spacing.h"
#include "srsran/support/executors/task_executor.h"

namespace srsran {

/// Describes a sector configuration.
struct lower_phy_sector_description {
  /// Indicates the sector bandwidth in resource blocks.
  unsigned bandwidth_rb;
  /// Indicates the downlink frequency.
  double dl_freq_hz;
  /// Indicates the uplink frequency.
  double ul_freq_hz;
  /// Number of transmit ports.
  unsigned nof_tx_ports;
  /// Number of receive ports.
  unsigned nof_rx_ports;
};

/// Lower physical layer configuration.
struct lower_phy_configuration {
  /// Subcarrier spacing for the overall PHY.
  subcarrier_spacing scs;
  /// Cyclic prefix.
  cyclic_prefix cp;
  /// Shifts the DFT window by a fraction of the cyclic prefix [0, 1).
  float dft_window_offset;
  /// \brief Number of slots the timing handler is notified in advance of the transmission time.
  ///
  /// Sets the maximum allowed processing delay in slots.
  unsigned max_processing_delay_slots;
  /// \brief Indicates the UL-to-DL slot context offset.
  ///
  /// Determines the time offset between the UL and DL processes in subframes or, equivalently, with a granularity of
  /// one millisecond.
  ///
  /// An assertion is triggered if it is equal to zero.
  unsigned ul_to_dl_subframe_offset;
  /// Sampling rate.
  sampling_rate srate;
  /// Time alignment offset.
  n_ta_offset ta_offset;
  /// \brief Time alignment calibration in number of samples.
  ///
  /// Models the reception and transmission time misalignment inherent to the RF device. This time adjustment is
  /// subtracted from the UL-to-DL processing time offset for calibrating the baseband device.
  ///
  /// \remark Positive values cause a reduction of the RF transmission delay with respect to the RF reception, while
  /// negative values increase it.
  int time_alignment_calibration;
  /// Amplitude control parameters, including baseband gain and clipping.
  amplitude_controller_clipping_config amplitude_config;
  /// Provides the sectors configuration.
  std::vector<lower_phy_sector_description> sectors;
  /// Provides the baseband gateway.
  baseband_gateway* bb_gateway;
  /// Provides a symbol handler to notify the reception of symbols.
  lower_phy_rx_symbol_notifier* rx_symbol_notifier;
  /// Provides the timing handler to notify the timing boundaries.
  lower_phy_timing_notifier* timing_notifier;
  /// Provides the error handler to notify runtime errors.
  lower_phy_error_notifier* error_notifier;
  /// Receive task executor.
  task_executor* rx_task_executor;
  /// Transmit task executor.
  task_executor* tx_task_executor;
  /// Downlink task executor.
  task_executor* dl_task_executor;
  /// Uplink task executor.
  task_executor* ul_task_executor;
  /// PRACH asynchronous task executor.
  task_executor* prach_async_executor;
};

/// Returns true if the given lower PHY configuration is valid, otherwise false.
inline bool is_valid_lower_phy_config(const lower_phy_configuration& config)
{
  // :TODO: Implement me!

  return true;
}

} // namespace srsran
