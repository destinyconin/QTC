#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <atomic>
#include <map>
#include <string>
#include <vector>
#include "blockchain/Transaction.h"
#include "blockchain/Block.h"

namespace QTC {
class P2P;

class Blockchain {
public:
  Blockchain();

  uint64_t getBlockCount() const;
  bool isChainValid();
  uint64_t getBalance(const std::string& address) const;

  void addTransaction(const Transaction& tx);
  void minePendingTransactions(const std::string& minerAddress);

  const Transaction* getPendingById(const std::string& id) const;

  std::unique_ptr<Block> getBlockCopyByIndex(uint64_t i);
  bool addBlockFromPeer(const Block& b);

  void setP2P(P2P* p);
  P2P* p2p() const;

  Block* getLatestBlock();

private:
  std::vector<std::unique_ptr<Block>> chain_;
  std::vector<Transaction> pending_;
  uint32_t difficulty_{4};
  std::map<std::string, uint64_t> balances_;
  std::mutex mu_;
  std::atomic<bool> mining_{false};
  uint64_t minted_{0};
  P2P* p2p_{nullptr};

  void createGenesisBlock();
  void updateBalances(Block* block);
  bool validAddress(const std::string& a) const;
};

} // namespace QTC
