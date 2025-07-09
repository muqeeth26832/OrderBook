#pragma once

/* enum for Types of Orders that are supported by the orderbook */

enum class OrderType {
    GoodTillCancel, // keep it till we cancel
    FillAndKill, // Give me as much as i can get then kill my order
    FillOrKill,  // Either fill totally or kill my order
    GoodForDay, // keep it till we cancel or 4pm whichever is early
    Market, // we dont care about price we care about quantity
};
