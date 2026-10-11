#include "replay.h"
#include "csv_reader.h"
#include "csv_writer.h"
#include "event_processor.h"
#include "output_file.h"
#include <algorithm>
#include <charconv>
#include <iostream>
#include <unordered_map>

namespace bc {

namespace {

bool parse_uint64(std::string_view s, std::uint64_t &out) {
  auto res = std::from_chars(s.data(), s.data() + s.size(), out);
  return res.ec == std::errc();
}

bool parse_uint32(std::string_view s, std::uint32_t &out) {
  auto res = std::from_chars(s.data(), s.data() + s.size(), out);
  return res.ec == std::errc();
}

bool parse_int64(std::string_view s, std::int64_t &out) {
  auto res = std::from_chars(s.data(), s.data() + s.size(), out);
  return res.ec == std::errc();
}

} // namespace

bool run_replay(const std::string &in_path, const std::string &out_path) {
  CsvReader reader;
  std::string err;
  if (!reader.open(in_path, err)) {
    std::cerr << "replay: failed to open input file: " << in_path << " - "
              << err << "\n";
    return false;
  }

  OutputFile out;
  if (!out.open(out_path, err)) {
    std::cerr << "replay: failed to open output file: " << out_path << " - "
              << err << "\n";
    return false;
  }

  // Write header
  {
    std::string &b = out.buffer();
    b += "tsec,tnsec,seqNo,id,type,side";
    for (int i = 0; i < 5; ++i)
      b += ",bid" + std::to_string(i);
    for (int i = 0; i < 5; ++i)
      b += ",bid_size" + std::to_string(i);
    for (int i = 0; i < 5; ++i)
      b += ",ask" + std::to_string(i);
    for (int i = 0; i < 5; ++i)
      b += ",ask_size" + std::to_string(i);
    b += "\n";
    out.row_done();
  }

  EventProcessor proc;
  std::unordered_map<std::string, int> symbol_map;

  std::size_t col_sec = 0;
  std::size_t col_nsec = 1;
  std::size_t col_venue = 2;
  std::size_t col_kind = 3;
  std::size_t col_shard = 4;
  std::size_t col_epoch = 5;
  std::size_t col_seq = 6;
  std::size_t col_sym = 7;
  std::size_t col_payload = 8;

  std::vector<std::string_view> row;
  bool first = true;
  while (true) {
    auto status = reader.next(row, err);
    if (status == CsvReader::Status::Eof)
      break;
    if (status == CsvReader::Status::Error) {
      std::cout << "CsvReader error: " << err << "\n";
      break;
    }
    if (first) {
      first = false;
      for (std::size_t i = 0; i < row.size(); ++i) {
        if (row[i] == "recv_tsec" || row[i] == "recv_sec") col_sec = i;
        else if (row[i] == "recv_tnsec" || row[i] == "recv_nsec") col_nsec = i;
        else if (row[i] == "venue" || row[i] == "exchange") col_venue = i;
        else if (row[i] == "stream_kind") col_kind = i;
        else if (row[i] == "shard_id") col_shard = i;
        else if (row[i] == "conn_epoch") col_epoch = i;
        else if (row[i] == "conn_seq" || row[i] == "message_id") col_seq = i;
        else if (row[i] == "symbol") col_sym = i;
        else if (row[i] == "payload_json" || row[i] == "payload") col_payload = i;
      }
      continue; // skip header
    }
    const std::size_t max_needed = std::max({col_sec, col_nsec, col_venue, col_kind,
                                             col_shard, col_epoch, col_seq, col_sym,
                                             col_payload});
    if (row.size() <= max_needed) {
      std::cout << "Skipped row with size " << row.size() << "\n";
      continue;
    }

    std::string_view recv_sec_s = row[col_sec];
    std::string_view recv_nsec_s = row[col_nsec];
    std::string_view exchange = row[col_venue];
    std::string_view stream_kind = row[col_kind];
    std::string_view shard_id_s = row[col_shard];
    std::string_view conn_epoch_s = row[col_epoch];
    std::string_view message_id_s = row[col_seq];
    std::string_view symbol = row[col_sym];
    std::string_view payload = row[col_payload];

    Venue venue =
        (exchange == "usdm" || exchange == "USDM") ? Venue::Usdm : Venue::Spot;
    StreamKind kind = StreamKind::Trade;
    if (stream_kind == "depth_diff" || stream_kind == "DEPTH_DIFF")
      kind = StreamKind::DepthDiff;
    else if (stream_kind == "depth5" || stream_kind == "DEPTH5")
      kind = StreamKind::Depth5;
    else if (stream_kind == "trade" || stream_kind == "TRADE")
      kind = StreamKind::Trade;
    else {
      std::cout << "Unknown stream_kind: " << stream_kind << "\n";
      continue;
    }

    std::uint64_t conn_epoch = 0;
    std::uint32_t shard_id = 0;
    std::uint64_t message_id = 0;
    std::int64_t recv_sec = 0;
    std::int64_t recv_nsec = 0;

    if (!parse_uint64(conn_epoch_s, conn_epoch)) {
      std::cout << "err epoch\n";
      continue;
    }
    if (!parse_uint32(shard_id_s, shard_id)) {
      std::cout << "err shard\n";
      continue;
    }
    if (!parse_uint64(message_id_s, message_id)) {
      std::cout << "err msg\n";
      continue;
    }
    if (!parse_int64(recv_sec_s, recv_sec)) {
      std::cout << "err sec\n";
      continue;
    }
    if (!parse_int64(recv_nsec_s, recv_nsec)) {
      std::cout << "err nsec\n";
      continue;
    }

    RecvTs ts{recv_sec, static_cast<std::int32_t>(recv_nsec)};

    auto it = symbol_map.find(std::string(symbol));
    int sid = -1;
    if (it == symbol_map.end()) {
      sid = static_cast<int>(symbol_map.size()); // 0-based index to write out
      int internal_id = proc.add_symbol(symbol, sid, &out);
      symbol_map[std::string(symbol)] = internal_id;
      sid = internal_id;
    } else {
      sid = it->second;
    }

    proc.process(ts, venue, kind, sid, shard_id, conn_epoch, message_id,
                 payload);
  }

  out.close();

  // Print metrics summary
  auto m = proc.metrics();
  std::cout << "[replay] messages: depth_diff=" << m.msgs_depth_diff
            << " depth5=" << m.msgs_depth5 << " trade=" << m.msgs_trade << "\n";
  std::cout << "[replay] parse_errors=" << m.parse_errors
            << " decimal_rejects=" << m.decimal_rejects << "\n";
  std::cout << "[replay] synced_gaps=" << m.gaps << " ob_rows=" << m.ob_rows
            << "\n";
  print_metrics(stdout, "replay", m, nullptr);

  return true;
}

} // namespace bc
