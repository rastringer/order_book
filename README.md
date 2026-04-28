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

## Building
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./order_book_demo
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