// src/wallet/Wallet.cpp
#include "wallet/Wallet.h"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <random>

namespace QTC {
static std::vector<std::string> g_addrs;

static std::string sha256(const std::string& s) {
  unsigned char h[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(s.data()), s.size(), h);
  std::ostringstream o;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    o << std::hex << std::setw(2) << std::setfill('0') << (int)h[i];
  return o.str();
}

std::string Wallet::Create() {
  std::mt19937_64 rng(static_cast<uint64_t>(std::time(nullptr)));
  std::ostringstream seed; seed << "k" << rng();
  std::string h = sha256(seed.str());
  std::string addr = "QTC" + h.substr(0, 40);
  g_addrs.push_back(addr);
  return addr;
}

const std::vector<std::string>& Wallet::All() { return g_addrs; }

}
