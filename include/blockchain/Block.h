#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <boost/property_tree/ptree.hpp>

namespace QTC {
class Transaction;

class Block {
public:
  Block(uint32_t idx, const std::string& prev, uint32_t diff);

  void addTransaction(const Transaction& tx);
  void mine();

  const std::string& getHash() const;
  const std::string& getPrev() const;
  uint32_t getIndex() const;
  uint32_t getDifficulty() const;
  uint64_t getTimestamp() const;
  const std::vector<Transaction>& getTransactions() const;

  boost::property_tree::ptree toPtree() const;
  static std::unique_ptr<Block> fromPtree(const boost::property_tree::ptree& b);
  void setHashForImport(const std::string& h);

private:
  uint32_t index_{0};
  uint64_t ts_{0};
  std::vector<Transaction> txs_;
  std::string prev_;
  std::string hash_;
  uint32_t nonce_{0};
  uint32_t diff_{0};
  std::string merkle_;

  void calcMerkle();
  std::string calcHash() const;
};

}
