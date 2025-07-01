#pragma once

#include "Usings.h"


/* Match Orders */
struct TradeInfo{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};
