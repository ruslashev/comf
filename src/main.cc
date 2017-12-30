#include <cstdio>
#include <cmath>
#include <iostream>
#include "../thirdparty/cpr/include/cpr/cpr.h"
#include "../thirdparty/json.hpp"
#include "secretz.hh"

using json = nlohmann::json;
using money_t = long double;
using std::cout;
using std::endl;

const auto auth = cpr::Authentication{ apikey, apisecret };

std::string url(const std::string &x) {
  return "https://api.hitbtc.com/api/2/" + x;
}

money_t num(const std::string &x) {
  return std::stold(x);
}

bool feq(money_t a, money_t b, money_t epsilon = 0.0000000000000001L) {
  return std::abs(a - b) < epsilon;
}

void balance() {
  auto response = cpr::Get(url("trading/balance"), auth);
  auto j = json::parse(response.text);
  for (auto &e : j) {
    money_t avail = num(e["available"]);
    if (feq(avail, 0.))
      continue;
    cout << e["currency"] << ' ' << e["available"] << ' ' << avail << endl;
  }
}

int main(int argc, char **argv) {
  cout.precision(16);
  balance();
  // auto json = nlohmann::json::parse(response.text);
  // std::cout << json.dump(2) << std::endl;
}

