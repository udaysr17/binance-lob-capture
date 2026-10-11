// #include "order_book.h"
// #include "test_framework.h"

// #include <algorithm>

// using namespace bc;

// namespace {

// struct Helper {
//   Level bids[5] = {
//       {100, 10},
//       {99, 20},
//       {98, 30},
//       {97, 40},
//       {96, 50},
//   };
//   Level asks[5] = {
//       {101, 15},
//       {102, 25},
//       {103, 35},
//       {104, 45},
//       {105, 55},
//   };
// };

// } // namespace

// TEST(order_book_qty_0_removal) {
//   OrderBook b;
//   Level bids[] = {{100, 10}, {90, 5}};
//   Level asks[] = {{110, 15}, {120, 20}};
//   b.load(bids, 2, asks, 2);

//   auto t = b.top5();
//   CHECK_EQ(t.n_bids, 2u);
//   CHECK_EQ(t.bid_px[0], 100);
//   CHECK_EQ(t.bid_qty[0], 10);

//   b.set_bid(100, 0);
//   t = b.top5();
//   CHECK_EQ(t.n_bids, 1u);
//   CHECK_EQ(t.bid_px[0], 90);
//   CHECK_EQ(t.bid_qty[0], 5);
// }

// TEST(T1_shrink_on_top_level_cancel) {
//   OrderBook b;
//   Helper h;
//   b.load(h.bids, 5, h.asks, 5);
//   b.set_bid(100, 0);

//   auto t = b.top5();
//   CHECK_EQ(t.n_bids, 4u);
//   CHECK_EQ(t.bid_px[0], 99);
//   CHECK_EQ(t.bid_qty[0], 20);
//   CHECK_EQ(t.bid_px[1], 98);
//   CHECK_EQ(t.bid_qty[1], 30);
//   CHECK_EQ(t.bid_px[2], 97);
//   CHECK_EQ(t.bid_qty[2], 40);
//   CHECK_EQ(t.bid_px[3], 96);
//   CHECK_EQ(t.bid_qty[3], 50);
//   CHECK_EQ(t.bid_px[4], 0);
//   CHECK_EQ(t.bid_qty[4], 0);
// }

// TEST(T2_beyond_the_edge_ignored) {
//   OrderBook b;
//   Helper h;
//   b.load(h.bids, 5, h.asks, 5);
//   b.set_bid(100, 0);

//   // Continue from T1: top5 has 99, 98, 97, 96, (0,0). edge is 96.
//   b.set_bid(94, 15);
//   b.set_bid(95, 25);

//   auto t1 = b.top5();
//   CHECK_EQ(t1.n_bids, 4u);
//   CHECK_EQ(t1.bid_px[0], 99);
//   CHECK_EQ(t1.bid_px[1], 98);
//   CHECK_EQ(t1.bid_px[2], 97);
//   CHECK_EQ(t1.bid_px[3], 96);
//   CHECK_EQ(t1.bid_qty[3], 50);
//   CHECK_EQ(t1.bid_px[4], 0);
//   CHECK_EQ(t1.bid_qty[4], 0);

//   // set_bid(96, newq) must update (edge is inclusive)
//   b.set_bid(96, 60);
//   auto t2 = b.top5();
//   CHECK_EQ(t2.n_bids, 4u);
//   CHECK_EQ(t2.bid_px[3], 96);
//   CHECK_EQ(t2.bid_qty[3], 60);
// }

// TEST(T3_edge_tightening_and_conservative_output) {
//   OrderBook b;
//   Helper h;
//   b.load(h.bids, 5, h.asks, 5);

//   // Insert (kBookCapacity - 5) better bids (101, 102, ...) so the array is
//   exactly full for (std::size_t i = 0; i < kBookCapacity - 5; ++i) {
//     b.set_bid(static_cast<std::int64_t>(101 + i), 10);
//   }
//   CHECK_EQ(b.bids().n, kBookCapacity);
//   CHECK_EQ(b.bids().edge, 96);

//   // Insert one more better bid
//   const std::int64_t next_better =
//       static_cast<std::int64_t>(101 + (kBookCapacity - 5));
//   b.set_bid(next_better, 10);

//   // Expect the old worst (96) dropped, edge == 97
//   CHECK_EQ(b.bids().n, kBookCapacity);
//   CHECK_EQ(b.bids().edge, 97);

//   // set_bid(96, q) now ignored
//   b.set_bid(96, 99);
//   CHECK_EQ(b.bids().edge, 97);
//   for (std::size_t i = 0; i < b.bids().n; ++i) {
//     CHECK(b.bids().lv[i].price != 96);
//   }

//   // set_bid(97, q2) accepted as an update
//   b.set_bid(97, 77);
//   CHECK_EQ(b.bids().lv[kBookCapacity - 1].price, 97);
//   CHECK_EQ(b.bids().lv[kBookCapacity - 1].qty, 77);

//   // Delete the best levels one at a time
//   while (b.bids().n > 0) {
//     const std::int64_t best_px = b.bids().lv[0].price;
//     b.set_bid(best_px, 0);

//     auto t = b.top5();
//     const std::size_t expected_emitted = std::min(b.bids().n, kTopLevels);
//     CHECK_EQ(t.n_bids, expected_emitted);

//     for (std::size_t i = 0; i < expected_emitted; ++i) {
//       CHECK_EQ(t.bid_px[i], b.bids().lv[i].price);
//       CHECK_EQ(t.bid_qty[i], b.bids().lv[i].qty);
//       if (i > 0) {
//         CHECK(t.bid_px[i] < t.bid_px[i - 1]);
//       }
//       CHECK(t.bid_px[i] >= 97);
//     }
//     for (std::size_t i = expected_emitted; i < kTopLevels; ++i) {
//       CHECK_EQ(t.bid_px[i], 0);
//       CHECK_EQ(t.bid_qty[i], 0);
//     }
//   }
// }

// TEST(T4_thin_side_accepts_deeper_levels) {
//   OrderBook b;
//   Helper h;
//   // Load asks with only 3 levels (101, 102, 103)
//   b.load(h.bids, 5, h.asks, 3);
//   CHECK(!b.asks().bounded);
//   CHECK_EQ(b.asks().n, 3u);

//   // set_ask far beyond the 3rd (e.g. +50 ticks) must appear as the 4th ask
//   b.set_ask(103 + 50, 10);
//   auto t = b.top5();
//   CHECK_EQ(t.n_asks, 4u);
//   CHECK_EQ(t.ask_px[0], 101);
//   CHECK_EQ(t.ask_px[1], 102);
//   CHECK_EQ(t.ask_px[2], 103);
//   CHECK_EQ(t.ask_px[3], 153);
//   CHECK_EQ(t.ask_px[4], 0);
//   CHECK(!b.asks().bounded);

//   // Fill up to kBookCapacity
//   for (std::size_t i = 4; i < kBookCapacity; ++i) {
//     b.set_ask(static_cast<std::int64_t>(200 + i), 10);
//   }
//   CHECK_EQ(b.asks().n, kBookCapacity);

//   // Fill past kBookCapacity: worse than all held levels
//   const std::int64_t worst_held = b.asks().lv[kBookCapacity - 1].price;
//   b.set_ask(5000, 10);
//   CHECK(b.asks().bounded);
//   CHECK_EQ(b.asks().edge, worst_held);

//   // Better than best held level tightens edge
//   b.set_ask(100, 10);
//   CHECK(b.asks().bounded);
//   CHECK_EQ(b.asks().edge, b.asks().lv[kBookCapacity - 1].price);
// }

// TEST(T5_reconcile_equals_the_snapshot) {
//   OrderBook b;
//   Helper h;
//   b.load(h.bids, 5, h.asks, 5);

//   // Mixed sequence of inserts/updates/deletes
//   b.set_bid(105, 10); // better than edge
//   b.set_bid(98, 99);  // update
//   b.set_bid(100, 0);  // delete
//   b.set_bid(90, 10);  // beyond edge (ignored)
//   b.set_ask(102, 0);  // delete
//   b.set_ask(110, 50); // beyond edge (ignored)

//   Level bids2[5] = {
//       {200, 1}, {199, 2}, {198, 3}, {197, 4}, {196, 5}};
//   Level asks2[5] = {
//       {201, 1}, {202, 2}, {203, 3}, {204, 4}, {205, 5}};

//   b.reconcile(bids2, 5, asks2, 5);

//   CHECK(b.top5() == make_top5(bids2, 5, asks2, 5));
//   // No old level may survive
//   CHECK_EQ(b.bids().n, 5u);
//   CHECK_EQ(b.asks().n, 5u);
//   for (std::size_t i = 0; i < 5; ++i) {
//     CHECK_EQ(b.bids().lv[i].price, bids2[i].price);
//     CHECK_EQ(b.asks().lv[i].price, asks2[i].price);
//   }

//   // set_bid for a price beyond snapshot2's fifth level (196) must be ignored
//   b.set_bid(195, 10);
//   CHECK(b.top5() == make_top5(bids2, 5, asks2, 5));
//   CHECK_EQ(b.bids().n, 5u);
//   CHECK_EQ(b.bids().edge, 196);
// }

// TEST(T6_more_than_5_held) {
//   OrderBook b;
//   Helper h;
//   b.load(h.bids, 5, h.asks, 5); // edge is 96

//   // Insert 3 bids inside the edge (103, 102, 101) to have 8 held bids
//   b.set_bid(103, 10);
//   b.set_bid(102, 20);
//   b.set_bid(101, 30);

//   CHECK_EQ(b.bids().n, 8u);

//   // top5 returns exactly the best 5 in order
//   auto t1 = b.top5();
//   CHECK_EQ(t1.n_bids, 5u);
//   CHECK_EQ(t1.bid_px[0], 103);
//   CHECK_EQ(t1.bid_px[1], 102);
//   CHECK_EQ(t1.bid_px[2], 101);
//   CHECK_EQ(t1.bid_px[3], 100);
//   CHECK_EQ(t1.bid_px[4], 99);

//   // Deleting the best one pulls the 6th (98) into the 5th slot
//   b.set_bid(103, 0);
//   auto t2 = b.top5();
//   CHECK_EQ(t2.n_bids, 5u);
//   CHECK_EQ(t2.bid_px[0], 102);
//   CHECK_EQ(t2.bid_px[1], 101);
//   CHECK_EQ(t2.bid_px[2], 100);
//   CHECK_EQ(t2.bid_px[3], 99);
//   CHECK_EQ(t2.bid_px[4], 98);
// }

// TEST(order_book_padding) {
//   OrderBook b;
//   Level bids[] = {{100, 10}, {99, 5}};
//   b.load(bids, 2, nullptr, 0);
//   auto t = b.top5();
//   CHECK_EQ(t.n_bids, 2u);
//   CHECK_EQ(t.n_asks, 0u);
//   // Unpopulated should be zero
//   CHECK_EQ(t.bid_px[2], 0);
//   CHECK_EQ(t.bid_qty[2], 0);
//   CHECK_EQ(t.ask_px[0], 0);
//   CHECK_EQ(t.ask_qty[0], 0);
// }

// TEST(order_book_ordering_and_violations) {
//   OrderBook b;
//   Level bids[] = {{100, 5}, {90, 10}};
//   Level asks[] = {{110, 20}, {120, 15}};
//   b.load(bids, 2, asks, 2);

//   auto t = b.top5();
//   CHECK_EQ(t.n_bids, 2u);
//   CHECK_EQ(t.bid_px[0], 100);
//   CHECK_EQ(t.bid_px[1], 90);
//   CHECK_EQ(t.ask_px[0], 110);
//   CHECK_EQ(t.ask_px[1], 120);

//   CHECK_EQ(count_top5_violations(t), 0);

//   // Create a bad Top5 artificially
//   Top5 bad;
//   bad.n_bids = 2;
//   bad.bid_px[0] = 90;
//   bad.bid_qty[0] = 10;
//   bad.bid_px[1] = 100;
//   bad.bid_qty[1] = 10; // Not strictly descending
//   bad.n_asks = 0;
//   CHECK_EQ(count_top5_violations(bad), 1);
// }

#include "order_book.h"
#include "test_framework.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <random>

using namespace bc;

static_assert(kBookCapacity > kTopLevels,
              "these tests assume capacity > kTopLevels");

namespace {

struct Helper {
  Level bids[5] = {
      {100, 10}, {99, 20}, {98, 30}, {97, 40}, {96, 50},
  };
  Level asks[5] = {
      {101, 15}, {102, 25}, {103, 35}, {104, 45}, {105, 55},
  };
};

} // namespace

TEST(order_book_qty_0_removal) {
  OrderBook b;
  Level bids[] = {{100, 10}, {90, 5}};
  Level asks[] = {{110, 15}, {120, 20}};
  b.load(bids, 2, asks, 2);

  auto t = b.top5();
  CHECK_EQ(t.n_bids, 2u);
  CHECK_EQ(t.bid_px[0], 100);
  CHECK_EQ(t.bid_qty[0], 10);

  b.set_bid(100, 0);
  t = b.top5();
  CHECK_EQ(t.n_bids, 1u);
  CHECK_EQ(t.bid_px[0], 90);
  CHECK_EQ(t.bid_qty[0], 5);
}

TEST(T1_shrink_on_top_level_cancel) {
  OrderBook b;
  Helper h;
  b.load(h.bids, 5, h.asks, 5);
  b.set_bid(100, 0);

  auto t = b.top5();
  CHECK_EQ(t.n_bids, 4u);
  CHECK_EQ(t.bid_px[0], 99);
  CHECK_EQ(t.bid_qty[0], 20);
  CHECK_EQ(t.bid_px[1], 98);
  CHECK_EQ(t.bid_qty[1], 30);
  CHECK_EQ(t.bid_px[2], 97);
  CHECK_EQ(t.bid_qty[2], 40);
  CHECK_EQ(t.bid_px[3], 96);
  CHECK_EQ(t.bid_qty[3], 50);
  CHECK_EQ(t.bid_px[4], 0);
  CHECK_EQ(t.bid_qty[4], 0);
}

TEST(T2_beyond_the_edge_ignored) {
  OrderBook b;
  Helper h;
  b.load(h.bids, 5, h.asks, 5);
  b.set_bid(100, 0);

  // Continue from T1: top5 has 99, 98, 97, 96, (0,0). edge is 96.
  b.set_bid(94, 15);
  b.set_bid(95, 25);

  auto t1 = b.top5();
  CHECK_EQ(t1.n_bids, 4u);
  CHECK_EQ(t1.bid_px[0], 99);
  CHECK_EQ(t1.bid_px[1], 98);
  CHECK_EQ(t1.bid_px[2], 97);
  CHECK_EQ(t1.bid_px[3], 96);
  CHECK_EQ(t1.bid_qty[3], 50);
  CHECK_EQ(t1.bid_px[4], 0);
  CHECK_EQ(t1.bid_qty[4], 0);

  // set_bid(96, newq) must update (edge is inclusive)
  b.set_bid(96, 60);
  auto t2 = b.top5();
  CHECK_EQ(t2.n_bids, 4u);
  CHECK_EQ(t2.bid_px[3], 96);
  CHECK_EQ(t2.bid_qty[3], 60);
}

TEST(T3_edge_tightening_and_conservative_output) {
  OrderBook b;
  Helper h;
  b.load(h.bids, 5, h.asks, 5);

  // Insert (kBookCapacity - 5) better bids (101, 102, ...) so the array is
  // exactly full
  for (std::size_t i = 0; i < kBookCapacity - 5; ++i) {
    b.set_bid(static_cast<std::int64_t>(101 + i), 10);
  }
  CHECK_EQ(b.bids().n, kBookCapacity);
  CHECK_EQ(b.bids().edge, 96);

  // Insert one more better bid
  const std::int64_t next_better =
      static_cast<std::int64_t>(101 + (kBookCapacity - 5));
  b.set_bid(next_better, 10);

  // Expect the old worst (96) dropped, edge == 97
  CHECK_EQ(b.bids().n, kBookCapacity);
  CHECK_EQ(b.bids().edge, 97);

  // set_bid(96, q) now ignored
  b.set_bid(96, 99);
  CHECK_EQ(b.bids().edge, 97);
  for (std::size_t i = 0; i < b.bids().n; ++i) {
    CHECK(b.bids().lv[i].price != 96);
  }

  // set_bid(97, q2) accepted as an update
  b.set_bid(97, 77);
  CHECK_EQ(b.bids().lv[kBookCapacity - 1].price, 97);
  CHECK_EQ(b.bids().lv[kBookCapacity - 1].qty, 77);

  // Delete the best levels one at a time
  while (b.bids().n > 0) {
    const std::int64_t best_px = b.bids().lv[0].price;
    b.set_bid(best_px, 0);

    auto t = b.top5();
    const std::size_t expected_emitted = std::min(b.bids().n, kTopLevels);
    CHECK_EQ(t.n_bids, expected_emitted);

    for (std::size_t i = 0; i < expected_emitted; ++i) {
      CHECK_EQ(t.bid_px[i], b.bids().lv[i].price);
      CHECK_EQ(t.bid_qty[i], b.bids().lv[i].qty);
      if (i > 0) {
        CHECK(t.bid_px[i] < t.bid_px[i - 1]);
      }
      CHECK(t.bid_px[i] >= 97);
    }
    for (std::size_t i = expected_emitted; i < kTopLevels; ++i) {
      CHECK_EQ(t.bid_px[i], 0);
      CHECK_EQ(t.bid_qty[i], 0);
    }
  }
}

TEST(T4_thin_side_accepts_deeper_levels) {
  OrderBook b;
  Helper h;
  // Load asks with only 3 levels (101, 102, 103)
  b.load(h.bids, 5, h.asks, 3);
  CHECK(!b.asks().bounded);
  CHECK_EQ(b.asks().n, 3u);

  // set_ask far beyond the 3rd (e.g. +50 ticks) must appear as the 4th ask
  b.set_ask(103 + 50, 10);
  auto t = b.top5();
  CHECK_EQ(t.n_asks, 4u);
  CHECK_EQ(t.ask_px[0], 101);
  CHECK_EQ(t.ask_px[1], 102);
  CHECK_EQ(t.ask_px[2], 103);
  CHECK_EQ(t.ask_px[3], 153);
  CHECK_EQ(t.ask_px[4], 0);
  CHECK(!b.asks().bounded);

  // Fill up to kBookCapacity
  for (std::size_t i = 4; i < kBookCapacity; ++i) {
    b.set_ask(static_cast<std::int64_t>(200 + i), 10);
  }
  CHECK_EQ(b.asks().n, kBookCapacity);

  // Fill past kBookCapacity: worse than all held levels
  const std::int64_t worst_held = b.asks().lv[kBookCapacity - 1].price;
  b.set_ask(5000, 10);
  CHECK(b.asks().bounded);
  CHECK_EQ(b.asks().edge, worst_held);

  // Better than best held level tightens edge
  b.set_ask(100, 10);
  CHECK(b.asks().bounded);
  CHECK_EQ(b.asks().edge, b.asks().lv[kBookCapacity - 1].price);
}

TEST(T5_reconcile_equals_the_snapshot) {
  OrderBook b;
  Helper h;
  b.load(h.bids, 5, h.asks, 5);

  // Mixed sequence of inserts/updates/deletes
  b.set_bid(105, 10); // better than edge
  b.set_bid(98, 99);  // update
  b.set_bid(100, 0);  // delete
  b.set_bid(90, 10);  // beyond edge (ignored)
  b.set_ask(102, 0);  // delete
  b.set_ask(110, 50); // beyond edge (ignored)

  Level bids2[5] = {{200, 1}, {199, 2}, {198, 3}, {197, 4}, {196, 5}};
  Level asks2[5] = {{201, 1}, {202, 2}, {203, 3}, {204, 4}, {205, 5}};

  b.reconcile(bids2, 5, asks2, 5);

  CHECK(b.top5() == make_top5(bids2, 5, asks2, 5));
  // No old level may survive
  CHECK_EQ(b.bids().n, 5u);
  CHECK_EQ(b.asks().n, 5u);
  for (std::size_t i = 0; i < 5; ++i) {
    CHECK_EQ(b.bids().lv[i].price, bids2[i].price);
    CHECK_EQ(b.asks().lv[i].price, asks2[i].price);
  }

  // set_bid for a price beyond snapshot2's fifth level (196) must be ignored
  b.set_bid(195, 10);
  CHECK(b.top5() == make_top5(bids2, 5, asks2, 5));
  CHECK_EQ(b.bids().n, 5u);
  CHECK_EQ(b.bids().edge, 196);
}

TEST(T6_more_than_5_held) {
  OrderBook b;
  Helper h;
  b.load(h.bids, 5, h.asks, 5); // edge is 96

  // Insert 3 bids inside the edge (103, 102, 101) to have 8 held bids
  b.set_bid(103, 10);
  b.set_bid(102, 20);
  b.set_bid(101, 30);

  CHECK_EQ(b.bids().n, 8u);

  // top5 returns exactly the best 5 in order
  auto t1 = b.top5();
  CHECK_EQ(t1.n_bids, 5u);
  CHECK_EQ(t1.bid_px[0], 103);
  CHECK_EQ(t1.bid_px[1], 102);
  CHECK_EQ(t1.bid_px[2], 101);
  CHECK_EQ(t1.bid_px[3], 100);
  CHECK_EQ(t1.bid_px[4], 99);

  // Deleting the best one pulls the 6th (98) into the 5th slot
  b.set_bid(103, 0);
  auto t2 = b.top5();
  CHECK_EQ(t2.n_bids, 5u);
  CHECK_EQ(t2.bid_px[0], 102);
  CHECK_EQ(t2.bid_px[1], 101);
  CHECK_EQ(t2.bid_px[2], 100);
  CHECK_EQ(t2.bid_px[3], 99);
  CHECK_EQ(t2.bid_px[4], 98);
}

TEST(order_book_padding) {
  OrderBook b;
  Level bids[] = {{100, 10}, {99, 5}};
  b.load(bids, 2, nullptr, 0);
  auto t = b.top5();
  CHECK_EQ(t.n_bids, 2u);
  CHECK_EQ(t.n_asks, 0u);
  // Unpopulated should be zero
  CHECK_EQ(t.bid_px[2], 0);
  CHECK_EQ(t.bid_qty[2], 0);
  CHECK_EQ(t.ask_px[0], 0);
  CHECK_EQ(t.ask_qty[0], 0);
}

TEST(order_book_ordering_and_violations) {
  OrderBook b;
  Level bids[] = {{100, 5}, {90, 10}};
  Level asks[] = {{110, 20}, {120, 15}};
  b.load(bids, 2, asks, 2);

  auto t = b.top5();
  CHECK_EQ(t.n_bids, 2u);
  CHECK_EQ(t.bid_px[0], 100);
  CHECK_EQ(t.bid_px[1], 90);
  CHECK_EQ(t.ask_px[0], 110);
  CHECK_EQ(t.ask_px[1], 120);

  CHECK_EQ(count_top5_violations(t), 0);

  // Create a bad Top5 artificially ({} so every other slot is a defined zero)
  Top5 bad{};
  bad.n_bids = 2;
  bad.bid_px[0] = 90;
  bad.bid_qty[0] = 10;
  bad.bid_px[1] = 100;
  bad.bid_qty[1] = 10; // Not strictly descending
  bad.n_asks = 0;
  CHECK_EQ(count_top5_violations(bad), 1);
}

// ===== ADDED TESTS (close the gaps found in review) =====

// Mirror of T2/T3 for the ASK side: the edge comparison for asks is the
// opposite direction, and nothing above exercised it on a bounded ask side.
TEST(ask_side_edge_shrink_and_ignore) {
  OrderBook b;
  Helper h;
  b.load(h.bids, 5, h.asks, 5); // asks 101..105, edge 105
  CHECK(b.asks().bounded);
  CHECK_EQ(b.asks().edge, 105);

  b.set_ask(106, 9); // beyond the edge: ignored
  CHECK_EQ(b.asks().n, 5u);

  b.set_ask(105, 77); // at the edge: updates (inclusive)
  CHECK_EQ(b.top5().ask_qty[4], 77);

  b.set_ask(101, 0); // cancel the best ask: side shrinks to 4
  auto t = b.top5();
  CHECK_EQ(t.n_asks, 4u);
  CHECK_EQ(t.ask_px[0], 102);
  CHECK_EQ(t.ask_px[1], 103);
  CHECK_EQ(t.ask_px[2], 104);
  CHECK_EQ(t.ask_px[3], 105);
  CHECK_EQ(t.ask_px[4], 0);
  CHECK_EQ(t.ask_qty[4], 0);

  b.set_ask(106, 9); // deeper than the edge after a shrink: STILL ignored
  t = b.top5();
  CHECK_EQ(t.n_asks, 4u);
  CHECK_EQ(t.ask_px[4], 0);

  b.set_ask(100, 4); // better than the best: accepted
  t = b.top5();
  CHECK_EQ(t.n_asks, 5u);
  CHECK_EQ(t.ask_px[0], 100);
  CHECK_EQ(t.ask_px[4], 105);

  // the bid side was never touched
  CHECK_EQ(t.n_bids, 5u);
  CHECK_EQ(t.bid_px[0], 100);
}

// T4's last assertion compared the edge with the array's own last element,
// which holds even if the insert was wrongly rejected. This version proves the
// insert happened, the old worst level left, and the edge moved.
TEST(T4b_better_than_best_when_full_is_accepted_and_tightens_edge) {
  OrderBook b;
  Helper h;
  b.load(h.bids, 5, h.asks, 3); // asks 101,102,103, unbounded
  for (std::size_t i = 0; i < kBookCapacity - 3; ++i) {
    b.set_ask(static_cast<std::int64_t>(200 + i),
              10); // contiguous 200, 201, ...
  }
  CHECK_EQ(b.asks().n, kBookCapacity);
  CHECK(!b.asks().bounded); // exactly full is not an overflow yet

  const std::int64_t worst = b.asks().lv[kBookCapacity - 1].price;
  b.set_ask(100, 10); // better than the best while full
  CHECK_EQ(b.asks().lv[0].price, 100);
  CHECK_EQ(b.asks().n, kBookCapacity);
  CHECK(b.asks().bounded);
  CHECK_EQ(b.asks().edge,
           worst - 1); // old worst pushed out; new worst is the level before it
  for (std::size_t i = 0; i < b.asks().n; ++i) {
    CHECK(b.asks().lv[i].price != worst);
  }

  b.set_ask(worst, 5); // the pushed-out level is now beyond the edge: ignored
  CHECK_EQ(b.asks().lv[kBookCapacity - 1].price, worst - 1);
}

TEST(snapshot_with_4_levels_is_unbounded_and_5_is_bounded) {
  OrderBook b;
  Helper h;
  b.load(h.bids, 4, h.asks, 5);
  CHECK(!b.bids().bounded);
  CHECK(b.asks().bounded);

  b.set_bid(10, 1); // far deeper: accepted, the 4-level side was fully listed
  CHECK_EQ(b.bids().n, 5u);
  CHECK_EQ(b.bids().lv[4].price, 10);
}

// A stale edge surviving a new snapshot or clear() would silently discard valid
// levels.
TEST(reconcile_and_clear_reset_the_edge) {
  OrderBook b;
  Helper h;
  b.load(h.bids, 5, h.asks, 5);
  CHECK(b.bids().bounded);
  CHECK(b.asks().bounded);

  b.reconcile(h.bids, 3, h.asks, 3); // thin snapshot replaces a bounded book
  CHECK(!b.bids().bounded);
  CHECK(!b.asks().bounded);
  b.set_bid(50, 7);
  b.set_ask(900, 7);
  auto t = b.top5();
  CHECK_EQ(t.n_bids, 4u);
  CHECK_EQ(t.bid_px[3], 50);
  CHECK_EQ(t.n_asks, 4u);
  CHECK_EQ(t.ask_px[3], 900);

  b.load(h.bids, 5, h.asks, 5);
  b.clear();
  CHECK(!b.bids().bounded);
  CHECK(!b.asks().bounded);
  CHECK_EQ(b.bids().n, 0u);
  CHECK_EQ(b.asks().n, 0u);
  b.set_bid(1, 1);
  CHECK_EQ(b.bids().n, 1u);
}

TEST(delete_of_missing_level_is_a_noop_and_gap_insert_works) {
  OrderBook b;
  Helper h;
  Level bids[5] = {
      {100, 1}, {98, 2}, {96, 3}, {94, 4}, {92, 5}}; // gaps on purpose
  b.load(bids, 5, h.asks, 5);
  const Top5 before = b.top5();

  b.set_bid(99, 0); // missing, inside the window
  b.set_bid(97, 0); // missing, inside the window
  b.set_bid(91, 0); // missing, beyond the edge
  CHECK(b.top5() == before);
  CHECK_EQ(b.bids().n, 5u);
  CHECK_EQ(b.bids().edge, 92);

  b.set_bid(99, 7); // insert into a gap inside the window
  auto t = b.top5();
  CHECK_EQ(b.bids().n, 6u); // 92 is still held (inside the edge)
  CHECK_EQ(t.bid_px[0], 100);
  CHECK_EQ(t.bid_px[1], 99);
  CHECK_EQ(t.bid_px[2], 98);
  CHECK_EQ(t.bid_px[3], 96);
  CHECK_EQ(t.bid_px[4], 94);
  CHECK_EQ(b.bids().edge, 92);
}

TEST(update_in_a_full_array_does_not_tighten_the_edge) {
  OrderBook b;
  Helper h;
  b.load(h.bids, 5, h.asks, 5);
  for (std::size_t i = 0; i < kBookCapacity - 5; ++i) {
    b.set_bid(static_cast<std::int64_t>(101 + i), 10);
  }
  CHECK_EQ(b.bids().n, kBookCapacity);
  CHECK_EQ(b.bids().edge, 96);

  b.set_bid(105, 999); // update an existing level while the array is full
  CHECK_EQ(b.bids().n, kBookCapacity);
  CHECK_EQ(b.bids().edge, 96);
  bool found = false;
  for (std::size_t i = 0; i < b.bids().n; ++i) {
    if (b.bids().lv[i].price == 105) {
      found = true;
      CHECK_EQ(b.bids().lv[i].qty, 999);
    }
  }
  CHECK(found);

  b.set_bid(96, 55); // update at the edge while full
  CHECK_EQ(b.bids().n, kBookCapacity);
  CHECK_EQ(b.bids().edge, 96);
  CHECK_EQ(b.bids().lv[kBookCapacity - 1].price, 96);
  CHECK_EQ(b.bids().lv[kBookCapacity - 1].qty, 55);
}

namespace {

// Independent oracle: a plain std::map with the documented edge semantics.
// Cmp = std::greater (bids) or std::less (asks), i.e. "best first".
template <class Cmp> struct RefSide {
  std::map<std::int64_t, std::int64_t, Cmp> m;
  bool bounded = false;
  std::int64_t edge = 0;

  void load(const Level *lv, std::size_t n) {
    m.clear();
    bounded = false;
    edge = 0;
    for (std::size_t i = 0; i < n; ++i)
      m[lv[i].price] = lv[i].qty;
    if (n >= kTopLevels) {
      bounded = true;
      edge = lv[n - 1].price;
    }
  }
  void clear() {
    m.clear();
    bounded = false;
    edge = 0;
  }
  void set(std::int64_t px, std::int64_t q) {
    if (bounded && Cmp()(edge, px))
      return; // worse than the edge: not tracked
    if (q == 0) {
      m.erase(px);
      return;
    }
    m[px] = q;
    if (m.size() >
        kBookCapacity) { // overflow: the worst level leaves, the edge tightens
      m.erase(std::prev(m.end()));
      bounded = true;
      edge = std::prev(m.end())->first;
    }
  }
};

template <class Cmp, class Side>
bool side_matches(const RefSide<Cmp> &r, const Side &s) {
  if (r.m.size() != s.n || r.bounded != s.bounded)
    return false;
  if (r.bounded && r.edge != s.edge)
    return false;
  std::size_t i = 0;
  for (const auto &kv : r.m) {
    if (s.lv[i].price != kv.first || s.lv[i].qty != kv.second)
      return false;
    ++i;
  }
  return true;
}

template <class Cmp, class PxAt, class QtyAt>
bool top_matches(const RefSide<Cmp> &r, std::size_t n_reported, PxAt px_at,
                 QtyAt qty_at) {
  if (n_reported != std::min(r.m.size(), kTopLevels))
    return false;
  auto it = r.m.begin();
  for (std::size_t i = 0; i < kTopLevels; ++i) {
    if (it != r.m.end()) {
      if (px_at(i) != it->first || qty_at(i) != it->second)
        return false;
      ++it;
    } else if (px_at(i) != 0 || qty_at(i) != 0) {
      return false;
    }
  }
  return true;
}

} // namespace

// Differential test: 200k random operations on both sides (prices chosen to hit
// overflow, the edge, gaps, deletes of missing levels, thin and full snapshots,
// reconcile and clear) compared against the oracle after EVERY operation:
// array contents, bounded flag, edge, and the emitted top 5.
TEST(model_based_random_ops_match_reference) {
  std::mt19937 rng(20241011);
  auto rnd = [&](std::uint32_t n) {
    return static_cast<std::int64_t>(rng() % n);
  };

  OrderBook b;
  RefSide<std::greater<std::int64_t>> rb;
  RefSide<std::less<std::int64_t>> ra;

  auto snapshot = [&](bool use_reconcile) {
    Level bl[5] = {};
    Level al[5] = {};
    const std::size_t nb =
        1 + static_cast<std::size_t>(rnd(5)); // 1..5 levels: thin and full
    const std::size_t na = 1 + static_cast<std::size_t>(rnd(5));
    std::int64_t px = 90 + rnd(21);
    for (std::size_t i = 0; i < nb; ++i) {
      bl[i] = Level{px, 1 + rnd(9)};
      px -= 1 + rnd(3);
    }
    px = 111 + rnd(20);
    for (std::size_t i = 0; i < na; ++i) {
      al[i] = Level{px, 1 + rnd(9)};
      px += 1 + rnd(3);
    }
    if (use_reconcile) {
      b.reconcile(bl, nb, al, na);
    } else {
      b.load(bl, nb, al, na);
    }
    rb.load(bl, nb);
    ra.load(al, na);
  };

  snapshot(false);
  for (int it = 0; it < 200000; ++it) {
    const std::uint32_t r = static_cast<std::uint32_t>(rng() % 1000);
    if (r < 3) {
      snapshot((rng() & 1u) != 0);
    } else if (r < 5) {
      b.clear();
      rb.clear();
      ra.clear();
    } else {
      const bool bid = (rng() & 1u) != 0;
      const std::int64_t px = 60 + rnd(81);                     // 60..140
      const std::int64_t q = (rng() % 10 < 4) ? 0 : 1 + rnd(9); // ~40% deletes
      if (bid) {
        b.set_bid(px, q);
        rb.set(px, q);
      } else {
        b.set_ask(px, q);
        ra.set(px, q);
      }
    }

    const Top5 t = b.top5();
    const bool ok =
        side_matches(rb, b.bids()) && side_matches(ra, b.asks()) &&
        top_matches(
            rb, t.n_bids, [&](std::size_t i) { return t.bid_px[i]; },
            [&](std::size_t i) { return t.bid_qty[i]; }) &&
        top_matches(
            ra, t.n_asks, [&](std::size_t i) { return t.ask_px[i]; },
            [&](std::size_t i) { return t.ask_qty[i]; });
    if (!ok) {
      std::fprintf(stderr, "  model mismatch at iteration %d\n", it);
      CHECK(ok);
      return;
    }
  }
}

// count_top5_violations feeds the live invariant counter; "0 violations" in a
// live run means nothing unless each rule is shown to fire.
TEST(count_top5_violations_flags_each_rule) {
  Top5 good{};
  good.n_bids = 2;
  good.bid_px[0] = 100;
  good.bid_qty[0] = 1;
  good.bid_px[1] = 99;
  good.bid_qty[1] = 1;
  good.n_asks = 1;
  good.ask_px[0] = 101;
  good.ask_qty[0] = 1;
  CHECK_EQ(count_top5_violations(good), 0);

  {
    Top5 t = good;
    t.bid_px[1] = 100;
    CHECK_EQ(count_top5_violations(t), 1);
  } // equal bids
  {
    Top5 t = good;
    t.n_asks = 2;
    t.ask_px[1] = 101;
    t.ask_qty[1] = 1;
    CHECK_EQ(count_top5_violations(t), 1);
  } // equal asks
  {
    Top5 t = good;
    t.n_asks = 2;
    t.ask_px[1] = 100;
    t.ask_qty[1] = 1;
    CHECK_EQ(count_top5_violations(t), 1);
  } // asks descending
  {
    Top5 t = good;
    t.bid_qty[0] = 0;
    CHECK_EQ(count_top5_violations(t), 1);
  } // zero qty in use
  {
    Top5 t = good;
    t.ask_qty[0] = -1;
    CHECK_EQ(count_top5_violations(t), 1);
  } // negative qty
  {
    Top5 t = good;
    t.bid_px[1] = 0;
    CHECK_EQ(count_top5_violations(t), 1);
  } // zero price in use
  {
    Top5 t = good;
    t.bid_px[3] = 7;
    CHECK_EQ(count_top5_violations(t), 1);
  } // dirty padding
  {
    Top5 t = good;
    t.ask_px[0] = 100;
    CHECK_EQ(count_top5_violations(t), 1);
  } // locked book
  {
    Top5 t = good;
    t.ask_px[0] = 99;
    CHECK_EQ(count_top5_violations(t), 1);
  } // crossed book
  {
    Top5 t = good;
    t.n_asks = 0;
    t.ask_px[0] = 0;
    t.ask_qty[0] = 0;
    CHECK_EQ(count_top5_violations(t), 0);
  } // one-sided is fine
}