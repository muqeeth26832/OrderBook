#pragma once


#include <list>
#include <exception>
#include <stdexcept>
#include <format>
#include <memory>
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"
#include "Constants.h"
#include <sstream>

/* class to make an order object */
/* it has public apis to know about the order details */

class Order {
public:
    Order(OrderType orderType,OrderId orderId,Side side,Price price,Quantity quantity)
    : orderType_(orderType), orderId_(orderId), side_(side), price_(price), initialQuantity_(quantity), remainingQuantity_(quantity)
    {}

    Order(OrderId orderId,Side side,Quantity quantity)
    : Order(OrderType::Market,orderId,side,Constants::InvalidPrice,quantity)
    {}

    OrderId GetOrderId() const {return orderId_;}
    Side GetSide() const {return side_;}
    Price GetPrice() const {return price_;}
    OrderType GetOrderType() const {return orderType_;}
    Quantity GetInitialQuantity() const {return initialQuantity_;}
    Quantity GetRemainingQuantity() const {return remainingQuantity_;}
    Quantity GetFilledQuantity() const {return GetInitialQuantity() - GetRemainingQuantity();}
    bool IsFilled() const {return remainingQuantity_ == 0;}
    /* fill the quantity required in order by qty */
    void Fill(Quantity quantity){
        if(quantity > remainingQuantity_){
            std::ostringstream oss;
            oss<< "Order ("<< GetOrderId() << ") cannot be filled for more than its remaining quantity.";
            throw std::logic_error(oss.str());
        }

        remainingQuantity_-=quantity;
    }
    void ToGoodTillCancel(Price price){
       if(GetOrderType()!=OrderType::Market){
           std::ostringstream oss;
           oss << "Order (" << GetOrderId() << ") cannot have its price adjusted, only market order can.";
           throw std::logic_error(oss.str());
       }
        price_ = price;
        orderType_ = OrderType::GoodTillCancel;
    }

private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};


/* Using shared pointers for better memory management */

/* refrencing our orders using pointers */
using OrderPointer = std::shared_ptr<Order>;
/* store order pointer in list .
 *
 * list gives us an iterator that cant be in validated given the list can grow very large
    list can be dispersed in memory wherease a vector is contiguous in memory
    I am using a list here for simplicity
 */

using OrderPointers  = std::list<OrderPointer>;
