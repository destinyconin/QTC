#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <boost/asio.hpp>

namespace QTC {
class Transaction;
class Block;
class Blockchain;

class P2P {
public:
  explicit P2P(Blockchain* c);
  ~P2P();

  void listen(unsigned short port);
  void connect(const std::string& host, unsigned short port);
  void stop();

  void broadcastTx(const Transaction& t);
  void broadcastBlock(const Block& b);

  std::vector<std::string> peers() const;

private:
  struct Peer {
    std::shared_ptr<boost::asio::ip::tcp::socket> sock;
    std::string remote;
    std::string inbuf;
  };

  enum class Msg {
    Hello, Inv, GetBlocks, Block, Tx, Ping, Pong
  };

  Blockchain* chain_{nullptr};

  std::unique_ptr<boost::asio::io_context> ioc_;
  std::unique_ptr<boost::asio::ip::tcp::acceptor> acc_;
  std::vector<std::thread> workers_;
  mutable std::mutex mu_;
  std::vector<std::shared_ptr<Peer>> peers_;
  bool running_{false};

  // dedup
  mutable std::mutex seen_mu_;
  std::unordered_set<std::string> seen_tx_;
  std::unordered_set<std::string> seen_block_;
  size_t max_seen_{20000};

  void do_accept();
  void start_read(const std::shared_ptr<Peer>& p);

  static std::string pack(Msg type, const std::string& payload);
  static bool unpack(const std::string& line, Msg& type, std::string& payload);

  void on_msg(const std::shared_ptr<Peer>& p, Msg type, const std::string& payload);
  void send_line(const std::shared_ptr<Peer>& p, const std::string& line);
  void send_all(const std::string& line, const std::shared_ptr<Peer>& except = nullptr);
  void trim_seen();
};

} // namespace QTC
