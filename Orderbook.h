#pragma once

#include <map>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <atomic>
#include "Usings.h"
#include "Order.h"
#include "OrderModify.h"
#include "OrderbookLevelInfos.h"
#include "Trade.h"


/* Orderbook */
/*
 *
 * storing orders in map
 * bids are sorted in descending order - best bid first (paying more to buy)
 * asks are sorted in ascending order -  best ask first (aksing less to sell)
 */
class Orderbook {
private:
    struct OrderEntry{ // Store pointer to order
        OrderPointer order_{nullptr};
        OrderPointers::iterator location_; // for quick access
    };

    struct LevelData{
        Quantity quantity_{};
        Quantity count_{};

        enum class Action {
            ADD,
            REMOVE,
            MATCH
        };
    };
    std::unordered_map<Price, LevelData> data_;
    // for a price store order pointers
    std::map<Price,OrderPointers,std::greater<Price>> bids_; // highest bid to lowest bid
    std::map<Price,OrderPointers,std::less<Price>> asks_; // lowest ask to highest ask
    std::unordered_map<OrderId,OrderEntry> orders_;
    //
    mutable std::mutex ordersMutex_;
    std::thread ordersPruneThread_;
    std::condition_variable shutdownConditionVariable_;
    std::atomic<bool> shutdown_{false};

    void PruneGoodForDayOrders();

    void CancelOrders(OrderIds orderIds);
    void CancelOrderInternal(OrderId orderId);

    void OnOrderCancelled(OrderPointer order);
    void OnOrderAdded(OrderPointer order);
    void OnOrderMatched(Price price,Quantity quantity,bool isFullyFilled);
    void UpdateLevelData(Price price,Quantity quantity,LevelData::Action action);

    bool CanFullyFill(Side side,Price price,Quantity quantity) const;
    bool CanMatch(Side side,Price price) const;
    Trades MatchOrders();
public:
    Orderbook();
    ~Orderbook();
    // Orderbook(const Orderbook&) = delete;
    // void operator=(const Orderbook&) = delete;
    // Orderbook(Orderbook&&) = delete;
    // void operator=(Orderbook&&) = delete;
    // ~Orderbook(){ shutdownConditionVariable_.notify_all();
    //     ordersPruneThread_.join();
    // };

    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades ModifyOrder(OrderModify order);

    /* to know how many orders are in the orderbook */
    std::size_t Size() const {return orders_.size();}
    OrderbookLevelInfos GetOrderInfos() const;
    void PrintOrderbook() const;

};
