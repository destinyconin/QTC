#include "blockchain/Blockchain.h"
#include "wallet/Wallet.h"
#include "network/Node.h"
#include "rpc/RpcServer.h"
#include <iostream>
#include <thread>
#include <chrono>

using PT = QTC::RpcServer::PTree;

int main(int, char**) {
  QTC::Blockchain chain;
  QTC::P2P p2p(&chain);
  chain.setP2P(&p2p);
  p2p.listen(18444);

  QTC::RpcServer rpc;

  rpc.add("getblockcount", [&chain](const PT&) {
    PT r; r.put("", static_cast<unsigned long long>(chain.getBlockCount())); return r;
  });

  rpc.add("createaddress", [](const PT&) {
    PT r; r.put("", QTC::Wallet::Create()); return r;
  });

  rpc.add("listaddresses", [](const PT&) {
    PT arr; 
    for (const auto& a : QTC::Wallet::All()) { 
      PT v; v.put("", a); 
      arr.push_back(std::make_pair("", v)); 
    } 
    return arr;
  });

  rpc.add("getbalance", [&chain](const PT& p) {
    std::string a; 
    for (auto& v : p) { a = v.second.get_value<std::string>(); break; }
    PT r; r.put("", static_cast<unsigned long long>(chain.getBalance(a))); 
    return r;
  });

  rpc.add("sendtoaddress", [&chain](const PT& p) {
    std::string to; uint64_t amount=0, fee=0;
    int i=0; 
    for (auto& v : p) { 
      if(i==0) to=v.second.get_value<std::string>(); 
      if(i==1) amount=v.second.get_value<uint64_t>(); 
      if(i==2) fee=v.second.get_value<uint64_t>(); 
      ++i; 
    }
    if (to.empty() || amount==0) { PT r; r.put("", ""); return r; }
    std::string from = "QTC00000000000000000000000000000000000000";
    QTC::Transaction tx(from, to, amount, fee);
    chain.addTransaction(tx);
    PT r; r.put("", tx.getId()); 
    return r;
  });

  rpc.add("generate", [&chain](const PT& p) {
    std::string to; 
    for (auto& v : p) { to=v.second.get_value<std::string>(); break; }
    if (to.empty()) { 
      const auto& all = QTC::Wallet::All(); 
      if (!all.empty()) to = all.front(); 
    }
    if (to.empty()) { PT r; r.put("", 0); return r; }
    chain.minePendingTransactions(to);
    PT r; r.put("", 1); 
    return r;
  });

  // NEW: connect to a peer
  rpc.add("connectpeer", [&p2p](const PT& p) {
    std::string host; uint16_t port = 0;
    int i=0; 
    for (auto& v : p) { 
      if (i==0) host = v.second.get_value<std::string>(); 
      if (i==1) port = v.second.get_value<uint16_t>(); 
      ++i; 
    }
    if (host.empty() || port == 0) { PT r; r.put("", 0); return r; }
    p2p.connect(host, port);
    PT r; r.put("", 1); 
    return r;
  });

  // NEW: list connected peers
  rpc.add("peers", [&p2p](const PT&) {
    PT arr; 
    for (auto& s : p2p.peers()) { 
      PT v; v.put("", s); 
      arr.push_back(std::make_pair("", v)); 
    }
    return arr;
  });

  rpc.start("127.0.0.1", 18443, 4);

  for (;;) std::this_thread::sleep_for(std::chrono::seconds(60));
  return 0;
}
