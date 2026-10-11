#include "metrics.h"

#include <cinttypes>

namespace bc {

void print_metrics(std::FILE *out, const char *title, const ProcessorMetrics &p,
                   const CaptureMetrics *c) {
  std::fprintf(out, "[metrics] %s\n", title);
  if (c != nullptr) {
    std::fprintf(out,
                 "  ws_messages=%" PRIu64 " non_data=%" PRIu64
                 " unknown_streams=%" PRIu64 " payloads_minified=%" PRIu64 "\n",
                 c->ws_messages, c->non_data_messages, c->unknown_streams,
                 c->payloads_minified);
    std::fprintf(out,
                 "  reconnects=%" PRIu64 " idle_timeouts=%" PRIu64
                 " read_errors=%" PRIu64 " connect_failures=%" PRIu64
                 " write_errors=%" PRIu64 "\n",
                 c->reconnects, c->idle_timeouts, c->read_errors,
                 c->connect_failures, c->write_errors);
  }
  std::fprintf(out,
               "  msgs depth_diff=%" PRIu64 " depth5=%" PRIu64 " trade=%" PRIu64
               " | parse_errors=%" PRIu64 " decimal_rejects=%" PRIu64 "\n",
               p.msgs_depth_diff, p.msgs_depth5, p.msgs_trade, p.parse_errors,
               p.decimal_rejects);
  std::fprintf(out,
               "  applied_diffs=%" PRIu64 " stale_diffs=%" PRIu64
               " stale_depth5=%" PRIu64 " gaps=%" PRIu64 " seeds=%" PRIu64
               " reconciles=%" PRIu64 " d5_comparable=%" PRIu64
               " d5_mismatch=%" PRIu64 " d5_ahead=%" PRIu64 "\n",
               p.applied_diffs, p.stale_diffs, p.stale_depth5, p.gaps, p.seeds,
               p.reconciles, p.d5_comparable, p.depth5_mismatches,
               p.depth5_ahead);
  std::fprintf(out,
               "  dropped_unsynced diffs=%" PRIu64 " trades=%" PRIu64
               " | reconnects=%" PRIu64 " epoch_resets=%" PRIu64 "\n",
               p.dropped_unsynced_diffs, p.dropped_unsynced_trades,
               c != nullptr ? c->reconnects : 0,
               p.epoch_resets);
  std::fprintf(out,
               "  invariant_violations=%" PRIu64 " (book=%" PRIu64
               " ts_regress=%" PRIu64 " conn_seq=%" PRIu64 ")\n",
               p.invariant_violations, p.inv_book_order,
               p.inv_ts_regress, p.inv_conn_seq);
  std::fprintf(out,
               "  rows_written market_data=%" PRIu64 " orderbook=%" PRIu64
               " (bid_padded=%" PRIu64 " ask_padded=%" PRIu64 ")\n",
               c != nullptr ? c->md_rows : 0, p.ob_rows,
               p.rows_bid_padded, p.rows_ask_padded);
  std::fflush(out);
}

} // namespace bc
