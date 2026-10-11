#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

#include "csv_reader.h"
#include "event_processor.h"
#include "json_scan.h"
#include "test_framework.h"

using namespace bc;

namespace {

std::string temp_csv(const char *tag) {
  return std::string("test_") + tag + "_" + std::to_string(::getpid()) + ".csv";
}

// Data rows of an order-book CSV (header and blank lines skipped, so this works
// whether or not OutputFile::open() writes the header).
std::vector<std::string> read_data_rows(const std::string &path) {
  std::ifstream in(path);
  std::vector<std::string> rows;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty())
      continue;
    if (line.compare(0, 5, "tsec,") == 0)
      continue;
    rows.push_back(line);
  }
  return rows;
}

} // namespace

// Full-row check (not just a prefix): book columns, scaling, padding, side
// mapping, and "no extra rows". The book here has only 2 levels per side (thin,
// unbounded).
TEST(event_processor_spot_flow) {
  const std::string out_path = temp_csv("ep_flow");
  std::remove(out_path.c_str());

  {
    EventProcessor p;
    OutputFile out;
    std::string err;
    out.open(out_path, err);
    int id = p.add_symbol("BTCUSDT", 1, &out);

    RecvTs ts{123, 456};
    // depth5
    std::string d5 =
        R"({"lastUpdateId":10,"bids":[["100","1"],["99","1"]],"asks":[["110","1"],["111","1"]]})";
    auto o1 = p.process(ts, Venue::Spot, StreamKind::Depth5, id, 1, 1, 1, d5);
    CHECK_EQ(o1, EventProcessor::Outcome::Seeded);

    // diff: bid 100 -> 2, ask 110 removed (qty 0)
    std::string d =
        R"({"e":"depthUpdate","E":1001,"s":"BTCUSDT","U":11,"u":12,"b":[["100","2"]],"a":[["110","0"]]})";
    auto o2 = p.process(ts, Venue::Spot, StreamKind::DepthDiff, id, 1, 1, 2, d);
    CHECK_EQ(o2, EventProcessor::Outcome::Applied);

    out.close();
  }

  const auto rows = read_data_rows(out_path);
  CHECK_EQ(rows.size(), 2u); // exactly two rows, nothing extra
  if (rows.size() == 2u) {
    // seed row: type P, side N; 1e8 scaling; 3 padded slots per side/size block
    CHECK_EQ(rows[0], "123,456,1,1,P,N,"
                      "10000000000,9900000000,0,0,0,"
                      "100000000,100000000,0,0,0,"
                      "11000000000,11100000000,0,0,0,"
                      "100000000,100000000,0,0,0");
    // diff row: both sides changed -> N; ask 110 removed so the 2nd ask moves
    // up
    CHECK_EQ(rows[1], "123,456,2,1,D,N,"
                      "10000000000,9900000000,0,0,0,"
                      "200000000,100000000,0,0,0,"
                      "11100000000,0,0,0,0,"
                      "100000000,0,0,0,0");
  }
  std::remove(out_path.c_str());
}

// payload_json in the market-data CSV is whatever this returns, so the slice
// must be exact, must point into the input (no copy), and must survive hostile
// content.
TEST(json_scan_returns_exact_raw_data_slice) {
  const std::string data_obj =
      R"({"e":"depthUpdate","E":1,"s":"BTCUSDT","U":1,"u":2,"b":[["100.5","1"]],"a":[]})";
  const std::string env =
      R"({"stream":"btcusdt@depth@100ms","data":)" + data_obj + "}";
  std::string_view stream, data;
  CHECK(bc::split_envelope(env, stream, data));
  CHECK(stream == "btcusdt@depth@100ms");
  CHECK(data == data_obj);
  CHECK(data.data() >= env.data() &&
        data.data() + data.size() <= env.data() + env.size());
}

TEST(json_scan_braces_and_quotes_inside_strings) {
  // braces and escaped quotes inside string values, nested objects/arrays
  const std::string data_obj =
      R"({"s":"A}B","m":"he said \"hi\" {","n":{"k":[1,{"z":2}]}})";
  const std::string env = R"({"stream":"x","data":)" + data_obj + "}";
  std::string_view stream, data;
  CHECK(bc::split_envelope(env, stream, data));
  CHECK(stream == "x");
  CHECK(data == data_obj);
}

TEST(json_scan_stream_value_that_looks_like_a_key) {
  const std::string env = R"({"stream":"data","data":{"a":1}})";
  std::string_view stream, data;
  CHECK(bc::split_envelope(env, stream, data));
  CHECK(stream == "data");
  CHECK(data == R"({"a":1})");
}

// Decision encoded here: anything that is not a complete
// {"stream":..,"data":{..}} envelope is rejected (returns false), never
// partially parsed.
TEST(json_scan_rejects_non_envelopes_and_truncation) {
  const std::vector<std::string> bad = {
      "",                                           // empty
      R"({"result":null,"id":1})",                  // subscription ack
      R"({"stream":"x"})",                          // no data
      R"({"data":{"a":1}})",                        // no stream
      R"({"stream":"x","data":{"a":1)",             // truncated inside data
      R"({"stream":"x","data":{"a":"unterminated)", // truncated inside a string
      R"({"stream":"x","data":)", // truncated before the value
      "{not json",
  };
  for (const auto &s : bad) {
    std::string_view stream, data;
    if (bc::split_envelope(s, stream, data)) {
      std::fprintf(stderr, "  accepted malformed input: %s\n", s.c_str());
      CHECK(false);
    }
  }
}

// Hand-built expectations. The seed has exactly 5 levels per side, so the book
// is bounded: bid 95 (and anything below 96) is NOT tracked.
//
// Ghost scenario (the bug in a full-map book that keeps levels it cannot
// verify):
//   1. diff u=1001 INSERTS bid 95 beyond the 5th level. A map book stores it,
//   the
//      array book ignores it.
//   2. depth5 id=1002 advances last_u.
//   3. diff u=1002 CANCELS bid 95 but arrives after the depth5, so it is
//   dropped as
//      stale. In a map book the level 95 now survives as a ghost.
//   4. the best bid (100) is cancelled. A map book then prints 95 as the 5th
//   bid;
//      the correct output is 99,98,97,96 and a padded 5th slot.
TEST(T7_ghost_scenario_at_processor_level) {
  auto run_scenario = [](std::string &emitted_csv) {
    EventProcessor p;
    OutputFile out;
    std::string path = temp_csv("ghost");
    std::string err;
    out.open(path, err);
    int sym = p.add_symbol("BTCUSDT", 0, &out);

    RecvTs ts{100, 0};

    // 1. seed at id 1000
    std::string seed =
        R"({"lastUpdateId":1000,"bids":[["100","10"],["99","20"],["98","30"],["97","40"],["96","50"]],"asks":[["101","15"],["102","25"],["103","35"],["104","45"],["105","55"]]})";
    auto o1 =
        p.process(ts, Venue::Spot, StreamKind::Depth5, sym, 0, 0, 1, seed);
    CHECK_EQ(o1, EventProcessor::Outcome::Seeded);

    // 2. diff u=1001 INSERTING a bid beyond the 5th level (95)
    std::string diff1 =
        R"({"e":"depthUpdate","E":1001,"s":"BTCUSDT","U":1001,"u":1001,"b":[["95","5"]],"a":[]})";
    ts.sec = 101;
    auto o2 =
        p.process(ts, Venue::Spot, StreamKind::DepthDiff, sym, 0, 0, 2, diff1);
    CHECK_EQ(o2, EventProcessor::Outcome::Applied);

    // 3. depth5 with id 1002
    std::string d5 =
        R"({"lastUpdateId":1002,"bids":[["100","10"],["99","20"],["98","30"],["97","40"],["96","50"]],"asks":[["101","15"],["102","25"],["103","35"],["104","45"],["105","55"]]})";
    ts.sec = 102;
    auto o3 = p.process(ts, Venue::Spot, StreamKind::Depth5, sym, 0, 0, 3, d5);
    CHECK_EQ(o3, EventProcessor::Outcome::Reconciled);

    // 4. diff u=1002 CANCELLING bid 95: stale (u <= last_u), no row
    std::string diff2 =
        R"({"e":"depthUpdate","E":1002,"s":"BTCUSDT","U":1002,"u":1002,"b":[["95","0"]],"a":[]})";
    ts.sec = 103;
    auto o4 =
        p.process(ts, Venue::Spot, StreamKind::DepthDiff, sym, 0, 0, 4, diff2);
    CHECK_EQ(o4, EventProcessor::Outcome::StaleDiff);

    // 5. diff cancelling the best bid (100)
    std::string diff3 =
        R"({"e":"depthUpdate","E":1003,"s":"BTCUSDT","U":1003,"u":1003,"b":[["100","0"]],"a":[]})";
    ts.sec = 104;
    auto o5 =
        p.process(ts, Venue::Spot, StreamKind::DepthDiff, sym, 0, 0, 5, diff3);
    CHECK_EQ(o5, EventProcessor::Outcome::Applied);

    out.close();

    std::ifstream in(path);
    std::string line;
    emitted_csv.clear();
    while (std::getline(in, line)) {
      emitted_csv += line + "\n";
    }
    std::remove(path.c_str());
  };

  std::string run1, run2;
  run_scenario(run1);
  run_scenario(run2);

  // A replay of the same input twice must be byte-identical
  CHECK_EQ(run1, run2);

  // Parse lines and verify hand-built expectations
  std::stringstream ss(run1);
  std::string line;
  std::vector<std::string> rows;
  while (std::getline(ss, line)) {
    if (!line.empty() && line.compare(0, 5, "tsec,") != 0)
      rows.push_back(line);
  }

  // Exactly 4 rows emitted:
  // Row 1: seed (P, N)
  // Row 2: diff1 (D, B) - 95 was beyond the edge, ignored, book unchanged
  // Row 3: depth5 (P, N) - reconciled
  // (diff2 was stale, no row)
  // Row 4: diff3 (D, B) - cancelled 100, 5th slot is padded (0,0), NO ghost 95
  CHECK_EQ(rows.size(), 4u);
  if (rows.size() != 4u)
    return;

  // tsec,tnsec,seqNo,id,type,side,bid0..4,bid_size0..4,ask0..4,ask_size0..4
  std::string exp1 =
      "100,0,1,0,P,N,10000000000,9900000000,9800000000,9700000000,9600000000,"
      "1000000000,2000000000,3000000000,4000000000,5000000000,10100000000,"
      "10200000000,10300000000,10400000000,10500000000,1500000000,2500000000,"
      "3500000000,4500000000,5500000000";
  std::string exp2 =
      "101,0,2,0,D,B,10000000000,9900000000,9800000000,9700000000,9600000000,"
      "1000000000,2000000000,3000000000,4000000000,5000000000,10100000000,"
      "10200000000,10300000000,10400000000,10500000000,1500000000,2500000000,"
      "3500000000,4500000000,5500000000";
  std::string exp3 =
      "102,0,3,0,P,N,10000000000,9900000000,9800000000,9700000000,9600000000,"
      "1000000000,2000000000,3000000000,4000000000,5000000000,10100000000,"
      "10200000000,10300000000,10400000000,10500000000,1500000000,2500000000,"
      "3500000000,4500000000,5500000000";
  std::string exp4 =
      "104,0,4,0,D,B,9900000000,9800000000,9700000000,9600000000,0,"
      "2000000000,3000000000,4000000000,5000000000,0,10100000000,10200000000,"
      "10300000000,10400000000,10500000000,1500000000,2500000000,3500000000,"
      "4500000000,5500000000";

  CHECK_EQ(rows[0], exp1);
  CHECK_EQ(rows[1], exp2);
  CHECK_EQ(rows[2], exp3);
  CHECK_EQ(rows[3], exp4);
}