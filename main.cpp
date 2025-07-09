#include "OrderType.h"
#include "Orderbook.h"
#include "Usings.h"
#include <memory>

#include <iostream>

int main()
{
    Orderbook orderbook;
    // Do work.
    const OrderId orderId= 1;



    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel,orderId,Side::Buy,Price(100),Quantity(10)));
    std::cout<<orderbook.Size()<<std::endl;
    orderbook.CancelOrder(orderId);
    std::cout<<orderbook.Size()<<std::endl;

    return 0;
}
