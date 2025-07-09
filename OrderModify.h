#pragma once

#include "Order.h"


/* Light weight representation of an order  used to modify an existing order */

class OrderModify {
public:
    OrderModify(OrderId orderId,Side side, Price price,Quantity quantity)
    : orderId_{orderId}, side_{side}, price_{price}, quantity_{quantity}
    {}
    OrderId GetOrderId() const {return orderId_;}
    Side GetSide() const {return side_;}
    Price GetPrice() const {return price_;}
    Quantity GetQuantity() const {return quantity_;}

    /* Modify Order and return a new one */
    OrderPointer ToOrderPointer(OrderType type) const {
        return std::make_shared<Order>(type,GetOrderId(),GetSide(),GetPrice(),GetQuantity());
    }
private:
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity quantity_;
};
