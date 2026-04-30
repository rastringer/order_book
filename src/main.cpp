#include "matching_engine.h"

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace order_book;

namespace {

// ---------------------------------------------------------------------------
// Snapshot: state of the book + trades produced *during* the current event.
// One snapshot captured per call to step()/cancel_step(), plus an initial
// empty-book snapshot. The full series is written out as JSON for the GIF
// generator (scripts/generate_book_gif.py).
// ---------------------------------------------------------------------------
struct Snapshot {
    std::string label;
    std::vector<std::pair<Price, Quantity>> bids;
    std::vector<std::pair<Price, Quantity>> asks;
    std::vector<Trade> new_trades;
};

void print_book(const MatchingEngine& engine, const std::string& label) {
    std::cout << std::format("\n--- {} ---\n", label);
    if (auto bid = engine.get_best_bid()) {
        std::cout << std::format("Best Bid: {}\n", *bid);
    } else {
        std::cout << "Best Bid: (none)\n";
    }
    if (auto ask = engine.get_best_ask()) {
        std::cout << std::format("Best Ask: {}\n", *ask);
    } else {
        std::cout << "Best Ask: (none)\n";
    }
}

Order make_order(OrderId id, Side side, Price price, Quantity qty,
                 OrderType type = OrderType::Limit) {
    return Order{
        .id = id,
        .side = side,
        .price = price,
        .quantity = qty,
        .filled_quantity = 0,
        .type = type,
        .arrival_time = std::chrono::steady_clock::now()
    };
}

void write_snapshots(const std::vector<Snapshot>& snaps, const std::string& path) {
    std::ofstream f(path);
    if (!f) {
        std::cerr << "warning: could not open " << path << " for snapshot output\n";
        return;
    }
    f << "{\n  \"frames\": [\n";
    for (size_t i = 0; i < snaps.size(); ++i) {
        const auto& s = snaps[i];
        f << "    {\n";
        f << "      \"step\": " << i << ",\n";
        f << "      \"label\": \"" << s.label << "\",\n";

        f << "      \"bids\": [";
        for (size_t j = 0; j < s.bids.size(); ++j) {
            f << "{\"price\": " << s.bids[j].first
              << ", \"qty\": " << s.bids[j].second << "}";
            if (j + 1 < s.bids.size()) f << ", ";
        }
        f << "],\n";

        f << "      \"asks\": [";
        for (size_t j = 0; j < s.asks.size(); ++j) {
            f << "{\"price\": " << s.asks[j].first
              << ", \"qty\": " << s.asks[j].second << "}";
            if (j + 1 < s.asks.size()) f << ", ";
        }
        f << "],\n";

        f << "      \"trades\": [";
        for (size_t j = 0; j < s.new_trades.size(); ++j) {
            const auto& t = s.new_trades[j];
            f << "{\"aggressor\": " << t.aggressor_id
              << ", \"resting\": " << t.resting_id
              << ", \"price\": " << t.price
              << ", \"qty\": " << t.quantity
              << ", \"side\": \""
              << (t.aggressor_side == Side::Buy ? "BUY" : "SELL") << "\"}";
            if (j + 1 < s.new_trades.size()) f << ", ";
        }
        f << "]\n";

        f << "    }";
        if (i + 1 < snaps.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
}

} // namespace

int main(int argc, char* argv[]) {
    // Optional CLI arg: path for the JSON snapshot dump.
    const std::string snapshot_path = (argc > 1) ? argv[1] : "snapshots.json";

    std::vector<Trade> trades;
    std::vector<Snapshot> snapshots;

    MatchingEngine engine([&trades](const Trade& trade) {
        trades.push_back(trade);
        std::cout << std::format(
            "  [TRADE] aggressor #{} vs resting #{} @ {} x {} ({})\n",
            trade.aggressor_id,
            trade.resting_id,
            trade.price,
            trade.quantity,
            trade.aggressor_side == Side::Buy ? "BUY" : "SELL"
        );
    });

    auto snap = [&](const std::string& label, size_t trades_before) {
        Snapshot s;
        s.label = label;
        s.bids = engine.get_bid_depth();
        s.asks = engine.get_ask_depth();
        for (size_t i = trades_before; i < trades.size(); ++i) {
            s.new_trades.push_back(trades[i]);
        }
        snapshots.push_back(std::move(s));
    };

    auto step = [&](Order& o, const std::string& label) {
        size_t before = trades.size();
        engine.process_order(o);
        snap(label, before);
    };

    auto cancel_step = [&](OrderId id, const std::string& label) {
        size_t before = trades.size();
        bool ok = engine.cancel_order(id);
        snap(label, before);
        return ok;
    };

    // Initial empty book.
    snap("Empty book", 0);

    // ---------------------------------------------------------------
    // 1. Seed resting limits on both sides of the book.
    // ---------------------------------------------------------------
    std::cout << "=== 1. Seed resting limit orders ===\n";
    Order o1 = make_order(1, Side::Buy,   99,  50);
    Order o2 = make_order(2, Side::Buy,   98, 100);
    Order o3 = make_order(3, Side::Sell, 101,  75);
    Order o4 = make_order(4, Side::Sell, 102, 200);
    Order o5 = make_order(5, Side::Sell, 101,  25);
    step(o1, "Limit BUY  @ 99 x  50  (#1)");
    step(o2, "Limit BUY  @ 98 x 100  (#2)");
    step(o3, "Limit SELL @101 x  75  (#3)");
    step(o4, "Limit SELL @102 x 200  (#4)");
    step(o5, "Limit SELL @101 x  25  (#5)");
    print_book(engine, "Book after seeding");

    // ---------------------------------------------------------------
    // 2. Aggressive limit BUY @101 x 60 — partial fill of #3.
    // ---------------------------------------------------------------
    std::cout << "\n=== 2. Aggressive limit BUY @101 x60 (partial fill of #3) ===\n";
    Order aggr1 = make_order(6, Side::Buy, 101, 60);
    step(aggr1, "Aggressive BUY @101 x  60  (#6)");
    print_book(engine, "Book after #6");

    // ---------------------------------------------------------------
    // 3. Market SELL x 80 — walks bids, clears #1 and partials #2.
    // ---------------------------------------------------------------
    std::cout << "\n=== 3. Market SELL x80 (walks bids: clears #1, partial #2) ===\n";
    Order aggr2 = make_order(7, Side::Sell, 0, 80, OrderType::Market);
    step(aggr2, "Market SELL x  80  (#7)");
    print_book(engine, "Book after #7");

    // ---------------------------------------------------------------
    // 4. Limit BUY @101 x 30 — price-time priority demo.
    // ---------------------------------------------------------------
    std::cout << "\n=== 4. Limit BUY @101 x30 (price-time: finishes #3, then takes #5) ===\n";
    Order aggr3 = make_order(8, Side::Buy, 101, 30);
    step(aggr3, "Limit BUY @101 x  30  (#8) — price-time priority");
    print_book(engine, "Book after #8");

    // ---------------------------------------------------------------
    // 5. Non-crossing limit BUY @100 x 40 — rests on the book.
    // ---------------------------------------------------------------
    std::cout << "\n=== 5. Limit BUY @100 x40 (does not cross — rests) ===\n";
    Order resting_buy = make_order(9, Side::Buy, 100, 40);
    step(resting_buy, "Limit BUY @100 x  40  (#9) — rests, no cross");
    print_book(engine, "Book after #9");

    // ---------------------------------------------------------------
    // 6. Cancel #4 — pull resting sell @102 x 200.
    // ---------------------------------------------------------------
    std::cout << "\n=== 6. Cancel resting order #4 (sell @102 x200) ===\n";
    bool cancelled = cancel_step(4, "Cancel #4 (sell @102 x200)");
    std::cout << std::format("Cancel #4: {}\n", cancelled ? "OK" : "FAILED");
    print_book(engine, "Book after cancelling #4");

    // ---------------------------------------------------------------
    // 7. Cancel of an unknown ID should fail gracefully.
    // ---------------------------------------------------------------
    std::cout << "\n=== 7. Cancel unknown order #999 ===\n";
    bool bad = engine.cancel_order(999);
    std::cout << std::format("Cancel #999: {}\n", bad ? "OK" : "FAILED (expected)");

    // ---------------------------------------------------------------
    // Summary
    // ---------------------------------------------------------------
    std::cout << std::format("\n=== Demo Complete ===\n");
    std::cout << std::format("Total trades executed: {}\n", trades.size());
    Quantity total_volume = 0;
    for (const auto& t : trades) total_volume += t.quantity;
    std::cout << std::format("Total volume traded:   {}\n", total_volume);

    write_snapshots(snapshots, snapshot_path);
    std::cout << std::format("Wrote {} snapshots to {}\n", snapshots.size(), snapshot_path);

    return 0;
}
