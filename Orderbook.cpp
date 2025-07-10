#include "Orderbook.h"
#include "Order.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <chrono>
#include <ctime>
#include <optional>

/* Constructers and more */
/* when i create an orderbook
 * i start pruning good for day orders thread
 * I need to pass this to it as thread expects a callable object
 */
Orderbook::Orderbook() : ordersPruneThread_{[this] {PruneGoodForDayOrders();}}{}
Orderbook::~Orderbook(){
    shutdown_.store(true, std::memory_order_release);
	shutdownConditionVariable_.notify_one();
	ordersPruneThread_.join();
}


/* Private functions */
void Orderbook::PruneGoodForDayOrders()
{
    using namespace std::chrono;

    const auto end = hours(16); // 4 PM

    while (true)
    {
        const auto now = system_clock::now();
        const auto now_c = system_clock::to_time_t(now);
        std::tm now_parts;

        //  Use thread-safe & portable time function
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&now_parts, &now_c);  // Windows
#else
        localtime_r(&now_c, &now_parts);  // Linux/macOS
#endif

        // Move to next day if it's already past 4 PM
        if (now_parts.tm_hour >= end.count())
            now_parts.tm_mday += 1;

        // Set exact 4:00:00 PM time
        now_parts.tm_hour = end.count();
        now_parts.tm_min = 0;
        now_parts.tm_sec = 0;

        auto next = system_clock::from_time_t(mktime(&now_parts));
        auto till = next - now + milliseconds(100);

        {
            std::unique_lock ordersLock{ ordersMutex_ };

            // Explicit check for shutdown using wait predicate
            if (shutdownConditionVariable_.wait_for(
                    ordersLock, till, [this]() { return shutdown_.load(std::memory_order_acquire); }))
            {
                return; // shutdown requested
            }
        }

        // Now prune GoodForDay orders
        OrderIds orderIds;

        {
            std::scoped_lock ordersLock{ ordersMutex_ };

            for (const auto& [_, entry] : orders_)
            {
                const auto& [order, __] = entry;

                if (order->GetOrderType() == OrderType::GoodForDay)
                {
                    orderIds.push_back(order->GetOrderId());
                }
            }
        }

        CancelOrders(orderIds);
    }
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
/* i am not deleting the orders one by one as it would have to lock mutes an realease multiple times
 * causing multiple cache flushed . this is not good for cache coherence
 */
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

	OnOrderCancelled(order);
}

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


bool Orderbook::CanFullyFill(Side side,Price price,Quantity quantity) const {
    // if i cant even find a match for it
    if(!CanMatch(side, price))
        return false;

    // i can find a match for it
    // but can it get fully filled
    std::optional<Price> threshold;

    if(side==Side::Buy){
        const auto [askPrice, _ ]  = *asks_.begin();
        threshold = askPrice;
    }else{
        const auto [bidPrice, _ ]  = *bids_.begin();
        threshold = bidPrice;
    }

    // we just want to know how much quanity is there between best askPrice and our price  (askPrice<price)
    // if there is enought its good for us

    for(const auto& [levelPrice,levelData] : data_){
        if(threshold.has_value() &&
            (side==Side::Buy && threshold.value() > levelPrice) ||
            (side==Side::Sell && threshold.value() <levelPrice))
            continue;

        if((side==Side::Buy && levelPrice > price) || (side==Side::Sell && levelPrice < price))
            continue;

        if(quantity<=levelData.quantity_)
            return true;
        quantity-=levelData.quantity_;
    }

    return false;
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

            trades.push_back(Trade{
                TradeInfo{bid->GetOrderId(),bid->GetPrice(),quantity}
               ,TradeInfo{ask->GetOrderId(),ask->GetPrice(),quantity}
            });

            OnOrderMatched(bid->GetPrice(),quantity,bid->IsFilled());
            OnOrderMatched(ask->GetPrice(),quantity,ask->IsFilled());
        }

        if(bids.empty())
        {
            bids_.erase(bidPrice);
            data_.erase(bidPrice);
        }
        if(asks.empty())
        {
            asks_.erase(askPrice);
            data_.erase(askPrice);
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
    std::scoped_lock ordersLock {ordersMutex_};
    if(orders_.find(order->GetOrderId()) != orders_.end()) // we already have this order
        return Trades{};

    if(order->GetOrderType()==OrderType::Market)
    {
        if(order->GetSide()==Side::Buy && !asks_.empty()){
            const auto& [worstAsk,_] = *asks_.rbegin(); // max sell price
            order->ToGoodTillCancel(worstAsk);
        }
        else if(order->GetSide()==Side::Sell && !bids_.empty()){
            const auto &[worstBid,_] = *bids_.rbegin(); // min buy price
            order->ToGoodTillCancel(worstBid);
        }
        else{
            return Trades{};
        }
    }


    if(order->GetOrderType()==OrderType::FillAndKill && !CanMatch(order->GetSide(),order->GetPrice()) )
        return Trades{};

    if(order->GetOrderType()==OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(),order->GetInitialQuantity()))
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
   OnOrderAdded(order);
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

/* Event based methods */

void Orderbook::OnOrderCancelled(OrderPointer order){
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::REMOVE);
}

void Orderbook::OnOrderAdded(OrderPointer order){
    UpdateLevelData(order->GetPrice(),order->GetInitialQuantity(),LevelData::Action::ADD);
}

void Orderbook::OnOrderMatched(Price price,Quantity quantity, bool isFullyFilled){
    UpdateLevelData(price, quantity, isFullyFilled? LevelData::Action::REMOVE : LevelData::Action::MATCH);
}

void Orderbook::UpdateLevelData(Price price,Quantity quantity,LevelData::Action action){
    auto& data = data_[price];
    data.count_ += action==LevelData::Action::REMOVE ? -1 : action==LevelData::Action::ADD ? 1 : 0;

    if(action==LevelData::Action::REMOVE || action==LevelData::Action::MATCH){
        data.quantity_ -= quantity;
    }
    else{
        data.quantity_ += quantity;
    }

    if(data.count_==0)
        data_.erase(price);
}



/* TO print Orderbook */


#include <iomanip>
#include <iostream>
#include <algorithm>
#include <string>

// ANSI color codes
const std::string GREEN = "\033[1;32m";   // bright green
const std::string RED   = "\033[1;31m";   // bright red
const std::string BOLD  = "\033[1m";
const std::string RESET = "\033[0m";

// Render a fixed-width depth bar
std::string DepthBar(int quantity, int maxQty, bool isBid) {
    int barWidth = 12;  // fixed width bar
    int filled = (maxQty == 0) ? 0 : (quantity * barWidth) / maxQty;

    std::string bar = std::string(filled, '*') + std::string(barWidth - filled, ' ');
    return (isBid ? GREEN : RED) + bar + RESET;
}

void Orderbook::PrintOrderbook() const{
    OrderbookLevelInfos info = GetOrderInfos();
    const LevelInfos& bids = info.GetBids();
    const LevelInfos& asks = info.GetAsks();

    int maxQty = 0;
    for (const auto& b : bids) maxQty = std::max(maxQty, (int)b.quantity_);
    for (const auto& a : asks) maxQty = std::max(maxQty, (int)a.quantity_);

    std::cout << "\n" << BOLD << "=================== ORDER BOOK ===================\n" << RESET;
    std::cout << BOLD
              << std::setw(25) << "BID (BUY) SIDE"
              << " | "
              << std::setw(25) << "ASK (SELL) SIDE"
              << RESET << "\n";
    std::cout << "----------------------------+-----------------------------\n";
    std::cout << BOLD
              << "QTY   PRICE     DEPTH       | DEPTH       PRICE   QTY"
              << RESET << "\n";
    std::cout << "----------------------------+-----------------------------\n";

    size_t levels = std::max(bids.size(), asks.size());

    for (size_t i = 0; i < levels; ++i) {
        // Left: BID side
        if (i < bids.size()) {
            const auto& b = bids[i];
            std::cout << std::setw(4) << b.quantity_
                      << "  " << std::setw(6) << b.price_
                      << "  " << DepthBar(b.quantity_, maxQty, true);
        } else {
            std::cout << std::setw(4) << "-" << "  " << std::setw(6) << "-" << "  " << std::string(12, ' ');
        }

        std::cout << " | ";

        // Right: ASK side
        if (i < asks.size()) {
            const auto& a = asks[i];
            std::cout << DepthBar(a.quantity_, maxQty, false)
                      << "  " << std::setw(6) << a.price_
                      << "  " << std::setw(4) << a.quantity_;
        } else {
            std::cout << std::string(12, ' ') << "  " << std::setw(6) << "-" << "  " << std::setw(4) << "-";
        }

        std::cout << "\n";
    }

    std::cout << "----------------------------+-----------------------------\n";

    // Trader Info
    if (!bids.empty() || !asks.empty()) {
        std::cout << BOLD;
        if (!bids.empty())
            std::cout << "Best Bid: " << bids[0].price_ << " (" << bids[0].quantity_ << " qty)";
        if (!bids.empty() && !asks.empty()) std::cout << " | ";
        if (!asks.empty())
            std::cout << "Best Ask: " << asks[0].price_ << " (" << asks[0].quantity_ << " qty)";
        if (!bids.empty() && !asks.empty())
            std::cout << "\nSpread: " << (asks[0].price_ - bids[0].price_);
        std::cout << RESET << "\n";
    }

    std::cout << BOLD << "==================================================\n" << RESET;
}
