#pragma once

#include <string>

using money_t = float;

money_t num(const std::string &x) {
  return std::stold(x);
}

bool feq(money_t a, money_t b, money_t epsilon = 0.0000000000000001L) {
  return std::abs(a - b) < epsilon;
}

