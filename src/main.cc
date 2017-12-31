#include "common.hh"
#include "secretz.hh"
#include "money.hh"
#include "rest.hh"

using namespace std;

bool verbose = true;

void balance() {
  json j = rest("trading/balance");
  for (auto &e : j) {
    money_t avail = num(e["available"]);
    if (feq(avail, 0.))
      continue;
    cout << e["currency"].get<std::string>() << " " << avail << endl;
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

int main(int argc, char **argv) {
  cout.precision(16);
  load_pair_symbols();
  balance();
}

