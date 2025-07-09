#include "OrderType.h"
#include "Orderbook.h"
#include "Usings.h"
#include <memory>

#include <iostream>


int main() {
    Orderbook ob;
    ob.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy,  Price(101), Quantity(10)));
    ob.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Buy,  Price(102), Quantity(40)));
    ob.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 3, Side::Sell, Price(105), Quantity(20)));
    ob.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 4, Side::Sell, Price(106), Quantity(35)));
    ob.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 5, Side::Sell, Price(107), Quantity(15)));

    ob.PrintOrderbook();

    return 0;
}
