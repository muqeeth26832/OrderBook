#pragma once

#include "Usings.h"

/* Trade Info is used to represent an order matchs information */

/* Match Orders */
struct TradeInfo{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};
