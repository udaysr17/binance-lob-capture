#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace bc {

// Exact header lines (without the trailing LF).
constexpr std::string_view kMarketDataHeader =
    "recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json";
constexpr std::string_view kOrderBookHeader =
    "tsec,tnsec,seqNo,id,type,side,"
    "bid0,bid1,bid2,bid3,bid4,bid_size0,bid_size1,bid_size2,bid_size3,bid_size4,"
    "ask0,ask1,ask2,ask3,ask4,ask_size0,ask_size1,ask_size2,ask_size3,ask_size4";
constexpr int kOrderBookColumns = 26;
constexpr int kMarketDataColumns = 9;

// Appends integers using std::to_chars (locale independent, no allocation
// beyond the output string's own growth).
void append_int(std::string& out, std::int64_t v);
void append_uint(std::string& out, std::uint64_t v);

// Appends `field` as an RFC 4180 quoted field: wrapped in double quotes,
// every '"' doubled. Always quotes (payload_json always contains quotes).
void append_csv_quoted(std::string& out, std::string_view field);

}  // namespace bc
