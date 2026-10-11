// #include "event_processor.h"
// #include "test_framework.h"

// using namespace bc;

// namespace {

// // Dummy OutputFile subclass to intercept rows or we can just let it buffer.
// class TestFile {
// public:
//     OutputFile out;
//     std::string path = "test_sync.tmp.csv";
//     TestFile() {
//         std::string err;
//         out.open(path, err);
//     }
//     ~TestFile() {
//         out.close();
//         std::remove(path.c_str());
//     }
// };

// std::string make_depth5(std::uint64_t id, bool usdm = false) {
//     if (!usdm) {
//         return R"({"lastUpdateId":)" + std::to_string(id) +
//         R"(,"bids":[["100","1"]],"asks":[["110","1"]]})";
//     }
//     return R"({"e":"depthUpdate","E":1,"T":1,"s":"BTCUSDT","U":)" +
//     std::to_string(id) + R"(,"u":)" + std::to_string(id) +
//     R"(,"pu":0,"b":[["100","1"]],"a":[["110","1"]]})";
// }

// std::string make_diff(std::uint64_t U, std::uint64_t u, std::uint64_t pu,
// bool usdm = false) {
//     std::string s = R"({"e":"depthUpdate","E":1,"s":"BTCUSDT","U":)" +
//     std::to_string(U) + R"(,"u":)" + std::to_string(u); if (usdm) {
//         s += R"(,"pu":)" + std::to_string(pu);
//     }
//     s += R"(,"b":[["100","2"]],"a":[]})";
//     return s;
// }

// } // namespace

// TEST(sync_seed_and_stale_diff) {
//     EventProcessor p;
//     TestFile tf;
//     int id = p.add_symbol("BTCUSDT", 1, &tf.out);

//     RecvTs ts{1, 0};
//     // diff before sync -> dropped
//     auto o1 = p.process(ts, Venue::Spot, StreamKind::DepthDiff, id, 0, 0, 1,
//     make_diff(1, 2, 0)); CHECK_EQ(o1,
//     EventProcessor::Outcome::DroppedUnsynced); CHECK(!p.is_synced(id));

//     // depth5 -> seeds
//     auto o2 = p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 2,
//     make_depth5(10)); CHECK_EQ(o2, EventProcessor::Outcome::Seeded);
//     CHECK(p.is_synced(id));
//     CHECK_EQ(p.last_update_id(id), 10u);

//     // stale diff (u <= last_u)
//     auto o3 = p.process(ts, Venue::Spot, StreamKind::DepthDiff, id, 0, 0, 3,
//     make_diff(5, 10, 0)); CHECK_EQ(o3, EventProcessor::Outcome::StaleDiff);
//     CHECK_EQ(p.last_update_id(id), 10u);
// }

// TEST(sync_overlap) {
//     EventProcessor p;
//     TestFile tf;
//     int id = p.add_symbol("BTCUSDT", 1, &tf.out);
//     RecvTs ts{1, 0};

//     p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 1,
//     make_depth5(10));
//     // overlap: U <= last_u + 1
//     auto o = p.process(ts, Venue::Spot, StreamKind::DepthDiff, id, 0, 0, 2,
//     make_diff(10, 12, 0)); CHECK_EQ(o, EventProcessor::Outcome::Applied);
//     CHECK_EQ(p.last_update_id(id), 12u);
// }

// TEST(sync_gap) {
//     EventProcessor p;
//     TestFile tf;
//     int id = p.add_symbol("BTCUSDT", 1, &tf.out);
//     RecvTs ts{1, 0};

//     p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 1,
//     make_depth5(10));
//     // gap: U > last_u + 1
//     auto o = p.process(ts, Venue::Spot, StreamKind::DepthDiff, id, 0, 0, 2,
//     make_diff(12, 14, 0)); CHECK_EQ(o, EventProcessor::Outcome::Gap);
//     CHECK(!p.is_synced(id));
// }

// TEST(sync_usdm_pu_mismatch) {
//     EventProcessor p;
//     TestFile tf;
//     int id = p.add_symbol("BTCUSDT", 1, &tf.out);
//     RecvTs ts{1, 0};

//     p.process(ts, Venue::Usdm, StreamKind::Depth5, id, 0, 0, 1,
//     make_depth5(10, true));
//     // overlap initially
//     p.process(ts, Venue::Usdm, StreamKind::DepthDiff, id, 0, 0, 2,
//     make_diff(10, 12, 9, true)); // pu = 9 <= 10 (ok for first)
//     // next diff should have pu == 12
//     auto o = p.process(ts, Venue::Usdm, StreamKind::DepthDiff, id, 0, 0, 3,
//     make_diff(13, 15, 11, true)); // pu=11 mismatch CHECK_EQ(o,
//     EventProcessor::Outcome::Gap); CHECK(!p.is_synced(id));
// }

// TEST(sync_new_epoch_reset) {
//     EventProcessor p;
//     TestFile tf;
//     int id = p.add_symbol("BTCUSDT", 1, &tf.out);
//     RecvTs ts{1, 0};

//     p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 1,
//     make_depth5(10)); CHECK(p.is_synced(id));
//     // new epoch resets sync
//     p.process(ts, Venue::Spot, StreamKind::DepthDiff, id, 0, 1, 1,
//     make_diff(11, 12, 0)); CHECK(!p.is_synced(id));
// }

// TEST(sync_depth5_stale_and_reconcile) {
//     EventProcessor p;
//     TestFile tf;
//     int id = p.add_symbol("BTCUSDT", 1, &tf.out);
//     RecvTs ts{1, 0};

//     p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 1,
//     make_depth5(10));

//     // stale depth5 (id < last_u)
//     auto o1 = p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 2,
//     make_depth5(9)); CHECK_EQ(o1, EventProcessor::Outcome::StaleDepth5);
//     CHECK_EQ(p.last_update_id(id), 10u);

//     // valid depth5
//     auto o2 = p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 3,
//     make_depth5(11)); CHECK_EQ(o2, EventProcessor::Outcome::Reconciled);
//     CHECK_EQ(p.last_update_id(id), 11u);
// }

// tests/test_sync.cpp
//
// Processor-level tests: sync state machine, emitted rows, book behaviour
// seen through the CSV, malformed input, multi-symbol isolation.
//
// Expected rows are built BY HAND (see exp_row), never copied from program
// output, so these tests are independent of the code under test.
//
// Assumptions about the rest of the code base (adapt if they differ):
//   * OutputFile::close() flushes buffered rows to disk.
//   * OutputFile::open() may or may not write the header; rows() skips it.
//   * Book semantics: bounded top-5 array with a certainty edge
//     (levels worse than the snapshot's 5th level are ignored).
//   * Prices/quantities are scaled by 1e8 in the CSV.

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

#include "event_processor.h"
#include "test_framework.h"

using namespace bc;

namespace {

using O = EventProcessor::Outcome;
constexpr Venue kSpot = Venue::Spot;
constexpr Venue kUsdm = Venue::Usdm;

// Set to false if you decided NOT to emit a 'P' row for a stale depth5.
// (The README must state whichever you choose.)
constexpr bool kStaleDepth5EmitsRow = true;

// ---------------------------------------------------------------- helpers

// Unique temp file per test, so parallel runs never collide.
class TestFile {
public:
  OutputFile out;
  std::string path;
  bool closed = false;

  explicit TestFile(const std::string &tag = "legacy") {
    static int counter = 0;
    path = (std::filesystem::temp_directory_path() /
            ("bc_sync_" + tag + "_" + std::to_string(::getpid()) + "_" +
             std::to_string(counter++) + ".csv"))
               .string();
    std::string err;
    out.open(path, err); // if open() returns bool in your code: wrap in CHECK
                         // at the call site
  }
  ~TestFile() {
    if (!closed)
      out.close();
    std::remove(path.c_str());
  }
  TestFile(const TestFile &) = delete;
  TestFile &operator=(const TestFile &) = delete;

  // Closes (flushes) the file once and returns the data rows (header skipped).
  // Call at the END of a test: nothing may be processed after this.
  std::vector<std::string> rows() {
    if (!closed) {
      out.close();
      closed = true;
    }
    std::ifstream f(path);
    std::vector<std::string> v;
    std::string line;
    while (std::getline(f, line)) {
      if (line.empty())
        continue;
      if (line.compare(0, 5, "tsec,") == 0)
        continue; // header
      v.push_back(line);
    }
    return v;
  }
};

// One symbol on one connection, with an auto-incrementing conn_seq.
struct Feed {
  TestFile tf;
  EventProcessor p;
  Venue venue;
  int id = -1;
  std::uint64_t epoch = 0;
  std::uint64_t seq = 0;

  explicit Feed(const char *tag, Venue v = kSpot, const char *sym = "BTCUSDT",
                int row_id = 1)
      : tf(tag), venue(v) {
    id = p.add_symbol(sym, row_id, &tf.out);
  }
  O push(RecvTs ts, StreamKind k, const std::string &payload) {
    return p.process(ts, venue, k, id, 0, epoch, ++seq, payload);
  }
  void new_epoch() { // simulates a reconnect
    ++epoch;
    seq = 0;
  }
};

// ----- payload builders -----

// Legacy single-level payloads (kept for the original tests).
std::string make_depth5(std::uint64_t id, bool usdm = false) {
  if (!usdm) {
    return R"({"lastUpdateId":)" + std::to_string(id) +
           R"(,"bids":[["100","1"]],"asks":[["110","1"]]})";
  }
  return R"({"e":"depthUpdate","E":1,"T":1,"s":"BTCUSDT","U":)" +
         std::to_string(id) + R"(,"u":)" + std::to_string(id) +
         R"(,"pu":0,"b":[["100","1"]],"a":[["110","1"]]})";
}

std::string make_diff(std::uint64_t U, std::uint64_t u, std::uint64_t pu,
                      bool usdm = false) {
  std::string s = R"({"e":"depthUpdate","E":1,"s":"BTCUSDT","U":)" +
                  std::to_string(U) + R"(,"u":)" + std::to_string(u);
  if (usdm) {
    s += R"(,"pu":)" + std::to_string(pu);
  }
  s += R"(,"b":[["100","2"]],"a":[]})";
  return s;
}

// Spot depth5 with 5 levels per side: bids bid0, bid0-1, ... (qty 1..5),
// asks ask0, ask0+1, ... (qty 1..5). A 5-level side makes the book "bounded".
std::string snap5_at(std::uint64_t id, int bid0, int ask0) {
  std::string s = R"({"lastUpdateId":)" + std::to_string(id) + R"(,"bids":[)";
  for (int i = 0; i < 5; ++i) {
    if (i)
      s += ",";
    s += "[\"" + std::to_string(bid0 - i) + "\",\"" + std::to_string(i + 1) +
         "\"]";
  }
  s += R"(],"asks":[)";
  for (int i = 0; i < 5; ++i) {
    if (i)
      s += ",";
    s += "[\"" + std::to_string(ask0 + i) + "\",\"" + std::to_string(i + 1) +
         "\"]";
  }
  s += "]}";
  return s;
}
std::string snap5(std::uint64_t id) { return snap5_at(id, 100, 101); }

// Spot depth diff with explicit level arrays.
std::string dl(std::uint64_t U, std::uint64_t u, const std::string &b,
               const std::string &a) {
  return R"({"e":"depthUpdate","E":1,"s":"BTCUSDT","U":)" + std::to_string(U) +
         R"(,"u":)" + std::to_string(u) + R"(,"b":)" + b + R"(,"a":)" + a + "}";
}
// One-level array: [["px","qty"]]
std::string lvl(const char *px, const char *q) {
  return std::string("[[\"") + px + "\",\"" + q + "\"]]";
}
std::string trade_json() {
  return R"({"e":"trade","E":1,"s":"BTCUSDT","t":1,"p":"100.5","q":"0.1"})";
}

// ----- expected-row builder (hand-computed expectations) -----

using Lv = std::vector<std::pair<std::int64_t, std::int64_t>>; // {price, qty}
                                                               // in plain units

Lv bids_from(int b0) {
  Lv v;
  for (int i = 0; i < 5; ++i)
    v.emplace_back(b0 - i, i + 1);
  return v;
}
Lv asks_from(int a0) {
  Lv v;
  for (int i = 0; i < 5; ++i)
    v.emplace_back(a0 + i, i + 1);
  return v;
}

// Order book CSV row: tsec,tnsec,seqNo,id,type,side,bid px x5,bid qty x5,ask px
// x5,ask qty x5. Missing levels are written as 0. Prices and quantities are
// scaled by 1e8.
std::string exp_row(std::int64_t sec, std::int64_t nsec, std::uint64_t seq,
                    int rid, char type, char side, const Lv &bids,
                    const Lv &asks) {
  constexpr std::int64_t kScale = 100000000;
  std::string r = std::to_string(sec) + "," + std::to_string(nsec) + "," +
                  std::to_string(seq) + "," + std::to_string(rid) + ",";
  r.push_back(type);
  r.push_back(',');
  r.push_back(side);
  auto px = [&](const Lv &v) {
    for (std::size_t i = 0; i < 5; ++i) {
      r.push_back(',');
      r +=
          i < v.size() ? std::to_string(v[i].first * kScale) : std::string("0");
    }
  };
  auto qty = [&](const Lv &v) {
    for (std::size_t i = 0; i < 5; ++i) {
      r.push_back(',');
      r += i < v.size() ? std::to_string(v[i].second * kScale)
                        : std::string("0");
    }
  };
  px(bids);
  qty(bids);
  px(asks);
  qty(asks);
  return r;
}

// String compare that prints both sides on mismatch.
bool same(const std::string &got, const std::string &want) {
  if (got == want)
    return true;
  std::fprintf(stderr, "\n  got : %s\n  want: %s\n", got.c_str(), want.c_str());
  return false;
}

} // namespace

// =====================================================================
// Legacy tests (unchanged behaviour; usdm test now also asserts the first
// outcome)
// =====================================================================

TEST(sync_seed_and_stale_diff) {
  EventProcessor p;
  TestFile tf;
  int id = p.add_symbol("BTCUSDT", 1, &tf.out);

  RecvTs ts{1, 0};
  // diff before sync -> dropped
  auto o1 = p.process(ts, Venue::Spot, StreamKind::DepthDiff, id, 0, 0, 1,
                      make_diff(1, 2, 0));
  CHECK_EQ(o1, EventProcessor::Outcome::DroppedUnsynced);
  CHECK(!p.is_synced(id));

  // depth5 -> seeds
  auto o2 = p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 2,
                      make_depth5(10));
  CHECK_EQ(o2, EventProcessor::Outcome::Seeded);
  CHECK(p.is_synced(id));
  CHECK_EQ(p.last_update_id(id), 10u);

  // stale diff (u <= last_u)
  auto o3 = p.process(ts, Venue::Spot, StreamKind::DepthDiff, id, 0, 0, 3,
                      make_diff(5, 10, 0));
  CHECK_EQ(o3, EventProcessor::Outcome::StaleDiff);
  CHECK_EQ(p.last_update_id(id), 10u);
}

TEST(sync_overlap) {
  EventProcessor p;
  TestFile tf;
  int id = p.add_symbol("BTCUSDT", 1, &tf.out);
  RecvTs ts{1, 0};

  p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 1, make_depth5(10));
  // overlap: U <= last_u + 1
  auto o = p.process(ts, Venue::Spot, StreamKind::DepthDiff, id, 0, 0, 2,
                     make_diff(10, 12, 0));
  CHECK_EQ(o, EventProcessor::Outcome::Applied);
  CHECK_EQ(p.last_update_id(id), 12u);
}

TEST(sync_gap) {
  EventProcessor p;
  TestFile tf;
  int id = p.add_symbol("BTCUSDT", 1, &tf.out);
  RecvTs ts{1, 0};

  p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 1, make_depth5(10));
  // gap: U > last_u + 1
  auto o = p.process(ts, Venue::Spot, StreamKind::DepthDiff, id, 0, 0, 2,
                     make_diff(12, 14, 0));
  CHECK_EQ(o, EventProcessor::Outcome::Gap);
  CHECK(!p.is_synced(id));
}

TEST(sync_usdm_pu_mismatch) {
  EventProcessor p;
  TestFile tf;
  int id = p.add_symbol("BTCUSDT", 1, &tf.out);
  RecvTs ts{1, 0};

  p.process(ts, Venue::Usdm, StreamKind::Depth5, id, 0, 0, 1,
            make_depth5(10, true));
  // first diff after the seed bridges by overlap
  auto o1 =
      p.process(ts, Venue::Usdm, StreamKind::DepthDiff, id, 0, 0, 2,
                make_diff(10, 12, 9, true)); // pu = 9 <= 10 (ok for first)
  CHECK_EQ(o1, EventProcessor::Outcome::Applied);
  // next diff should have pu == 12
  auto o = p.process(ts, Venue::Usdm, StreamKind::DepthDiff, id, 0, 0, 3,
                     make_diff(13, 15, 11, true)); // pu = 11 mismatch
  CHECK_EQ(o, EventProcessor::Outcome::Gap);
  CHECK(!p.is_synced(id));
}

TEST(sync_new_epoch_reset) {
  EventProcessor p;
  TestFile tf;
  int id = p.add_symbol("BTCUSDT", 1, &tf.out);
  RecvTs ts{1, 0};

  p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 1, make_depth5(10));
  CHECK(p.is_synced(id));
  // new epoch resets sync
  p.process(ts, Venue::Spot, StreamKind::DepthDiff, id, 0, 1, 1,
            make_diff(11, 12, 0));
  CHECK(!p.is_synced(id));
}

TEST(sync_depth5_stale_and_reconcile) {
  EventProcessor p;
  TestFile tf;
  int id = p.add_symbol("BTCUSDT", 1, &tf.out);
  RecvTs ts{1, 0};

  p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 1, make_depth5(10));

  // stale depth5 (id < last_u)
  auto o1 = p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 2,
                      make_depth5(9));
  CHECK_EQ(o1, EventProcessor::Outcome::StaleDepth5);
  CHECK_EQ(p.last_update_id(id), 10u);

  // valid depth5: last_u MUST advance to the snapshot id
  auto o2 = p.process(ts, Venue::Spot, StreamKind::Depth5, id, 0, 0, 3,
                      make_depth5(11));
  CHECK_EQ(o2, EventProcessor::Outcome::Reconciled);
  CHECK_EQ(p.last_update_id(id), 11u);
}

// =====================================================================
// New: emitted rows (timestamps, seqNo, type, side, padding, edge, no-row
// cases)
// =====================================================================

TEST(rows_exact_and_only_for_applied_events) {
  Feed f("rows");
  // 1. diff before any seed: dropped, no row
  CHECK_EQ(f.push(RecvTs{5, 100}, StreamKind::DepthDiff,
                  dl(1, 2, lvl("100", "9"), "[]")),
           O::DroppedUnsynced);
  // 2. seed: row #1 ('P','N')
  CHECK_EQ(f.push(RecvTs{5, 111}, StreamKind::Depth5, snap5(10)), O::Seeded);
  // 3. stale diff (u <= 10): no row
  CHECK_EQ(f.push(RecvTs{5, 150}, StreamKind::DepthDiff,
                  dl(5, 10, lvl("100", "9"), "[]")),
           O::StaleDiff);
  // 4. cancel best bid: row #2 ('D','B'), book shrinks to 4 bids, 5th slot
  // zero-padded
  CHECK_EQ(f.push(RecvTs{5, 222}, StreamKind::DepthDiff,
                  dl(11, 12, lvl("100", "0"), "[]")),
           O::Applied);
  // 5. trade: logged elsewhere, never a book row
  CHECK_EQ(f.push(RecvTs{5, 250}, StreamKind::Trade, trade_json()), O::Trade);
  // 6. bid 95 is beyond the certainty edge (96): the book must IGNORE it, but
  // an
  //    applied diff still yields a row (row #3), identical to row #2 except
  //    seq/ts.
  CHECK_EQ(f.push(RecvTs{5, 333}, StreamKind::DepthDiff,
                  dl(13, 13, lvl("95", "9"), "[]")),
           O::Applied);

  auto r = f.tf.rows();
  CHECK(r.size() == 3u);
  if (r.size() < 3u)
    return;

  const Lv after_cancel = {{99, 2}, {98, 3}, {97, 4}, {96, 5}};
  CHECK(same(r[0],
             exp_row(5, 111, 1, 1, 'P', 'N', bids_from(100), asks_from(101))));
  CHECK(same(r[1],
             exp_row(5, 222, 2, 1, 'D', 'B', after_cancel, asks_from(101))));
  CHECK(same(r[2],
             exp_row(5, 333, 3, 1, 'D', 'B', after_cancel, asks_from(101))));
}

TEST(rows_side_mapping_and_empty_diff) {
  Feed f("side");
  CHECK_EQ(f.push(RecvTs{1, 0}, StreamKind::Depth5, snap5(10)), O::Seeded);
  // bids only -> 'B'
  CHECK_EQ(f.push(RecvTs{2, 0}, StreamKind::DepthDiff,
                  dl(11, 11, lvl("100", "7"), "[]")),
           O::Applied);
  // asks only -> 'S'
  CHECK_EQ(f.push(RecvTs{3, 0}, StreamKind::DepthDiff,
                  dl(12, 12, "[]", lvl("101", "8"))),
           O::Applied);
  // both -> 'N'
  CHECK_EQ(f.push(RecvTs{4, 0}, StreamKind::DepthDiff,
                  dl(13, 13, lvl("100", "6"), lvl("101", "5"))),
           O::Applied);
  // neither (empty arrays) -> still an applied diff, still a row, 'N'
  CHECK_EQ(f.push(RecvTs{5, 0}, StreamKind::DepthDiff, dl(14, 14, "[]", "[]")),
           O::Applied);

  auto r = f.tf.rows();
  CHECK(r.size() == 5u);
  if (r.size() < 5u)
    return;

  Lv b1 = bids_from(100), a1 = asks_from(101);
  CHECK(same(r[0], exp_row(1, 0, 1, 1, 'P', 'N', b1, a1)));
  b1[0].second = 7; // bid 100 -> 7
  CHECK(same(r[1], exp_row(2, 0, 2, 1, 'D', 'B', b1, a1)));
  a1[0].second = 8; // ask 101 -> 8
  CHECK(same(r[2], exp_row(3, 0, 3, 1, 'D', 'S', b1, a1)));
  b1[0].second = 6; // both change
  a1[0].second = 5;
  CHECK(same(r[3], exp_row(4, 0, 4, 1, 'D', 'N', b1, a1)));
  CHECK(same(r[4], exp_row(5, 0, 5, 1, 'D', 'N', b1, a1)));
}

TEST(trades_never_emit_rows) {
  Feed f("trade");
  // unsynced trade: dropped
  CHECK_EQ(f.push(RecvTs{1, 0}, StreamKind::Trade, trade_json()),
           O::DroppedUnsynced);
  CHECK_EQ(f.push(RecvTs{2, 0}, StreamKind::Depth5, snap5(10)), O::Seeded);
  // synced trade: accepted, no row
  CHECK_EQ(f.push(RecvTs{3, 0}, StreamKind::Trade, trade_json()), O::Trade);
  // malformed trade (missing q): parse error, no row
  CHECK_EQ(f.push(RecvTs{4, 0}, StreamKind::Trade,
                  R"({"e":"trade","s":"BTCUSDT","t":1,"p":"100.5"})"),
           O::ParseError);
  auto r = f.tf.rows();
  CHECK(r.size() == 1u); // only the seed row
}

// =====================================================================
// New: sync rules
// =====================================================================

TEST(sync_adjacent_and_chained_diffs_apply) {
  Feed f("adjacent");
  CHECK_EQ(f.push(RecvTs{1, 0}, StreamKind::Depth5, snap5(10)), O::Seeded);
  // U == last_u + 1 is the most common real case
  CHECK_EQ(f.push(RecvTs{2, 0}, StreamKind::DepthDiff, dl(11, 12, "[]", "[]")),
           O::Applied);
  CHECK_EQ(f.push(RecvTs{3, 0}, StreamKind::DepthDiff, dl(13, 14, "[]", "[]")),
           O::Applied);
  CHECK_EQ(f.p.last_update_id(f.id), 14u);
  CHECK(f.p.is_synced(f.id));
}

// FLAW TEST: diffs 13..20 are lost, then a depth5 jumps last_u ahead. A diff
// with U=21 then LOOKS contiguous with last_u, but the chain from the last
// applied diff (u=12) is broken. Needs the spot chain check (U == previous diff
// u + 1).
TEST(sync_lost_diffs_before_depth5_is_a_gap) {
  Feed f("lost");
  CHECK_EQ(f.push(RecvTs{1, 0}, StreamKind::Depth5, snap5(10)), O::Seeded);
  CHECK_EQ(f.push(RecvTs{2, 0}, StreamKind::DepthDiff,
                  dl(11, 12, lvl("100", "7"), "[]")),
           O::Applied);
  // (diffs 13..20 never arrive)
  CHECK_EQ(f.push(RecvTs{3, 0}, StreamKind::Depth5, snap5(20)), O::Reconciled);
  CHECK_EQ(f.push(RecvTs{4, 0}, StreamKind::DepthDiff,
                  dl(21, 22, lvl("100", "7"), "[]")),
           O::Gap);
  CHECK(!f.p.is_synced(f.id));
}

// CONTROL: the same depth5 jump with nothing lost (late, interleaved diffs) is
// NOT a gap.
TEST(sync_interleaved_late_diffs_are_not_a_gap) {
  Feed f("interleave");
  CHECK_EQ(f.push(RecvTs{1, 0}, StreamKind::Depth5, snap5(10)), O::Seeded);
  CHECK_EQ(f.push(RecvTs{2, 0}, StreamKind::Depth5, snap5(20)), O::Reconciled);
  CHECK_EQ(f.push(RecvTs{3, 0}, StreamKind::DepthDiff, dl(11, 12, "[]", "[]")),
           O::StaleDiff);
  CHECK_EQ(f.push(RecvTs{4, 0}, StreamKind::DepthDiff, dl(13, 20, "[]", "[]")),
           O::StaleDiff);
  CHECK_EQ(f.push(RecvTs{5, 0}, StreamKind::DepthDiff,
                  dl(21, 22, lvl("100", "7"), "[]")),
           O::Applied);
  CHECK(f.p.is_synced(f.id));
  CHECK_EQ(f.p.last_update_id(f.id), 22u);
}

TEST(sync_usdm_chain_positive_and_negative) {
  Feed f("usdm", kUsdm);
  CHECK_EQ(f.push(RecvTs{1, 0}, StreamKind::Depth5, make_depth5(10, true)),
           O::Seeded);
  // first diff bridges the seed (pu <= last_u)
  CHECK_EQ(
      f.push(RecvTs{2, 0}, StreamKind::DepthDiff, make_diff(11, 12, 10, true)),
      O::Applied);
  // positive chain: pu == previous diff's u (12)
  CHECK_EQ(
      f.push(RecvTs{3, 0}, StreamKind::DepthDiff, make_diff(13, 15, 12, true)),
      O::Applied);
  CHECK_EQ(f.p.last_update_id(f.id), 15u);
  // negative chain: pu (14) != previous u (15)
  CHECK_EQ(
      f.push(RecvTs{4, 0}, StreamKind::DepthDiff, make_diff(16, 18, 14, true)),
      O::Gap);
  CHECK(!f.p.is_synced(f.id));
}

TEST(sync_depth5_boundaries_and_gap_recovery) {
  Feed f("recover");
  CHECK_EQ(f.push(RecvTs{1, 0}, StreamKind::Depth5, snap5(10)), O::Seeded);
  // depth5 with id == last_u is applicable (not stale)
  CHECK_EQ(f.push(RecvTs{2, 0}, StreamKind::Depth5, snap5(10)), O::Reconciled);
  CHECK_EQ(f.p.last_update_id(f.id), 10u);
  // real gap
  CHECK_EQ(f.push(RecvTs{3, 0}, StreamKind::DepthDiff,
                  dl(30, 31, lvl("100", "3"), "[]")),
           O::Gap);
  CHECK(!f.p.is_synced(f.id));
  // while unsynced: diffs and trades are dropped, no rows
  CHECK_EQ(f.push(RecvTs{4, 0}, StreamKind::DepthDiff,
                  dl(32, 33, lvl("100", "3"), "[]")),
           O::DroppedUnsynced);
  CHECK_EQ(f.push(RecvTs{5, 0}, StreamKind::Trade, trade_json()),
           O::DroppedUnsynced);
  // next depth5 re-seeds and rows resume; seqNo continues (3)
  CHECK_EQ(f.push(RecvTs{6, 0}, StreamKind::Depth5, snap5(40)), O::Seeded);
  CHECK(f.p.is_synced(f.id));

  auto r = f.tf.rows();
  CHECK(r.size() == 3u);
  if (r.size() < 3u)
    return;
  CHECK(same(r[0],
             exp_row(1, 0, 1, 1, 'P', 'N', bids_from(100), asks_from(101))));
  CHECK(same(r[1],
             exp_row(2, 0, 2, 1, 'P', 'N', bids_from(100), asks_from(101))));
  CHECK(same(r[2],
             exp_row(6, 0, 3, 1, 'P', 'N', bids_from(100), asks_from(101))));
}

TEST(sync_new_epoch_full_reset) {
  Feed f("epoch");
  CHECK_EQ(f.push(RecvTs{1, 0}, StreamKind::Depth5, snap5(1000)), O::Seeded);
  f.new_epoch(); // reconnect: conn_epoch + 1, conn_seq restarts
  // old-epoch continuity is gone: a diff is dropped, book state discarded
  CHECK_EQ(f.push(RecvTs{2, 0}, StreamKind::DepthDiff,
                  dl(1001, 1002, lvl("100", "9"), "[]")),
           O::DroppedUnsynced);
  CHECK(!f.p.is_synced(f.id));
  // a depth5 whose id is LOWER than the old last_u must still seed in the new
  // epoch, and the row must show only the new snapshot's levels
  CHECK_EQ(f.push(RecvTs{3, 0}, StreamKind::Depth5, snap5_at(5, 90, 91)),
           O::Seeded);
  CHECK_EQ(f.p.last_update_id(f.id), 5u);

  auto r = f.tf.rows();
  CHECK(r.size() == 2u);
  if (r.size() < 2u)
    return;
  CHECK(same(r[0],
             exp_row(1, 0, 1, 1, 'P', 'N', bids_from(100), asks_from(101))));
  // seqNo continues across epochs; nothing from the old book survives
  CHECK(
      same(r[1], exp_row(3, 0, 2, 1, 'P', 'N', bids_from(90), asks_from(91))));
}

TEST(stale_depth5_leaves_book_unchanged) {
  Feed f("stale5");
  CHECK_EQ(f.push(RecvTs{1, 0}, StreamKind::Depth5, snap5(10)), O::Seeded);
  // stale depth5 carrying DIFFERENT levels must not touch the book
  CHECK_EQ(f.push(RecvTs{2, 0}, StreamKind::Depth5, snap5_at(9, 50, 51)),
           O::StaleDepth5);
  CHECK_EQ(f.p.last_update_id(f.id), 10u);
  CHECK_EQ(f.push(RecvTs{3, 0}, StreamKind::DepthDiff,
                  dl(11, 11, lvl("100", "7"), "[]")),
           O::Applied);

  Lv b = bids_from(100);
  Lv a = asks_from(101);
  auto r = f.tf.rows();
  if (kStaleDepth5EmitsRow) {
    CHECK(r.size() == 3u);
    if (r.size() < 3u)
      return;
    CHECK(same(r[0], exp_row(1, 0, 1, 1, 'P', 'N', b, a)));
    CHECK(same(r[1], exp_row(2, 0, 2, 1, 'P', 'N', b,
                             a))); // refresh row from the UNCHANGED book
    b[0].second = 7;
    CHECK(same(r[2], exp_row(3, 0, 3, 1, 'D', 'B', b, a)));
  } else {
    CHECK(r.size() == 2u);
    if (r.size() < 2u)
      return;
    CHECK(same(r[0], exp_row(1, 0, 1, 1, 'P', 'N', b, a)));
    b[0].second = 7;
    CHECK(same(r[1], exp_row(3, 0, 2, 1, 'D', 'B', b, a)));
  }
}

// =====================================================================
// New: malformed input never changes state and never emits a row
// =====================================================================

TEST(malformed_inputs_are_rejected_without_side_effects) {
  Feed f("bad");
  CHECK_EQ(f.push(RecvTs{1, 0}, StreamKind::Depth5, snap5(10)), O::Seeded);

  // U > u
  CHECK_EQ(f.push(RecvTs{2, 0}, StreamKind::DepthDiff,
                  dl(12, 11, lvl("100", "9"), "[]")),
           O::ParseError);
  // missing "b"
  CHECK_EQ(
      f.push(RecvTs{2, 1}, StreamKind::DepthDiff,
             R"({"e":"depthUpdate","E":1,"s":"BTCUSDT","U":11,"u":12,"a":[]})"),
      O::ParseError);
  // 9 decimals in a price: rejected, never truncated
  CHECK_EQ(f.push(RecvTs{2, 2}, StreamKind::DepthDiff,
                  dl(11, 12, lvl("100.123456789", "1"), "[]")),
           O::DecimalReject);
  // depth5 with unordered bids
  CHECK_EQ(
      f.push(
          RecvTs{2, 3}, StreamKind::Depth5,
          R"({"lastUpdateId":11,"bids":[["99","1"],["100","1"]],"asks":[["110","1"]]})"),
      O::ParseError);
  // depth5 with a zero-quantity level
  CHECK_EQ(
      f.push(
          RecvTs{2, 4}, StreamKind::Depth5,
          R"({"lastUpdateId":11,"bids":[["100","0"]],"asks":[["110","1"]]})"),
      O::ParseError);
  // broken JSON
  CHECK_EQ(f.push(RecvTs{2, 5}, StreamKind::DepthDiff, "{not json"),
           O::ParseError);
  // invalid symbol ids
  CHECK_EQ(f.p.process(RecvTs{2, 6}, kSpot, StreamKind::Depth5, 99, 0, 0, 100,
                       snap5(11)),
           O::ParseError);
  CHECK_EQ(f.p.process(RecvTs{2, 7}, kSpot, StreamKind::Depth5, -1, 0, 0, 101,
                       snap5(11)),
           O::ParseError);

  // state untouched ...
  CHECK(f.p.is_synced(f.id));
  CHECK_EQ(f.p.last_update_id(f.id), 10u);
  // ... and the next valid diff applies normally (U == last_u + 1)
  CHECK_EQ(f.push(RecvTs{3, 0}, StreamKind::DepthDiff,
                  dl(11, 12, lvl("100", "7"), "[]")),
           O::Applied);

  auto r = f.tf.rows();
  CHECK(r.size() ==
        2u); // seed row + one applied diff; the bad inputs produced no rows
  if (r.size() < 2u)
    return;
  Lv b = bids_from(100);
  b[0].second = 7;
  CHECK(same(r[1], exp_row(3, 0, 2, 1, 'D', 'B', b, asks_from(101))));
}

// =====================================================================
// New: symbols are independent
// =====================================================================

TEST(two_symbols_are_independent) {
  TestFile fa("symA");
  TestFile fb("symB");
  EventProcessor p;
  int a = p.add_symbol("BTCUSDT", 1, &fa.out);
  int b = p.add_symbol("ETHUSDT", 2, &fb.out);
  std::uint64_t seq = 0; // one connection: conn_seq is shared across symbols
  auto push = [&](int sid, RecvTs ts, StreamKind k, const std::string &pl) {
    return p.process(ts, kSpot, k, sid, 0, 0, ++seq, pl);
  };

  CHECK_EQ(push(a, RecvTs{1, 0}, StreamKind::Depth5, snap5(10)), O::Seeded);
  CHECK_EQ(push(b, RecvTs{1, 1}, StreamKind::Depth5, snap5(10)), O::Seeded);
  // a real gap on symbol A only
  CHECK_EQ(push(a, RecvTs{2, 0}, StreamKind::DepthDiff,
                dl(30, 31, lvl("100", "3"), "[]")),
           O::Gap);
  CHECK(!p.is_synced(a));
  CHECK(p.is_synced(b));
  // B keeps applying normally; A drops
  CHECK_EQ(push(b, RecvTs{3, 0}, StreamKind::DepthDiff,
                dl(11, 12, lvl("100", "7"), "[]")),
           O::Applied);
  CHECK_EQ(push(a, RecvTs{3, 1}, StreamKind::DepthDiff,
                dl(11, 12, lvl("100", "7"), "[]")),
           O::DroppedUnsynced);

  auto ra = fa.rows();
  auto rb = fb.rows();
  CHECK(ra.size() == 1u);
  CHECK(rb.size() == 2u);
  if (ra.size() < 1u || rb.size() < 2u)
    return;
  CHECK(same(ra[0],
             exp_row(1, 0, 1, 1, 'P', 'N', bids_from(100), asks_from(101))));
  Lv bb = bids_from(100);
  bb[0].second = 7;
  CHECK(same(rb[0],
             exp_row(1, 1, 1, 2, 'P', 'N', bids_from(100), asks_from(101))));
  CHECK(same(rb[1], exp_row(3, 0, 2, 2, 'D', 'B', bb, asks_from(101))));
}