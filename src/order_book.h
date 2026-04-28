#pragma once

#include "types.h"
#include <map>
#include <vector>
#include <memory>
#include <deque>
#include <unordered_map>
#include <optional>

namespace order_book {

class OrderBook {
public:
    std::optional<OrderId> add_order(Order order);
    bool cancel_order(OrderId id);
    std::optional<Price> get_best_bid() const;
    std::optional<Price> get_best_ask() const;
    std::vector<std::pair<Price, Quantity>> get_depth(int levels = 5) const;

    // Full per-side depth, aggregated by price level
    // Bids are returned highest-price-first, asks lowest-price-first
    std::vector<std::pair<Price, Quantity>> get_bid_depth() const;
    std::vector<std::pair<Price, Quantity>> get_ask_depth() const;

    // Helper Methods for Matching Engine
    bool asks_empty() const { return asks_map_.empty(); }
    bool bids_empty() const { return bids_map_.empty(); }
    
    // Return non-const iterators so we can modify the maps
    auto begin_asks() { return asks_map_.begin(); }
    auto begin_bids() { return bids_map_.begin(); }
    
    // Return non-const references to the deques
    std::deque<Order>& get_best_ask_orders() { return asks_map_.begin()->second; }
    std::deque<Order>& get_best_bid_orders() { return bids_map_.begin()->second; }
    
    // Erase methods
    void erase_ask(auto it) { asks_map_.erase(it); }
    void erase_bid(auto it) { bids_map_.erase(it); }
    
    void remove_order_from_lookup(OrderId id) { order_lookup_.erase(id); }

private:
    OrderId next_id_{1};
    std::map<Price, std::deque<Order>, std::greater<Price>> bids_map_;
    std::map<Price, std::deque<Order>> asks_map_;
    std::unordered_map<OrderId, std::pair<Price, Side>> order_lookup_;
};

} // namespace order_book