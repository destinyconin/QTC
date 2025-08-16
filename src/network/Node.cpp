#include "network/Node.h"
#include "blockchain/Blockchain.h"
#include "blockchain/Transaction.h"
#include "blockchain/Block.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using pt = boost::property_tree::ptree;

namespace QTC {

P2P::P2P(Blockchain* c) : chain_(c) {}
P2P::~P2P() { stop(); }

void P2P::listen(unsigned short port) {
  if (running_) return;
  running_ = true;
  ioc_.reset(new net::io_context());
  acc_.reset(new tcp::acceptor(*ioc_, tcp::endpoint(tcp::v4(), port)));
  acc_->set_option(net::socket_base::reuse_address(true));
  std::cout << "p2p listening on 0.0.0.0:" << port << "\n";
  do_accept();
  workers_.emplace_back([this]{ ioc_->run(); });
}

void P2P::connect(const std::string& host, unsigned short port) {
  if (!ioc_) return;
  try {
    auto s = std::make_shared<tcp::socket>(*ioc_);
    tcp::resolver res(*ioc_);
    auto it = res.resolve(host, std::to_string(port));
    net::connect(*s, it);
    auto p = std::make_shared<Peer>();
    p->sock = s;
    p->remote = host + ":" + std::to_string(port);
    {
      std::lock_guard<std::mutex> lk(mu_);
      peers_.push_back(p);
    }
    start_read(p);
    pt j; j.put("height", static_cast<unsigned long long>(chain_->getBlockCount()));
    std::ostringstream o; write_json(o, j, false);
    send_line(p, pack(Msg::Hello, o.str()));
  } catch (...) {}
}

void P2P::stop() {
  if (!running_) return;
  running_ = false;
  if (acc_) { boost::system::error_code ec; acc_->close(ec); }
  if (ioc_)  ioc_->stop();
  for (auto& t : workers_) if (t.joinable()) t.join();
  workers_.clear();
  {
    std::lock_guard<std::mutex> lk(mu_);
    peers_.clear();
  }
  acc_.reset();
  ioc_.reset();
}

void P2P::do_accept() {
  auto sock = std::make_shared<tcp::socket>(*ioc_);
  acc_->async_accept(*sock, [this, sock](const boost::system::error_code& ec){
    if (!running_) return;
    if (!ec) {
      auto p = std::make_shared<Peer>();
      p->sock = sock;
      try {
        p->remote = sock->remote_endpoint().address().to_string() + ":" +
                    std::to_string(sock->remote_endpoint().port());
      } catch (...) { p->remote = "unknown"; }
      {
        std::lock_guard<std::mutex> lk(mu_);
        peers_.push_back(p);
      }
      start_read(p);
      pt j; j.put("height", static_cast<unsigned long long>(chain_->getBlockCount()));
      std::ostringstream o; write_json(o, j, false);
      send_line(p, pack(Msg::Hello, o.str()));
    }
    if (running_) do_accept();
  });
}

void P2P::start_read(const std::shared_ptr<Peer>& p) {
  auto buf = std::make_shared<net::streambuf>();
  net::async_read_until(*p->sock, *buf, '\n',
    [this, p, buf](const boost::system::error_code& ec, std::size_t){
      if (ec) {
        std::lock_guard<std::mutex> lk(mu_);
        peers_.erase(std::remove(peers_.begin(), peers_.end(), p), peers_.end());
        return;
      }
      std::istream is(buf.get());
      std::string line; std::getline(is, line);
      Msg t; std::string payload;
      if (unpack(line, t, payload)) on_msg(p, t, payload);
      start_read(p);
    });
}

std::string P2P::pack(Msg type, const std::string& payload) {
  pt j; j.put("t", static_cast<int>(type)); j.put("p", payload);
  std::ostringstream o; write_json(o, j, false); o << "\n"; return o.str();
}
bool P2P::unpack(const std::string& line, Msg& type, std::string& payload) {
  pt j; std::istringstream i(line);
  try { read_json(i, j); } catch (...) { return false; }
  int t = j.get<int>("t", -1);
  type = static_cast<Msg>(t);
  payload = j.get<std::string>("p", "");
  return t >= 0;
}

void P2P::send_line(const std::shared_ptr<Peer>& p, const std::string& line) {
  if (!p || !p->sock) return;
  net::async_write(*p->sock, net::buffer(line), [](auto, auto){});
}
void P2P::send_all(const std::string& line, const std::shared_ptr<Peer>& except) {
  std::lock_guard<std::mutex> lk(mu_);
  for (auto& x : peers_) if (x != except) send_line(x, line);
}

void P2P::trim_seen() {
  if (seen_tx_.size() > max_seen_) { seen_tx_.clear(); }
  if (seen_block_.size() > max_seen_) { seen_block_.clear(); }
}

void P2P::on_msg(const std::shared_ptr<Peer>& p, Msg type, const std::string& payload) {
  if (type == Msg::Hello) {
    // request missing blocks if peer is ahead
    pt j; std::istringstream i(payload); try { read_json(i, j); } catch (...) { return; }
    uint64_t h = j.get<uint64_t>("height", 0);
    if (h > chain_->getBlockCount()) {
      pt q; q.put("from", static_cast<unsigned long long>(chain_->getBlockCount()));
      std::ostringstream o; write_json(o, q, false);
      send_line(p, pack(Msg::GetBlocks, o.str()));
    }
    return;
  }

  if (type == Msg::GetBlocks) {
    pt j; std::istringstream i(payload); try { read_json(i, j); } catch (...) { return; }
    uint64_t from = j.get<uint64_t>("from", 0);
    for (uint64_t k = from; k < chain_->getBlockCount(); ++k) {
      auto b = chain_->getBlockCopyByIndex(k);
      if (!b) break;
      auto bt = b->toPtree();
      std::ostringstream o; write_json(o, bt, false);
      send_line(p, pack(Msg::Block, o.str()));
    }
    return;
  }

  if (type == Msg::Block) {
    pt b; std::istringstream i(payload); try { read_json(i, b); } catch (...) { return; }
    auto blk = QTC::Block::fromPtree(b);
    if (!blk) return;
    std::string h = b.get<std::string>("hash", "");
    {
      std::lock_guard<std::mutex> lk(seen_mu_);
      if (seen_block_.count(h)) return;
      seen_block_.insert(h);
      trim_seen();
    }
    if (chain_->addBlockFromPeer(*blk)) {
      send_all(pack(Msg::Block, payload), p);
    }
    return;
  }

  if (type == Msg::Tx) {
    pt t; std::istringstream i(payload); try { read_json(i, t); } catch (...) { return; }
    auto tx = QTC::Transaction::fromPtree(t);
    if (!tx) return;
    std::string id = tx->getId();
    {
      std::lock_guard<std::mutex> lk(seen_mu_);
      if (seen_tx_.count(id)) return;
      seen_tx_.insert(id);
      trim_seen();
    }
    chain_->addTransaction(*tx);
    send_all(pack(Msg::Tx, payload), p);
    return;
  }
}

void P2P::broadcastTx(const Transaction& t) {
  auto tp = t.toPtree(); std::ostringstream o; write_json(o, tp, false);
  send_all(pack(Msg::Tx, o.str()));
}
void P2P::broadcastBlock(const Block& b) {
  auto bp = b.toPtree(); std::ostringstream o; write_json(o, bp, false);
  send_all(pack(Msg::Block, o.str()));
}

std::vector<std::string> P2P::peers() const {
  std::vector<std::string> out;
  std::lock_guard<std::mutex> lk(mu_);
  for (auto& p : peers_) out.push_back(p->remote);
  return out;
}

} // namespace QTC
