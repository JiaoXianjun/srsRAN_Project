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

#include "srsran/ran/pdcch/dci_packing.h"
#include "srsran/adt/span.h"
#include "srsran/ran/pdcch/dci_packing_formatters.h"
#include "srsran/support/math_utils.h"
#include <unordered_set>

using namespace srsran;

// Maximum number of resource block groups per BWP.
static constexpr unsigned MAX_NOF_RBGS = 18;

// Computes the number of information bits before padding for a DCI format 0_0 message.
static dci_0_0_size dci_f0_0_bits_before_padding(unsigned N_rb_ul_bwp)
{
  dci_0_0_size sizes = {};

  // Identifier for DCI formats - 1 bit.
  ++sizes.total;

  // Frequency domain resource assignment. Number of bits as per TS38.214 Section 6.1.2.2.2.
  sizes.frequency_resource = units::bits(log2_ceil(N_rb_ul_bwp * (N_rb_ul_bwp + 1) / 2));
  sizes.total += sizes.frequency_resource;

  // Time domain resource assignment - 4 bit.
  sizes.total += units::bits(4);

  // Frequency hopping flag - 1 bit.
  ++sizes.total;

  // Modulation and coding scheme - 5 bit.
  sizes.total += units::bits(5);

  // New data indicator - 1 bit.
  ++sizes.total;

  // Redundancy version - 2 bit.
  sizes.total += units::bits(2);

  // HARQ process number - 4 bit.
  sizes.total += units::bits(4);

  // TPC command for scheduled PUSCH - 2 bit.
  sizes.total += units::bits(2);

  return sizes;
}

// Computes the number of information bits before padding for a DCI format 1_0 message.
static dci_1_0_size dci_f1_0_bits_before_padding(unsigned N_rb_dl_bwp)
{
  dci_1_0_size sizes = {};

  // Contribution to the DCI payload size that is fixed. It is the same number of bits for all format 1_0 variants.
  sizes.total = units::bits(28U);

  // Frequency domain resource assignment. Number of bits as per TS38.214 Section 5.1.2.2.2.
  sizes.frequency_resource = units::bits(log2_ceil(N_rb_dl_bwp * (N_rb_dl_bwp + 1) / 2));
  sizes.total += sizes.frequency_resource;

  return sizes;
}

// Computes the BWP indicator field size for DCI formats 0_1 and 1_1.
static unsigned bwp_indicator_size(unsigned nof_bwp_rrc)
{
  unsigned bwp_ind_size = 0;
  if (nof_bwp_rrc > 0) {
    unsigned n_bwp = nof_bwp_rrc;
    if (n_bwp <= 3) {
      ++n_bwp;
    }
    bwp_ind_size = log2_ceil(n_bwp);
    srsran_assert(bwp_ind_size <= 2,
                  "The derived BWP indicator field size, i.e., {} exceeds the maximum expected size of 2",
                  bwp_ind_size);
  }
  return bwp_ind_size;
}

// Determines the size of the frequency domain resource assignment field for DCI formats 0_1 and 1_1.
static unsigned freq_resource_assignment_size(resource_allocation res_allocation_type,
                                              optional<unsigned>  nof_rb_groups,
                                              unsigned            nof_prb_bwp)
{
  unsigned freq_resource_size = 0;
  switch (res_allocation_type) {
    case resource_allocation::resource_allocation_type_0:
      // For resource allocation type 0, the field size is the number of UL/DL RBG, as per TS38.214 Section 6.1.2.2.1 /
      // 5.1.2.2.1.
      freq_resource_size = nof_rb_groups.value();
      break;
    case resource_allocation::resource_allocation_type_1:
      // For resource allocation type 1, the field size is derived from the bandwidth of the active BWP, as per TS38.212
      // Section 7.3.1.
      freq_resource_size = log2_ceil(nof_prb_bwp * (nof_prb_bwp + 1) / 2);
      break;
    case resource_allocation::dynamic_switch:
      // For dynamic resource allocation type, the field size is determined by the resource allocation type that results
      // in a larger payload, and one extra bit is added to provide dynamic selection between type 0 and type 1.
      freq_resource_size = 1 + std::max(nof_rb_groups.value(), log2_ceil(nof_prb_bwp * (nof_prb_bwp + 1) / 2));
      break;
  }
  return freq_resource_size;
}

// Computes the UL antenna ports field size for a specific DM-RS configuration.
static unsigned ul_dmrs_ports_size(dmrs_config_type dmrs_type, dmrs_max_length dmrs_len, bool transform_precoding)
{
  // 2 bits as defined by Tables 7.3.1.1.2-6, if transform precoder is enabled, dmrs-Type=1, and maxLength=1.
  if (transform_precoding && (dmrs_type == dmrs_config_type::type1) && (dmrs_len == dmrs_max_length::len1)) {
    return 2;
  }

  // 4 bits as defined by Tables 7.3.1.1.2-7, if transform precoder is enabled, dmrs-Type=1, and maxLength=2.
  if (transform_precoding && (dmrs_type == dmrs_config_type::type1) && (dmrs_len == dmrs_max_length::len2)) {
    return 4;
  }

  // 3 bits as defined by Tables 7.3.1.1.2-8/9/10/11, if transform precoder is disabled, dmrs-Type=1, and maxLength=1.
  if (!transform_precoding && (dmrs_type == dmrs_config_type::type1) && (dmrs_len == dmrs_max_length::len1)) {
    return 3;
  }

  // 4 bits as defined by Tables 7.3.1.1.2-12/13/14/15, if transform precoder is disabled, dmrs-Type=1, and
  // maxLength=2.
  if (!transform_precoding && (dmrs_type == dmrs_config_type::type1) && (dmrs_len == dmrs_max_length::len2)) {
    return 4;
  }

  // 4 bits as defined by Tables 7.3.1.1.2-16/17/18/19, if transform precoder is disabled, dmrs-Type=2, and
  // maxLength=1.
  if (!transform_precoding && (dmrs_type == dmrs_config_type::type2) && (dmrs_len == dmrs_max_length::len1)) {
    return 4;
  }

  // 5 bits as defined by Tables 7.3.1.1.2-20/21/22/23, if transform precoder is disabled, dmrs-Type=2, and
  // maxLength=2.
  if (!transform_precoding && (dmrs_type == dmrs_config_type::type2) && (dmrs_len == dmrs_max_length::len2)) {
    return 5;
  }

  srsran_assertion_failure("Invalid combination of PUSCH DM-RS and transform precoding parameters.");
  return 0;
}

// Computes the DL antenna ports field size for a specific DM-RS configuration.
static unsigned dl_dmrs_ports_size(dmrs_config_type dmrs_type, dmrs_max_length dmrs_len)
{
  // 4, 5 or 6 bits as defined by Tables 7.3.1.2.2-1/2/3/4.
  unsigned ports_size = 4;
  if (dmrs_type == dmrs_config_type::type2) {
    ++ports_size;
  }
  if (dmrs_len == dmrs_max_length::len2) {
    ++ports_size;
  }
  return ports_size;
}

// Computes the antenna ports field size for DCI format 0_1.
static units::bits ul_ports_size(optional<dmrs_config_type> dmrs_A_type,
                                 optional<dmrs_max_length>  dmrs_A_max_len,
                                 optional<dmrs_config_type> dmrs_B_type,
                                 optional<dmrs_max_length>  dmrs_B_max_len,
                                 bool                       transform_precoding_enabled)
{
  bool mapping_type_A_configured = dmrs_A_type.has_value() && dmrs_A_max_len.has_value();
  bool mapping_type_B_configured = dmrs_B_type.has_value() && dmrs_B_max_len.has_value();

  units::bits pusch_dmrs_A_ports_size =
      mapping_type_A_configured
          ? units::bits(ul_dmrs_ports_size(dmrs_A_type.value(), dmrs_A_max_len.value(), transform_precoding_enabled))
          : units::bits(0);

  units::bits pusch_dmrs_B_ports_size =
      mapping_type_B_configured
          ? units::bits(ul_dmrs_ports_size(dmrs_B_type.value(), dmrs_B_max_len.value(), transform_precoding_enabled))
          : units::bits(0);

  return std::max(pusch_dmrs_A_ports_size, pusch_dmrs_B_ports_size);
}

// Computes the antenna ports field size for DCI format 1_1.
static units::bits dl_ports_size(optional<dmrs_config_type> dmrs_A_type,
                                 optional<dmrs_max_length>  dmrs_A_max_len,
                                 optional<dmrs_config_type> dmrs_B_type,
                                 optional<dmrs_max_length>  dmrs_B_max_len)
{
  bool mapping_type_A_configured = dmrs_A_type.has_value() && dmrs_A_max_len.has_value();
  bool mapping_type_B_configured = dmrs_B_type.has_value() && dmrs_B_max_len.has_value();

  units::bits pdsch_dmrs_A_ports_size =
      mapping_type_A_configured ? units::bits(dl_dmrs_ports_size(dmrs_A_type.value(), dmrs_A_max_len.value()))
                                : units::bits(0);

  units::bits pdsch_dmrs_B_ports_size =
      mapping_type_B_configured ? units::bits(dl_dmrs_ports_size(dmrs_B_type.value(), dmrs_B_max_len.value()))
                                : units::bits(0);

  return std::max(pdsch_dmrs_A_ports_size, pdsch_dmrs_B_ports_size);
}

// Computes the SRS resource indicator field size for DCI format 0_1.
static unsigned srs_resource_indicator_size(const dci_size_config& dci_config)
{
  // SRS resource indicator size for non-codebook based transmission.
  if (dci_config.tx_config_non_codebook) {
    // Derived from TS38.212 Table 7.3.1.1.2-28.
    switch (dci_config.nof_srs_resources) {
      case 1:
        // If only a single SRS resource is configured, the SRS resource indicator does not occupy any bit.
        return 0;
      case 2:
        return 1;
      case 3:
      case 4:
      default:
        return 2;
    }
  }

  // SRS resource indicator size for codebook based transmission.
  return log2_ceil(dci_config.nof_srs_resources);
}

// Computes the number of information bits before padding for a DCI format 0_1 message.
static dci_0_1_size dci_f0_1_bits_before_padding(const dci_size_config& dci_config)
{
  dci_0_1_size sizes = {};

  // Identifier for DCI formats - 1 bit.
  ++sizes.total;

  // Carrier indicator - 0 or 3 bits.
  sizes.carrier_indicator = dci_config.cross_carrier_configured ? units::bits(3) : units::bits(0);
  sizes.total += sizes.carrier_indicator;

  // UL/SUL indicator - 0 or 1 bit.
  sizes.ul_sul_indicator = dci_config.sul_configured ? units::bits(1) : units::bits(0);
  sizes.total += sizes.ul_sul_indicator;

  // BWP indicator - 0, 1 or 2 bits.
  sizes.bwp_indicator = units::bits(bwp_indicator_size(dci_config.nof_ul_bwp_rrc));
  sizes.total += sizes.bwp_indicator;

  // Frequency domain resource assignment - number of bits as per TS38.212 Section 7.3.1.1.2.
  sizes.frequency_resource = units::bits(freq_resource_assignment_size(
      dci_config.pusch_res_allocation_type, dci_config.nof_ul_rb_groups, dci_config.ul_bwp_active_bw));
  sizes.total += sizes.frequency_resource;

  // Time domain resource assignment - 0, 1, 2, 3 or 4 bits.
  sizes.time_resource = units::bits(log2_ceil(dci_config.nof_ul_time_domain_res));
  sizes.total += sizes.time_resource;

  // Frequency hopping flag - 0 or 1 bit.
  sizes.freq_hopping_flag = (dci_config.pusch_res_allocation_type != resource_allocation::resource_allocation_type_0) &&
                                    dci_config.frequency_hopping_configured
                                ? units::bits(1)
                                : units::bits(0);
  sizes.total += sizes.freq_hopping_flag;

  // Modulation and coding scheme - 5 bits.
  sizes.total += units::bits(5);

  // New Data indicator - 1 bit.
  ++sizes.total;

  // Redundancy version - 2 bits.
  sizes.total += units::bits(2);

  // HARQ process number - 4 bits.
  sizes.total += units::bits(4);

  // First downlink assignment index - 1 or 2 bits.
  sizes.first_dl_assignment_idx =
      (dci_config.pdsch_harq_ack_cb == pdsch_harq_ack_codebook::dynamic) ? units::bits(2) : units::bits(1);
  sizes.total += sizes.first_dl_assignment_idx;

  // Second downlink assignment index - 0 or 2 bits.
  sizes.second_dl_assignment_idx =
      (dci_config.pdsch_harq_ack_cb == pdsch_harq_ack_codebook::dynamic) && dci_config.dynamic_dual_harq_ack_cb.value()
          ? units::bits(2)
          : units::bits(0);
  sizes.total += sizes.second_dl_assignment_idx;

  // TPC command for scheduled PUSCH - 2 bits.
  sizes.total += units::bits(2);

  // SRS resource indicator.
  sizes.srs_resource_indicator = units::bits(srs_resource_indicator_size(dci_config));
  sizes.total += sizes.srs_resource_indicator;

  // Precoding information and number of layers - 0, 1, 2, 3, 4, 5 or 6 bits.
  // ... Not implemented.

  // Antenna ports - 2, 3, 4 or 5 bits.
  sizes.antenna_ports = ul_ports_size(dci_config.pusch_dmrs_A_type,
                                      dci_config.pusch_dmrs_A_max_len,
                                      dci_config.pusch_dmrs_B_type,
                                      dci_config.pusch_dmrs_B_max_len,
                                      dci_config.transform_precoding_enabled);
  sizes.total += sizes.antenna_ports;

  // SRS request - 2 or 3 bits.
  sizes.srs_request = dci_config.sul_configured ? units::bits(3) : units::bits(2);
  sizes.total += sizes.srs_request;

  // CSI request - 0, 1, 2, 3, 4, 5 or 6 bits.
  sizes.csi_request = units::bits(dci_config.report_trigger_size);
  sizes.total += sizes.csi_request;

  // CBG Transmission Information (CBGTI) - 0, 2, 4, 6 or 8 bits.
  if (dci_config.max_cbg_tb_pusch.has_value()) {
    sizes.cbg_transmission_info = units::bits(dci_config.max_cbg_tb_pusch.value());
    sizes.total += sizes.cbg_transmission_info;
  }

  // PT-RS/DM-RS association - 0 or 2 bits.
  // ... Not implemented.

  // Beta offset indicator - 0 or 2 bits.
  sizes.beta_offset_indicator = dci_config.dynamic_beta_offsets ? units::bits(2) : units::bits(0);
  sizes.total += sizes.beta_offset_indicator;

  // DM-RS sequence initialization - 0 or 1 bit.
  sizes.dmrs_seq_initialization = dci_config.transform_precoding_enabled ? units::bits(0) : units::bits(1);
  sizes.total += sizes.dmrs_seq_initialization;

  // UL-SCH indicator - 1 bit.
  ++sizes.total;

  return sizes;
}

// Computes the number of information bits before padding for a DCI format 1_1 message.
static dci_1_1_size dci_f1_1_bits_before_padding(const dci_size_config& dci_config)
{
  dci_1_1_size sizes = {};

  // Identifier for DCI formats - 1 bit.
  ++sizes.total;

  // Carrier indicator - 0 or 3 bits.
  sizes.carrier_indicator = dci_config.cross_carrier_configured ? units::bits(3) : units::bits(0);
  sizes.total += sizes.carrier_indicator;

  // BWP indicator - 0, 1 or 2 bits.
  sizes.bwp_indicator = units::bits(bwp_indicator_size(dci_config.nof_dl_bwp_rrc));
  sizes.total += sizes.bwp_indicator;

  // Frequency domain resource assignment - number of bits as per TS38.212 Section 7.3.1.2.2.
  sizes.frequency_resource = units::bits(freq_resource_assignment_size(
      dci_config.pdsch_res_allocation_type, dci_config.nof_dl_rb_groups, dci_config.dl_bwp_active_bw));
  sizes.total += sizes.frequency_resource;

  // Time domain resource assignment - 0, 1, 2, 3 or 4 bits.
  sizes.time_resource = units::bits(log2_ceil(dci_config.nof_dl_time_domain_res));
  sizes.total += sizes.time_resource;

  // VRP-to-PRB mapping - 0 or 1 bit.
  sizes.vrb_prb_mapping = (dci_config.pdsch_res_allocation_type != resource_allocation::resource_allocation_type_0) &&
                                  dci_config.interleaved_vrb_prb_mapping
                              ? units::bits(1)
                              : units::bits(0);
  sizes.total += sizes.vrb_prb_mapping;

  // PRB bundling size indicator - 0 or 1 bit.
  sizes.prb_bundling_size_indicator = dci_config.dynamic_prb_bundling ? units::bits(1) : units::bits(0);
  sizes.total += sizes.prb_bundling_size_indicator;

  // Rate matching indicator - 0, 1 or 2 bits.
  unsigned nof_rm_pattern_groups =
      static_cast<unsigned>(dci_config.rm_pattern_group1) + static_cast<unsigned>(dci_config.rm_pattern_group2);
  sizes.rate_matching_indicator = units::bits(nof_rm_pattern_groups);
  sizes.total += sizes.rate_matching_indicator;

  // ZP CSI-RS trigger - 0, 1 or 2 bits.
  sizes.zp_csi_rs_trigger = units::bits(log2_ceil(dci_config.nof_aperiodic_zp_csi + 1));
  sizes.total += sizes.zp_csi_rs_trigger;

  // Modulation and coding scheme for TB 1 - 5 bits.
  sizes.total += units::bits(5);

  // New data indicator for TB 1 - 1 bit.
  ++sizes.total;

  // Redundancy version for TB 1 - 2 bits.
  sizes.total += units::bits(2);

  if (dci_config.pdsch_two_codewords) {
    // Modulation and coding scheme for TB 2 - 0 or 5 bits.
    sizes.tb2_modulation_coding_scheme = units::bits(5);
    sizes.total += sizes.tb2_modulation_coding_scheme;

    // New data indicator for TB 2 - 0 or 1 bit.
    sizes.tb2_new_data_indicator = units::bits(1);
    sizes.total += sizes.tb2_new_data_indicator;

    // Redundancy version for TB 2 - 0 or 2 bits.
    sizes.tb2_redundancy_version = units::bits(2);
    sizes.total += sizes.tb2_redundancy_version;
  }

  // HARQ process number - 4 bits.
  sizes.total += units::bits(4);

  // Downlink Assignment Index (DAI) - 0, 2 or 4 bits.
  if (dci_config.pdsch_harq_ack_cb == pdsch_harq_ack_codebook::dynamic) {
    sizes.downlink_assignment_index = dci_config.multiple_scells ? units::bits(4) : units::bits(2);
    sizes.total += sizes.downlink_assignment_index;
  }

  // TPC command for scheduled PUCCH - 2 bits.
  sizes.total += units::bits(2);

  // PUCCH resource indicator - 3 bits as per TS38.213 Section 9.2.3.
  sizes.total += units::bits(3);

  // PDSCH to HARQ feedback timing indicator - 0, 1, 2 or 3 bits.
  sizes.pdsch_harq_fb_timing_indicator = units::bits(log2_ceil(dci_config.nof_pdsch_ack_timings));
  sizes.total += sizes.pdsch_harq_fb_timing_indicator;

  // Antenna ports - 4, 5 or 6 bits.
  sizes.antenna_ports = dl_ports_size(dci_config.pdsch_dmrs_A_type,
                                      dci_config.pdsch_dmrs_A_max_len,
                                      dci_config.pdsch_dmrs_B_type,
                                      dci_config.pdsch_dmrs_B_max_len);
  sizes.total += sizes.antenna_ports;

  // Transmission configuration indication - 0 or 3 bits.
  sizes.tx_config_indication = dci_config.pdsch_tci ? units::bits(3) : units::bits(0);
  sizes.total += sizes.tx_config_indication;

  // SRS request - 2 or 3 bits.
  sizes.srs_request = dci_config.sul_configured ? units::bits(3) : units::bits(2);
  sizes.total += sizes.srs_request;

  // CBG Transmission Information (CBGTI) - 0, 2, 4, 6 or 8 bits.
  if (dci_config.max_cbg_tb_pdsch.has_value()) {
    sizes.cbg_transmission_info = units::bits(dci_config.max_cbg_tb_pdsch.value());
    sizes.total += sizes.cbg_transmission_info;
  }

  // CBG Flushing Out Information (CBGFI) - 0 or 1 bit.
  sizes.cbg_flushing_info = dci_config.cbg_flush_indicator ? units::bits(1) : units::bits(0);
  sizes.total += sizes.cbg_flushing_info;

  // DM-RS sequence initialization - 1 bit.
  ++sizes.total;

  return sizes;
}

static void assert_dci_size_config(const dci_size_config& config)
{ // Asserts for all DCI formats.
  srsran_assert((config.dl_bwp_initial_bw > 0) && (config.dl_bwp_initial_bw <= MAX_RB),
                "The initial DL BWP bandwidth, i.e., {} must be within the range [1, {}].",
                config.dl_bwp_initial_bw,
                MAX_RB);

  srsran_assert((config.ul_bwp_initial_bw > 0) && (config.ul_bwp_initial_bw <= MAX_RB),
                "The initial UL BWP bandwidth, i.e., {} must be within the range [1, {}].",
                config.ul_bwp_initial_bw,
                MAX_RB);

  srsran_assert(config.coreset0_bw <= MAX_RB,
                "The CORESET 0 bandwidth, i.e., {} must be within the range [0, {}].",
                config.coreset0_bw,
                MAX_RB);

  srsran_assert(!config.sul_configured, "SUL is not currently supported.");

  // Asserts for fallback DCI formats on a UE-specific search space.
  if (config.dci_0_0_and_1_0_ue_ss) {
    srsran_assert((config.dl_bwp_active_bw > 0) && (config.dl_bwp_active_bw <= MAX_RB),
                  "The active DL BWP bandwidth, i.e., {} must be within the range [1, {}].",
                  config.dl_bwp_active_bw,
                  MAX_RB);

    srsran_assert((config.ul_bwp_active_bw > 0) && (config.ul_bwp_active_bw <= MAX_RB),
                  "The active UL BWP bandwidth, i.e., {} must be within the range [1, {}].",
                  config.ul_bwp_active_bw,
                  MAX_RB);
  }

  // Asserts for non-fallback DCI formats.
  if (config.dci_0_1_and_1_1_ue_ss) {
    srsran_assert((config.nof_ul_bwp_rrc <= 4),
                  "The number of UL BWP configured by higher layers, i.e., {}, cannot exceed 4.",
                  config.nof_ul_bwp_rrc);
    srsran_assert((config.nof_dl_bwp_rrc <= 4),
                  "The number of DL BWP configured by higher layers, i.e., {}, cannot exceed 4.",
                  config.nof_dl_bwp_rrc);

    srsran_assert((config.nof_ul_time_domain_res > 0) && (config.nof_ul_time_domain_res <= 16),
                  "The number of UL time domain resource allocations, i.e., {} must be within the range [1, 16].",
                  config.nof_ul_time_domain_res);

    srsran_assert((config.nof_dl_time_domain_res > 0) && (config.nof_dl_time_domain_res <= 16),
                  "The number of DL time domain resource allocations, i.e., {} must be within the range [1, 16].",
                  config.nof_dl_time_domain_res);

    srsran_assert((config.nof_aperiodic_zp_csi <= 3),
                  "The number of aperiodic ZP CSI-RS resource sets, i.e., {}, cannot be larger than 3.",
                  config.nof_aperiodic_zp_csi);

    srsran_assert((config.nof_pdsch_ack_timings > 0) && (config.nof_pdsch_ack_timings <= 8),
                  "The number of PDSCH HARQ-ACK timings, i.e., {}, must be within the range [1, 8].",
                  config.nof_pdsch_ack_timings);

    srsran_assert((config.report_trigger_size <= 6),
                  "The report trigger size, i.e., {}, cannot be larger than 6.",
                  config.report_trigger_size);

    srsran_assert(!config.max_cbg_tb_pusch.has_value() ||
                      (std::unordered_set<unsigned>({2, 4, 6, 8}).count(config.max_cbg_tb_pusch.value()) > 0),
                  "Invalid Maximum CBG per PUSCH TB, i.e., {}. Valid options: 2, 4, 6, 8.",
                  config.max_cbg_tb_pusch.value());

    srsran_assert(!config.max_cbg_tb_pdsch.has_value() ||
                      (std::unordered_set<unsigned>({2, 4, 6, 8}).count(config.max_cbg_tb_pdsch.value()) > 0),
                  "Invalid Maximum CBG per PDSCH TB, i.e., {}. Valid options: 2, 4, 6, 8.",
                  config.max_cbg_tb_pdsch.value());

    // Asserts for transform precoding.
    srsran_assert(!config.transform_precoding_enabled || !config.pusch_dmrs_A_type.has_value() ||
                      (config.pusch_dmrs_A_type != dmrs_config_type::type2),
                  "UL DM-RS configuration type 2 cannot be used with transform precoding.");
    srsran_assert(!config.transform_precoding_enabled || !config.pusch_dmrs_B_type.has_value() ||
                      (config.pusch_dmrs_B_type != dmrs_config_type::type2),
                  "UL DM-RS configuration type 2 cannot be used with transform precoding.");

    srsran_assert((config.pdsch_harq_ack_cb != pdsch_harq_ack_codebook::dynamic) ||
                      config.dynamic_dual_harq_ack_cb.has_value(),
                  "Dynamic dual HARQ-ACK codebook flag is required for dynamic PDSCH HARQ-ACK codebook.");

    if (config.pusch_res_allocation_type != resource_allocation::resource_allocation_type_1) {
      // Asserts for UL resource allocation type 0.
      srsran_assert(config.nof_ul_rb_groups.has_value(),
                    "The number of UL RBGs is required for resource allocation type 0.");
      srsran_assert((config.nof_ul_rb_groups.value() > 0) && (config.nof_ul_rb_groups.value() <= MAX_NOF_RBGS),
                    "The number of UL RBGs, i.e., {}, must be within the range [1, {}].",
                    config.nof_ul_rb_groups.value(),
                    MAX_NOF_RBGS);
    }

    if (config.pdsch_res_allocation_type != resource_allocation::resource_allocation_type_1) {
      // Asserts for DL resource allocation type 0.
      srsran_assert(config.nof_dl_rb_groups.has_value(),
                    "The number of DL RBGs is required for resource allocation type 0.");
      srsran_assert((config.nof_dl_rb_groups.value() > 0) && (config.nof_dl_rb_groups.value() <= MAX_NOF_RBGS),
                    "The number of DL RBGs, i.e., {}, must be within the range [1, {}].",
                    config.nof_dl_rb_groups.value(),
                    MAX_NOF_RBGS);
    }

    // Asserts for DL resource allocation type 1.
    srsran_assert((config.pdsch_res_allocation_type == resource_allocation::resource_allocation_type_0) ||
                      config.interleaved_vrb_prb_mapping.has_value(),
                  "Interleaved VRB to PRB mapping flag is required for PDSCH resource allocation type 1.");

    if (config.tx_config_non_codebook) {
      // Asserts for non-codebook based transmission.
      srsran_assert(config.pusch_max_layers.has_value(),
                    "Maximum number of PUSCH layers is required for non-codebook transmission.");

      srsran_assert((config.pusch_max_layers.value() > 0) && (config.pusch_max_layers.value() <= 4),
                    "Maximum number of PUSH layers, i.e., {}, must be within the valid range [1, 4].",
                    config.pusch_max_layers.value());

      srsran_assert((config.nof_srs_resources > 0) && (config.nof_srs_resources <= 4),
                    "Number of SRS resources, i.e., {}, must be within the range [1, 4] for non-codebook transmission.",
                    config.nof_srs_resources);

      // Temporary assertion, until UL MIMO is supported.
      srsran_assert((config.pusch_max_layers.value() == 1), "Multiple layers on PUSCH are not currently supported.");
    } else {
      // Asserts for codebook-based transmission.
      srsran_assert(config.max_rank.has_value(), "Maximum rank is required for codebook transmission.");

      srsran_assert((config.max_rank.value() > 0) && (config.max_rank.value() <= 4),
                    "Maximum rank, i.e., {}, must be within the valid range [1, 4].",
                    config.max_rank.value());

      srsran_assert(config.nof_srs_ports.has_value(),
                    "Number of SRS antenna ports is required for codebook transmission.");

      srsran_assert(std::unordered_set<unsigned>({1, 2, 4}).count(config.nof_srs_ports.value()) > 0,
                    "Invalid number of SRS ports, i.e., {}. Valid options: 1, 2, 4.",
                    config.nof_srs_ports.value());

      srsran_assert((config.max_rank.value() <= config.nof_srs_ports.value()),
                    "Maximum rank, i.e., {}, cannot be larger than the number of SRS antenna ports, i.e., {}.",
                    config.max_rank.value(),
                    config.nof_srs_ports.value());

      srsran_assert((config.nof_srs_resources > 0) && (config.nof_srs_resources <= 2),
                    "Number of SRS resources, i.e., {}, must be within the range [1, 2] for codebook transmission.",
                    config.nof_srs_resources);

      srsran_assert((config.nof_srs_ports.value() == 1) || config.cb_subset.has_value(),
                    "Codebook subset is required for codebook transmission with multiple antenna ports.");

      // Temporary assertion, until UL precoding is supported.
      srsran_assert((config.nof_srs_ports.value() == 1), "UL precoding is not currently supported.");
    }

    srsran_assert(!config.ptrs_uplink_configured || config.transform_precoding_enabled ||
                      (!config.tx_config_non_codebook && (config.max_rank.value() == 1)),
                  "PT-RS with more than one DM-RS is not currently supported.");

    srsran_assert((config.pusch_dmrs_A_type.has_value() && config.pusch_dmrs_A_max_len.has_value()) ||
                      (config.pusch_dmrs_B_type.has_value() && config.pusch_dmrs_B_max_len.has_value()),
                  "At least one PUSCH DM-RS mapping (type A or type B) must be configured.");

    srsran_assert((config.pdsch_dmrs_A_type.has_value() && config.pdsch_dmrs_A_max_len.has_value()) ||
                      (config.pdsch_dmrs_B_type.has_value() && config.pdsch_dmrs_B_max_len.has_value()),
                  "At least one PDSCH DM-RS mapping (type A or type B) must be configured.");
  }
}

dci_sizes srsran::get_dci_sizes(const dci_size_config& config)
{
  // Assert DCI size configuration parameters.
  assert_dci_size_config(config);

  dci_sizes final_sizes = {};

  // Step 0.
  // - Determine DCI format 0_0 monitored in a common search space according to TS38.212 Section 7.3.1.1.1 where
  // N_UL_BWP_RB is given by the size of the initial UL bandwidth part.
  const dci_0_0_size format0_0_info_bits_common = dci_f0_0_bits_before_padding(config.ul_bwp_initial_bw);

  // - Determine DCI format 1_0 monitored in a common search space according to TS38.212 Section 7.3.1.2.1 where
  // N_DL_BWP_RB given by:
  //   - the size of CORESET 0 if CORESET 0 is configured for the cell
  //   - the size of initial DL bandwidth part if CORESET 0 is not configured for the cell.
  const dci_1_0_size format1_0_info_bits_common =
      dci_f1_0_bits_before_padding((config.coreset0_bw != 0) ? config.coreset0_bw : config.dl_bwp_initial_bw);

  final_sizes.format0_0_common_size = format0_0_info_bits_common;
  final_sizes.format1_0_common_size = format1_0_info_bits_common;

  // - If DCI format 0_0 is monitored in common search space and if the number of information bits in the DCI format 0_0
  // prior to padding is less than the payload size of the DCI format 1_0 monitored in common search space for
  // scheduling the same serving cell, a number of zero padding bits are generated for the DCI format 0_0 until the
  // payload size equals that of the DCI format 1_0.
  if (format0_0_info_bits_common.total < format1_0_info_bits_common.total) {
    // The number of padding bits is computed here, including the single bit UL/SUL field. This field is located after
    // the padding, and it must only be included if the format 1_0 payload has a larger amount of bits before the
    // padding bits than the format 0_0 payload. Therefore, the UL/SUL can be though of as a field that takes the space
    // of the last padding bit within the format 0_0 payload, if present. See TS38.212 Sections 7.3.1.0 and 7.3.1.1.1.
    final_sizes.format0_0_common_size.padding_incl_ul_sul =
        format1_0_info_bits_common.total - format0_0_info_bits_common.total;

    // Update the DCI format 0_0 total payload size.
    final_sizes.format0_0_common_size.total += final_sizes.format0_0_common_size.padding_incl_ul_sul;
  }

  // - If DCI format 0_0 is monitored in common search space and if the number of information bits in the DCI format 0_0
  // prior to truncation is larger than the payload size of the DCI format 1_0 monitored in common search space for
  // scheduling the same serving cell, the bitwidth of the frequency domain resource assignment field in the DCI format
  // 0_0 is reduced by truncating the first few most significant bits such that the size of DCI format 0_0 equals the
  // size of the DCI format 1_0.
  else if (format0_0_info_bits_common.total > format1_0_info_bits_common.total) {
    units::bits nof_truncated_bits = format0_0_info_bits_common.total - format1_0_info_bits_common.total;
    final_sizes.format0_0_common_size.frequency_resource -= nof_truncated_bits;
    final_sizes.format0_0_common_size.total -= nof_truncated_bits;
  }

  srsran_assert(final_sizes.format1_0_common_size.total == final_sizes.format0_0_common_size.total,
                "DCI format 0_0 and 1_0 payload sizes must match");

  // Step 1.
  if (config.dci_0_0_and_1_0_ue_ss) {
    // - Determine DCI format 0_0 monitored in a UE-specific search space according to TS38.212 Section 7.3.1.1.1 where
    // N_UL_BWP_RB is the size of the active UL bandwidth part.
    const dci_0_0_size format0_0_info_bits_ue = dci_f0_0_bits_before_padding(config.ul_bwp_active_bw);

    // - Determine DCI format 1_0 monitored in a UE-specific search space according to TS38.212 Section 7.3.1.2.1 where
    // N_DL_BWP_RB is the size of the active DL bandwidth part.
    const dci_1_0_size format1_0_info_bits_ue = dci_f1_0_bits_before_padding(config.dl_bwp_active_bw);

    // - For a UE configured with supplementaryUplink in ServingCellConfig in a cell, if PUSCH is configured to be
    // transmitted on both the SUL and the non-SUL of the cell and if the number of information bits in DCI format 0_0
    // in UE-specific search space for the SUL is not equal to the number of information bits in DCI format 0_0 in
    // UE-specific search space for the non-SUL, a number of zero padding bits are generated for the smaller DCI format
    // 0_0 until the payload size equals that of the larger DCI format 0_0.

    // Not implemented.

    final_sizes.format0_0_ue_size = format0_0_info_bits_ue;
    final_sizes.format1_0_ue_size = format1_0_info_bits_ue;

    // - If DCI format 0_0 is monitored in UE-specific search space and if the number of information bits in the DCI
    // format 0_0 prior to padding is less than the payload size of the DCI format 1_0 monitored in UE-specific search
    // space for scheduling the same serving cell, a number of zero padding bits are generated for the DCI format 0_0
    // until the payload size equals that of the DCI format 1_0.
    if (format0_0_info_bits_ue.total < format1_0_info_bits_ue.total) {
      units::bits nof_padding_bits_incl_ul_sul = format1_0_info_bits_ue.total - format0_0_info_bits_ue.total;
      final_sizes.format0_0_ue_size.value().padding_incl_ul_sul = nof_padding_bits_incl_ul_sul;
      final_sizes.format0_0_ue_size.value().total += nof_padding_bits_incl_ul_sul;
    }

    // - If DCI format 1_0 is monitored in UE-specific search space and if the number of information bits in the DCI
    // format 1_0 prior to padding is less than the payload size of the DCI format 0_0 monitored in UE-specific search
    // space for scheduling the same serving cell, zeros shall be appended to the DCI format 1_0 until the payload size
    // equals that of the DCI format 0_0
    else if (format1_0_info_bits_ue.total < format0_0_info_bits_ue.total) {
      units::bits nof_padding_bits                  = format0_0_info_bits_ue.total - format1_0_info_bits_ue.total;
      final_sizes.format1_0_ue_size.value().padding = nof_padding_bits;
      final_sizes.format1_0_ue_size.value().total += final_sizes.format1_0_ue_size.value().padding;
    }

    srsran_assert(final_sizes.format1_0_ue_size.value().total == final_sizes.format0_0_ue_size.value().total,
                  "DCI format 0_0 and 1_0 payload sizes must match");
  }

  // Step 2.
  if (config.dci_0_1_and_1_1_ue_ss) {
    // Determine the size of DCI format 0_1 according to TS38.212 Section 7.3.1.1.2.
    dci_0_1_size format0_1_ue_size = dci_f0_1_bits_before_padding(config);

    // Determine the size of DCI format 1_1 according to TS38.212 Section 7.3.1.2.2.
    dci_1_1_size format1_1_ue_size = dci_f1_1_bits_before_padding(config);

    // - For a UE configured with supplementaryUplink in ServingCellConfig in a cell, if PUSCH is configured to be
    // transmitted on both the SUL and the non-SUL of the cell and if the number of information bits in format 0_1 for
    // the SUL is not equal to the number of information bits in format 0_1 for the non-SUL, zeros shall be appended to
    // smaller format 0_1 until the payload size equals that of the larger format 0_1.

    // Not implemented.

    // - If the size of DCI format 0_1 monitored in a UE-specific search space equals that of a DCI format 0_0/1_0
    // monitored in another UE-specific search space, one bit of zero padding shall be appended to DCI format 0_1.
    if (config.dci_0_0_and_1_0_ue_ss && (format0_1_ue_size.total == final_sizes.format0_0_ue_size.value().total)) {
      format0_1_ue_size.padding = units::bits(1);
      format0_1_ue_size.total += format0_1_ue_size.padding;
    }

    // - If the size of DCI format 1_1 monitored in a UE-specific search space equals that of a DCI format 0_0/1_0
    // monitored in another UE-specific search space, one bit of zero padding shall be appended to DCI format 1_1.
    if (config.dci_0_0_and_1_0_ue_ss && (format1_1_ue_size.total == final_sizes.format1_0_ue_size.value().total)) {
      format1_1_ue_size.padding = units::bits(1);
      format1_1_ue_size.total += format1_1_ue_size.padding;
    }

    final_sizes.format0_1_ue_size = format0_1_ue_size;
    final_sizes.format1_1_ue_size = format1_1_ue_size;
  }

  // Step 3.
  // If both of the following conditions are fulfilled the size alignment procedure is complete.
  // - The total number of different DCI sizes configured to monitor is no more than 4 for the cell.
  // - The total number of different DCI sizes with C-RNTI configured to monitor is no more than 3 for the cell.

  // Maximum number of DCI payload sizes given the current implementation.
  constexpr unsigned           max_nof_dci_sizes = 4;
  std::unordered_set<unsigned> unique_dci_sizes(max_nof_dci_sizes);

  // Fallback DCI formats monitored in Common Search Space are always included.
  unique_dci_sizes.insert(final_sizes.format0_0_common_size.total.value());

  // Fallback DCI formats monitored in UE-specific Search Space. Count them if they are included in the DCI size
  // alignment procedure and the resulting payload size is different from the fallback DCI formats monitored in a CSS.
  if (config.dci_0_0_and_1_0_ue_ss) {
    unique_dci_sizes.insert(final_sizes.format0_0_ue_size.value().total.value());
  }

  // Non-fallback DCI formats monitored in UE-specific Search Space. Count them if they are included in the DCI size
  // alignment procedure and the resulting payload size is different from other DCI formats.
  if (config.dci_0_1_and_1_1_ue_ss) {
    unique_dci_sizes.insert(final_sizes.format0_1_ue_size.value().total.value());
    unique_dci_sizes.insert(final_sizes.format1_1_ue_size.value().total.value());
  }

  // Get the actual number of distinct DCI payload sizes. No special DCI formats are implemented, so the most
  // restrictive condition imposed by the size alignment procedure is no more than 3 DCI sizes scrambled by C-RNTI.
  unsigned nof_c_rnti_dci_sizes = unique_dci_sizes.size();

  // Step 4.
  // If the above conditions are not met, set the size of the fallback DCI formats in a UE-specific Search Space to the
  // size of the fallback DCI formats monitored in a Common Search Space.
  if (nof_c_rnti_dci_sizes > 3) {
    final_sizes.format0_0_ue_size = final_sizes.format0_0_common_size;
    final_sizes.format1_0_ue_size = final_sizes.format1_0_common_size;
  }

  return final_sizes;
}

dci_payload srsran::dci_0_0_c_rnti_pack(const dci_0_0_c_rnti_configuration& config)
{
  srsran_assert(config.payload_size.total.value() >= 12, "DCI payloads must be at least 12 bit long");

  dci_payload payload;
  units::bits frequency_resource_nof_bits = config.payload_size.frequency_resource;

  // Identifier for DCI formats - 1 bit. This field is always 0, indicating an UL DCI format.
  payload.push_back(0x00U, 1);

  if (config.frequency_hopping_flag) {
    // Assert that the number of bits used to pack the frequency hopping offset is valid.
    srsran_assert((config.N_ul_hop == 1) || (config.N_ul_hop == 2),
                  "DCI frequency offset number of bits must be either 1 or 2");

    // Assert that the frequency resource field has enough bits to include the frequency hopping offset.
    srsran_assert(config.N_ul_hop < frequency_resource_nof_bits.value(),
                  "The frequency resource field must have enough bits to hold the frequency hopping offset");

    // Assert that the frequency hopping offset can be packed with the allocated bits.
    srsran_assert(config.hopping_offset < (1U << config.N_ul_hop),
                  "DCI frequency offset value ({}) cannot be packed with the allocated number of bits ({})",
                  config.hopping_offset,
                  config.N_ul_hop);

    // Truncate the frequency resource allocation field.
    frequency_resource_nof_bits -= units::bits(config.N_ul_hop);

    // Frequency hopping offset - N_ul_hop bits.
    payload.push_back(config.hopping_offset, config.N_ul_hop);
  }

  // Frequency domain resource assignment - frequency_resource_nof_bits bits.
  payload.push_back(config.frequency_resource, frequency_resource_nof_bits.value());

  // Time domain resource assignment - 4 bit.
  payload.push_back(config.time_resource, 4);

  // Frequency hopping flag - 1 bit.
  payload.push_back(config.frequency_hopping_flag, 1);

  // Modulation coding scheme - 5 bits.
  payload.push_back(config.modulation_coding_scheme, 5);

  // New data indicator - 1 bit.
  payload.push_back(config.new_data_indicator, 1);

  // Redundancy version - 2 bit.
  payload.push_back(config.redundancy_version, 2);

  // HARQ process number - 4 bit.
  payload.push_back(config.harq_process_number, 4);

  // TPC command for scheduled PUSCH - 2 bit.
  payload.push_back(config.tpc_command, 2);

  if (config.payload_size.padding_incl_ul_sul.value() > 0) {
    if (config.ul_sul_indicator.has_value()) {
      // UL/SUL field is included if it is present in the DCI message and the number of DCI format 1_0 bits before
      // padding is larger than the number of DCI format 0_0 bits before padding.
      constexpr unsigned nof_ul_sul_bit = 1U;
      // Padding bits, if necessary, as per TS38.212 Section 7.3.1.0.
      payload.push_back(0x00U, config.payload_size.padding_incl_ul_sul.value() - nof_ul_sul_bit);

      // UL/SUL indicator - 1 bit.
      payload.push_back(config.ul_sul_indicator.value(), nof_ul_sul_bit);
    } else {
      // UL/SUL field is not included otherwise.
      payload.push_back(0x00U, config.payload_size.padding_incl_ul_sul.value());
    }
  }

  // Assert total payload size.
  srsran_assert(units::bits(payload.size()) == config.payload_size.total,
                "Constructed payload size (i.e., {}) does not match expected payload size. Expected sizes:\n{}",
                units::bits(payload.size()),
                config.payload_size);

  return payload;
}

dci_payload srsran::dci_0_0_tc_rnti_pack(const dci_0_0_tc_rnti_configuration& config)
{
  srsran_assert(config.payload_size.total.value() >= 12, "DCI payloads must be at least 12 bit long");

  units::bits frequency_resource_nof_bits = config.payload_size.frequency_resource;
  dci_payload payload;

  // Identifier for DCI formats - 1 bit. This field is always 0, indicating an UL DCI format.
  payload.push_back(0x00U, 1);

  unsigned freq_resource_payload = config.frequency_resource;

  if (config.frequency_hopping_flag) {
    // Assert that the number of bits used to pack the frequency hopping offset is valid.
    srsran_assert((config.N_ul_hop == 1) || (config.N_ul_hop == 2),
                  "DCI frequency offset number of bits must be either 1 or 2");

    // Assert that the frequency resource field has enough bits to include the frequency hopping offset.
    srsran_assert(config.N_ul_hop < frequency_resource_nof_bits.value(),
                  "The frequency resource field must have enough bits to hold the frequency hopping offset");

    // Assert that the frequency hopping offset can be packed with the allocated bits.
    srsran_assert(config.hopping_offset < (1U << config.N_ul_hop),
                  "DCI frequency offset value ({}) cannot be packed with the allocated number of bits ({})",
                  config.hopping_offset,
                  config.N_ul_hop);

    // Position of the LSB bit of the hopping offset within the frequency domain resource assignment field, as per
    // TS38.212 Section 7.3.1.1.1.
    unsigned hopping_offset_lsb_pos = frequency_resource_nof_bits.value() - config.N_ul_hop;

    // Frequency resource mask, to truncate the frequency resource payload before adding the hopping offset bits.
    unsigned freq_resource_mask = (1U << hopping_offset_lsb_pos) - 1;

    // Add the frequency hopping offset to the frequency domain resource assignment field.
    freq_resource_payload =
        (config.frequency_resource & freq_resource_mask) + (config.hopping_offset << hopping_offset_lsb_pos);
  }

  // Frequency domain resource assignment - frequency_resource_nof_bits bits.
  payload.push_back(freq_resource_payload, frequency_resource_nof_bits.value());

  // Time domain resource assignment - 4 bit.
  payload.push_back(config.time_resource, 4);

  // Frequency hopping flag - 1 bit.
  payload.push_back(config.frequency_hopping_flag, 1);

  // Modulation coding scheme - 5 bits.
  payload.push_back(config.modulation_coding_scheme, 5);

  // New data indicator - 1 bit, reserved.
  payload.push_back(0x00U, 1);

  // Redundancy version - 2 bit.
  payload.push_back(config.redundancy_version, 2);

  // HARQ process number - 4 bit, reserved.
  payload.push_back(0x00U, 4);

  // TPC command for scheduled PUSCH - 2 bit.
  payload.push_back(config.tpc_command, 2);

  if (config.payload_size.padding_incl_ul_sul.value() > 0) {
    // Padding bits, including UL/SUL reserved field.
    payload.push_back(0x00U, config.payload_size.padding_incl_ul_sul.value());
  }

  // Assert total payload size.
  srsran_assert(units::bits(payload.size()) == config.payload_size.total,
                "Constructed payload size (i.e., {}) does not match expected payload size. Expected sizes:\n{}",
                units::bits(payload.size()),
                config.payload_size);

  return payload;
}

dci_payload srsran::dci_1_0_c_rnti_pack(const dci_1_0_c_rnti_configuration& config)
{
  srsran_assert(config.payload_size.total.value() >= 12, "DCI payloads must be at least 12 bit long");

  dci_payload payload;

  // Identifier for DCI formats - 1 bit. This field is always 1, indicating a DL DCI format.
  payload.push_back(0x01U, 1);

  // Frequency domain resource assignment - frequency_resource_nof_bits bits.
  payload.push_back(config.frequency_resource, config.payload_size.frequency_resource.value());

  // Time domain resource assignment - 4 bit.
  payload.push_back(config.time_resource, 4);

  // VRB-to-PRB mapping - 1 bit.
  payload.push_back(config.vrb_to_prb_mapping, 1);

  // Modulation coding scheme - 5 bits.
  payload.push_back(config.modulation_coding_scheme, 5);

  // New data indicator - 1 bit.
  payload.push_back(config.new_data_indicator, 1);

  // Redundancy version - 2 bit.
  payload.push_back(config.redundancy_version, 2);

  // HARQ process number - 4 bit.
  payload.push_back(config.harq_process_number, 4);

  // Downlink assignment index - 2 bit.
  payload.push_back(config.dl_assignment_index, 2);

  // TPC command for scheduled PUCCH - 2 bit.
  payload.push_back(config.tpc_command, 2);

  // PUCCH resource indicator - 3 bit.
  payload.push_back(config.pucch_resource_indicator, 3);

  // PDSCH to HARQ feedback timing indicator - 3 bit.
  payload.push_back(config.pdsch_harq_fb_timing_indicator, 3);

  // Padding - nof_padding_bits bits.
  payload.push_back(0x00U, config.payload_size.padding.value());

  // Assert total payload size.
  srsran_assert(units::bits(payload.size()) == config.payload_size.total,
                "Constructed payload size (i.e., {}) does not match expected payload size. Expected sizes:\n{}",
                units::bits(payload.size()),
                config.payload_size);

  return payload;
}

dci_payload srsran::dci_1_0_p_rnti_pack(const dci_1_0_p_rnti_configuration& config)
{
  units::bits frequency_resource_nof_bits(log2_ceil(config.N_rb_dl_bwp * (config.N_rb_dl_bwp + 1) / 2));
  dci_payload payload;

  // Short Message Indicator - 2 bits.
  switch (config.short_messages_indicator) {
    case dci_1_0_p_rnti_configuration::payload_info::scheduling_information:
      payload.push_back(0b01U, 2);
      break;
    case dci_1_0_p_rnti_configuration::payload_info::short_messages:
      payload.push_back(0b10U, 2);
      break;
    case dci_1_0_p_rnti_configuration::payload_info::both:
      payload.push_back(0b11U, 2);
      break;
  }

  // Short Messages - 8 bits.
  if (config.short_messages_indicator == dci_1_0_p_rnti_configuration::payload_info::scheduling_information) {
    // If only the scheduling information for paging is carried, this bit field is reserved.
    payload.push_back(0x00U, 8);
  } else {
    payload.push_back(config.short_messages, 8);
  }

  if (config.short_messages_indicator == dci_1_0_p_rnti_configuration::payload_info::short_messages) {
    // If only the short message is carried, the scheduling information for paging bit fields are reserved.
    payload.push_back(0x00U, frequency_resource_nof_bits.value() + 12);
  } else {
    // Frequency domain resource assignment - frequency_resource_nof_bits bits.
    payload.push_back(config.frequency_resource, frequency_resource_nof_bits.value());

    // Time domain resource assignment - 4 bits.
    payload.push_back(config.time_resource, 4);

    // VRB-to-PRB mapping - 1 bit.
    payload.push_back(config.vrb_to_prb_mapping, 1);

    // Modulation and coding scheme - 5 bits.
    payload.push_back(config.modulation_coding_scheme, 5);

    // Transport Block scaling - 2 bits.
    payload.push_back(config.tb_scaling, 2);
  }

  // Reserved bits: 6 bits.
  payload.push_back(0x00U, 6);

  return payload;
}

dci_payload srsran::dci_1_0_si_rnti_pack(const dci_1_0_si_rnti_configuration& config)
{
  units::bits frequency_resource_nof_bits(log2_ceil(config.N_rb_dl_bwp * (config.N_rb_dl_bwp + 1) / 2));
  dci_payload payload;

  // Frequency domain resource assignment - frequency_resource_nof_bits bits.
  payload.push_back(config.frequency_resource, frequency_resource_nof_bits.value());

  // Time domain resource assignment - 4 bit.
  payload.push_back(config.time_resource, 4);

  // VRB-to-PRB mapping - 1 bit.
  payload.push_back(config.vrb_to_prb_mapping, 1);

  // Modulation coding scheme - 5 bits.
  payload.push_back(config.modulation_coding_scheme, 5);

  // Redundancy version - 2 bits.
  payload.push_back(config.redundancy_version, 2);

  // System information indicator - 1 bit.
  payload.push_back(config.system_information_indicator, 1);

  // Reserved bits - 15 bit.
  payload.push_back(0x00U, 15);

  return payload;
}

dci_payload srsran::dci_1_0_ra_rnti_pack(const dci_1_0_ra_rnti_configuration& config)
{
  units::bits frequency_resource_nof_bits(log2_ceil(config.N_rb_dl_bwp * (config.N_rb_dl_bwp + 1) / 2));
  dci_payload payload;

  // Frequency domain resource assignment - frequency_resource_nof_bits bits.
  payload.push_back(config.frequency_resource, frequency_resource_nof_bits.value());

  // Time domain resource assignment - 4 bits.
  payload.push_back(config.time_resource, 4);

  // VRB-to-PRB mapping - 1 bit.
  payload.push_back(config.vrb_to_prb_mapping, 1);

  // Modulation and coding scheme - 5 bits.
  payload.push_back(config.modulation_coding_scheme, 5);

  // Transport Block scaling - 2 bits.
  payload.push_back(config.tb_scaling, 2);

  // Reserved bits - 16 bits.
  payload.push_back(0x00U, 16);

  return payload;
}

dci_payload srsran::dci_1_0_tc_rnti_pack(const dci_1_0_tc_rnti_configuration& config)
{
  units::bits frequency_resource_nof_bits(log2_ceil(config.N_rb_dl_bwp * (config.N_rb_dl_bwp + 1) / 2));
  dci_payload payload;

  // Identifier for DCI formats - 1 bit. This field is always 1, indicating a DL DCI format.
  payload.push_back(0x01U, 1);

  // Frequency domain resource assignment - frequency_resource_nof_bits bits.
  payload.push_back(config.frequency_resource, frequency_resource_nof_bits.value());

  // Time domain resource assignment - 4 bit.
  payload.push_back(config.time_resource, 4);

  // VRB-to-PRB mapping - 1 bit.
  payload.push_back(config.vrb_to_prb_mapping, 1);

  // Modulation coding scheme - 5 bits.
  payload.push_back(config.modulation_coding_scheme, 5);

  // New data indicator - 1 bit.
  payload.push_back(config.new_data_indicator, 1);

  // Redundancy version - 2 bit.
  payload.push_back(config.redundancy_version, 2);

  // HARQ process number - 4 bit.
  payload.push_back(config.harq_process_number, 4);

  // Downlink assignment index - 2 bit, reserved.
  payload.push_back(0x00U, 2);

  // TPC command for scheduled PUCCH - 2 bit.
  payload.push_back(config.tpc_command, 2);

  // PUCCH resource indicator - 3 bit.
  payload.push_back(config.pucch_resource_indicator, 3);

  // PDSCH to HARQ feedback timing indicator - 3 bit.
  payload.push_back(config.pdsch_harq_fb_timing_indicator, 3);

  return payload;
}

dci_payload srsran::dci_0_1_pack(const dci_0_1_configuration& config)
{
  srsran_assert(config.payload_size.total.value() >= 12, "DCI payloads must be at least 12 bit long");

  // Assertions for unsupported fields.
  srsran_assert(!config.ul_sul_indicator.has_value(), "UL/SUL indicator field is not currently supported.");
  srsran_assert(!config.precoding_info_nof_layers.has_value(),
                "Precoding information and number of layers field is not currently supported.");
  srsran_assert(!config.ptrs_dmrs_association.has_value(), "PT-RS/DM-RS association field is not currently supported.");

  dci_payload payload;

  // Identifier for DCI formats - 1 bit. This field is always 0, indicating an UL DCI format.
  payload.push_back(0x00U, 1);

  // Carrier indicator - 0 or 3 bits.
  if (config.carrier_indicator.has_value()) {
    payload.push_back(config.carrier_indicator.value(), config.payload_size.carrier_indicator.value());
  }

  // UL/SUL indicator - 0 or 1 bit.
  if (config.ul_sul_indicator.has_value()) {
    payload.push_back(config.ul_sul_indicator.value(), config.payload_size.ul_sul_indicator.value());
  }

  // Bandwidth part indicator - 0, 1 or 2 bits.
  if (config.bwp_indicator.has_value()) {
    payload.push_back(config.bwp_indicator.value(), config.payload_size.bwp_indicator.value());
  }

  units::bits frequency_resource_nof_bits = config.payload_size.frequency_resource;

  if (config.dynamic_pusch_res_allocation_type.has_value()) {
    // Indicates the DCI resource allocation type if both resource allocation type 0 and type 1 are configured.
    unsigned dynamic_alloc_type_indicator =
        config.dynamic_pusch_res_allocation_type == dynamic_resource_allocation::type_0 ? 0 : 1;

    // The MSB bit of the frequency domain allocation field is used to indicate the resource allocation type, as per
    // TS38.212 Section 7.3.1.1.2.
    payload.push_back(dynamic_alloc_type_indicator, 1);

    // The rest of the LSB bits are used to pack the frequency domain resource allocation.
    frequency_resource_nof_bits -= units::bits(1);
  }

  if (config.frequency_hopping_flag.has_value() && (config.frequency_hopping_flag.value() == 1)) {
    // Assert that the number of bits used to pack the frequency hopping offset is valid.
    srsran_assert((config.N_ul_hop.value() == 1) || (config.N_ul_hop.value() == 2),
                  "DCI frequency offset number of bits must be either 1 or 2");

    // Assert that the frequency resource field has enough bits to include the frequency hopping offset.
    srsran_assert(config.N_ul_hop.value() < frequency_resource_nof_bits.value(),
                  "The frequency resource field must have enough bits to hold the frequency hopping offset");

    // Assert that the frequency hopping offset can be packed with the allocated bits.
    srsran_assert(config.hopping_offset.value() < (1U << config.N_ul_hop.value()),
                  "DCI frequency offset value, i.e., {} cannot be packed with the allocated number of bits, i.e., {}",
                  config.hopping_offset,
                  config.N_ul_hop);

    // Truncate the frequency resource allocation field.
    frequency_resource_nof_bits -= units::bits(config.N_ul_hop.value());

    // Frequency hopping offset - 1 or 2 bits.
    payload.push_back(config.hopping_offset.value(), config.N_ul_hop.value());
  }

  // Frequency domain resource assignment - frequency_resource_nof_bits bits.
  payload.push_back(config.frequency_resource, frequency_resource_nof_bits.value());

  // Time domain resource assignment - 0, 1, 2, 3 or 4 bits.
  if (config.payload_size.time_resource != units::bits(0)) {
    payload.push_back(config.time_resource, config.payload_size.time_resource.value());
  }

  // Frequency hopping flag - 0 or 1 bit.
  if (config.frequency_hopping_flag.has_value()) {
    payload.push_back(config.frequency_hopping_flag.value(), config.payload_size.freq_hopping_flag.value());
  }

  // Modulation coding scheme - 5 bits.
  payload.push_back(config.modulation_coding_scheme, 5);

  // New data indicator - 1 bit.
  payload.push_back(config.new_data_indicator, 1);

  // Redundancy version - 2 bits.
  payload.push_back(config.redundancy_version, 2);

  // HARQ process number - 4 bits.
  payload.push_back(config.harq_process_number, 4);

  // 1st downlink assignment index - 1 or 2 bits.
  payload.push_back(config.first_dl_assignment_index, config.payload_size.first_dl_assignment_idx.value());

  // 2nd downlink assignment index - 0 or 2 bits.
  if (config.second_dl_assignment_index.has_value()) {
    payload.push_back(config.second_dl_assignment_index.value(), config.payload_size.second_dl_assignment_idx.value());
  }

  // TPC command for scheduled PUSCH - 2 bits.
  payload.push_back(config.tpc_command, 2);

  // SRS resource indicator (SRI).
  payload.push_back(config.srs_resource_indicator, config.payload_size.srs_resource_indicator.value());

  // Precoding information and number of layers - 0 to 6 bits.
  if (config.precoding_info_nof_layers.has_value()) {
    payload.push_back(config.precoding_info_nof_layers.value(), config.payload_size.precoding_info_nof_layers.value());
  }

  // Antenna ports for PUSCH transmission - 2, 3, 4 or 5 bits.
  payload.push_back(config.antenna_ports, config.payload_size.antenna_ports.value());

  // SRS request - 2 or 3 bits.
  payload.push_back(config.srs_request, config.payload_size.srs_request.value());

  // CSI request - 0 to 6 bits.
  if (config.csi_request.has_value()) {
    payload.push_back(config.csi_request.value(), config.payload_size.csi_request.value());
  }

  // CBG Transmission Information (CBGTI) - 0, 2, 4, 6 or 8 bits.
  if (config.cbg_transmission_info.has_value()) {
    payload.push_back(config.cbg_transmission_info.value(), config.payload_size.cbg_transmission_info.value());
  }

  // PT-RS/DM-RS association - 0 or 2 bits.
  if (config.ptrs_dmrs_association.has_value()) {
    payload.push_back(config.ptrs_dmrs_association.value(), config.payload_size.ptrs_dmrs_association.value());
  }

  // Beta offset indicator - 0 or 2 bits.
  if (config.beta_offset_indicator.has_value()) {
    payload.push_back(config.beta_offset_indicator.value(), config.payload_size.beta_offset_indicator.value());
  }

  // DM-RS sequence initialization - 0 or 1 bit.
  if (config.dmrs_seq_initialization.has_value()) {
    payload.push_back(config.dmrs_seq_initialization.value(), config.payload_size.dmrs_seq_initialization.value());
  }

  // UL-SCH indicator - 1 bit.
  payload.push_back(config.ul_sch_indicator, 1);

  // Padding bits, if necessary, as per TS38.212 Section 7.3.1.0.
  if (config.payload_size.padding.value() > 0) {
    payload.push_back(0x00U, config.payload_size.padding.value());
  }

  // Assert total payload size.
  srsran_assert(units::bits(payload.size()) == config.payload_size.total,
                "Constructed payload size (i.e., {}) does not match expected payload size. Expected sizes:\n{}",
                units::bits(payload.size()),
                config.payload_size);

  return payload;
}

dci_payload srsran::dci_1_1_pack(const dci_1_1_configuration& config)
{
  srsran_assert(config.payload_size.total.value() >= 12, "DCI payloads must be at least 12 bit long");

  dci_payload payload;

  // Identifier for DCI formats - 1 bit. This field is always 1, indicating a DL DCI format.
  payload.push_back(0x01U, 1);

  // Carrier indicator - 0 or 3 bits.
  if (config.carrier_indicator.has_value()) {
    payload.push_back(config.carrier_indicator.value(), config.payload_size.carrier_indicator.value());
  }

  // Bandwidth part indicator - 0, 1 or 2 bits.
  if (config.bwp_indicator.has_value()) {
    payload.push_back(config.bwp_indicator.value(), config.payload_size.bwp_indicator.value());
  }

  units::bits frequency_resource_nof_bits = config.payload_size.frequency_resource;

  if (config.dynamic_pdsch_res_allocation_type.has_value()) {
    // Indicates the DCI resource allocation type if both resource allocation type 0 and type 1 are configured.
    unsigned dynamic_alloc_type_indicator =
        config.dynamic_pdsch_res_allocation_type == dynamic_resource_allocation::type_0 ? 0 : 1;

    // The MSB bit of the frequency domain allocation field is used to indicate the resource allocation type, as per
    // TS38.212 Section 7.3.1.2.2.
    payload.push_back(dynamic_alloc_type_indicator, 1);

    // The rest of the LSB bits are used to pack the frequency domain resource allocation.
    frequency_resource_nof_bits -= units::bits(1);
  }

  // Frequency domain resource assignment - frequency_resource_nof_bits bits.
  payload.push_back(config.frequency_resource, frequency_resource_nof_bits.value());

  // Time domain resource assignment - 0, 1, 2, 3 or 4 bits.
  if (config.payload_size.time_resource != units::bits(0)) {
    payload.push_back(config.time_resource, config.payload_size.time_resource.value());
  }

  // VRB-to-PRB mapping - 0 or 1 bit.
  if (config.vrb_prb_mapping.has_value()) {
    payload.push_back(config.vrb_prb_mapping.value(), config.payload_size.vrb_prb_mapping.value());
  }

  // PRB bundling size indicator - 0 or 1 bit.
  if (config.prb_bundling_size_indicator.has_value()) {
    payload.push_back(config.prb_bundling_size_indicator.value(),
                      config.payload_size.prb_bundling_size_indicator.value());
  }

  // Rate matching indicator - 0, 1 or 2 bits.
  if (config.rate_matching_indicator.has_value()) {
    payload.push_back(config.rate_matching_indicator.value(), config.payload_size.rate_matching_indicator.value());
  }

  // ZP CSI-RS trigger - 0, 1 or 2 bits.
  if (config.zp_csi_rs_trigger.has_value()) {
    payload.push_back(config.zp_csi_rs_trigger.value(), config.payload_size.zp_csi_rs_trigger.value());
  }

  // Modulation coding scheme for TB 1 - 5 bits.
  payload.push_back(config.tb1_modulation_coding_scheme, 5);

  // New data indicator for TB 1 - 1 bit.
  payload.push_back(config.tb1_new_data_indicator, 1);

  // Redundancy version for TB 1 - 2 bits.
  payload.push_back(config.tb1_redundancy_version, 2);

  // Modulation coding scheme for TB 2 - 0 or 5 bits.
  if (config.tb2_modulation_coding_scheme.has_value()) {
    payload.push_back(config.tb2_modulation_coding_scheme.value(),
                      config.payload_size.tb2_modulation_coding_scheme.value());
  }

  // New data indicator for TB 2 - 0 or 1 bit.
  if (config.tb2_new_data_indicator.has_value()) {
    payload.push_back(config.tb2_new_data_indicator.value(), config.payload_size.tb2_new_data_indicator.value());
  }

  // Redundancy version for TB 2 - 0 or 2 bits.
  if (config.tb2_redundancy_version.has_value()) {
    payload.push_back(config.tb2_redundancy_version.value(), config.payload_size.tb2_redundancy_version.value());
  }

  // HARQ process number - 4 bits.
  payload.push_back(config.harq_process_number, 4);

  // Downlink Assignment Index (DAI) - 0, 2 or 4 bits.
  if (config.downlink_assignment_index.has_value()) {
    payload.push_back(config.downlink_assignment_index.value(), config.payload_size.downlink_assignment_index.value());
  }

  // TPC command for scheduled PUSCH - 2 bits.
  payload.push_back(config.tpc_command, 2);

  // PUCCH resource indicator - 3 bits.
  payload.push_back(config.pucch_resource_indicator, 3);

  // PDSCH to HARQ feedback timing indicator - 0, 1, 2 or 3 bits.
  if (config.pdsch_harq_fb_timing_indicator.has_value()) {
    payload.push_back(config.pdsch_harq_fb_timing_indicator.value(),
                      config.payload_size.pdsch_harq_fb_timing_indicator.value());
  }

  // Antenna ports for PDSCH transmission - 4, 5 or 6 bits.
  payload.push_back(config.antenna_ports, config.payload_size.antenna_ports.value());

  // Transmission configuration indication - 0 or 3 bits.
  if (config.tx_config_indication.has_value()) {
    payload.push_back(config.tx_config_indication.value(), config.payload_size.tx_config_indication.value());
  }

  // SRS request - 2 or 3 bits.
  payload.push_back(config.srs_request, config.payload_size.srs_request.value());

  // CBG Transmission Information (CBGTI) - 0, 2, 4, 6 or 8 bits.
  if (config.cbg_transmission_info.has_value()) {
    payload.push_back(config.cbg_transmission_info.value(), config.payload_size.cbg_transmission_info.value());
  }

  // CBG Flushing Information (CBGFI) - 0 or 1 bit.
  if (config.cbg_flushing_info.has_value()) {
    payload.push_back(config.cbg_flushing_info.value(), config.payload_size.cbg_flushing_info.value());
  }

  // DM-RS sequence initialization - 1 bit.
  payload.push_back(config.dmrs_seq_initialization, 1);

  // Padding bits, if necessary, as per TS38.212 Section 7.3.1.0.
  if (config.payload_size.padding.value() > 0) {
    payload.push_back(0x00U, config.payload_size.padding.value());
  }

  // Assert total payload size.
  srsran_assert(units::bits(payload.size()) == config.payload_size.total,
                "Constructed payload size (i.e., {}) does not match expected payload size. Expected sizes:\n{}",
                units::bits(payload.size()),
                config.payload_size);

  return payload;
}

dci_payload srsran::dci_rar_pack(const dci_rar_configuration& config)
{
  dci_payload payload;

  // Frequency hopping flag - 1 bit.
  payload.push_back(config.frequency_hopping_flag, 1);

  // PUSCH frequency resource allocation - 14 bits.
  payload.push_back(config.frequency_resource, 14);

  // PUSCH time resource allocation - 4 bits.
  payload.push_back(config.time_resource, 4);

  // Modulation and coding scheme - 4 bits.
  payload.push_back(config.modulation_coding_scheme, 4);

  // TPC command for PUSCH - 3 bits.
  payload.push_back(config.tpc, 3);

  // CSI request - 1 bit.
  payload.push_back(config.csi_request, 1);

  return payload;
}

bool srsran::validate_dci_size_config(const dci_size_config& config)
{
  // Check that UL and DL BWP and CORESET 0 bandwidths are within range.
  if ((config.dl_bwp_initial_bw > MAX_RB) || (config.ul_bwp_initial_bw > MAX_RB) || (config.coreset0_bw > MAX_RB)) {
    return false;
  }

  // Fallback DCI formats monitored on a CSS need the initial UL and DL BWP bandwidth.
  if ((config.dl_bwp_initial_bw == 0) || (config.ul_bwp_initial_bw == 0)) {
    return false;
  }

  // Supplementary Uplink is not currently supported by the DCI size alignment procedure.
  if (config.sul_configured) {
    return false;
  }

  // Checks pertaining to any DCI format on a USS.
  if (config.dci_0_0_and_1_0_ue_ss || config.dci_0_1_and_1_1_ue_ss) {
    // DCI formats monitored on a USS need the active UL and DL BWP bandwidth.
    if ((config.dl_bwp_active_bw == 0) || (config.dl_bwp_active_bw > MAX_RB) || (config.ul_bwp_active_bw == 0) ||
        (config.ul_bwp_active_bw > MAX_RB)) {
      return false;
    }
  }

  // Checks pertaining to non-fallback DCI formats.
  if (config.dci_0_1_and_1_1_ue_ss) {
    // Number of BWP configured by higher layers cannot exceed 4.
    if ((config.nof_ul_bwp_rrc > 4) || config.nof_dl_bwp_rrc > 4) {
      return false;
    }

    // Number of UL time domain resource allocations must be within the valid range {1, ..., 16}
    if ((config.nof_ul_time_domain_res == 0) || (config.nof_ul_time_domain_res > 16)) {
      return false;
    }

    // Number of DL time domain resource allocations must be within the valid range {1, ..., 16}
    if ((config.nof_dl_time_domain_res == 0) || (config.nof_dl_time_domain_res > 16)) {
      return false;
    }

    // Size of the DCI request field, determined by the higher layer parameter reportTriggerSize, cannot exceed 6.
    if (config.report_trigger_size > 6) {
      return false;
    }

    // Number of aperiodic ZP CSI-RS resource sets cannot exceed 3.
    if (config.nof_aperiodic_zp_csi > 3) {
      return false;
    }

    // Number of PDSCH to DL ACK timings exceeds the valid range {1, ..., 8}.
    if ((config.nof_pdsch_ack_timings == 0) || (config.nof_pdsch_ack_timings > 8)) {
      return false;
    }

    // Requirements if transform precoding is enabled.
    if (config.transform_precoding_enabled) {
      // With transform precoding enabled for the UL, the PUSCH DM-RS configuration can only be type 1.
      if (config.pusch_dmrs_A_type.has_value() && (config.pusch_dmrs_A_type == dmrs_config_type::type2)) {
        return false;
      }
      if (config.pusch_dmrs_B_type.has_value() && (config.pusch_dmrs_B_type == dmrs_config_type::type2)) {
        return false;
      }
    }

    // Requirement if the PDSCH HARQ-ACK codebook type is set to dynamic.
    if (config.pdsch_harq_ack_cb == pdsch_harq_ack_codebook::dynamic) {
      // The dynamic dual HARQ-ACK codebook flag is required.
      if (!config.dynamic_dual_harq_ack_cb.has_value()) {
        return false;
      }
    }

    // Requirements for UL resource allocation type 0.
    if (config.pusch_res_allocation_type != resource_allocation::resource_allocation_type_1) {
      // Number of UL RBGs is required, and must not exceed the valid range.
      if (!config.nof_ul_rb_groups.has_value() || (config.nof_ul_rb_groups.value() > MAX_NOF_RBGS) ||
          (config.nof_ul_rb_groups.value() == 0)) {
        return false;
      }
    }

    // Requirements for DL resource allocation type 0.
    if (config.pdsch_res_allocation_type != resource_allocation::resource_allocation_type_1) {
      // Number of DL RBGs is required, and must not exceed the valid range.
      if (!config.nof_dl_rb_groups.has_value() || (config.nof_dl_rb_groups.value() > MAX_NOF_RBGS) ||
          (config.nof_dl_rb_groups.value() == 0)) {
        return false;
      }
    }

    // Requirements for DL resource allocation type 1.
    if (config.pdsch_res_allocation_type != resource_allocation::resource_allocation_type_0) {
      // Interleaved VRB to PRB mapping flag is required.
      if (!config.interleaved_vrb_prb_mapping.has_value()) {
        return false;
      }
    }

    // Requirements for non-codebook based transmission.
    if (config.tx_config_non_codebook) {
      // PUSCH max number of layers is required, and it must be set to one.
      if (!config.pusch_max_layers.has_value() || (config.pusch_max_layers.value() != 1)) {
        return false;
      }

      // For non-codebook based transmission, the number of SRS resources must be within the valid range {1, ..., 4}.
      if ((config.nof_srs_resources == 0) || (config.nof_srs_resources > 4)) {
        return false;
      }

      // Requirements for codebook based transmission.
    } else {
      // Maximum rank is required for codebook-based transmission, and it must be within the valid range {1, ..., 4}.
      if (!config.max_rank.has_value() || (config.max_rank.value() == 0) || (config.max_rank.value() > 4)) {
        return false;
      }

      // For codebook based transmission, the number of SRS ports is required.
      if (!config.nof_srs_ports.has_value()) {
        return false;
      }

      // For codebook based transmission, the number of SRS resources must be within the valid range {1, 2}.
      if ((config.nof_srs_resources == 0) || (config.nof_srs_resources > 2)) {
        return false;
      }

      // Maximum rank cannot be greater than the number of SRS ports.
      if (config.max_rank.value() > config.nof_srs_ports.value()) {
        return false;
      }

      // The number of SRS ports must be a valid value {1, 2, 4}.
      switch (config.nof_srs_ports.value()) {
        case 1:
          break;
        case 2:
        case 4:
          // Codebook subset is required for codebook based transmission with more than one antenna port.
          if (!config.cb_subset.has_value()) {
            return false;
          }
          // Currently, UL precoding with multiple antenna ports is not supported.
          return false;
        default:
          return false;
      }
    }

    // PT-RS to DM-RS association is not currently supported.
    if (config.ptrs_uplink_configured && !config.transform_precoding_enabled &&
        (config.tx_config_non_codebook || (config.max_rank.value() > 1))) {
      return false;
    }

    // At least one PUSCH DM-RS mapping must be configured.
    if ((!config.pusch_dmrs_A_type.has_value() || !config.pusch_dmrs_A_max_len.has_value()) &&
        (!config.pusch_dmrs_B_type.has_value() || !config.pusch_dmrs_B_max_len.has_value())) {
      return false;
    }

    // At least one PDSCH DM-RS mapping must be configured.
    if ((!config.pdsch_dmrs_A_type.has_value() || !config.pdsch_dmrs_A_max_len.has_value()) &&
        (!config.pdsch_dmrs_B_type.has_value() || !config.pdsch_dmrs_B_max_len.has_value())) {
      return false;
    }

    if (config.max_cbg_tb_pusch.has_value()) {
      // The Maximum PUSCH CBG per TB must be set to a valid value.
      if (std::unordered_set<unsigned>({2, 4, 6, 8}).count(config.max_cbg_tb_pusch.value()) == 0) {
        return false;
      }
    }

    if (config.max_cbg_tb_pdsch.has_value()) {
      // The Maximum PDSCH CBG per TB must be set to a valid value.
      if (std::unordered_set<unsigned>({2, 4, 6, 8}).count(config.max_cbg_tb_pdsch.value()) == 0) {
        return false;
      }
    }
  }

  return true;
}
