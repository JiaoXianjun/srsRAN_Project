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

#include "srsran/ran/prach/prach_configuration.h"

using namespace srsran;

static prach_configuration prach_configuration_get_fr1_paired(uint8_t prach_config_index)
{
  // TS38.211 Table 6.3.3.2-2.
  static const std::array<prach_configuration, 108> table = {{
      {prach_format_type::zero, 16, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::zero, 16, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::zero, 16, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::zero, 16, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::zero, 8, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::zero, 8, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::zero, 8, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::zero, 8, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::zero, 4, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::zero, 4, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::zero, 4, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::zero, 4, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::zero, 2, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::zero, 2, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::zero, 2, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::zero, 2, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {1}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {4}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {7}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {1, 6}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {2, 7}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {3, 8}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {1, 4, 7}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {2, 5, 8}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {3, 6, 9}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {0, 2, 4, 6, 8}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {1, 3, 5, 7, 9}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, 0, 0, 0, 0},
      {prach_format_type::one, 16, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::one, 16, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::one, 16, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::one, 16, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::one, 8, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::one, 8, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::one, 8, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::one, 8, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::one, 4, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::one, 4, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::one, 4, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::one, 4, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::one, 2, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::one, 2, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::one, 2, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::one, 2, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::one, 1, 0, {1}, 0, 0, 0, 0},
      {prach_format_type::one, 1, 0, {4}, 0, 0, 0, 0},
      {prach_format_type::one, 1, 0, {7}, 0, 0, 0, 0},
      {prach_format_type::one, 1, 0, {1, 6}, 0, 0, 0, 0},
      {prach_format_type::one, 1, 0, {2, 7}, 0, 0, 0, 0},
      {prach_format_type::one, 1, 0, {3, 8}, 0, 0, 0, 0},
      {prach_format_type::one, 1, 0, {1, 4, 7}, 0, 0, 0, 0},
      {prach_format_type::one, 1, 0, {2, 5, 8}, 0, 0, 0, 0},
      {prach_format_type::one, 1, 0, {3, 6, 9}, 0, 0, 0, 0},
      {prach_format_type::two, 16, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::two, 8, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::two, 4, 0, {1}, 0, 0, 0, 0},
      {prach_format_type::two, 2, 0, {1}, 0, 0, 0, 0},
      {prach_format_type::two, 2, 0, {5}, 0, 0, 0, 0},
      {prach_format_type::two, 1, 0, {1}, 0, 0, 0, 0},
      {prach_format_type::two, 1, 0, {5}, 0, 0, 0, 0},
      {prach_format_type::three, 16, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::three, 16, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::three, 16, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::three, 16, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::three, 8, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::three, 8, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::three, 8, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::three, 4, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::three, 4, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::three, 4, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::three, 4, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::three, 2, 1, {1}, 0, 0, 0, 0},
      {prach_format_type::three, 2, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::three, 2, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::three, 2, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {1}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {4}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {7}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {1, 6}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {2, 7}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {3, 8}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {1, 4, 7}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {2, 5, 8}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {3, 6, 9}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {0, 2, 4, 6, 8}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {1, 3, 5, 7, 9}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, 0, 0, 0, 0},
      {prach_format_type::A1, 16, 0, {4, 9}, 0, 1, 6, 2},
      {prach_format_type::A1, 16, 1, {4}, 0, 2, 6, 2},
      {prach_format_type::A1, 8, 0, {4, 9}, 0, 1, 6, 2},
      {prach_format_type::A1, 8, 1, {4}, 0, 2, 6, 2},
      {prach_format_type::A1, 4, 0, {4, 9}, 0, 1, 6, 2},
      {prach_format_type::A1, 4, 1, {4, 9}, 0, 1, 6, 2},
      {prach_format_type::A1, 4, 0, {4}, 0, 2, 6, 2},
      {prach_format_type::A1, 2, 0, {4, 9}, 0, 1, 6, 2},
      {prach_format_type::A1, 2, 0, {1}, 0, 2, 6, 2},
      {prach_format_type::A1, 2, 0, {4}, 0, 2, 6, 2},
      {prach_format_type::A1, 2, 0, {7}, 0, 2, 6, 2},
      {prach_format_type::A1, 1, 0, {4}, 0, 1, 6, 2},
      {prach_format_type::A1, 1, 0, {1, 6}, 0, 1, 6, 2},
      {prach_format_type::A1, 1, 0, {4, 9}, 0, 1, 6, 2},
      {prach_format_type::A1, 1, 0, {1}, 0, 2, 6, 2},
      {prach_format_type::A1, 1, 0, {7}, 0, 2, 6, 2},
      {prach_format_type::A1, 1, 0, {2, 7}, 0, 2, 6, 2},
      {prach_format_type::A1, 1, 0, {1, 4, 7}, 0, 2, 6, 2},
      {prach_format_type::A1, 1, 0, {0, 2, 4, 6, 8}, 0, 2, 6, 2},
      {prach_format_type::A1, 1, 0, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, 0, 2, 6, 2},
      {prach_format_type::A1, 1, 0, {1, 3, 5, 7, 9}, 0, 2, 6, 2},
  }};

  if (prach_config_index < table.size()) {
    return table[prach_config_index];
  }

  return PRACH_CONFIG_RESERVED;
}

static prach_configuration prach_configuration_get_fr1_unpaired(uint8_t prach_config_index)
{
  // TS38.211 Table 6.3.3.2-3.
  static const std::array<prach_configuration, 87> table = {{
      {prach_format_type::zero, 16, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::zero, 8, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::zero, 4, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::zero, 2, 0, {9}, 0, 0, 0, 0},
      {prach_format_type::zero, 2, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::zero, 2, 0, {4}, 0, 0, 0, 0},
      {prach_format_type::zero, 2, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {9}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {8}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {7}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {6}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {5}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {4}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {3}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {2}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {1, 6}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {1, 6}, 7, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {4, 9}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {3, 8}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {2, 7}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {8, 9}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {4, 8, 9}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {3, 4, 9}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {7, 8, 9}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {3, 4, 8, 9}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {6, 7, 8, 9}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {1, 4, 6, 9}, 0, 0, 0, 0},
      {prach_format_type::zero, 1, 0, {1, 3, 5, 7, 9}, 0, 0, 0, 0},
      {prach_format_type::one, 16, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::one, 8, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::one, 4, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::one, 2, 0, {7}, 0, 0, 0, 0},
      {prach_format_type::one, 2, 1, {7}, 0, 0, 0, 0},
      {prach_format_type::one, 1, 0, {7}, 0, 0, 0, 0},
      {prach_format_type::two, 16, 1, {6}, 0, 0, 0, 0},
      {prach_format_type::two, 8, 1, {6}, 0, 0, 0, 0},
      {prach_format_type::two, 4, 1, {6}, 0, 0, 0, 0},
      {prach_format_type::two, 2, 0, {6}, 7, 0, 0, 0},
      {prach_format_type::two, 2, 1, {6}, 7, 0, 0, 0},
      {prach_format_type::two, 1, 0, {6}, 7, 0, 0, 0},
      {prach_format_type::three, 16, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::three, 8, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::three, 4, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::three, 2, 0, {9}, 0, 0, 0, 0},
      {prach_format_type::three, 2, 1, {9}, 0, 0, 0, 0},
      {prach_format_type::three, 2, 0, {4}, 0, 0, 0, 0},
      {prach_format_type::three, 2, 1, {4}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {9}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {8}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {7}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {6}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {5}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {4}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {3}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {2}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {1, 6}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {1, 6}, 7, 0, 0, 0},
      {prach_format_type::three, 1, 0, {4, 9}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {3, 8}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {2, 7}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {8, 9}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {4, 8, 9}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {3, 4, 9}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {7, 8, 9}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {3, 4, 8, 9}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {1, 4, 6, 9}, 0, 0, 0, 0},
      {prach_format_type::three, 1, 0, {1, 3, 5, 7, 9}, 0, 0, 0, 0},
      {prach_format_type::A1, 16, 1, {9}, 0, 2, 6, 2},
      {prach_format_type::A1, 8, 1, {9}, 0, 2, 6, 2},
      {prach_format_type::A1, 4, 1, {9}, 0, 1, 6, 2},
      {prach_format_type::A1, 2, 1, {9}, 0, 1, 6, 2},
      {prach_format_type::A1, 2, 1, {4, 9}, 7, 1, 3, 2},
      {prach_format_type::A1, 2, 1, {7, 9}, 7, 1, 3, 2},
      {prach_format_type::A1, 2, 1, {7, 9}, 0, 1, 6, 2},
      {prach_format_type::A1, 2, 1, {8, 9}, 0, 2, 6, 2},
      {prach_format_type::A1, 2, 1, {4, 9}, 0, 2, 6, 2},
      {prach_format_type::A1, 2, 1, {2, 3, 4, 7, 8, 9}, 0, 1, 6, 2},
      {prach_format_type::A1, 1, 0, {9}, 0, 2, 6, 2},
      {prach_format_type::A1, 1, 0, {9}, 7, 1, 3, 2},
      {prach_format_type::A1, 1, 0, {9}, 0, 1, 6, 2},
      {prach_format_type::A1, 1, 0, {8, 9}, 0, 2, 6, 2},
      {prach_format_type::A1, 1, 0, {4, 9}, 0, 1, 6, 2},
      {prach_format_type::A1, 1, 0, {7, 9}, 7, 1, 3, 2},
      {prach_format_type::A1, 1, 0, {3, 4, 8, 9}, 0, 1, 6, 2},
      {prach_format_type::A1, 1, 0, {3, 4, 8, 9}, 0, 2, 6, 2},
      {prach_format_type::A1, 1, 0, {1, 3, 5, 7, 9}, 0, 1, 6, 2},
      {prach_format_type::A1, 1, 0, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, 7, 1, 3, 2},
  }};

  if (prach_config_index < table.size()) {
    return table[prach_config_index];
  }

  return PRACH_CONFIG_RESERVED;
}

prach_configuration srsran::prach_configuration_get(frequency_range fr, duplex_mode dm, uint8_t prach_config_index)
{
  if ((fr == frequency_range::FR1) && (dm == duplex_mode::FDD || dm == duplex_mode::SUL)) {
    return prach_configuration_get_fr1_paired(prach_config_index);
  }

  if ((fr == frequency_range::FR1) && (dm == duplex_mode::TDD)) {
    return prach_configuration_get_fr1_unpaired(prach_config_index);
  }

  return PRACH_CONFIG_RESERVED;
}
