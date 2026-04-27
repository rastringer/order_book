#include "matching_engine.h"
#include <iostream>
#include <iomanip>
#include <format>

using namespace order_book;

int main() {
    std::vector<Trade> trades;

        // Current time to use for all orders
        auto now = std::chrono::steady_clock::now();

        MatchingEngine engine([&trades](const Trade& trade) {
        trades.push_back(trade);
        // C++20: std::format is type-safe and cleaner
        std::cout << std::format(
            "[TRADE] Aggressor: {} | Resting: {} | Price: {} | Qty: {} | Side: {}\n",
            trade.aggressor_id,
            trade.resting_id,
            trade.price,
            trade.quantity,
            trade.aggressor_side == Side::Buy ? "BUY" : "SELL"
        );
    });

    std::cout << std::format("\n--- Current Book State ---\n");
    std::cout << std::format("Best Bid: {}\n", engine.get_best_bid().value_or(-1));
    std::cout << std::format("Best Ask: {}\n", engine.get_best_ask().value_or(-1));

    std::cout << std::format("\n--- Aggressive Buy Order (Price: 101, Qty: 150) ---\n");
    Order aggressive_buy{.id = 0, .side = Side::Buy, .price = 101, .quantity = 150, .filled_quantity = 0, .type = OrderType::Limit, .arrival_time = now};
    engine.process_order(aggressive_buy);

    std::cout << std::format("\n--- Final Book State ---\n");
    std::cout << std::format("Best Bid: {}\n", engine.get_best_bid().value_or(-1));
    std::cout << std::format("Best Ask: {}\n", engine.get_best_ask().value_or(-1));

    std::cout << std::format("\n=== Demo Complete ===\n");
    std::cout << std::format("Total Trades Executed: {}\n", trades.size());

    return 0;
}