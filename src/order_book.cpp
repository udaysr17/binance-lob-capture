#include "order_book.h"

#include <algorithm>

namespace bc {

bool Top5::operator==(const Top5 &o) const {
  if (n_bids != o.n_bids || n_asks != o.n_asks)
    return false;
  for (std::size_t i = 0; i < kTopLevels; ++i) {
    if (bid_px[i] != o.bid_px[i] || bid_qty[i] != o.bid_qty[i] ||
        ask_px[i] != o.ask_px[i] || ask_qty[i] != o.ask_qty[i]) {
      return false;
    }
  }
  return true;
}

Top5 make_top5(const Level *bids, std::size_t n_bids, const Level *asks,
               std::size_t n_asks) {
  Top5 t;
  t.n_bids = n_bids < kTopLevels ? n_bids : kTopLevels;
  t.n_asks = n_asks < kTopLevels ? n_asks : kTopLevels;
  for (std::size_t i = 0; i < t.n_bids; ++i) {
    t.bid_px[i] = bids[i].price;
    t.bid_qty[i] = bids[i].qty;
  }
  for (std::size_t i = 0; i < t.n_asks; ++i) {
    t.ask_px[i] = asks[i].price;
    t.ask_qty[i] = asks[i].qty;
  }
  return t;
}

namespace {

template <typename Better>
void set_level(BookSide &side, std::int64_t price, std::int64_t qty,
               Better better) {
  // 1. If side.bounded and better(side.edge, price): return (beyond the edge,
  // unverified; ignore, also for qty==0). Price equal to edge is accepted.
  if (side.bounded && better(side.edge, price)) {
    return;
  }

  // 2. Find index i = first slot whose price is not better than `price`.
  // exists = (i < n && lv[i].price == price).
  std::size_t i = 0;
  while (i < side.n && better(side.lv[i].price, price)) {
    ++i;
  }
  const bool exists = (i < side.n && side.lv[i].price == price);

  // 3. qty==0: if exists, erase it (shift the tail left, --n, zero the freed slot);
  // `bounded` and `edge` do NOT change. If not exists, do nothing.
  if (qty == 0) {
    if (exists) {
      for (std::size_t j = i; j + 1 < side.n; ++j) {
        side.lv[j] = side.lv[j + 1];
      }
      --side.n;
      side.lv[side.n] = Level{};
    }
    return;
  }

  // 4. exists: update qty in place.
  if (exists) {
    side.lv[i].qty = qty;
    return;
  }

  // 5. Insert:
  //    - if n < kBookCapacity: shift right from i, place the level, ++n.
  //    - else (array full): if i == kBookCapacity (worse than every held level):
  //        set bounded=true, edge=lv[kBookCapacity-1].price and return without inserting.
  //      Otherwise shift right from i, dropping the old last element, place the level,
  //      and then set bounded=true, edge=lv[kBookCapacity-1].price
  //      (the tightened edge is the new worst held price).
  // Never keep a level worse than `edge`.
  if (side.n < kBookCapacity) {
    for (std::size_t j = side.n; j > i; --j) {
      side.lv[j] = side.lv[j - 1];
    }
    side.lv[i] = Level{price, qty};
    ++side.n;
  } else {
    if (i == kBookCapacity) {
      side.bounded = true;
      side.edge = side.lv[kBookCapacity - 1].price;
      return;
    }
    for (std::size_t j = kBookCapacity - 1; j > i; --j) {
      side.lv[j] = side.lv[j - 1];
    }
    side.lv[i] = Level{price, qty};
    side.bounded = true;
    side.edge = side.lv[kBookCapacity - 1].price;
  }
}

} // namespace

void OrderBook::clear() {
  bids_ = BookSide{};
  asks_ = BookSide{};
}

void OrderBook::set_bid(std::int64_t price, std::int64_t qty) {
  set_level(bids_, price, qty,
            [](std::int64_t a, std::int64_t b) { return a > b; });
}

void OrderBook::set_ask(std::int64_t price, std::int64_t qty) {
  set_level(asks_, price, qty,
            [](std::int64_t a, std::int64_t b) { return a < b; });
}

void OrderBook::load(const Level *bids, std::size_t n_bids, const Level *asks,
                     std::size_t n_asks) {
  clear();

  const std::size_t nb = std::min(n_bids, kBookCapacity);
  for (std::size_t i = 0; i < nb; ++i) {
    bids_.lv[i] = bids[i];
  }
  bids_.n = nb;
  if (n_bids >= kTopLevels) {
    bids_.bounded = true;
    bids_.edge = (n_bids <= kBookCapacity) ? bids[n_bids - 1].price
                                          : bids[kBookCapacity - 1].price;
  }

  const std::size_t na = std::min(n_asks, kBookCapacity);
  for (std::size_t i = 0; i < na; ++i) {
    asks_.lv[i] = asks[i];
  }
  asks_.n = na;
  if (n_asks >= kTopLevels) {
    asks_.bounded = true;
    asks_.edge = (n_asks <= kBookCapacity) ? asks[n_asks - 1].price
                                          : asks[kBookCapacity - 1].price;
  }
}

void OrderBook::reconcile(const Level *bids, std::size_t n_bids,
                          const Level *asks, std::size_t n_asks) {
  load(bids, n_bids, asks, n_asks);
}

Top5 OrderBook::top5() const {
  Top5 t;
  const std::size_t nb = std::min(bids_.n, kTopLevels);
  for (std::size_t i = 0; i < nb; ++i) {
    t.bid_px[i] = bids_.lv[i].price;
    t.bid_qty[i] = bids_.lv[i].qty;
  }
  t.n_bids = nb;

  const std::size_t na = std::min(asks_.n, kTopLevels);
  for (std::size_t i = 0; i < na; ++i) {
    t.ask_px[i] = asks_.lv[i].price;
    t.ask_qty[i] = asks_.lv[i].qty;
  }
  t.n_asks = na;

  return t;
}

int count_top5_violations(const Top5 &t) {
  int v = 0;
  for (std::size_t i = 0; i < kTopLevels; ++i) {
    const bool bid_on = i < t.n_bids;
    const bool ask_on = i < t.n_asks;
    if (bid_on) {
      if (t.bid_qty[i] <= 0 || t.bid_px[i] <= 0)
        ++v;
      if (i > 0 && !(t.bid_px[i] < t.bid_px[i - 1]))
        ++v;
    } else if (t.bid_px[i] != 0 || t.bid_qty[i] != 0) {
      ++v;
    }
    if (ask_on) {
      if (t.ask_qty[i] <= 0 || t.ask_px[i] <= 0)
        ++v;
      if (i > 0 && !(t.ask_px[i] > t.ask_px[i - 1]))
        ++v;
    } else if (t.ask_px[i] != 0 || t.ask_qty[i] != 0) {
      ++v;
    }
  }
  if (t.n_bids > 0 && t.n_asks > 0 && !(t.bid_px[0] < t.ask_px[0]))
    ++v;
  return v;
}

} // namespace bc
