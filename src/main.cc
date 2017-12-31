#include <cstdio>
#include <cmath>
#include <iostream>
#include <uWS/uWS.h>
#include <openssl/hmac.h>
#include "../thirdparty/json.hpp"
#include "secretz.hh"
#include "money.hh"

using json = nlohmann::json;
using namespace std;

#define die(...) do { printf(__VA_ARGS__); puts(""); exit(1); } while (0)

// map<string, pair<string, string>> symbols;

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

enum class state_k {
  unconnected,
  connected,
  awaiting_auth_response,
  awaiting_balance_response,
  awaiting_symbols_response,
  ready
};
const char* state_to_str(state_k state) {
  switch (state) {
    case state_k::unconnected: return "unconnected";
    case state_k::connected: return "connected";
    case state_k::awaiting_auth_response: return "awaiting_auth_response";
    case state_k::awaiting_balance_response: return "awaiting_balance_response";
    case state_k::ready: return "ready";
    default: return "unknown state";
  }
}

void start_loop() {
  const string url = "wss://api.hitbtc.com/api/2/ws";
  uWS::Hub h;
  state_k state = state_k::unconnected;
  static bool first_time = true;

  h.onError([](void *user) {
    explain_error((long)user);
  });

  h.onConnection([&h, &state](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest) {
    puts("connect");
    state = state_k::connected;
    if (first_time) {
      string txmsg = auth();
      printf("connected, sending auth... %s\n", txmsg.c_str());
      ws->send(txmsg.c_str(), txmsg.length(), uWS::OpCode::TEXT);
      state = state_k::awaiting_auth_response;
    }
  });

  map<string, money_t> balance;

  h.onMessage([&h, &state, &balance](uWS::WebSocket<uWS::CLIENT> *ws, char *msg,
        size_t length, uWS::OpCode opCode) {
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
        cout << rxj.dump() << endl;
        state = state_k::ready;
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
  cout.precision(16);
  // load_pair_symbols();
  // balance();
  start_loop();
}

