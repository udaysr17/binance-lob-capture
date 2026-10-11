#pragma once

#include <cstdint>
#include <cstdio>

namespace bc {

// Counters owned by the EventProcessor (single-threaded, plain integers).
struct ProcessorMetrics {
  std::uint64_t msgs_depth_diff = 0;
  std::uint64_t msgs_depth5 = 0;
  std::uint64_t msgs_trade = 0;
  std::uint64_t parse_errors = 0;
  std::uint64_t decimal_rejects = 0;
  std::uint64_t applied_diffs = 0;
  std::uint64_t stale_diffs = 0;
  std::uint64_t stale_depth5 = 0;
  std::uint64_t gaps = 0;
  std::uint64_t seeds = 0;
  std::uint64_t reconciles = 0;
  std::uint64_t d5_comparable = 0;
  std::uint64_t depth5_mismatches = 0;
  std::uint64_t depth5_ahead = 0;
  std::uint64_t dropped_unsynced_diffs = 0;
  std::uint64_t dropped_unsynced_trades = 0;
  std::uint64_t epoch_resets = 0;
  std::uint64_t invariant_violations = 0; // sum of the breakdown below
  std::uint64_t inv_book_order = 0; // ordering/size/crossed on an emitted top-5
  std::uint64_t inv_ts_regress = 0; // recv timestamp went backwards
  std::uint64_t inv_conn_seq = 0;   // conn_seq not increasing within an epoch
  std::uint64_t ob_rows = 0;        // order-book rows emitted
  std::uint64_t rows_bid_padded = 0; // rows where 5th bid level is 0
  std::uint64_t rows_ask_padded = 0; // rows where 5th ask level is 0
};

// Counters owned by the network/capture layer.
struct CaptureMetrics {
  std::uint64_t ws_messages = 0;       // every inbound text frame
  std::uint64_t non_data_messages = 0; // no stream/data envelope (acks, errors)
  std::uint64_t unknown_streams =
      0; // envelope with a stream we did not subscribe to
  std::uint64_t payloads_minified =
      0;                     // payload had whitespace and was minified
  std::uint64_t md_rows = 0; // market-data rows written
  std::uint64_t reconnects = 0;
  std::uint64_t idle_timeouts = 0;
  std::uint64_t read_errors = 0;
  std::uint64_t connect_failures = 0;
  std::uint64_t write_errors = 0;
};

void print_metrics(std::FILE *out, const char *title, const ProcessorMetrics &p,
                   const CaptureMetrics *c);

} // namespace bc
