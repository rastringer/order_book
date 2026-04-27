#include "matching_engine.h"
#include <algorithm>


namespace order_book {

MatchingEngine::MatchingEngine(TradeCallback on_trade) : on_trade(std::move(on_trade)) {}

void MatchingEngine::process_order(Order& order) {
    if (order.type == OrderType::Market) {
        // Market orders match immediately
        if (order.side == Side::Buy) match_buy(order);
        else {
            match_sell(order);
        }
        return;
    }

    // Limit orders first try to match, rest if unfulfilled
    if (order.side == Side::Buy) {
        match_buy(order);
        if (!order.is_filled()) book_.add_order(order);
    } else {
        match_sell(order);
        if (!order.is_filled()) {
            book_.add_order(order);
        }
    }
}

void MatchingEngine::match_buy(Order& order) {
    // Match against asks
    while (!order.is_filled() && !book_.asks_empty()) {
        auto best_ask_it = book_.begin_asks();

        // Check if we can match
        if (order.type == OrderType::Limit && order.price < best_ask_it->first) {
            break; // Price too low for limit order
        }

        auto& resting_orders = best_ask_it->second;
        if (resting_orders.empty()) {
            book_.erase_ask(best_ask_it);
            continue;
        }

        Order& resting = resting_orders.front();
        Price execution_price = resting.price; // Price-time priority: resting order's price
        
        Quantity fill_qty = std::min(order.remaining(), resting.remaining());
        
        execute_trade(order, resting, execution_price, fill_qty);

        // Remove resting order if fully filled
        if (resting.is_filled()) {
            resting_orders.pop_front();
            book_.remove_order_from_lookup(resting.id);
            if (resting_orders.empty()) {
                book_.erase_ask(best_ask_it);
            }
        }
     }
}

void MatchingEngine::match_sell(Order& order) {
    // Match against bids (buy side)
    while (!order.is_filled() && !book_.bids_empty()) {
        auto best_bid_it = book_.begin_bids();

        // Check if we can match (aggressor sell 
        // price <= resting buy price)
        if (order.type == OrderType::Limit && order.price > best_bid_it->first) {
            break; // Price too high for limit order
        }

        auto& resting_orders = book_.get_best_bid_orders(); 
        if (resting_orders.empty()) {
            book_.erase_bid(best_bid_it);
            continue;
        }

        Order& resting = resting_orders.front();
        Price execution_price = resting.price; 

        Quantity fill_qty = std::min(order.remaining(), resting.remaining());
        execute_trade(order, resting, execution_price, fill_qty);

        // Remove resting order if filled
        if (resting.is_filled()) {
            resting_orders.pop_front();
            book_.remove_order_from_lookup(resting.id);
            if (resting_orders.empty()) {
                book_.erase_bid(best_bid_it);
            }
        }
    }
}

void MatchingEngine::execute_trade(Order& aggressor, Order& resting, Price price, Quantity qty) {
    aggressor.filled_quantity += qty;
    resting.filled_quantity += qty;

    Trade trade{
        .aggressor_id = aggressor.id,
        .resting_id = resting.id,
        .price = price,
        .quantity = qty,
        .aggressor_side = aggressor.side,
        .execution_time = std::chrono::steady_clock::now()
    };

    if (on_trade) {
        on_trade(trade);
    }
}

} // namespace order_book