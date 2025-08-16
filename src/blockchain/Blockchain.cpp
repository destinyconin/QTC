#include "blockchain/Blockchain.h"
#include "blockchain/Block.h"
#include "blockchain/Transaction.h"
#include "config/Constants.h"
#include "network/Node.h"
#include <algorithm>
#include <iostream>

namespace QTC {

Blockchain::Blockchain() { createGenesisBlock(); }

void Blockchain::createGenesisBlock() {
  auto g = std::unique_ptr<Block>(new Block(0, "0", difficulty_));
  g->mine();
  chain_.push_back(std::move(g));
}

Block* Blockchain::getLatestBlock() { return chain_.back().get(); }

uint64_t Blockchain::getBlockCount() const { return static_cast<uint64_t>(chain_.size()); }

bool Blockchain::validAddress(const std::string& a) const {
  return a.size() >= 8 && a.rfind("QTC", 0) == 0;
}

void Blockchain::addTransaction(const Transaction& tx) {
  if (!validAddress(tx.getFrom())) return;
  if (!validAddress(tx.getTo())) return;
  if (tx.getFrom() == "COINBASE") return;
  if (tx.getAmount() == 0) return;
  uint64_t need = tx.getAmount() + tx.getFee();
  uint64_t bal = 0;
  auto it = balances_.find(tx.getFrom());
  if (it != balances_.end()) bal = it->second;
  if (bal < need) return;
  pending_.push_back(tx);
  if (p2p_) p2p_->broadcastTx(tx);
}

void Blockchain::minePendingTransactions(const std::string& minerAddress) {
  if (mining_.exchange(true)) return;
  auto nb = std::unique_ptr<Block>(new Block(static_cast<uint32_t>(chain_.size()), getLatestBlock()->getHash(), difficulty_));
  Transaction coin("COINBASE", minerAddress, BLOCK_REWARD, 0);
  nb->addTransaction(coin);
  for (const auto& t : pending_) nb->addTransaction(t);
  nb->mine();
  updateBalances(nb.get());
  chain_.push_back(std::move(nb));
  pending_.clear();
  if (p2p_ && chain_.back()) p2p_->broadcastBlock(*chain_.back());
  mining_ = false;
}

void Blockchain::updateBalances(Block* block) {
  for (const auto& tx : block->getTransactions()) {
    if (tx.getFrom() != "COINBASE") {
      auto& fb = balances_[tx.getFrom()];
      uint64_t spend = tx.getAmount() + tx.getFee();
      if (fb >= spend) fb -= spend; else fb = 0;
    } else {
      uint64_t newMint = minted_ + tx.getAmount();
      minted_ = (newMint > TOTAL_SUPPLY) ? TOTAL_SUPPLY : newMint;
    }
    auto& tb = balances_[tx.getTo()];
    uint64_t addv = tb + tx.getAmount();
    tb = (addv < tb) ? UINT64_MAX : addv;
  }
}

uint64_t Blockchain::getBalance(const std::string& addr) const {
  auto it = balances_.find(addr);
  return (it != balances_.end()) ? it->second : 0ULL;
}

bool Blockchain::isChainValid() {
  for (size_t i = 1; i < chain_.size(); ++i) {
    if (chain_[i]->getPrev() != chain_[i-1]->getHash()) return false;
  }
  return true;
}

const Transaction* Blockchain::getPendingById(const std::string& id) const {
  for (const auto& t : pending_) if (t.getId() == id) return &t;
  return nullptr;
}

std::unique_ptr<Block> Blockchain::getBlockCopyByIndex(uint64_t i) {
  if (i >= chain_.size()) return nullptr;
  return std::unique_ptr<Block>(new Block(*chain_[i]));
}

bool Blockchain::addBlockFromPeer(const Block& b) {
  if (b.getIndex() != chain_.size()) return false;
  if (b.getPrev() != getLatestBlock()->getHash()) return false;
  auto nb = std::unique_ptr<Block>(new Block(b));
  updateBalances(nb.get());
  chain_.push_back(std::move(nb));
  return true;
}

void Blockchain::setP2P(P2P* p) { p2p_ = p; }
P2P* Blockchain::p2p() const { return p2p_; }

}
