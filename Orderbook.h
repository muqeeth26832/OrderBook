#pragma once

#include <map>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <numeric>

#include "Usings.h"
#include "Order.h"
#include "OrderModify.h"
#include "OrderbookLevelInfos.h"
#include "Trade.h"


/* Orderbook */
class Orderbook {
private:
    struct OrderEntry{
        OrderPointer order_{nullptr};
        OrderPointers::iterator location_;
    };
    std::map<Price,OrderPointers,std::greater<Price>> bids_; // highest bid to lowest bid
    std::map<Price,OrderPointers,std::less<Price>> asks_; // lowest ask to highest ask
    std::unordered_map<OrderId,OrderEntry> orders_;

    bool CanMatch(Side side,Price price) const
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

    Trades MatchOrders(){
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
public:
    Trades AddOrder(OrderPointer order)
    {
        if(orders_.find(order->GetOrderId()) != orders_.end()) // we already have this order
            return Trades{};

        if(order->GetOrderType()==OrderType::FillAndKill && !CanMatch(order->GetSide(),order->GetPrice()) )
            return Trades{};

        OrderPointers::iterator iterator;
        if(order->GetSide()==Side::Buy){
            auto & orders = bids_[order->GetPrice()]; // get orders of same price
            orders.push_back(order);
            // iterator =std::next(orders.begin(),orders.size()-1); // iterator to last element in list
            iterator = std::prev(orders.end());
        }else{
            // sell
            auto & orders = asks_[order->GetPrice()]; // get orders of same price
            orders.push_back(order);
            iterator = std::prev(orders.end());
        }

       orders_.insert({order->GetOrderId(),OrderEntry{order,iterator}});
       // match it and return trades
       return MatchOrders();
    }

    void CancelOrder(OrderId orderId)
    {
        if(orders_.find(orderId)==orders_.end()) return;

        const auto& [order,orderIterator] = orders_[orderId];
        orders_.erase(orderId);


        if(order->GetSide()==Side::Sell){
            auto price = order->GetPrice();
            auto& orders = asks_[price];
            orders.erase(orderIterator);
            if(orders.empty()){
                asks_.erase(price);
            }
        }else{
            auto price = order->GetPrice();
            auto& orders = bids_[price];
            orders.erase(orderIterator);
            if(orders.empty()){
                bids_.erase(price);
            }
        }
    }

    Trades MatchOrder(OrderModify order)
    {
        if(orders_.find(order.GetOrderId())==orders_.end()) return Trades{};

        const auto &[existingOrder,_] = orders_[order.GetOrderId()];
        CancelOrder(order.GetOrderId());
        return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
    }

    std::size_t Size() const {return orders_.size();}

    OrderbookLevelInfos GetOrderInfos() const
    {
        LevelInfos bidInfos,askInfos;
        bidInfos.reserve(orders_.size());
        askInfos.reserve(orders_.size());

        auto CreateLevelInfos = [](Price price,const OrderPointers& orders){
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

};
