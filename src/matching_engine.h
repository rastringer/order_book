#pragma once

#include "types.h"
#include "order_book.h"
#include <functional>
#include <vector>

namespace order_book {

using TradeCallback = std::function<void(const Trade&)>;

class MatchingEngine {

public: 
    explicit MatchingEngine(TradeCallback on_trade);

    // Process incoming orders. Will modify order book and 
    // trigger trade callback
    void process_order(Order& order);

    // Expose book accessors for main.cpp
    std::optional<Price> get_best_bid() const { return book_.get_best_bid(); }
    std::optional<Price> get_best_ask() const { return book_.get_best_ask(); }


private: 
    OrderBook book_;
    TradeCallback on_trade;

    // Internal matching logic
    void match_buy(Order& order);
    void match_sell(Order& order);

    // Execute a single trade between two orders
    void execute_trade(Order& aggressor, Order& resting, 
        Price price, Quantity qty);
};
} // namespace order_book