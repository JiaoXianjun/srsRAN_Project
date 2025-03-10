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

#include "du_ue.h"

namespace srsran {
namespace srs_du {

/// \brief Interface used by DU Manager procedures to add/find/remove UEs from the DU UE Manager.
class du_ue_manager_repository
{
public:
  virtual ~du_ue_manager_repository() = default;

  /// \brief Adds a new UE context in the DU UE manager repository.
  virtual du_ue* add_ue(std::unique_ptr<du_ue> u) = 0;

  /// \brief removes an existing ue context from the du ue manager repository.
  virtual void remove_ue(du_ue_index_t ue_index) = 0;

  /// \brief Find UE context based on DU UE index.
  virtual du_ue* find_ue(du_ue_index_t ue_index) = 0;

  /// \brief Find UE context based on UE RNTI.
  virtual du_ue* find_rnti(rnti_t rnti) = 0;
};

} // namespace srs_du
} // namespace srsran
