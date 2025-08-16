#include "blockchain/Block.h"
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

Block::Block(uint32_t idx, const std::string& prev, uint32_t diff)
  : index_(idx), prev_(prev), diff_(diff) {
  ts_ = static_cast<uint64_t>(std::time(nullptr));
}

void Block::addTransaction(const Transaction& tx) { txs_.push_back(tx); }

void Block::calcMerkle() {
  std::vector<std::string> h;
  for (auto& t : txs_) h.push_back(t.getId());
  if (h.empty()) { merkle_.clear(); return; }
  while (h.size() > 1) {
    std::vector<std::string> n;
    for (size_t i = 0; i < h.size(); i += 2) {
      const std::string& a = h[i];
      const std::string& b = (i + 1 < h.size()) ? h[i + 1] : h[i];
      n.push_back(sha256(a + b));
    }
    h.swap(n);
  }
  merkle_ = h[0];
}

std::string Block::calcHash() const {
  std::ostringstream s;
  s << index_ << ts_ << prev_ << merkle_ << nonce_ << diff_;
  return sha256(s.str());
}

void Block::mine() {
  calcMerkle();
  std::string target(diff_, '0');
  for (;;) {
    ++nonce_;
    hash_ = calcHash();
    if (hash_.substr(0, diff_) == target) break;
  }
}

const std::string& Block::getHash() const { return hash_; }
const std::string& Block::getPrev() const { return prev_; }
uint32_t Block::getIndex() const { return index_; }
uint32_t Block::getDifficulty() const { return diff_; }
uint64_t Block::getTimestamp() const { return ts_; }
const std::vector<Transaction>& Block::getTransactions() const { return txs_; }

pt Block::toPtree() const {
  pt b;
  b.put("index", static_cast<unsigned long long>(index_));
  b.put("timestamp", static_cast<unsigned long long>(ts_));
  b.put("prev", prev_);
  b.put("hash", hash_);
  b.put("nonce", static_cast<unsigned long long>(nonce_));
  b.put("difficulty", static_cast<unsigned long long>(diff_));
  b.put("merkle", merkle_);
  pt arr;
  for (auto& t : txs_) arr.push_back(std::make_pair("", t.toPtree()));
  b.add_child("tx", arr);
  return b;
}

std::unique_ptr<Block> Block::fromPtree(const pt& b) {
  uint32_t idx = static_cast<uint32_t>(b.get<uint64_t>("index", 0));
  std::string prev = b.get<std::string>("prev", "");
  uint32_t diff = static_cast<uint32_t>(b.get<uint64_t>("difficulty", 0));
  auto blk = std::unique_ptr<Block>(new Block(idx, prev, diff));
  blk->ts_ = b.get<uint64_t>("timestamp", blk->ts_);
  blk->nonce_ = static_cast<uint32_t>(b.get<uint64_t>("nonce", 0));
  blk->merkle_ = b.get<std::string>("merkle", "");
  auto arr = b.get_child_optional("tx");
  if (arr) for (auto& it : *arr) { auto tx = Transaction::fromPtree(it.second); if (tx) blk->txs_.push_back(*tx); }
  blk->hash_ = b.get<std::string>("hash", "");
  return blk;
}

void Block::setHashForImport(const std::string& h) { hash_ = h; }

}
