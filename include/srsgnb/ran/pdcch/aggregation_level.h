/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#ifndef SRSGNB_AGGREGATION_LEVEL_H
#define SRSGNB_AGGREGATION_LEVEL_H

#include <cstdint>

namespace srsgnb {

/// Aggregation Level of PDCCH allocation.
enum class aggregation_level : uint8_t { n1 = 0, n2, n4, n8, n16 };

inline unsigned to_nof_cces(aggregation_level lvl)
{
  return 1U << static_cast<uint8_t>(lvl);
}

} // namespace srsgnb

#endif // SRSGNB_AGGREGATION_LEVEL_H
