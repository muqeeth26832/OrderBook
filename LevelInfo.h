#pragma once

#include "Usings.h"

/* a level has specific price and quantity of those orders */

struct LevelInfo {
  Price price_;
  Quantity quantity_;
};

/* as each side has multiple levels */
using LevelInfos = std::vector<LevelInfo>;
