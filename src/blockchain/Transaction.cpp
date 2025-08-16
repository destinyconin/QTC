#include "blockchain/Transaction.h"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <ctime>

using pt = boost::property_tree::ptree;

namespace QTC {

static std::string sha256(const std::string& s) {
  unsigned char h[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(s.data()), s.size(), h);
  std::ostringstream o;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    o << std::hex << std::setw(2) << std::setfill('0') << (int)h[i];
  return o.str();
}

Transaction::Transaction(const std::string& from, const std::string& to, uint64_t amount, uint64_t fee)
  : from_(from), to_(to), amount_(amount), fee_(fee) {
  ts_ = static_cast<uint64_t>(std::time(nullptr));
  computeId();
}

void Transaction::computeId() {
  std::ostringstream s; s << from_ << to_ << amount_ << fee_ << ts_;
  id_ = sha256(s.str());
}

const std::string& Transaction::getId() const { return id_; }
const std::string& Transaction::getFrom() const { return from_; }
const std::string& Transaction::getTo() const { return to_; }
uint64_t Transaction::getAmount() const { return amount_; }
uint64_t Transaction::getFee() const { return fee_; }
uint64_t Transaction::getTimestamp() const { return ts_; }

pt Transaction::toPtree() const {
  pt t;
  t.put("id", id_);
  t.put("from", from_);
  t.put("to", to_);
  t.put("amount", static_cast<unsigned long long>(amount_));
  t.put("fee", static_cast<unsigned long long>(fee_));
  t.put("timestamp", static_cast<unsigned long long>(ts_));
  return t;
}

std::unique_ptr<Transaction> Transaction::fromPtree(const pt& t) {
  std::string from = t.get<std::string>("from", "");
  std::string to = t.get<std::string>("to", "");
  uint64_t amount = t.get<uint64_t>("amount", 0);
  uint64_t fee = t.get<uint64_t>("fee", 0);
  uint64_t ts = t.get<uint64_t>("timestamp", 0);
  auto tx = std::unique_ptr<Transaction>(new Transaction(from, to, amount, fee));
  tx->ts_ = ts ? ts : tx->ts_;
  tx->computeId();
  return tx;
}

}
