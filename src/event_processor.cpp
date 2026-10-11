#include "event_processor.h"

#include <cinttypes>
#include <cstdio>

#include "csv_writer.h"
#include "decimal.h"

namespace bc {

namespace {

bool ts_less(RecvTs a, RecvTs b) {
  return a.sec < b.sec || (a.sec == b.sec && a.nsec < b.nsec);
}

bool strictly_ordered_bids(const std::vector<Level> &v) {
  for (std::size_t i = 1; i < v.size(); ++i) {
    if (!(v[i].price < v[i - 1].price))
      return false;
  }
  return true;
}

bool strictly_ordered_asks(const std::vector<Level> &v) {
  for (std::size_t i = 1; i < v.size(); ++i) {
    if (!(v[i].price > v[i - 1].price))
      return false;
  }
  return true;
}

} // namespace

void EventProcessor::Parsed::reset() {
  has_U = has_u = has_pu = has_last_update_id = false;
  has_bids = has_asks = has_p = has_q = false;
  U = u = pu = last_update_id = 0;
  bids.clear();
  asks.clear();
}

EventProcessor::EventProcessor() {
  symbols_.reserve(kMaxSymbols);
  parsed_.bids.reserve(1024);
  parsed_.asks.reserve(1024);
}

int EventProcessor::add_symbol(std::string_view symbol, std::int32_t row_id,
                               OutputFile *out) {
  Symbol s;
  s.name = std::string(symbol);
  s.row_id = row_id;
  s.out = out;
  symbols_.push_back(std::move(s));
  return static_cast<int>(symbols_.size() - 1);
}

void EventProcessor::add_invariant(std::uint64_t &bucket, std::uint64_t n) {
  bucket += n;
  m_.invariant_violations += n;
}

void EventProcessor::reset_sync(Symbol &s) {
  s.synced = false;
  s.last_u = 0;
  s.have_prev_diff = false;
  s.prev_diff_u = 0;
  s.book.clear();
}

EventProcessor::Outcome
EventProcessor::process(RecvTs ts, Venue venue, StreamKind kind, int symbol_id,
                        std::uint32_t shard_id, std::uint64_t conn_epoch,
                        std::uint64_t conn_seq, std::string_view payload) {
  if (symbol_id < 0 || static_cast<std::size_t>(symbol_id) >= symbols_.size()) {
    ++m_.parse_errors;
    std::fprintf(stderr, "[error] process: invalid symbol id %d\n", symbol_id);
    return Outcome::ParseError;
  }
  Symbol &s = symbols_[static_cast<std::size_t>(symbol_id)];

  switch (kind) {
  case StreamKind::DepthDiff:
    ++m_.msgs_depth_diff;
    break;
  case StreamKind::Depth5:
    ++m_.msgs_depth5;
    break;
  case StreamKind::Trade:
    ++m_.msgs_trade;
    break;
  }

  // Connection identity: a new (shard, epoch) means a new socket, so any
  // continuity with the previous one is lost.
  if (!s.seen_conn) {
    s.seen_conn = true;
  } else if (shard_id != s.shard_id || conn_epoch != s.conn_epoch) {
    if (s.synced)
      ++m_.epoch_resets;
    reset_sync(s);
  } else if (conn_seq <= s.last_conn_seq) {
    add_invariant(m_.inv_conn_seq, 1);
  }
  s.shard_id = shard_id;
  s.conn_epoch = conn_epoch;
  s.last_conn_seq = conn_seq;

  if (s.have_ts && ts_less(ts, s.last_ts))
    add_invariant(m_.inv_ts_regress, 1);
  s.have_ts = true;
  s.last_ts = ts;

  const ParseStatus ps = parse_payload(payload, s);
  if (ps == ParseStatus::Decimal) {
    ++m_.decimal_rejects;
    return Outcome::DecimalReject;
  }
  if (ps == ParseStatus::Structure) {
    ++m_.parse_errors;
    if (should_log(m_.parse_errors)) {
      std::fprintf(
          stderr, "[warn] parse error #%" PRIu64 " symbol=%s kind=%.*s\n",
          m_.parse_errors, s.name.c_str(),
          static_cast<int>(to_string(kind).size()), to_string(kind).data());
    }
    return Outcome::ParseError;
  }

  switch (kind) {
  case StreamKind::DepthDiff:
    return on_diff(s, ts, venue);
  case StreamKind::Depth5:
    return on_depth5(s, ts);
  case StreamKind::Trade:
    return on_trade(s);
  }
  return Outcome::ParseError;
}

EventProcessor::Outcome EventProcessor::on_diff(Symbol &s, RecvTs ts,
                                                Venue venue) {
  const Parsed &p = parsed_;
  const bool usdm = (venue == Venue::Usdm);
  if (!p.has_U || !p.has_u || !p.has_bids || !p.has_asks ||
      (usdm && !p.has_pu) || p.U > p.u) {
    ++m_.parse_errors;
    if (should_log(m_.parse_errors)) {
      std::fprintf(stderr,
                   "[warn] malformed depth diff for %s (missing U/u/pu/b/a)\n",
                   s.name.c_str());
    }
    return Outcome::ParseError;
  }
  if (!s.synced) {
    ++m_.dropped_unsynced_diffs;
    return Outcome::DroppedUnsynced;
  }
  if (p.u <= s.last_u) {
    ++m_.stale_diffs;
    s.have_prev_diff = true;
    s.prev_diff_u = p.u;
    return Outcome::StaleDiff;
  }
  const bool bridge_ok = (p.U <= s.last_u + 1) || (usdm && p.pu <= s.last_u);
  const bool chain_ok = !s.have_prev_diff ||
                        (usdm ? (p.pu == s.prev_diff_u) : (p.U == s.prev_diff_u + 1));
  if (!(bridge_ok && chain_ok)) {
    ++m_.gaps;
    if (should_log(m_.gaps)) {
      std::fprintf(stderr,
                   "[warn] gap #%" PRIu64 " symbol=%s last_u=%" PRIu64
                   " U=%" PRIu64 " u=%" PRIu64 " pu=%" PRIu64
                   " prev_diff_u=%" PRIu64 " -> UNSYNCED until next depth5\n",
                   m_.gaps, s.name.c_str(), s.last_u, p.U, p.u, p.pu,
                   s.prev_diff_u);
    }
    reset_sync(s);
    return Outcome::Gap;
  }
  for (const Level &l : p.bids)
    s.book.set_bid(l.price, l.qty);
  for (const Level &l : p.asks)
    s.book.set_ask(l.price, l.qty);
  s.last_u = p.u;
  s.have_prev_diff = true;
  s.prev_diff_u = p.u;
  ++m_.applied_diffs;
  const bool b = !p.bids.empty();
  const bool a = !p.asks.empty();
  emit_row(s, ts, 'D', (b && !a) ? 'B' : (a && !b) ? 'S' : 'N');
  return Outcome::Applied;
}

EventProcessor::Outcome EventProcessor::on_depth5(Symbol &s, RecvTs ts) {
  const Parsed &p = parsed_;
  // Spot: {lastUpdateId,bids,asks}. USD-M: {e,E,T,s,U,u,pu,b,a}.
  if (!(p.has_last_update_id || p.has_u) || !p.has_bids || !p.has_asks ||
      !strictly_ordered_bids(p.bids) || !strictly_ordered_asks(p.asks)) {
    ++m_.parse_errors;
    if (should_log(m_.parse_errors)) {
      std::fprintf(
          stderr,
          "[warn] malformed depth5 for %s (missing id/levels or unordered)\n",
          s.name.c_str());
    }
    return Outcome::ParseError;
  }
  for (const Level &l : p.bids) {
    if (l.qty == 0) {
      ++m_.parse_errors;
      if (should_log(m_.parse_errors)) {
        std::fprintf(stderr,
                     "[warn] depth5 rejected for %s (zero-quantity bid level)\n",
                     s.name.c_str());
      }
      return Outcome::ParseError;
    }
  }
  for (const Level &l : p.asks) {
    if (l.qty == 0) {
      ++m_.parse_errors;
      if (should_log(m_.parse_errors)) {
        std::fprintf(stderr,
                     "[warn] depth5 rejected for %s (zero-quantity ask level)\n",
                     s.name.c_str());
      }
      return Outcome::ParseError;
    }
  }
  const std::uint64_t id = p.has_last_update_id ? p.last_update_id : p.u;

  if (!s.synced) {
    s.book.load(p.bids.data(), p.bids.size(), p.asks.data(), p.asks.size());
    s.synced = true;
    s.last_u = id;
    s.have_prev_diff = false;
    s.prev_diff_u = 0;
    ++m_.seeds;
    emit_row(s, ts, 'P', 'N');
    return Outcome::Seeded;
  }
  if (id < s.last_u) {
    ++m_.stale_depth5;
    emit_row(s, ts, 'P', 'N'); // book unchanged; one row per well-formed depth5
    return Outcome::StaleDepth5;
  }
  if (id == s.last_u) {
    ++m_.d5_comparable;
    const Top5 before = s.book.top5();
    const Top5 snap =
        make_top5(p.bids.data(), p.bids.size(), p.asks.data(), p.asks.size());
    if (before != snap)
      ++m_.depth5_mismatches;
  } else {
    ++m_.depth5_ahead;
  }
  s.book.reconcile(p.bids.data(), p.bids.size(), p.asks.data(), p.asks.size());
  ++m_.reconciles;
  s.last_u = id;
  emit_row(s, ts, 'P', 'N');
  return Outcome::Reconciled;
}

EventProcessor::Outcome EventProcessor::on_trade(Symbol &s) {
  const Parsed &p = parsed_;
  if (!p.has_p || !p.has_q) {
    ++m_.parse_errors;
    if (should_log(m_.parse_errors)) {
      std::fprintf(stderr, "[warn] malformed trade for %s (missing p/q)\n",
                   s.name.c_str());
    }
    return Outcome::ParseError;
  }
  if (!s.synced) {
    ++m_.dropped_unsynced_trades;
    return Outcome::DroppedUnsynced;
  }
  return Outcome::Trade; // trades never change the book and emit no row
}

void EventProcessor::emit_row(Symbol &s, RecvTs ts, char type, char side) {
  const Top5 t = s.book.top5();
  if (t.bid_px[kTopLevels - 1] == 0) {
    ++m_.rows_bid_padded;
  }
  if (t.ask_px[kTopLevels - 1] == 0) {
    ++m_.rows_ask_padded;
  }
  const int v = count_top5_violations(t);
  if (v > 0) {
    add_invariant(m_.inv_book_order, static_cast<std::uint64_t>(v));
    if (should_log(m_.inv_book_order)) {
      std::fprintf(stderr,
                   "[warn] book invariant violated for %s (%d rule(s))\n",
                   s.name.c_str(), v);
    }
  }
  const std::uint64_t seq = s.seq_no + 1;
  s.seq_no = seq;

  std::string &b = s.out->buffer();
  append_int(b, ts.sec);
  b.push_back(',');
  append_int(b, ts.nsec);
  b.push_back(',');
  append_uint(b, seq);
  b.push_back(',');
  append_int(b, s.row_id);
  b.push_back(',');
  b.push_back(type);
  b.push_back(',');
  b.push_back(side);
  for (std::size_t i = 0; i < kTopLevels; ++i) {
    b.push_back(',');
    append_int(b, t.bid_px[i]);
  }
  for (std::size_t i = 0; i < kTopLevels; ++i) {
    b.push_back(',');
    append_int(b, t.bid_qty[i]);
  }
  for (std::size_t i = 0; i < kTopLevels; ++i) {
    b.push_back(',');
    append_int(b, t.ask_px[i]);
  }
  for (std::size_t i = 0; i < kTopLevels; ++i) {
    b.push_back(',');
    append_int(b, t.ask_qty[i]);
  }
  b.push_back('\n');
  s.out->row_done();
  ++m_.ob_rows;
}

EventProcessor::ParseStatus EventProcessor::check_decimal(std::string_view text,
                                                          std::int64_t &out,
                                                          const Symbol &s) {
  const DecimalError e = parse_scaled_1e8(text, out);
  if (e == DecimalError::Ok)
    return ParseStatus::Ok;
  if (should_log(m_.decimal_rejects + 1)) {
    std::fprintf(
        stderr,
        "[warn] decimal reject #%" PRIu64 " symbol=%s reason=%s value='%.*s'\n",
        m_.decimal_rejects + 1, s.name.c_str(), to_string(e),
        static_cast<int>(text.size() > 64 ? 64 : text.size()), text.data());
  }
  return ParseStatus::Decimal;
}

EventProcessor::ParseStatus
EventProcessor::parse_levels(JsonCursor &c, std::vector<Level> &out,
                             const Symbol &s) {
  if (!c.consume('['))
    return ParseStatus::Structure;
  bool first = true, done = false;
  while (c.next_element(first, done)) {
    if (done)
      return ParseStatus::Ok;
    if (!c.consume('['))
      return ParseStatus::Structure;
    std::string_view px, qty;
    if (!c.read_string(px) || !c.consume(',') || !c.read_string(qty)) {
      return ParseStatus::Structure;
    }
    // Tolerate (skip) any additional elements in a level.
    while (c.consume(',')) {
      if (!c.skip_value())
        return ParseStatus::Structure;
    }
    if (!c.consume(']'))
      return ParseStatus::Structure;
    Level l;
    if (check_decimal(px, l.price, s) != ParseStatus::Ok)
      return ParseStatus::Decimal;
    if (check_decimal(qty, l.qty, s) != ParseStatus::Ok)
      return ParseStatus::Decimal;
    out.push_back(l);
  }
  return ParseStatus::Structure;
}

EventProcessor::ParseStatus
EventProcessor::parse_payload(std::string_view payload, const Symbol &s) {
  parsed_.reset();
  Parsed &p = parsed_;
  JsonCursor c(payload);
  if (!c.consume('{'))
    return ParseStatus::Structure;
  bool first = true, done = false;
  std::string_view key;
  while (c.next_key(first, key, done)) {
    if (done)
      break;
    ParseStatus st = ParseStatus::Ok;
    if (key == "U") {
      if (!c.read_uint(p.U))
        return ParseStatus::Structure;
      p.has_U = true;
    } else if (key == "u") {
      if (!c.read_uint(p.u))
        return ParseStatus::Structure;
      p.has_u = true;
    } else if (key == "pu") {
      if (!c.read_uint(p.pu))
        return ParseStatus::Structure;
      p.has_pu = true;
    } else if (key == "lastUpdateId") {
      if (!c.read_uint(p.last_update_id))
        return ParseStatus::Structure;
      p.has_last_update_id = true;
    } else if (key == "b" || key == "bids") {
      p.bids.clear();
      st = parse_levels(c, p.bids, s);
      p.has_bids = true;
    } else if (key == "a" || key == "asks") {
      p.asks.clear();
      st = parse_levels(c, p.asks, s);
      p.has_asks = true;
    } else if (key == "p" || key == "q") {
      std::string_view text;
      if (!c.read_string(text))
        return ParseStatus::Structure;
      std::int64_t ignored = 0;
      st = check_decimal(text, ignored, s);
      (key == "p" ? p.has_p : p.has_q) = true;
    } else {
      if (!c.skip_value())
        return ParseStatus::Structure;
    }
    if (st != ParseStatus::Ok)
      return st;
  }
  if (!done || !c.at_end())
    return ParseStatus::Structure;
  return ParseStatus::Ok;
}

} // namespace bc
