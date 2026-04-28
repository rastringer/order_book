// Current coverage: empty book BBO, BBO ordering, crossing limit at resting price, partial fills, aggressor leftover resting on book, price-time priority, market order walking levels, market order on empty side, non-crossing limit, cancel, cancel-unknown, cancel-after-partial-fill, cancelling the only order at a level shifts BBO

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "matching_engine.h"
#include <chrono>
#include <vector>


namespace order_book {

Order mk(OrderId id, Side side, Price price, Quantity qty, 
    OrderType type = OrderType::Limit) {
        return Order {
            .id = id,
            .side = side,
            .price = price,
            .quantity = qty,
            .filled_quantity = 0,
            .type = type,
            .arrival_time = std::chrono::steady_clock::now()
        };
    }

struct Harness {
    std::vector<Trade> trades;
    MatchingEngine engine;
    Harness() : engine([this](const Trade& t) {trades.push_back(t);}) {}
};

} // namespace order_book

TEST_CASE("Empty book has no best bid or ask") {
    Harness h;
    CHECK_FALSE(h.engine.get_best_bid().has_value());
    CHECK_FALSE(h.engine.get_best_ask().has_value());
    CHECK(h.trades.empty());
}

TEST_CASE("Resting limit orders set BBO") {
    Harness h;
    Order b = mk(1, Side::Buy,  99, 10);
    Order s = mk(2, Side::Sell, 101, 10);
    h.engine.process_order(b);
    h.engine.process_order(s);

    REQUIRE(h.engine.get_best_bid().has_value());
    REQUIRE(h.engine.get_best_ask().has_value());
    CHECK(*h.engine.get_best_bid() == 99);
    CHECK(*h.engine.get_best_ask() == 101);
    CHECK(h.trades.empty());
}

TEST_CASE("Best bid is the highest buy, best ask is the lowest sell") {
    Harness h;
    for (auto [id, p] : std::vector<std::pair<OrderId, Price>>{
             {1, 95}, {2, 99}, {3, 97}}) {
        Order o = mk(id, Side::Buy, p, 10);
        h.engine.process_order(o);
    }
    for (auto [id, p] : std::vector<std::pair<OrderId, Price>>{
             {4, 105}, {5, 101}, {6, 103}}) {
        Order o = mk(id, Side::Sell, p, 10);
        h.engine.process_order(o);
    }
    CHECK(*h.engine.get_best_bid() == 99);
    CHECK(*h.engine.get_best_ask() == 101);
}

TEST_CASE("Crossing limit produces a trade at the resting order's price") {
    Harness h;
    Order resting = mk(1, Side::Sell, 100, 10);
    h.engine.process_order(resting);
    Order aggressor = mk(2, Side::Buy, 105, 10);  // willing to pay 105
    h.engine.process_order(aggressor);

    REQUIRE(h.trades.size() == 1);
    CHECK(h.trades[0].price == 100);              // executes at resting price
    CHECK(h.trades[0].quantity == 10);
    CHECK(h.trades[0].aggressor_id == 2);
    CHECK(h.trades[0].resting_id == 1);
    CHECK(h.trades[0].aggressor_side == Side::Buy);

    CHECK_FALSE(h.engine.get_best_ask().has_value());
    CHECK_FALSE(h.engine.get_best_bid().has_value());
}

TEST_CASE("Partial fill of resting order leaves remainder on the book") {
    Harness h;
    Order resting = mk(1, Side::Sell, 100, 100);
    h.engine.process_order(resting);
    Order aggressor = mk(2, Side::Buy, 100, 30);
    h.engine.process_order(aggressor);

    REQUIRE(h.trades.size() == 1);
    CHECK(h.trades[0].quantity == 30);
    REQUIRE(h.engine.get_best_ask().has_value());
    CHECK(*h.engine.get_best_ask() == 100);   // resting #1 still there with 70
}

TEST_CASE("Aggressor limit with leftover quantity rests on the book") {
    Harness h;
    Order resting = mk(1, Side::Sell, 100, 10);
    h.engine.process_order(resting);
    Order aggressor = mk(2, Side::Buy, 100, 30);  // takes 10, rests 20 @100
    h.engine.process_order(aggressor);

    REQUIRE(h.trades.size() == 1);
    CHECK(h.trades[0].quantity == 10);
    REQUIRE(h.engine.get_best_bid().has_value());
    CHECK(*h.engine.get_best_bid() == 100);
    CHECK_FALSE(h.engine.get_best_ask().has_value());
}

TEST_CASE("Price-time priority: older order at the same price fills first") {
    Harness h;
    Order older  = mk(1, Side::Sell, 100, 10);   // arrived first
    Order newer  = mk(2, Side::Sell, 100, 10);
    h.engine.process_order(older);
    h.engine.process_order(newer);

    Order taker = mk(3, Side::Buy, 100, 10);
    h.engine.process_order(taker);

    REQUIRE(h.trades.size() == 1);
    CHECK(h.trades[0].resting_id == 1);          // not 2
    REQUIRE(h.engine.get_best_ask().has_value());
    CHECK(*h.engine.get_best_ask() == 100);
}

TEST_CASE("Market BUY walks multiple ask levels until filled") {
    Harness h;
    Order a1 = mk(1, Side::Sell, 100, 5);
    Order a2 = mk(2, Side::Sell, 101, 5);
    Order a3 = mk(3, Side::Sell, 102, 20);
    h.engine.process_order(a1);
    h.engine.process_order(a2);
    h.engine.process_order(a3);

    Order mkt = mk(4, Side::Buy, 0, 12, OrderType::Market);
    h.engine.process_order(mkt);

    REQUIRE(h.trades.size() == 3);
    CHECK(h.trades[0].price == 100);
    CHECK(h.trades[0].quantity == 5);
    CHECK(h.trades[1].price == 101);
    CHECK(h.trades[1].quantity == 5);
    CHECK(h.trades[2].price == 102);
    CHECK(h.trades[2].quantity == 2);

    REQUIRE(h.engine.get_best_ask().has_value());
    CHECK(*h.engine.get_best_ask() == 102);      // 18 left at 102
}

TEST_CASE("Market order on empty opposite side does not crash and produces no trade") {
    Harness h;
    Order mkt_sell = mk(1, Side::Sell, 0, 50, OrderType::Market);
    h.engine.process_order(mkt_sell);
    CHECK(h.trades.empty());
    CHECK_FALSE(h.engine.get_best_bid().has_value());
    CHECK_FALSE(h.engine.get_best_ask().has_value());
}

TEST_CASE("Limit price worse than best does not match and rests") {
    Harness h;
    Order resting = mk(1, Side::Sell, 105, 10);
    h.engine.process_order(resting);
    Order taker = mk(2, Side::Buy, 100, 10);     // 100 < 105: cannot cross
    h.engine.process_order(taker);

    CHECK(h.trades.empty());
    REQUIRE(h.engine.get_best_bid().has_value());
    REQUIRE(h.engine.get_best_ask().has_value());
    CHECK(*h.engine.get_best_bid() == 100);
    CHECK(*h.engine.get_best_ask() == 105);
}

TEST_CASE("Cancel removes a resting order") {
    Harness h;
    Order o = mk(1, Side::Sell, 100, 10);
    h.engine.process_order(o);
    REQUIRE(h.engine.get_best_ask().has_value());
    CHECK(h.engine.cancel_order(1));
    CHECK_FALSE(h.engine.get_best_ask().has_value());
}

TEST_CASE("Cancel on unknown ID returns false") {
    Harness h;
    CHECK_FALSE(h.engine.cancel_order(999));
}

TEST_CASE("Cancel after a partial fill removes the remainder") {
    Harness h;
    Order resting = mk(1, Side::Sell, 100, 100);
    h.engine.process_order(resting);
    Order aggressor = mk(2, Side::Buy, 100, 30);
    h.engine.process_order(aggressor);
    REQUIRE(h.trades.size() == 1);
    REQUIRE(h.engine.get_best_ask().has_value());

    CHECK(h.engine.cancel_order(1));
    CHECK_FALSE(h.engine.get_best_ask().has_value());
}

TEST_CASE("Cancelling the only order at a price level removes the level") {
    Harness h;
    Order a1 = mk(1, Side::Sell, 100, 10);
    Order a2 = mk(2, Side::Sell, 101, 10);
    h.engine.process_order(a1);
    h.engine.process_order(a2);
    CHECK(*h.engine.get_best_ask() == 100);

    CHECK(h.engine.cancel_order(1));
    REQUIRE(h.engine.get_best_ask().has_value());
    CHECK(*h.engine.get_best_ask() == 101);     // best ask shifted up
}
