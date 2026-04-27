// Defining the core types: Side (Buy/Sell), OrderType (Limit/Market)
// and Order struct. Using fixed-size integers to avoid  
// floating-point issues with prices and quantities.

#pragma once
#include <cstdint>
#include <string>
#include <optional>
#include <chrono>

namespace order_book {

// Concept to ensure types are integral
template<typename T>
concept IntegralType = std::integral<T>;

using Price = int64_t;
using Quantity = int64_t;
using OrderId = uint64_t;

enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

enum class OrderType : uint8_t {
    Limit = 0,
    Market = 1
};

struct Order {
    OrderId id; 
    Side side;
    Price price;
    Quantity quantity;
    Quantity filled_quantity;
    OrderType type;
    // Timestamp
    std::chrono::steady_clock::time_point arrival_time;

    bool is_filled() const {
        return filled_quantity >= quantity;
    }

    Quantity remaining() const {
        return quantity - filled_quantity;
    }
};

struct Trade {
    OrderId aggressor_id;
    OrderId resting_id;
    Price price;
    Quantity quantity;
    Side aggressor_side;
    std::chrono::steady_clock::time_point execution_time;
};

} // namespace order_book
