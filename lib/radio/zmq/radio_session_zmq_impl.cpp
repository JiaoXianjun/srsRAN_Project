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

#include "radio_session_zmq_impl.h"

using namespace srsran;

radio_session_zmq_impl::radio_session_zmq_impl(const radio_configuration::radio& config,
                                               task_executor&                    async_task_executor,
                                               radio_notification_handler&       notifier) :
  logger(srslog::fetch_basic_logger("RF", false))
{
  // Make ZMQ context.
  zmq_context = zmq_ctx_new();
  if (zmq_context == nullptr) {
    logger.error("Failed to create ZMQ context. {}.", zmq_strerror(zmq_errno()));
    return;
  }

  // For each Tx stream.
  for (unsigned stream_id = 0; stream_id != config.tx_streams.size(); ++stream_id) {
    const radio_configuration::stream& radio_stream_config = config.tx_streams[stream_id];

    // Prepare transmit stream configuration.
    radio_zmq_tx_stream::stream_description stream_config;
    stream_config.socket_type = ZMQ_REP;
    for (unsigned channel_id = 0; channel_id != config.tx_streams.size(); ++channel_id) {
      stream_config.address.push_back(radio_stream_config.channels[channel_id].args);
    }
    stream_config.stream_id         = stream_id;
    stream_config.stream_id_str     = "zmq:tx:" + std::to_string(stream_id);
    stream_config.log_level         = config.log_level;
    stream_config.trx_timeout_ms    = DEFAULT_TRX_TIMEOUT_MS;
    stream_config.linger_timeout_ms = DEFAULT_LINGER_TIMEOUT_MS;
    stream_config.buffer_size       = DEFAULT_STREAM_BUFFER_SIZE;

    // Create stream.
    tx_streams.push_back(
        std::make_unique<radio_zmq_tx_stream>(zmq_context, stream_config, async_task_executor, notifier));

    // Check is the stream was created successfully.
    if (!tx_streams.back()->is_successful()) {
      return;
    }
  }

  // For each Rx stream.
  for (unsigned stream_id = 0; stream_id != config.rx_streams.size(); ++stream_id) {
    const radio_configuration::stream& radio_stream_config = config.rx_streams[stream_id];

    // Prepare transmit stream configuration.
    radio_zmq_rx_stream::stream_description stream_config;
    stream_config.socket_type = ZMQ_REQ;
    for (unsigned channel_id = 0; channel_id != config.rx_streams.size(); ++channel_id) {
      stream_config.address.push_back(radio_stream_config.channels[channel_id].args);
    }
    stream_config.stream_id         = stream_id;
    stream_config.stream_id_str     = "zmq:rx:" + std::to_string(stream_id);
    stream_config.log_level         = config.log_level;
    stream_config.trx_timeout_ms    = DEFAULT_TRX_TIMEOUT_MS;
    stream_config.linger_timeout_ms = DEFAULT_LINGER_TIMEOUT_MS;
    stream_config.buffer_size       = DEFAULT_STREAM_BUFFER_SIZE;

    // Create stream.
    rx_streams.push_back(std::make_unique<radio_zmq_rx_stream>(
        zmq_context, stream_config, async_task_executor, *tx_streams[stream_id], notifier));

    // Check is the stream was created successfully.
    if (!rx_streams.back()->is_successful()) {
      return;
    }
  }

  successful = true;
}

radio_session_zmq_impl::~radio_session_zmq_impl()
{
  // Destroy transmit and receive streams prior to ZMQ context destruction.
  tx_streams.clear();
  rx_streams.clear();

  // Destroy ZMQ context.
  if (zmq_context != nullptr) {
    zmq_ctx_shutdown(zmq_context);
    zmq_ctx_destroy(zmq_context);
    zmq_context = nullptr;
  }
}

void radio_session_zmq_impl::stop()
{
  // Signal stop for each transmit stream.
  for (auto& stream : tx_streams) {
    stream->stop();
  }

  // Signal stop for each receive stream.
  for (auto& stream : rx_streams) {
    stream->stop();
  }

  // Wait for transmitter streams to join.
  for (auto& stream : tx_streams) {
    stream->wait_stop();
  }
  for (auto& stream : rx_streams) {
    stream->wait_stop();
  }
}

baseband_gateway_transmitter& radio_session_zmq_impl::get_transmitter(unsigned stream_id)
{
  srsran_assert(stream_id < tx_streams.size(),
                "Stream identifier ({}) exceeds the number of transmit streams ({})",
                stream_id,
                tx_streams.size());

  return *tx_streams[stream_id];
}

baseband_gateway_receiver& radio_session_zmq_impl::get_receiver(unsigned stream_id)
{
  srsran_assert(stream_id < rx_streams.size(),
                "Stream identifier ({}) exceeds the number of receive streams ({})",
                stream_id,
                rx_streams.size());

  return *rx_streams[stream_id];
}

bool radio_session_zmq_impl::set_tx_gain(unsigned port_id, double gain_dB)
{
  return false;
}

bool radio_session_zmq_impl::set_rx_gain(unsigned port_id, double gain_dB)
{
  return false;
}

void radio_session_zmq_impl::start()
{
  // Do nothing.
}
