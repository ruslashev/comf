#include "common.hh"
#include "secretz.hh"
#include "money.hh"
#include "rest.hh"
#include <uWS/uWS.h>

using namespace std;

#define die(...) do { printf(__VA_ARGS__); puts(""); exit(1); } while (0)

bool verbose = true;

void balance() {
  json j = rest("trading/balance");
  for (auto &e : j) {
    money_t avail = num(e["available"]);
    if (feq(avail, 0.))
      continue;
    cout << e["currency"].get<string>() << " " << avail << endl;
  }
}

map<string, pair<string, string>> symbols;

void load_pair_symbols() {
  json j = rest_public("symbol");
  for (auto &e : j)
    symbols[e["id"]] = pair<string, string>(e["baseCurrency"],
        e["quoteCurrency"]);

  if (verbose)
    for (auto it = symbols.begin(); it != symbols.end(); ++it)
      cout << it->first << " => (" << it->second.first << "," <<
        it->second.second << ")\n";
}

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
      die("%d should not emit error!", err);
  }
}

void start_loop() {
  const string url = "wss://api.hitbtc.com/api/2/ws";
  uWS::Hub h;

  h.onError([](void *user) {
    explain_error((long)user);
  });

  h.onConnection([&h](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest req) {
    printf("connect\n");
    printf("can send\n");
    h.getDefaultGroup<uWS::CLIENT>().close();
  });

  h.onDisconnection([](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *message,
        size_t length) {
    printf("client close (%d): <%s>\n", code, string(message, length).c_str());
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

