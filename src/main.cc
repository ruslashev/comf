#include "common.hh"
#include "secretz.hh"
#include "money.hh"
#include "rest.hh"

using namespace std;

void balance() {
  json j = rest("trading/balance");
  for (auto &e : j) {
    money_t avail = num(e["available"]);
    if (feq(avail, 0.))
      continue;
    cout << e["currency"].get<std::string>() << " " << avail << endl;
  }
}

int main(int argc, char **argv) {
  cout.precision(16);
  balance();
}

