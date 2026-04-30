# HFT-Style Order Book Engine

A low-latency order book and matching engine built as a learning project in optimizing modern C++23 applications.

![Demo gif](https://github.com/rastringer/order_book/blob/main/docs/order_book_demo.gif)

## What's an Order Book?

An order book is like a digital waiting room for buyers and sellers. It's the core of stock or crypto exchanges and trading platforms. 

Imagine you're at a flea market and a vendor is selling vintage hats. Buyers are willing to pay $100, $95, or $90, while the seller is asking $120 (to the $100 bidder), $125 (to the $95 bidder) and $130 (to the $90 bidder). The order book shows all these buy and sell offers in one place. 

### Where C++ Comes In

The order book collects orders and matches them, so when a buyer's price equals a seller's, a trade occurs. This typically happens on following a first-in-first-out (FIFO) pattern, so the best prices are matched before earlier orders. 

Obviously, this is a process market participants would like to happen quickly, and reliably without errors.

C++ provides deterministic memory management and zero-overhead abstractions, so we can execute trades in nanoseconds without the garbage collection pauses or runtime overhead found in higher-level languages. 

Optimization is key for trading platforms (and in particular high-frequency trading) since microseconds can translate to profit. A faster engine captures price discrepancies before competitors, so the raw speed quickly becomes financial advantage.

## Features
- Limit & Market orders
- FIFO price-time priority
- Partial fills
- Order cancellation
- Best bid/ask queries

The FIFO price-time priority is `O(log n)` via `std::map<Price, std::deque<Order>>`. Exploration for V2 will focus on improvements from using `std::array` or `std::vector`. Bids are ordered by `std::greater`, `O(1)` order cancellation is via an `unordered_map` ID lookup. 

Fixed-width integer prices and quantities throughout eliminate floating-point error on the hot path adn separate the order book container from the matching policy to keep the matching loop unit-testable.

## Building
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./order_book_demo
./order_book_tests
```

## Benchmarking

To run the benchmarks found in `bench/`, run:
```
./build/order_book_bench
```

## Roadmap

* Phase 1: Core functionality
* Phase 2: Memory pools & custom hash maps
* Phase 3: Lock-free concurrency
* Phase 4: Benchmarking & profiling

## Architecture

The engine follows a classic event-driven design:
```text
┌─────────────────────────────────────────────────────────────────────┐
│                         ORDER BOOK ENGINE                           │
└─────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌───────────────────────┐     ┌───────────────────────┐
│   Incoming Orders     │────▶│   Matching Engine     │
│  (Limit / Market)     │     │                       │
└───────────────────────┘     │  ┌─────────────────┐  │
                              │  │  Price-Time     │  │
                              │  │  Priority Logic │  │
                              │  └────────┬────────┘  │
                              │           │           │
                              │           ▼           │
                              │  ┌─────────────────┐  │
                              │  │   Order Book    │  │
                              │  │  ┌───────────┐  │  │
                              │  │  │  Bids     │  │  │
                              │  │  │ (Highest) │  │  │
                              │  │  └───────────┘  │  │
                              │  │  ┌───────────┐  │  │
                              │  │  │  Asks     │  │  │
                              │  │  │ (Lowest)  │  │  │
                              │  │  └───────────┘  │  │
                              │  └────────┬────────┘  │
                              │           │           │
                              └───────────┼───────────┘
                                          │
                                          ▼
                              ┌───────────────────────┐
                              │     Trade Execution   │
                              │  (Callback / Event)   │
                              └───────────────────────┘
                                          │
                                          ▼
                              ┌───────────────────────┐
                              │   Market Data Feed    │
                              │  (Best Bid/Ask, Depth)│
                              └───────────────────────┘
```

## Data flow


```text
Order Input
    │
    ▼
┌──────────────┐
│  Validation  │
└──────┬───────┘
       │
       ▼
┌──────────────┐      ┌──────────────┐
│  Match?      │─────▶│  Execute     │
│  (Bid ≥ Ask) │      │  Trade       │
└──────┬───────┘      └──────┬───────┘
       │                     │
       ▼                     ▼
┌──────────────┐      ┌──────────────┐
│  Rest in     │      │  Emit Trade  │
│  Book        │      │  Event       │
└──────────────┘      └──────────────┘
```

## Project Layout

- `include/` — public headers (`types.h`, `utils.h`)
- `src/` — implementation (`main.cpp`, `order_book.{h,cpp}`, `matching_engine.{h,cpp}`)
- `test/` — unit tests (placeholder)
- `CMakeLists.txt` — top-level build config
- `build.sh` — convenience script that configures, builds, and runs the demo
- `build/` — out-of-source build directory (gitignored)
- `bench/` - benchmark tests

## Benchmark
- Source: `bench/bench.cpp`, target: `order_book_bench` (built by default; disable with `-DBUILD_BENCH=OFF`).
- Generates a deterministic synthetic workload (default 1M hot orders, 5K seeded liquidity, mix of 5% market / 20% crossing limit / 75% passive limit), times the matching loop with `std::chrono::steady_clock`, and reports throughput plus p50/p90/p95/p99/p99.9/p99.99 per-order latency.
- Usage: `./build/order_book_bench [hot_orders] [seed_orders] [rng_seed] [--no-per-op]`.
- Per-op clock sampling adds ~20-30ns/call; use `--no-per-op` for an unbiased throughput number.

## Performance

Current `perf` stats (running the engine on a laptop) indicate the engine procees 1 million orders in 0.266 seconds. CPU utlization is at 98%. We will work on improving the branch mispredictions and the Instructions Per Cycle (IPC).

```
Performance counter stats for './build/order_book_bench':

260,914,232      task-clock         #    0.980 CPUs utilized             
6      context-switches             #   22.996 /sec                      
3      cpu-migrations               #   11.498 /sec                      
32,728      page-faults             #  125.436 K/sec                     
1,528,288,428      instructions     #    1.20  insn per cycle            
                                    #    0.30  stalled cycles per insn   
1,273,020,939      cycles           #    4.879 GHz                       
461,223,839      stalled-cycles-frontend    #   36.23% frontend cycles idle      
285,345,873      branches           #    1.094 G/sec                     
11,474,158      branch-misses       #    4.02% of all branches           

0.266134713 seconds time elapsed

0.206038000 seconds user
0.060011000 seconds sys
```