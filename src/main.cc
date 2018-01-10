#include <cstdio>
#include <cmath>
#include <set>
#include <iostream>
#include <fstream>
#include <deque>
#include <uWS/uWS.h>
#include <openssl/hmac.h>
#include "../thirdparty/json.hpp"
#include "secretz.hh"

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

using money_t = long double;

money_t num(const std::string &x) {
  return stold(x);
}

static const int C = 250;
static map<string, money_t> balance;
static vector<string> currencies;
static size_t n = 0;
static money_t exch_mtx[C][C];
static money_t profit[C][C][C];
static int path[C][C][C];

void dump() {
  ofstream out("fuark.csv");
  out << fixed;
  out.precision(15);

  out << "n," << n << endl;
  out << endl;

  for (size_t i = 0; i < currencies.size(); ++i)
    out << "," << currencies[i];
  out << endl;
  for (size_t y = 0; y < n; ++y) {
    out << currencies[y] << ",";
    for (size_t x = 0; x < n; ++x)
      out << path[y][x] << ",";
    out << endl;
  }
}

void magic() {
  puts("finding arbitrage opportunities...");

  memset(profit, 0, sizeof(profit));

  size_t i, j, k, steps;
  for (i = 0; i < n; ++i)
    for (j = 0; j < n; ++j) {
      profit[0][i][j] = (i == j) ? 1 : exch_mtx[i][j];
      path[0][i][j] = i;
    }

  for (steps = 1; steps < n; ++steps)
    for (k = 0; k < n; ++k)
      for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j)
          if (profit[steps - 1][i][k] * profit[0][k][j] > profit[steps][i][j]) {
            profit[steps][i][j] = profit[steps - 1][i][k] * profit[0][k][j];
            path[steps][i][j] = k;
          }

  size_t max_profit_currency = 0, max_profit_steps = 0;
  money_t max_profit = profit[1][0][0];
  for (steps = 1; steps < 20; ++steps)
    for (i = 0; i < n; ++i)
      if (profit[steps][i][i] > max_profit) {
        max_profit_currency = i;
        max_profit_steps = steps;
        max_profit = profit[steps][i][i];
      }

  cout << "max profit currency = " << currencies[max_profit_currency] << endl;
  cout << "max profit = " << max_profit << " in " << max_profit_steps <<
    " steps" << endl;

  deque<int> seq;
  int current_currency = max_profit_currency;
  seq.push_back(current_currency);
  for (int s = max_profit_steps; s >= 0; --s) {
    current_currency = path[s][max_profit_currency][current_currency];
    seq.push_front(current_currency);
  }

  for (i = 0; i < seq.size(); ++i)
    cout << currencies[seq[i]] << (i == seq.size() - 1 ? "\n" : " -> ");

  puts("check:");
  int prev = 0;
  for (i = 1; i < seq.size(); ++i) {
    cout << currencies[seq[prev]] << " -> " << currencies[seq[i]] << " = ";
    cout << exch_mtx[seq[prev]][seq[i]] << endl;
    prev = i;
  }

  exit(0);
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

  map<string, pair<string, string>> symbols;
  map<string, int> currency_indices;

  for (size_t y = 0; y < C; ++y)
    for (size_t x = 0; x < C; ++x)
      exch_mtx[y][x] = 0;
  int upd = 0;

  h.onMessage([&h, &state, &symbols, &currency_indices, &upd](
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
        set<string> currencies_set;
        for (auto &e : rxj["result"]) {
          symbols[e["id"]] = pair<string, string>(e["baseCurrency"],
              e["quoteCurrency"]);
          currencies_set.insert(e["baseCurrency"].get<string>());
          currencies_set.insert(e["quoteCurrency"].get<string>());
          string txmsg = subscribe_symbol_ticker(e["id"]);
          ws->send(txmsg.c_str(), txmsg.length(), uWS::OpCode::TEXT);
        }
        cout << "total currencies = " << currencies_set.size() << endl;
        currencies = vector<string>(currencies_set.begin(), currencies_set.end());
        for (size_t i = 0; i < currencies.size(); ++i)
          currency_indices[currencies[i]] = i;
        n = currencies.size();
        state = state_k::ready;
        puts("ready");
        break;
      }
      case state_k::ready: {
        int chan = rxj.count("channel"), method = rxj.count("method");
        if (chan || method) {
          json p;
          if (chan) {
            if (rxj["channel"] != "ticker")
              die("rx unknown channel");
            p = rxj["data"];
          }
          if (method) {
            if (rxj["method"] != "ticker")
              die("rx unknown method");
            p = rxj["params"];
          }
          money_t ask = p["ask"].is_null() ? 0 : num(p["ask"]),
                  bid = p["bid"].is_null() ? 0 : num(p["bid"]);
          const pair<string, string> s = symbols[p["symbol"]];
          exch_mtx[currency_indices[s.first]][currency_indices[s.second]] = bid;
          exch_mtx[currency_indices[s.second]][currency_indices[s.first]] = 1.0L / ask;
          ++upd;
          if (upd > 241)
            magic();
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
  start_loop();
}

