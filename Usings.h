#pragma once

#include <vector>
#include <cstdint>

/* Defining some type used in the order book */

using Price = std::int32_t; // can be negative
using Quantity = std::uint32_t; // should be positive
// order ids
using OrderId = std::uint64_t;
using OrderIds = std::vector<OrderId>;
