// Order book benchmark harness

// Generates a deterministic synthetic workload (seed->hot path), times the hot path with std::chrono::steady_clock and reports throughput plus per-order latency percentiles.

// Usage: 
//     ./order_book_bench [hot_orders] [seed_orders] [rng_seed] [--no-per-op]
// Defaults: 1,000,000 hot, 5,000 seed, RNG seed 42, per-op timing on.

// Notes: 
//     * Per-op timing adds ~20-30ns of clock overhead per call on x86 Linux. Run with --no-per-op for a pure-throughput measurement.
//     * The trade callback only increments counters so I/O does not pollute the hot loop.

#include "matching_engine.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace order_book;
using Clock = std::chrono::steady_clock;

namespace {

constexpr Price MID = 10'000;

struct Workload {
    std::vector<Order> seed; // pre-seeded liquidity (untimed)
    std::vector<Order> hot; // measured workload
};

Workload generate_workload(std::size_t hot_n, std::size_t seed_n, std::uint32_t seed_value) {
    // Mersenne Twister 18837 pseudo random generator
    // of 32 but numbers https://cplusplus.com/reference/random/mt19937/
    std::mt19937 rng(seed_value);
    std::uniform_int_distribution<int> qty_dist(1, 100);
    std::uniform_int_distribution<int> offset_dist(1, 50); // dist from MID
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> action_dist(0, 99);

    auto t0 = Clock::now();
    OrderId id = 1;

    Workload w;
    // .reserve() to pre-allocate memory and avoid reallocations during the generation loop
    w.seed.reserve(seed_n);
    for (std::size_t i = 0; i < seed_n; ++i) {
        Side s = side_dist(rng) ? Side::Sell : Side::Buy;
        Price p = (s == Side::Buy) ? MID - offset_dist(rng) : MID + offset_dist(rng);
        w.seed.push_back(Order{
            .id = id++, .side = s, .price = p,
            .quantity = qty_dist(rng), .filled_quantity = 0,
            .type = OrderType::Limit, .arrival_time = t0,
        });
    }

    // Hot mix: 5% market, 20% crossing limit, 75% passive limit.
    w.hot.reserve(hot_n);
    for (std::size_t i = 0; i < hot_n; ++i) {
        int a = action_dist(rng);
        Side s = side_dist(rng) ? Side::Sell : Side::Buy;
        OrderType type;
        Price p;
        if (a < 5) {
            type = OrderType::Market;
            p = 0;
        } else if (a < 25) {
            // Aggressive: buy reaches above MID, sell reaches below MID.
            type = OrderType::Limit;
            p = (s == Side::Buy) ? MID + offset_dist(rng) : MID - offset_dist(rng);
        } else {
            // Passive: rests on its own side of the book.
            type = OrderType::Limit;
            p = (s == Side::Buy) ? MID - offset_dist(rng) : MID + offset_dist(rng);
        }
        w.hot.push_back(Order{
            .id = id++, .side = s, .price = p,
            .quantity = qty_dist(rng), .filled_quantity = 0,
            .type = type, .arrival_time = t0,
        });
    }
    return w;
}

double pct(const std::vector<std::int64_t>& sorted_ns, double p) {
    if (sorted_ns.empty()) return 0.0;
    auto n = sorted_ns.size();
    auto k = static_cast<std::size_t>(p / 100.0 * static_cast<double>(n - 1));
    if (k >= n) k = n - 1;
    return static_cast<double>(sorted_ns[k]);
}

} // namespace

 int main(int argc, char* argv[]) {
    std::size_t hot_n = 1'000'000;
    std::size_t seed_n = 5'000;
    std::uint32_t rng_seed = 42;
    bool capture_per_op = true;

    int positional = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            std::cout << "Usage: " << argv[0]
                      << " [hot_orders] [seed_orders] [rng_seed] [--no-per-op]\n";
            
            return 0;
    }

    switch (positional++) {
            // std::stoull converts string to unsigned
            // long long
            case 0: hot_n = std::stoull(a); break;
            case 1: seed_n = std::stoull(a); break;
            case 2: rng_seed = static_cast<std::uint32_t>(std::stoul(a)); break;
            default: break;
        }
    }

    std::cout << "=== Order Book Benchmark ===\n";
    std::cout << std::format("Workload         : {} hot orders, {} seed orders, rng_seed={}\n",
                              hot_n, seed_n, rng_seed);
    std::cout << std::format("Per-op timing    : {}\n", capture_per_op ? "ON" : "OFF");
    std::cout << std::format("Compiler         : {} (built {} {})\n",
#ifdef __clang__
                              "clang",
#elif defined(__GNUC__)
                              "gcc",
#else
                              "unknown",
#endif
                              __DATE__, __TIME__);

    Workload w = generate_workload(hot_n, seed_n, rng_seed);

    std::uint64_t trade_count = 0;
    std::uint64_t volume = 0;
    MatchingEngine engine([&](const Trade& t) {
        ++trade_count;
        volume += static_cast<std::uint64_t>(t.quantity);
    });

    // Pre-seed liquidity (untimed).
    for (auto& o : w.seed) engine.process_order(o);

    std::vector<std::int64_t> per_op_ns;
    if (capture_per_op) per_op_ns.reserve(hot_n);

    // ----- timed hot loop -----
    auto t_start = Clock::now();
    if (capture_per_op) {
        for (auto& o : w.hot) {
            auto a = Clock::now();
            engine.process_order(o);
            auto b = Clock::now();
            per_op_ns.push_back(
                std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count());
        }
    } else {
        for (auto& o : w.hot) engine.process_order(o);
    }
    auto t_end = Clock::now();
    // ---------------------------

    auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
    double secs = static_cast<double>(total_ns) / 1e9;
    double throughput = static_cast<double>(hot_n) / secs;
    double avg_ns = static_cast<double>(total_ns) / static_cast<double>(hot_n);

    std::cout << "\n--- Throughput ---\n";
    std::cout << std::format("Total wall time  : {:.3f} s\n", secs);
    std::cout << std::format("Throughput       : {:.2f} M orders/sec\n", throughput / 1e6);
    std::cout << std::format("Avg per order    : {:.0f} ns\n", avg_ns);
    std::cout << std::format("Trades produced  : {}\n", trade_count);
    std::cout << std::format("Volume matched   : {}\n", volume);

    if (capture_per_op) {
        std::sort(per_op_ns.begin(), per_op_ns.end());
        std::cout << "\n--- Per-order latency (ns) ---\n";
        std::cout << std::format("min     : {:>9.0f}\n", pct(per_op_ns,   0.0));
        std::cout << std::format("p50     : {:>9.0f}\n", pct(per_op_ns,  50.0));
        std::cout << std::format("p90     : {:>9.0f}\n", pct(per_op_ns,  90.0));
        std::cout << std::format("p95     : {:>9.0f}\n", pct(per_op_ns,  95.0));
        std::cout << std::format("p99     : {:>9.0f}\n", pct(per_op_ns,  99.0));
        std::cout << std::format("p99.9   : {:>9.0f}\n", pct(per_op_ns,  99.9));
        std::cout << std::format("p99.99  : {:>9.0f}\n", pct(per_op_ns, 99.99));
        std::cout << std::format("max     : {:>9.0f}\n", pct(per_op_ns, 100.0));
        std::cout << "\nNote: per-op steady_clock sampling adds ~20-30 ns of overhead per call.\n";
        std::cout << "      Use --no-per-op for an unbiased throughput-only measurement.\n";
    }

    return 0;
}
