#include <cstdio>
#include <iostream>
#include "../thirdparty/cpr/include/cpr/cpr.h"
#include "../thirdparty/json.hpp"

int main(int argc, char **argv) {
  auto response = cpr::Get(cpr::Url{"https://httpbin.org/get"});
  std::cout << response.text << std::endl;
  auto json = nlohmann::json::parse(response.text);
  std::cout << json.dump(4) << std::endl;
}

