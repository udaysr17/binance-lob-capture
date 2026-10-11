#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "json_scan.h"
#include "metrics.h"
#include "order_book.h"
#include "output_file.h"
#include "types.h"

namespace bc {

// The single place that parses payloads and mutates order books. Live
// capture and replay both feed it identical arguments, so they make identical
// decisions and emit identical rows. It never reads a clock: row timestamps
// are the receive timestamps passed in.
//
// Sync state machine per symbol (see README "Sync policy"):
//   UNSYNCED -> first depth5 seeds the book -> SYNCED (emit 'P')
//   SYNCED diff:   u <= last_u               -> stale (dropped, counted)
//                  bridge & chain ok (below) -> apply, last_u = u (emit 'D')
//                  otherwise                 -> gap, UNSYNCED (counted, logged)
//     bridge: U <= last_u + 1; for USD-M also accepted when pu <= last_u.
//     chain:  !have_prev_diff || (usdm ? pu == prev_diff_u : U == prev_diff_u + 1).
//   SYNCED depth5: id < last_u -> stale (counted, emit 'P' from unchanged book);
//                  else reconcile top-5 window, last_u = id (emit 'P').
//   A change of (shard_id, conn_epoch) resets the symbol to UNSYNCED.
class EventProcessor {
public:
    enum class Outcome : std::uint8_t {
        Seeded,
        Applied,
        Reconciled,
        StaleDiff,
        StaleDepth5,
        Gap,
        DroppedUnsynced,
        Trade,
        ParseError,
        DecimalReject,
    };

    EventProcessor();

    // Registers a symbol and returns its index (the symbol_id for process()).
    // `row_id` is written in the order-book `id` column. `out` receives rows;
    // an OutputFile that was never opened simply accumulates rows in memory.
    int add_symbol(std::string_view symbol, std::int32_t row_id, OutputFile* out);

    Outcome process(RecvTs ts, Venue venue, StreamKind kind, int symbol_id,
                    std::uint32_t shard_id, std::uint64_t conn_epoch, std::uint64_t conn_seq,
                    std::string_view payload);

    const ProcessorMetrics& metrics() const { return m_; }
    std::size_t symbol_count() const { return symbols_.size(); }
    const std::string& symbol_name(int id) const { return symbols_[static_cast<std::size_t>(id)].name; }

    // Introspection for tests.
    bool is_synced(int id) const { return symbols_[static_cast<std::size_t>(id)].synced; }
    std::uint64_t last_update_id(int id) const { return symbols_[static_cast<std::size_t>(id)].last_u; }
    const OrderBook& book(int id) const { return symbols_[static_cast<std::size_t>(id)].book; }

private:
    struct Symbol {
        std::string name;
        std::int32_t row_id = 0;
        OutputFile* out = nullptr;
        OrderBook book;
        bool synced = false;
        std::uint64_t last_u = 0;
        bool have_prev_diff = false;
        std::uint64_t prev_diff_u = 0;
        std::uint64_t seq_no = 0;  // last emitted seqNo (first row is 1)
        bool seen_conn = false;
        std::uint32_t shard_id = 0;
        std::uint64_t conn_epoch = 0;
        std::uint64_t last_conn_seq = 0;
        bool have_ts = false;
        RecvTs last_ts;
    };

    struct Parsed {
        bool has_U = false, has_u = false, has_pu = false, has_last_update_id = false;
        bool has_bids = false, has_asks = false, has_p = false, has_q = false;
        std::uint64_t U = 0, u = 0, pu = 0, last_update_id = 0;
        std::vector<Level> bids;
        std::vector<Level> asks;
        void reset();
    };

    enum class ParseStatus : std::uint8_t { Ok, Structure, Decimal };

    ParseStatus parse_payload(std::string_view payload, const Symbol& s);
    ParseStatus parse_levels(JsonCursor& c, std::vector<Level>& out, const Symbol& s);
    ParseStatus check_decimal(std::string_view text, std::int64_t& out, const Symbol& s);

    Outcome on_diff(Symbol& s, RecvTs ts, Venue venue);
    Outcome on_depth5(Symbol& s, RecvTs ts);
    Outcome on_trade(Symbol& s);

    void reset_sync(Symbol& s);
    void emit_row(Symbol& s, RecvTs ts, char type, char side);
    void add_invariant(std::uint64_t& bucket, std::uint64_t n);
    bool should_log(std::uint64_t count) const { return count <= 20 || count % 1000 == 0; }

    std::vector<Symbol> symbols_;
    Parsed parsed_;
    ProcessorMetrics m_;
};

}  // namespace bc
