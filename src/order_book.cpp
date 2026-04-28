#include "order_book.h"
#include <stdexcept>
#include <ranges>

namespace order_book {

std::optional<OrderId> OrderBook::add_order(Order order) {    
    // Validate order
    if (order.quantity <= 0 || order.price <= 0) {
        return std::nullopt;
    }

    order.id = next_id_++;

    // Add to appropriate book
    if (order.side == Side::Buy) {
        bids_map_[order.price].push_back(order);
    } else {
        asks_map_[order.price].push_back(order);
    }

    // Store in lookup table for fast cancellation
    order_lookup_[order.id] = {order.price, order.side};

    return order.id;
}

bool OrderBook::cancel_order(OrderId id) {
    auto it = order_lookup_.find(id);
    if (it == order_lookup_.end()) {
        return false;
    }

    Price price = it->second.first;
    Side side = it->second.second;

    auto& level = (side == Side::Buy) ? bids_map_[price] : asks_map_[price];

    // Find, remove order (currently O(n) for initial deque setup)
    for (auto iter = level.begin(); iter != level.end(); ++iter) {
        if (iter->id == id) {
            level.erase(iter);
            order_lookup_.erase(it);

            // Clean up empty price level
            if (level.empty()) {
                if (side == Side::Buy) {
                    bids_map_.erase(price);
                } else {
                    asks_map_.erase(price);
                }
            }
            return true;
        }
    }

    return false;

}


std::optional<Price> OrderBook::get_best_bid() const {
    if (bids_map_.empty()) return std::nullopt;
    return bids_map_.begin()->first;
}

std::optional<Price> OrderBook::get_best_ask() const {
    if (asks_map_.empty()) return std::nullopt;
    return asks_map_.begin()->first;
}

std::vector<std::pair<Price, Quantity>> OrderBook::get_bid_depth() const {
    std::vector<std::pair<Price, Quantity>> result;
    result.reserve(bids_map_.size());
    for (const auto& [price, level] : bids_map_) {
        Quantity total = 0;
        for (const auto& o : level) total += o.remaining();
        if (total > 0) result.emplace_back(price, total);
    }
    return result;
}

std::vector<std::pair<Price, Quantity>> OrderBook::get_ask_depth() const {
    std::vector<std::pair<Price, Quantity>> result;
    result.reserve(asks_map_.size());
    for (const auto& [price, level] : asks_map_) {
        Quantity total = 0;
        for (const auto& o : level) total += o.remaining();
        if (total > 0) result.emplace_back(price, total);
    }
    return result;
}

std::vector<std::pair<Price, Quantity>> OrderBook::get_depth(int levels) const {
    std::vector<std::pair<Price, Quantity>> result;
    
    // Let's use ranges to take first N elements, transform to 
    // {price, sum_qty}
    auto bid_view = bids_map_
        | std::views::take(levels)
        | std::views::transform([](const auto& pair) {
            const auto& [price, orders] = pair;
            Quantity total_qty = 0;
            for (const auto& order : orders) {
                total_qty += order.remaining();
            }
            return std::make_pair(price, total_qty);
        });
    
    std::ranges::copy(bid_view, std::back_inserter(result));
    return result; 
}
} // namespace order_book