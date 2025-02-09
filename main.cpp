#include <iostream>
#include <map>
#include <set>
#include <list>
#include <cmath>
#include <ctime>
#include <deque>
#include <queue>
#include <stack>
#include <limits>
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <variant>
#include <optional>
#include <tuple>
#include <format>
#include <stdexcept>

// Order types and sides.
enum class OrderType {
    GoodTillCancel,
    FillAndKill
};

enum class Side {
    Buy,
    Sell
};

// Holds price and aggregated quantity at a given level
struct LevelInfo {
    std::int32_t price;
    std::uint32_t quantity;
};

// Aggregated order book levels.
class OrderbookLevelInfos {
public:
    OrderbookLevelInfos(const std::vector<LevelInfo>& bids, const std::vector<LevelInfo>& asks)
            : bids_{bids}, asks_{asks} {}

    const std::vector<LevelInfo>& GetBids() const { return bids_; }
    const std::vector<LevelInfo>& GetAsks() const { return asks_; }

private:
    std::vector<LevelInfo> bids_;
    std::vector<LevelInfo> asks_;
};

// Represents an individual order
class Order {
public:
    Order(OrderType orderType, std::uint64_t orderId, Side side, std::int32_t price, std::uint32_t quantity)
            : orderType_{orderType}, orderId_{orderId}, side_{side}, price_{price},
              initialQuantity_{quantity}, remainingQuantity_{quantity} {}

    OrderType GetOrderType() const { return orderType_; }
    std::uint64_t GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    std::int32_t GetPrice() const { return price_; }
    std::uint32_t GetInitialQuantity() const { return initialQuantity_; }
    std::uint32_t GetRemainingQuantity() const { return remainingQuantity_; }
    std::uint32_t GetFilledQuantity() const { return initialQuantity_ - remainingQuantity_; }
    bool isFilled() const { return remainingQuantity_ == 0; }

    // Fill part of the order.
    void Fill(std::uint32_t quantity) {
        if (quantity > remainingQuantity_)
            throw std::logic_error(std::format("Order ({}) cannot be filled for more than its remaining quantity", orderId_));
        remainingQuantity_ -= quantity;
    }

private:
    OrderType orderType_;
    std::uint64_t orderId_;
    Side side_;
    std::int32_t price_;
    std::uint32_t initialQuantity_;
    std::uint32_t remainingQuantity_;
};

// Represents a modification request for an existing order
class OrderModify {
public:
    OrderModify(std::uint64_t orderId, Side side, std::int32_t price, std::uint32_t quantity)
            : orderId_{orderId}, side_{side}, price_{price}, quantity_{quantity} {}

    std::uint64_t GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    std::int32_t GetPrice() const { return price_; }
    std::uint32_t GetQuantity() const { return quantity_; }

    // Create a new Order with the given type
    std::shared_ptr<Order> ToOrderPointer(OrderType type) const {
        return std::make_shared<Order>(type, orderId_, side_, price_, quantity_);
    }

private:
    std::uint64_t orderId_;
    Side side_;
    std::int32_t price_;
    std::uint32_t quantity_;
};

// Holds trade details.
struct TradeInfo {
    std::uint64_t order_id;
    std::int32_t price_;
    std::uint32_t quantity_;
};

// Represents a trade between a bid and an ask
class Trade {
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
            : bidTrade_{bidTrade}, askTrade_{askTrade} {}

private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
};

// OrderBook maintains and matches orders
class OrderBook {
private:
    // Stores an order and its position in the order list
    struct OrderEntry {
        std::shared_ptr<Order> order_{nullptr};
        std::list<std::shared_ptr<Order>>::iterator location;
    };

    // Bids: descending order
    std::map<std::int32_t, std::list<std::shared_ptr<Order>>, std::greater<std::int32_t>> bids_;
    // Asks: ascending order
    std::map<std::int32_t, std::list<std::shared_ptr<Order>>, std::less<std::int32_t>> asks_;
    // Lookup table for orders by ID.
    std::unordered_map<std::uint64_t, OrderEntry> orders_;

    // Check if an order can match based on its price
    bool CanMatch(Side side, std::int32_t price) const {
        if (side == Side::Buy) {
            if (asks_.empty())
                return false;
            const auto& [bestAsk, _] = *asks_.begin();
            return price >= bestAsk;
        } else {
            if (bids_.empty())
                return false;
            const auto& [bestBid, _] = *bids_.begin();
            return price <= bestBid;
        }
    }

    // Attempt to match orders and generate trades
    std::vector<Trade> MatchOrders() {
        std::vector<Trade> trades;
        trades.reserve(orders_.size());

        while (true) {
            if (bids_.empty() || asks_.empty())
                break;

            auto& [bidPrice, bidList] = *bids_.begin();
            auto& [askPrice, askList] = *asks_.begin();

            if (bidPrice < askPrice)
                break;

            while (!bidList.empty() && !askList.empty()) {
                auto& bid = bidList.front();
                auto& ask = askList.front();
                std::uint32_t quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

                bid->Fill(quantity);
                ask->Fill(quantity);

                if (bid->isFilled()) {
                    bidList.pop_front();
                    orders_.erase(bid->GetOrderId());
                }
                if (ask->isFilled()) {
                    askList.pop_front();
                    orders_.erase(ask->GetOrderId());
                }

                if (bidList.empty())
                    bids_.erase(bidPrice);
                if (askList.empty())
                    asks_.erase(askPrice);

                trades.push_back(Trade{
                        TradeInfo{ bid->GetOrderId(), bid->GetPrice(), quantity },
                        TradeInfo{ ask->GetOrderId(), ask->GetPrice(), quantity }
                });
            }
        }

        // Cancel FillAndKill orders if they remain unmatched
        if (!bids_.empty()) {
            auto& [_, bidList] = *bids_.begin();
            auto& order = bidList.front();
            if (order->GetOrderType() == OrderType::FillAndKill)
                CancelOrder(order->GetOrderId());
        }
        if (!asks_.empty()) {
            auto& [_, askList] = *asks_.begin();
            auto& order = askList.front();
            if (order->GetOrderType() == OrderType::FillAndKill)
                CancelOrder(order->GetOrderId());
        }
        return trades;
    }

public:
    // Add a new order and try to match
    std::vector<Trade> AddOrder(std::shared_ptr<Order> order) {
        if (orders_.contains(order->GetOrderId()))
            return {};
        if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
            return {};

        std::list<std::shared_ptr<Order>>::iterator iterator;
        if (order->GetSide() == Side::Buy) {
            auto& orderList = bids_[order->GetPrice()];
            orderList.push_back(order);
            iterator = std::next(orderList.begin(), orderList.size() - 1);
        } else {
            auto& orderList = asks_[order->GetPrice()];
            orderList.push_back(order);
            iterator = std::next(orderList.begin(), orderList.size() - 1);
        }
        orders_.insert({ order->GetOrderId(), OrderEntry{ order, iterator } });
        return MatchOrders();
    }

    // Cancel an order by its ID
    void CancelOrder(std::uint64_t orderId) {
        if (!orders_.contains(orderId))
            return;

        const auto& [order, orderIterator] = orders_.at(orderId);
        orders_.erase(orderId);

        if (order->GetSide() == Side::Sell) {
            std::int32_t price = order->GetPrice();
            auto& orderList = asks_.at(price);
            orderList.erase(orderIterator);
            if (orderList.empty())
                asks_.erase(price);
        } else {
            std::int32_t price = order->GetPrice();
            auto& orderList = bids_.at(price);
            orderList.erase(orderIterator);
            if (orderList.empty())
                bids_.erase(price);
        }
    }

    // Modify an existing order
    std::vector<Trade> MatchOrder(OrderModify order) {
        if (!orders_.contains(order.GetOrderId()))
            return {};
        const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
        CancelOrder(order.GetOrderId());
        return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
    }

    std::size_t Size() const { return orders_.size(); }

    // Get aggregated order levels for bids and asks
    OrderbookLevelInfos GetOrderInfos() const {
        std::vector<LevelInfo> bidInfos, askInfos;
        bidInfos.reserve(orders_.size());
        askInfos.reserve(orders_.size());

        auto CreateLevelInfos = [](std::int32_t price, const std::list<std::shared_ptr<Order>>& orders) {
            return LevelInfo{
                    price,
                    std::accumulate(orders.begin(), orders.end(), static_cast<std::uint32_t>(0),
                                    [](std::uint32_t sum, const std::shared_ptr<Order>& order) {
                                        return sum + order->GetRemainingQuantity();
                                    })
            };
        };

        for (const auto& [price, orders] : bids_)
            bidInfos.push_back(CreateLevelInfos(price, orders));

        for (const auto& [price, orders] : asks_)
            askInfos.push_back(CreateLevelInfos(price, orders));

        return OrderbookLevelInfos{ bidInfos, askInfos };
    }
};

int main() {

    return 0;
}
