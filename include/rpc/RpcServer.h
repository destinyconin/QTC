// include/rpc/RpcServer.h
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <boost/property_tree/ptree.hpp>

namespace QTC {

class RpcServer {
public:
  using PTree = boost::property_tree::ptree;
  using Handler = std::function<PTree(const PTree& params)>;

  RpcServer();
  ~RpcServer();

  void add(const std::string& method, Handler h);
  void start(const std::string& host, unsigned short port, int threads);
  void stop();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}
