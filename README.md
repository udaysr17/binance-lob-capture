# Binance Capture: WebSocket capture + local order book (C++17)

`binance_capture` connects to Binance public WebSocket combined streams (spot or USD-M futures), records
every inbound `depth@100ms`, `depth5@100ms` and `trade` message to a market-data CSV, and maintains a local
top-of-book that is written to an order-book CSV after every applied depth event. A replay mode rebuilds the
order-book CSV from a saved market-data CSV with no network access.

Everything in this file describes what the code does today. Items that are deliberately not implemented are
listed under *Known limitations*.

## Requirements

| | |
|---|---|
| Compiler | GCC 12 (`g++-12`); also builds with Clang 15 |
| Language standard | C++17 |
| Build system | CMake >= 3.16 (Ninja optional) |
| Dependencies | Boost 1.74 (Beast/Asio, header-only use), OpenSSL, zlib. JSON scanning is hand-written; no other third-party code. |
| Target OS | Ubuntu 22.04 / Debian |

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++-12 libssl-dev libboost-all-dev zlib1g-dev
```

## Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-12
cmake --build build --parallel
```

`cmake -B build && cmake --build build` also works (the default build type is Release).
Warning flags are set in `CMakeLists.txt` (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`); the build has zero
warnings with GCC 12 and Clang 15. Sanitizer build: `-DBINANCE_SANITIZE=ON` (AddressSanitizer + UBSan).

## Run

```bash
# live capture, one symbol, spot, 2 minutes
./build/binance_capture --venue spot --symbols BTCUSDT --output-dir ./output --duration 120

# several symbols on one connection
./build/binance_capture --venue spot --symbols BTCUSDT,ETHUSDT --output-dir ./output

# USD-M futures
./build/binance_capture --venue usdm --symbols BTCUSDT --output-dir ./output

# replay: rebuild the order-book CSV from a saved market-data CSV (NO network calls)
./build/binance_capture --replay output/market_data_spot_BTCUSDT_<date>.csv -o output/replay_orderbook.csv
```

| Flag | Meaning |
|---|---|
| `--venue spot\|usdm` | Venue (lowercase). |
| `--symbols A,B,...` | Comma-separated, uppercase, no spaces. At most 20 symbols (kMaxSymbols = 20; 20 symbols = 60 streams, within Binance limit of 1024 streams per connection). |
| `--output-dir DIR` | Live capture: output directory (created if missing). |
| `--duration SEC` | Stop after SEC seconds. Without it, runs until SIGINT/SIGTERM. |
| `--replay FILE` | Replay mode. Reads only `FILE`; opens no sockets. Venue and symbol are taken from the rows. |
| `-o FILE` | Replay only: path of the order-book CSV to write. Must not be the input file. |

Exit code is 0 on a normal stop (duration reached, SIGINT or SIGTERM).

## Output files

Per symbol and run:

```
output/market_data_<venue>_<SYMBOL>_<UTC-date>.csv              # Deliverable A (audit log)
output/market_data_<venue>_<SYMBOL>_<UTC-date>_orderbook.csv    # Deliverable B (top-5 snapshots)
```

`<UTC-date>` is the UTC date at process start. A run that crosses UTC midnight keeps writing to the same files.
Both files are UTF-8 with `\n` line endings. Fields that contain a comma, quote or newline are double-quoted and
internal `"` is written as `""` (RFC 4180 quoting; the only line-ending deviation is LF instead of CRLF).

### Market-data CSV

Header (exact): `recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json`

| Column | Value |
|---|---|
| `recv_tsec`, `recv_tnsec` | Receive wall-clock time (see *Time policy*). `recv_tnsec` is in `[0, 999999999]`. |
| `venue` | `spot` or `usdm` |
| `stream_kind` | `depth_diff`, `depth5` or `trade` |
| `shard_id` | Always `0` (one connection carries all streams). |
| `conn_epoch` | Starts at `1` (incremented on connect); increases by 1 on every reconnect. |
| `conn_seq` | Per-connection message counter; restarts when `conn_epoch` increases. |
| `symbol` | Uppercase, taken from the envelope's `"stream"` name (spot `depth5` payloads carry no symbol). |
| `payload_json` | The inner `data` object, exactly as received (already minified by Binance), CSV-quoted. |

One row is written per inbound data message, in processing order, including messages the book later drops
(stale, unsynced) and trades. Acks and other non-envelope messages are counted, not written.

### Order-book CSV

Header (exact, 26 columns after the header on every row):

```
tsec,tnsec,seqNo,id,type,side,bid0,bid1,bid2,bid3,bid4,bid_size0,bid_size1,bid_size2,bid_size3,bid_size4,ask0,ask1,ask2,ask3,ask4,ask_size0,ask_size1,ask_size2,ask_size3,ask_size4
```

| Column | Meaning |
|---|---|
| `tsec`, `tnsec` | Copied from the `recv_tsec`/`recv_tnsec` of the market-data row that caused this row. |
| `seqNo` | Per-symbol row counter: starts at 1, +1 per emitted row, continues across reconnects. |
| `id` | Zero-based index of the symbol in `--symbols` (stable for the run). |
| `type` | `D` = applied depth diff, `P` = depth5 (seed, refresh, or stale refresh). |
| `side` | For `D`: `B` if only bids changed in the message, `S` if only asks, `N` if both or neither. For `P`: `N`. |
| `bidN`, `askN` | Prices, best first, scaled integers (see *Scaling*). |
| `bid_sizeN`, `ask_sizeN` | Quantities, same scaling. |

`0,0` in a price/size slot means "no verified level here" (absent or not known; see *Order book design*).

**When a row is written**

| Event | Row? |
|---|---|
| Diff applied | Yes (`D`), even if the top 5 did not change |
| Depth5 that seeds or reconciles the book | Yes (`P`) |
| Depth5 older than the book's last applied id | Yes (`P`), from the unchanged book |
| Stale diff (`u <= last_u`) | No |
| Diff or trade while unsynced | No |
| Trade | No (trades never change the diff-based book) |
| Malformed message | No (counted) |

So every applied diff and every well-formed depth5 produces exactly one row.

## Scaling

Binance sends decimals as strings. Prices and quantities are both parsed by a hand-written decimal parser into
`int64` ticks at a fixed scale of **10^8** (`"97234.5"` -> `9723450000000`, `"0.012"` -> `1200000`). No `float` or
`double` is used on the path from JSON text to CSV integers. A value with more than 8 decimals, a non-digit
character, or an overflow is rejected: the message is not applied, `decimal_rejects` is incremented and a warning
is logged. Values are never silently truncated.

## Time policy

Recommended policy: `std::chrono::system_clock::now()` is read immediately after a complete WebSocket message
has been read (before parsing) and split into `recv_tsec` / `recv_tnsec` (nanoseconds). The order-book row copies
these values, which is what makes replay reproducible: the processor never reads a clock. Wall-clock time can step
backwards (NTP); such events are counted in `ts_regress` and not corrected.

## Order book design

The book is built from WebSocket data only. There is no REST snapshot, so the book never claims to know a level
it has not been shown.

- **Structure:** per side, a fixed array of `kBookCapacity = 20` levels, best first (bids descending, asks
  ascending), plus a certainty `edge` price.
- **Invariant:** the array holds *every* level at or better than `edge` (every level on the side if there is no
  edge), each with its current quantity. Levels worse than `edge` are not stored, and diffs for them are ignored.
- **Seed / refresh (`depth5`):** replaces the whole book with the snapshot's levels. If a side lists 5 levels, its
  `edge` is the 5th level's price; a side listing fewer than 5 is fully known and has no edge.
- **Diff (`depth`):** quantity `0` removes a level, otherwise the level is inserted or updated. If an insert pushes
  the array past its capacity, the worst level is dropped and the `edge` tightens to the new worst level.
- **Output:** the best 5 stored levels per side; slots with no verified level are `0,0`.
- **Cost:** a level update touches at most 20 array slots, with no heap allocation and no tree rebalancing; the
  top-5 read is O(5).

**Trade-off vs a full-map book.** A full `std::map` keeps deeper levels and promotes the 6th level when a top level
is cancelled. Without a REST snapshot that deeper level may never have been observed, or may be stale (a diff
that arrived before the latest depth5 is dropped), so a full map can print a level that does not exist. This design
prints `0,0` instead until the next depth5 refreshes the book. The cost is some padded rows (measured: 0 of 623 rows in the 60-second sample run had a padded bid or ask level; in
other 60-second captures up to 2.8% of rows had a padded ask level); the benefit is that every printed level is verified. REST snapshot resync would
recover the true deeper book (see *Known limitations*).

## Stream semantics and sync rules

- `depth5` has **replace** semantics; `depth` (diff) has **delta** semantics; `trade` does not touch the book.
- Venue differences handled: spot `depth5` is `{lastUpdateId,bids,asks}`; USD-M `depth5` and diffs carry
  `U`, `u`, `pu`, `b`, `a`. The symbol always comes from the envelope's stream name.
- **State per symbol:** `UNSYNCED` or `SYNCED`, plus `last_u` (last applied/accepted update id).
- **UNSYNCED** (start, new `conn_epoch`, after a gap): diffs and trades are logged to the market-data CSV but are
  not applied and emit no book row. The next `depth5` seeds the book, sets `last_u` to its update id (`lastUpdateId`
  on spot, `u` on USD-M) and becomes `SYNCED`. Diffs received before that are dropped, not buffered.
- **SYNCED, diff:** a diff with `u <= last_u` is stale and ignored. Otherwise it must bridge the book
  (`U <= last_u + 1`; USD-M also accepts `pu <= last_u`) **and** continue the diff chain: `U` equals the previous
  diff's `u + 1` (USD-M: `pu` equals the previous diff's `u`). The chain starts at the first diff after a seed, is
  advanced by stale diffs too, and is not changed by a depth5 refresh, so diffs lost before a depth5 jump are still
  detected. A failed check is a **gap**: the book is cleared and the symbol returns to `UNSYNCED`.
- **SYNCED, depth5:** if its id is older than `last_u` it is not applied (a `P` row is still written from the
  unchanged book). Otherwise the book is replaced by the snapshot and `last_u` is set to the snapshot id.
- **New `conn_epoch`:** treated as a hard disconnect: the book is cleared and the symbol returns to `UNSYNCED`.
  `seqNo` is not reset.

## Reconnect and gap handling

- A read error, close, or idle timeout ends the connection. The session reconnects with exponential backoff
  (constants in `src/websocket_session.cpp`), increments `conn_epoch` and restarts `conn_seq`. Binance's periodic
  24-hour disconnect is handled the same way. The number of reconnects is reported in the metrics summary.
- After a reconnect or a gap no order-book rows are written until the next `depth5` seeds the book, so the CSV
  never mixes pre- and post-gap state.
- Parse errors, decimal rejects and gaps are counted and logged (rate-limited), never silent.

## Design note: threading and I/O

- **One thread.** A synchronous Beast read loop (TLS with SNI and certificate + hostname verification enabled)
  reads a message, timestamps it, writes the market-data row, then calls the shared event processor, which updates
  the book and appends the order-book row. Message rates here are tens per second per symbol, so a second thread
  would add synchronization without benefit.
- **Same code path in live and replay:** `EventProcessor::process` is the only place payloads are parsed and the
  book is updated; replay feeds it rows from the market-data CSV.
- **Buffered writes:** rows are formatted into reused buffers and written in chunks; buffers are flushed at row
  boundaries, periodically, and on shutdown, so a stop never leaves half a row. Disk back-pressure blocks the
  reader, which is acceptable at this rate.
- **Shutdown:** SIGINT/SIGTERM set an atomic flag; the read loop uses a short timeout so it notices the flag,
  closes the WebSocket, flushes and closes files and prints the metrics summary.
- **Memory limits:** bounded read buffer, bounded symbol count (`kMaxSymbols = 20`) and URL length.

## Metrics

Printed every 10 seconds and at exit: messages per kind, `parse_errors`, `decimal_rejects`, `applied_diffs`,
`stale_diffs`, `stale_depth5`, `gaps`, `seeds`, `reconciles`, `reconnects`, `epoch_resets`, dropped unsynced diffs/
trades, rows written (market data and order book), padded-row counts (`bid_padded`, `ask_padded`), invariant
violations (book order/crossing, timestamp regressions, `conn_seq`) and depth5 comparisons.

`reconnects` counts WebSocket reconnections; `epoch_resets` counts book resets caused by a new epoch (only when the
symbol was synced at that moment), so the two can differ.

Consistency identities that hold for a healthy run:
`market_data rows == total messages`, and
`order-book rows == applied_diffs + seeds + reconciles + stale_depth5`.

`d5_comparable` counts depth5 messages whose id equals the book's last applied id (the only case where the two
streams describe the same state); `d5_mismatch` counts those where the top 5 differed; `d5_ahead` counts depth5
messages newer than the book. Because the streams are sampled at different instants, comparable messages are rare
(43 of 579 reconciled depth5 messages, 7.4%, in the 60-second sample run). In all 43 the diff-maintained top 5 equalled
the snapshot exactly (`d5_mismatch=0`). That is a real but narrow check (one diff applied on top of the previous
snapshot), **not** a long-run accuracy proof.

**Stale diffs are the common case.** In the sample run 536 of 579 diffs (92.6%) were stale. Comparing each diff's `u`
with the most recent depth5 `lastUpdateId` in the saved market-data CSV: all 536 stale diffs had `u` exactly equal
to it, none were older, and the remaining 43 were newer (equal to `applied_diffs`). This is consistent with both
streams being emitted on the same 100 ms tick, with the depth5 message processed first, so the diff adds nothing the
snapshot did not already contain. As a result the top 5 in that run was mostly set by depth5 refreshes (consistent
with no padded levels appearing), and the diff path (quantity-0 removal, edge handling, chain checks) is covered by
the unit and fixture tests and by those 43 applied diffs, not by the volume of the live sample.

## Testing

```bash
cmake --build build --target unit_tests
./build/tests/unit_tests          # decimals, CSV, order book, sync state machine, row contents, JSON envelope
tests/run_golden_test.sh ./build/binance_capture      # replay of a committed fixture vs expected CSV
```

Determinism check (live vs replay must be byte-identical):

```bash
./build/binance_capture --replay output/market_data_spot_BTCUSDT_2026-10-11.csv -o output/replay_orderbook.csv
cmp output/market_data_spot_BTCUSDT_2026-10-11_orderbook.csv output/replay_orderbook.csv && echo IDENTICAL
```

Schema check: `awk -F',' 'NR==2{print NF}' output/*_orderbook.csv` prints `26`.
Sanitizers: build with `-DBINANCE_SANITIZE=ON`, then run the unit tests and a replay.

## Sample run

`samples/` contains a 60-second spot BTCUSDT capture taken on 2026-10-11:
`market_data_spot_BTCUSDT_2026-10-11.csv` and the matching `market_data_spot_BTCUSDT_2026-10-11_orderbook.csv` from the
same window (regenerated from the final binary; replaying the first file reproduces the second byte-for-byte).

Message mix in that run: 579 `depth_diff`, 580 `depth5`, 1653 `trade` (2812 messages); 623 order-book rows
(43 applied diffs, 1 seed, 579 reconciles); 536 stale diffs; 0 gaps, 0 parse errors, 0 decimal rejects, 0 invariant
violations, 0 reconnects.

## Measurements

| Metric | Value |
|---|---|
| depth5 interval, spot / USD-M (measured on 2026-10-11: 60 s spot, 15 s USD-M) | 100.2 ms / 107.7 ms |
| Replay throughput | ~450,000–520,000 msgs/s (measured on 98,420 msgs / 44.8 MB file: 0.19–0.22 s wall time, 5.2 MB RSS on Apple Silicon Mac / Docker Ubuntu 22.04 aarch64 native) |
| Heap allocations per message in replay | 0 (none observed; measured via Valgrind memcheck two-file differential on 2,812 vs 98,420 messages: 36 allocs in both, Δ = 0) |
| Order-book rows with a padded 5th level (sample run) | bid 0%, ask 0% (0 of 623 rows) |

## Known limitations

- Levels deeper than the 5th are only known after a snapshot or diff has shown them; after a top-level cancel
  the 5th slot can be `0,0` until the next depth5.
- No REST snapshot resync. It would need an extra documented `stream_kind` so replay stays reproducible from the
  market-data CSV alone.
- Single connection (`shard_id` is always 0); no sharding.
- Wall-clock timestamps can step backwards (counted, not corrected).

## How to Submit on GitHub

```bash
git init
git add .
git commit -m "Initial submission: Binance WebSocket capture + LOB"
git remote add origin https://github.com/<your-username>/binance-lob-capture.git
git branch -M main
git push -u origin main
git tag -a v1.0.0 -m "Submission v1.0.0"
git push origin v1.0.0
```

`.gitignore` excludes `build/`, `*.csv`, `*.o`, `*.a`, `*.so` and `.env`, with the exception `!samples/*.csv` (and
`!tests/fixture_*.csv`) so the sample run and test fixtures are committed. Never commit credentials or private keys;
configuration, if ever needed, goes in environment variables or a `.env.example` template with no real values.