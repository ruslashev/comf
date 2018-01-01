#include <cstdio>
#include <cmath>
#include <set>
#include <iostream>
#include <uWS/uWS.h>
#include <openssl/hmac.h>
#include "../thirdparty/json.hpp"
#include "secretz.hh"
#include "money.hh"

using json = nlohmann::json;
using namespace std;

#define die(...) do { printf(__VA_ARGS__); puts(""); exit(1); } while (0)

void explain_error(long err) {
  string text = "Client emitted error on ";
  switch (err) {
    case 1:  text += "invalid URI"; break;
    case 2:  text += "resolve failure"; break;
    case 3:  text += "connection timeout (non-SSL)"; break;
    case 5:  text += "connection timeout (SSL)"; break;
    case 6:  text += "HTTP response without upgrade (non-SSL)"; break;
    case 7:  text += "HTTP response without upgrade (SSL)"; break;
    case 10: text += "poll error"; break;
    case 11: text += "invalid protocol";
      static int protocol_error_count = 0;
      ++protocol_error_count;
      if (protocol_error_count > 1)
        die("%d errors emitted for one connection!", protocol_error_count);
      break;
    default:
      die("%d should not emit error!", (int)err);
  }
  cout << text << endl;
}

string auth() {
  char nonce[] = ":LWE%daF;XDweLIE135";
  unsigned char *signed_nonce = HMAC(EVP_sha256(), apisecret, strlen(apisecret),
      (unsigned char*)nonce, strlen(nonce), nullptr, nullptr);
  char signature[65];
  for (int i = 0; i < 32; ++i)
    sprintf(&signature[i * 2], "%02x", (unsigned int)signed_nonce[i]);
  json j = {
    { "method", "login" },
    { "params",
      {
        { "algo", "HS256" },
        { "pKey", apikey },
        { "nonce", nonce },
        { "signature", signature }
      }
    }
  };
  return j.dump();
}

string get_trading_balance() {
  json j = {
    { "method", "getTradingBalance" },
    { "params", {} },
    { "id", 420 }
  };
  return j.dump();
}

string get_symbols() {
  json j = {
    { "method", "getSymbols" },
    { "params", {} },
    { "id", 420 }
  };
  return j.dump();
}

string subscribe_symbol_ticker(const string &symbol) {
  json j = {
    { "method", "subscribeTicker" },
    { "params",
      {
        { "symbol", symbol }
      }
    },
    { "id", 420 }
  };
  return j.dump();
}

enum class state_k {
  unconnected,
  connected,
  awaiting_auth_response,
  awaiting_balance_response,
  awaiting_symbols_response,
  awaiting_ticker_sub_response,
  ready
};

const char* state_to_str(state_k state) {
  switch (state) {
    case state_k::unconnected:                  return "unconnected";
    case state_k::connected:                    return "connected";
    case state_k::awaiting_auth_response:       return "awaiting_auth_response";
    case state_k::awaiting_balance_response:    return "awaiting_balance_response";
    case state_k::awaiting_symbols_response:    return "awaiting_symbols_response";
    case state_k::awaiting_ticker_sub_response: return "awaiting_ticker_sub_response";
    case state_k::ready:                        return "ready";
    default:                                    return "unknown state";
  }
}

using mp = pair<money_t, money_t>;

void magic(const map<string, money_t> &balance, const set<string> &currencies,
    const map<string, pair<string, string>> &symbols,
    const map<string, mp> &prices) {
  if (!prices.count("BTCUSD") || !prices.count("ETHBTC")
      || !prices.count("ETHUSD"))
    return;

  // puts("");
  // for (auto it = prices.begin(); it != prices.end(); ++it)
  //   cout << it->first << " = " << it->second.first << " " << it->second.first << endl;

  enum type_k { UXB = 0, UXBE = 1, UXE = 2, UXEB = 3 };
  type_k type;
  string currency;
  money_t max_profit = 0.;

  const mp btcusd = prices.at("BTCUSD"), ethbtc = prices.at("ETHBTC"),
        ethusd = prices.at("ETHUSD");
  const money_t bu = btcusd.first, be = ethbtc.second, eu = ethusd.first,
        eb = ethbtc.first;
  for (const string &x : currencies) {
    if (x == "USD" || x == "ETH" || x == "BTC")
      continue;
    if (!prices.count(x + "USD") || !prices.count(x + "ETH") ||
        !prices.count(x + "BTC"))
      continue;
    const money_t ux = 1.L / prices.at(x + "USD").first,
          xb = prices.at(x + "BTC").second,
          xe = prices.at(x + "ETH").second;
    if (ux * xb * bu > max_profit) {
      max_profit = ux * xb * bu;
      type = UXB;
      currency = x;
    }
    if (ux * xb * be * eu > max_profit) {
      max_profit = ux * xb * be * eu;
      type = UXBE;
      currency = x;
    }
    if (ux * xe * eu > max_profit) {
      max_profit = ux * xe * eu;
      type = UXE;
      currency = x;
    }
    if (ux * xe * eb * bu > max_profit) {
      max_profit = ux * xe * eb * bu;
      type = UXEB;
      currency = x;
    }
  }
  cout << "max_profit=" << max_profit << endl;
  cout << "type=" << type << endl;
  cout << "currency=" << currency << endl;
  cout << endl;
}

void start_loop() {
  const string url = "wss://api.hitbtc.com/api/2/ws";
  uWS::Hub h;
  state_k state = state_k::unconnected;
  bool first_time = true;

  h.onError([](void *user) {
    explain_error((long)user);
  });

  h.onConnection([&h, &state, &first_time](uWS::WebSocket<uWS::CLIENT> *ws,
        uWS::HttpRequest) {
    state = state_k::connected;
    if (first_time) {
      first_time = false;
      string txmsg = auth();
      printf("connected, sending auth... %s\n", txmsg.c_str());
      ws->send(txmsg.c_str(), txmsg.length(), uWS::OpCode::TEXT);
      state = state_k::awaiting_auth_response;
    }
  });

  map<string, money_t> balance;
  set<string> currencies;
  map<string, pair<string, string>> symbols;
  map<string, mp> prices;

  h.onMessage([&h, &state, &balance, &symbols, &prices, &currencies](
        uWS::WebSocket<uWS::CLIENT> *ws, char *msg, size_t length,
        uWS::OpCode opCode) {
    json rxj = json::parse(string(msg, length));
    switch (state) {
      case state_k::awaiting_auth_response: {
        if (!rxj["result"])
          die("failed to auth");
        string txmsg = get_trading_balance();
        printf("authenticated, sending balance query... %s\n", txmsg.c_str());
        ws->send(txmsg.c_str(), txmsg.length(), uWS::OpCode::TEXT);
        state = state_k::awaiting_balance_response;
        break;
      }
      case state_k::awaiting_balance_response: {
        for (auto &e : rxj["result"]) {
          std::string availstr = e["available"];
          if (availstr == "0")
            continue;
          money_t avail = num(availstr);
          balance[e["currency"]] = avail;
          cout << e["currency"].get<string>() << " " << avail << endl;
        }
        string txmsg = get_symbols();
        printf("rx balance, sending symbols query... %s\n", txmsg.c_str());
        ws->send(txmsg.c_str(), txmsg.length(), uWS::OpCode::TEXT);
        state = state_k::awaiting_symbols_response;
        break;
      }
      case state_k::awaiting_symbols_response: {
        printf("rx symbols, sending symbol ticker subs...\n");
        for (auto &e : rxj["result"]) {
          currencies.insert(e["baseCurrency"].get<string>());
          symbols[e["id"]] = pair<string, string>(e["baseCurrency"],
              e["quoteCurrency"]);
          string txmsg = subscribe_symbol_ticker(e["id"]);
          ws->send(txmsg.c_str(), txmsg.length(), uWS::OpCode::TEXT);
        }
        state = state_k::ready;
        puts("ready");
        break;
      }
      case state_k::ready: {
        int chan = rxj.count("channel"), method = rxj.count("method");
        if (chan || method) {
          json p;
          if (chan) {
            if (rxj["channel"] == "ticker")
              p = rxj["data"];
            else
              die("rx unknown channel");
          }
          if (method) {
            if (rxj["method"] == "ticker")
              p = rxj["params"];
            else
              die("rx unknown method");
          }
          money_t ask = !p["ask"].is_null() ? num(p["ask"]) : 0.,
                  bid = !p["bid"].is_null() ? num(p["bid"]) : 0.;
          prices[p["symbol"]] = { ask, bid };
          // cout << "get " << p["symbol"] << " = " << prices[p["symbol"]].first
          //   << " " << prices[p["symbol"]].second << endl;
          magic(balance, currencies, symbols, prices);
          break;
        }
        if (rxj.count("result")) {
          if (!rxj["result"])
            die("request failed");
          break;
        }
        cout << "rx unknown msg: " << rxj.dump() << endl;
        break;
      }
      default: die("unhandled state in onMessage");
    }
  });

  h.onDisconnection([](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *msg,
        size_t length) {
    printf("client close (%d): <%s>\n", code, string(msg, length).c_str());
  });

  h.listen(3000);
  h.connect(url, nullptr);
  h.run();
  puts("Falling through");
}

int main(int argc, char **argv) {
  cout << fixed;
  cout.precision(15);
  // load_pair_symbols();
  // balance();
  start_loop();
}

