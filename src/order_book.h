#pragma once

#include <cstddef>
#include <cstdint>

namespace bc {

struct Level {
  std::int64_t price = 0; // scaled 1e8
  std::int64_t qty = 0;   // scaled 1e8
};

constexpr std::size_t kTopLevels = 5;
constexpr std::size_t kBookCapacity = 20;

// Top-5 view, best first, padded with (0,0) for missing levels.
struct Top5 {
  std::int64_t bid_px[kTopLevels] = {};
  std::int64_t bid_qty[kTopLevels] = {};
  std::int64_t ask_px[kTopLevels] = {};
  std::int64_t ask_qty[kTopLevels] = {};
  std::size_t n_bids = 0;
  std::size_t n_asks = 0;

  bool operator==(const Top5 &o) const;
  bool operator!=(const Top5 &o) const { return !(*this == o); }
};

// Builds a padded Top5 from (already ordered, best-first) snapshot levels.
Top5 make_top5(const Level *bids, std::size_t n_bids, const Level *asks,
               std::size_t n_asks);

// INVARIANT: the array contains EVERY level whose price is at-or-better than
// `edge` (or every level on the side, if !bounded), each with its current
// quantity. Anything worse than `edge` is not stored.
struct BookSide {
  Level lv[kBookCapacity]{}; // best-first: bids descending, asks ascending
  std::size_t n = 0;         // slots in use
  bool bounded = false;      // true once levels beyond the array/snapshot may exist that we do not hold
  std::int64_t edge = 0;     // worst price we can vouch for (valid only if bounded)
};

// Fixed-capacity order book for one symbol. Holds up to kBookCapacity levels
// per side. Bounded sides never hold or emit unverified levels beyond edge.
class OrderBook {
public:
  void clear();

  // Absolute update: qty == 0 erases the level, otherwise sets it.
  void set_bid(std::int64_t price, std::int64_t qty);
  void set_ask(std::int64_t price, std::int64_t qty);

  // Seed: clear everything and load the given snapshot levels.
  void load(const Level *bids, std::size_t n_bids, const Level *asks,
            std::size_t n_asks);

  // Snapshot reconciliation: authoritative snapshot, identical to load.
  void reconcile(const Level *bids, std::size_t n_bids, const Level *asks,
                 std::size_t n_asks);

  Top5 top5() const;

  std::size_t bid_depth() const { return bids_.n; }
  std::size_t ask_depth() const { return asks_.n; }
  const BookSide &bids() const { return bids_; }
  const BookSide &asks() const { return asks_; }

private:
  BookSide bids_;
  BookSide asks_;
};

// Invariant check on an emitted top-5 view: bids strictly descending, asks
// strictly ascending, positive sizes on populated levels, zero padding, and
// best bid < best ask. Returns the number of violated rules (0 = clean).
int count_top5_violations(const Top5 &t);

} // namespace bc
