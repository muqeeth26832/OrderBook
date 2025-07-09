#pragma once

#include "TradeInfo.h"

/* Trade is used to represent an order match */
/* Trade info has order id, price , quantity */
/* for a match / trade we store both sides
 *
  the bids side and asks side (buy and sell)
 *
 */

class Trade {
public:
    Trade(const TradeInfo& bidTrade,const TradeInfo& askTrade)
    : bidTrade_(bidTrade), askTrade_(askTrade)
    {}

    const TradeInfo& GetBidTrade() const {return bidTrade_;}
    const TradeInfo& GetAskTrade() const {return askTrade_;}

private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
};

/* as 1 order can fill up a bunch of trades
 *
 * lets store then in a vector
 */
using Trades= std::vector<Trade>;
