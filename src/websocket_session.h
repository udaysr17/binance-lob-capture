#pragma once

#include <string>
#include <vector>
#include <functional>

namespace bc {

struct WsConfig {
    std::string host;
    std::string port;
    std::string path;
    std::string subscribe_payload;
    std::uint32_t shard_id;
    std::uint64_t conn_epoch;
};

// Start a synchronous websocket session that connects, subscribes, and reads indefinitely.
// `on_message` is invoked with (recv_sec, recv_nsec, payload).
// Returns true on clean termination, false on error.
bool run_websocket(const WsConfig& config, 
                   const std::function<void(std::int64_t sec, std::int64_t nsec, std::string_view msg)>& on_message,
                   const std::function<bool()>& check_stop);

} // namespace bc
