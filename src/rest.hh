#pragma once

#include "common.hh"

static json _rest(const std::string &x, bool public_api) {
  static const auto auth = cpr::Authentication{ apikey, apisecret };
  const std::string url = "https://api.hitbtc.com/api/2/";
  auto response = public_api ? cpr::Get(url + x) : cpr::Get(url + x, auth);
  if (response.status_code != 200)
    printf("api/1 malfunction: status_code=%d\n", response.status_code);
  return json::parse(response.text);
}

json rest(const std::string &x) {
  return _rest(x, false);
}

json rest_public(const std::string &x) {
  return _rest("public/" + x, true);
}

