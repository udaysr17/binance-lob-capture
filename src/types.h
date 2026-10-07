#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bc {

enum class Venue : std::uint8_t { Spot, Usdm };
enum class StreamKind : std::uint8_t { DepthDiff, Depth5, Trade };

// Wall-clock receive timestamp (CLOCK_REALTIME), taken once per inbound
// message by the network layer. The processor only ever copies it.
struct RecvTs {
    std::int64_t sec = 0;
    std::int32_t nsec = 0;  // [0, 999999999]
};

constexpr std::size_t kMaxSymbols = 20;
constexpr std::size_t kMaxSymbolLen = 20;
constexpr std::int64_t kScale = 100000000;  // 1e8 for both price and quantity
constexpr int kScaleDecimals = 8;

constexpr std::string_view to_string(Venue v) {
    return v == Venue::Spot ? std::string_view("spot") : std::string_view("usdm");
}

constexpr std::string_view to_string(StreamKind k) {
    switch (k) {
        case StreamKind::DepthDiff: return "depth_diff";
        case StreamKind::Depth5: return "depth5";
        case StreamKind::Trade: return "trade";
    }
    return "unknown";
}

inline bool parse_venue(std::string_view s, Venue& out) {
    if (s == "spot") { out = Venue::Spot; return true; }
    if (s == "usdm") { out = Venue::Usdm; return true; }
    return false;
}

inline bool parse_stream_kind(std::string_view s, StreamKind& out) {
    if (s == "depth_diff") { out = StreamKind::DepthDiff; return true; }
    if (s == "depth5") { out = StreamKind::Depth5; return true; }
    if (s == "trade") { out = StreamKind::Trade; return true; }
    return false;
}

}  // namespace bc
