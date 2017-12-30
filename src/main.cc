#include <cstdio>
#include <iostream>
#include "../thirdparty/cpr/include/cpr/cpr.h"
#include "../thirdparty/pjson.h"

int main(int argc, char **argv) {
  auto response = cpr::Get(cpr::Url{"https://httpbin.org/get"});
  std::cout << response.text << std::endl;
  pjson::document doc;
  doc.deserialize_in_place(response.text);
}

