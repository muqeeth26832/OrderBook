#include "Orderbook.h"
#include "OrderType.h"
#include "Usings.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <chrono>
#include <ctime>

/* Constructers and more */
// Orderbook::Orderbook() : ordersPruneThread_{
//     [this] {PruneGoodForDayOrders();}}{}
Orderbook::Orderbook() {}


/* Private functions */

void Orderbook::PruneGoodForDayOrders()
{
    return;
    // using namespace std::chrono;
    // const auto end  = hours(16); // at 4pm

    // while(true){
    //     const auto now = system_clock::now();
    //     const auto now_c = system_clock::to_time_t(now);
    //     std::tm now_parts;
    //     // localtime_s(&now_parts,&now_c);

    //     if(now_parts.tm_hour>=end.count())
    //         now_parts.tm_mday +=1;

    //     // exactly stop at 4PM
    //     now_parts.tm_hour = end.count();
    //     now_parts.tm_min = 0;
    //     now_parts.tm_sec = 0;

    //     auto next = system_clock::from_time_t(mktime(&now_parts));
    //     auto till = next-now+milliseconds(100);

    //     {
    //         std::unique_lock ordersLock{ ordersMutex_ };
    //         if(shutdown_.load(std::memory_order_acquire) ||
    //             shutdownConditionVariable_.wait_for(ordersLock,till)==std::cv_status::no_timeout)
    //             return;
    //     }

    //     OrderIds orderIds;

    //     {
    //         std::scoped_lock ordersLock{ordersMutex_};

    //         for(const auto& [_,entry] : orders_){
    //             const auto& [order,_] = entry;

    //             if(order->GetOrderType()!=OrderType::GoodForDay)
    //                 continue;

    //             orderIds.push_back(order->GetOrderId());
    //         }
    //     }

    //     CancelOrders(orderIds);
    // }
}

/* to cancel the order */
void Orderbook::CancelOrder(OrderId orderId)
{
    std::scoped_lock ordersLock{ordersMutex_};
    CancelOrderInternal(orderId);
}

void Orderbook::CancelOrders(OrderIds orderIds){
    std::scoped_lock orderLock {ordersMutex_};

    for(const auto & orderId : orderIds){
        CancelOrderInternal(orderId);
    }
}

void Orderbook::CancelOrderInternal(OrderId orderId){
    if(!orders_.contains(orderId)) return;

    const auto [order,iterator] =orders_[orderId];
    orders_.erase(orderId);

    if (order->GetSide() == Side::Sell)
	{
		auto price = order->GetPrice();
		auto& orders = asks_.at(price);
		orders.erase(iterator);
		if (orders.empty())
			asks_.erase(price);
	}
	else
	{
		auto price = order->GetPrice();
		auto& orders = bids_.at(price);
		orders.erase(iterator);
		if (orders.empty())
			bids_.erase(price);
	}

	// OnOrderCancelled(order);
}

#include <iostream>
/* for fill and kill type see if it can match  */
bool Orderbook::CanMatch(Side side,Price price) const
{
    if(side==Side::Buy){
        if(asks_.empty()) return false;

        // check if someone is selling at some price that is <= our bid
        const auto& [bestAsk,_] = *asks_.begin(); // lowest some one is asking for
        return price>=bestAsk; // someone is willing to sell at or below my bid
    }else{
        if(bids_.empty()) return false;

        // check if someone is buying at some price that is >= our ask
        const auto& [bestBid,_] = *bids_.begin(); // highest some one is bidding for
        return price<=bestBid; // some is willing to buy atleast at my ask
    }
}

/* match orders and return them */
Trades Orderbook::MatchOrders(){
    Trades trades;
    trades.reserve(orders_.size());// may be all orders can match

    while(true){
        if(bids_.empty() || asks_.empty()) break;

        auto &[bidPrice,bids] = *bids_.begin();
        auto &[askPrice,asks] = *asks_.begin();

        if(bidPrice<askPrice) break;

        while(bids.size() && asks.size()){
            auto& bid = bids.front();
            auto& ask = asks.front();

            Quantity quantity = std::min(bid->GetRemainingQuantity(),ask->GetRemainingQuantity());

            bid->Fill(quantity);
            ask->Fill(quantity);

            if(bid->IsFilled()){
                bids.pop_front();
                orders_.erase(bid->GetOrderId());
            }

            if(ask->IsFilled()){
                asks.pop_front();
                orders_.erase(ask->GetOrderId());
            }

            if(bids.empty())
                bids_.erase(bidPrice);
            if(asks.empty())
                asks_.erase(askPrice);
            trades.push_back(Trade{
                TradeInfo{bid->GetOrderId(),bid->GetPrice(),quantity}
               ,TradeInfo{ask->GetOrderId(),ask->GetPrice(),quantity}
            });
        }
    }



    if(!bids_.empty()){
        auto& [_,bids] = *bids_.begin();
        auto& order = bids.front();
        if(order->GetOrderType()==OrderType::FillAndKill){
            CancelOrder(order->GetOrderId());
        }
    }

    if(!asks_.empty()){
        auto& [_,asks] = *asks_.begin();
        auto& order = asks.front();
        if(order->GetOrderType()==OrderType::FillAndKill){
            CancelOrder(order->GetOrderId());
        }
    }
    return trades;
}



/*Public functions */
Trades Orderbook::AddOrder(OrderPointer order)
{
    if(orders_.find(order->GetOrderId()) != orders_.end()) // we already have this order
        return Trades{};

    // if(order->GetOrderType()==OrderType::Market)
    // {
    //     if(order->GetSide()==Side::Buy && !asks_.empty()){
    //         const auto& [worstAsk,_] = *asks_.rbegin(); // max sell price
    //         order->ToGoodTillCancel(worstAsk);
    //     }
    //     else if(order->GetSide()==Side::Sell && !bids_.empty()){
    //         const auto &[worstBid,_] = *bids_.rbegin(); // min buy price
    //         order->ToGoodTillCancel(worstBid);
    //     }
    //     else{
    //         return Trades{};
    //     }
    // }


    if(order->GetOrderType()==OrderType::FillAndKill && !CanMatch(order->GetSide(),order->GetPrice()) )
        return Trades{};

    OrderPointers::iterator iterator;
    if(order->GetSide()==Side::Buy){
        auto & orders = bids_[order->GetPrice()]; // get orders of same price
        orders.push_back(order);

        if (orders.empty()) {
            throw std::runtime_error("Logic error: orders list is unexpectedly empty after push_back.");
        }
        iterator =std::next(orders.begin(),orders.size()-1); // iterator to last element in list
        // iterator = std::prev(orders.end());
    }else{
        // sell
        auto & orders = asks_[order->GetPrice()]; // get orders of same price
        orders.push_back(order);

        if (orders.empty()) {
            throw std::runtime_error("Logic error: orders list is unexpectedly empty after push_back.");
        }
        iterator = std::prev(orders.end());
    }
    // we have added that order is asks_ or bids_
    // now add in orders_
    // id, OrderEntry
    // so like to get that order from id we store order id mapped to which list in map and iterator in that map
   orders_.insert({order->GetOrderId(),OrderEntry{order,iterator}});
   // match it and return trades
   return MatchOrders();
}

/* to modify the order */
Trades Orderbook::ModifyOrder(OrderModify order)
{
    if(orders_.find(order.GetOrderId())==orders_.end()) return Trades{};

    const auto &[existingOrder,_] = orders_[order.GetOrderId()];
    CancelOrder(order.GetOrderId());
    return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
}


/* to get information about levels on both sides  */
OrderbookLevelInfos Orderbook::GetOrderInfos() const
{
    LevelInfos bidInfos,askInfos;
    bidInfos.reserve(orders_.size());
    askInfos.reserve(orders_.size());

    auto CreateLevelInfos = [](Price price,const OrderPointers& orders){
        // return for a price how many quantities are there
        return LevelInfo{
            price,
            std::accumulate(
                orders.begin(),
                orders.end(),
                (Quantity)0,
            [](Quantity runningSum,const OrderPointer& order){
                    return runningSum + order->GetRemainingQuantity();
                }
            )
        };
    };

   for(const auto& [price,order]: bids_){
       bidInfos.push_back(CreateLevelInfos(price,order));
   }
   for(const auto& [price,order]: asks_){
       askInfos.push_back(CreateLevelInfos(price,order));
   }
   return OrderbookLevelInfos{bidInfos,askInfos};
}
