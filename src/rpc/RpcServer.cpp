// src/rpc/RpcServer.cpp
#include "rpc/RpcServer.h"
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>

namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace QTC {

static std::string pt_dump(const boost::property_tree::ptree& t) {
  std::ostringstream o; boost::property_tree::write_json(o, t, false); return o.str();
}
static bool pt_parse(const std::string& s, boost::property_tree::ptree& out) {
  std::istringstream i(s); try { boost::property_tree::read_json(i, out); return true; } catch (...) { return false; }
}

struct RpcServer::Impl {
  net::io_context ioc;
  std::unique_ptr<tcp::acceptor> acc;
  std::vector<std::thread> workers;
  std::unordered_map<std::string, Handler> routes;
  std::mutex mu;
  std::atomic<bool> running{false};

  void add(const std::string& m, Handler h) { std::lock_guard<std::mutex> lk(mu); routes[m] = std::move(h); }

  static std::string http_200(const std::string& body) {
    std::ostringstream o;
    o << "HTTP/1.1 200 OK\r\n"
      << "Content-Type: application/json\r\n"
      << "Access-Control-Allow-Origin: *\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n\r\n"
      << body;
    return o.str();
  }

  static std::string http_400() {
    static const char* body = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"parse error\"},\"id\":null}";
    std::ostringstream o;
    o << "HTTP/1.1 400 Bad Request\r\n"
      << "Content-Type: application/json\r\n"
      << "Access-Control-Allow-Origin: *\r\n"
      << "Content-Length: " << std::strlen(body) << "\r\n"
      << "Connection: close\r\n\r\n"
      << body;
    return o.str();
  }

  static bool parse_http_request(const std::string& req, std::string& body_out) {
    auto p = req.find("\r\n\r\n");
    if (p == std::string::npos) return false;
    std::string headers = req.substr(0, p + 4);
    std::string body = req.substr(p + 4);
    std::size_t cl = 0;
    std::istringstream hs(headers);
    std::string line;
    while (std::getline(hs, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      auto pos = line.find(':');
      if (pos != std::string::npos) {
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(val.begin());
        if (key == "Content-Length" || key == "content-length") cl = static_cast<std::size_t>(std::stoul(val));
      }
    }
    if (cl > 0 && body.size() < cl) return false;
    if (cl > 0 && body.size() > cl) body.resize(cl);
    body_out.swap(body);
    return true;
  }

  void handle_session(tcp::socket s) {
    try {
      net::streambuf buf;
      boost::system::error_code ec;
      net::read_until(s, buf, "\r\n\r\n", ec);
      if (ec && ec != net::error::eof) return;
      std::istream is(&buf);
      std::string req_headers((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());

      std::string body;
      if (!parse_http_request(req_headers, body)) {
        auto resp = http_400(); net::write(s, net::buffer(resp)); return;
      }
      boost::property_tree::ptree call;
      if (!pt_parse(body, call)) {
        auto resp = http_400(); net::write(s, net::buffer(resp)); return;
      }
      std::string id = call.get<std::string>("id", "");
      std::string method = call.get<std::string>("method", "");
      auto params = call.get_child_optional("params");

      Handler h;
      {
        std::lock_guard<std::mutex> lk(mu);
        auto it = routes.find(method);
        if (it != routes.end()) h = it->second;
      }
      boost::property_tree::ptree p = params ? *params : boost::property_tree::ptree{};
      boost::property_tree::ptree res;
      res.put("jsonrpc", "2.0");
      res.put("id", id);

      if (!h) {
        boost::property_tree::ptree e; e.put("code", -32601); e.put("message", "method not found");
        res.add_child("error", e);
        auto resp = http_200(pt_dump(res)); net::write(s, net::buffer(resp)); return;
      }

      boost::property_tree::ptree result = h(p);
      res.add_child("result", result);
      auto resp = http_200(pt_dump(res)); net::write(s, net::buffer(resp));
    } catch (...) {}
  }

  void do_accept() {
    auto sock = std::make_shared<tcp::socket>(ioc);
    acc->async_accept(*sock, [this, sock](const boost::system::error_code& ec) {
      if (!ec) std::thread([this, s = std::move(*sock)]() mutable { handle_session(std::move(s)); }).detach();
      if (running.load()) do_accept();
    });
  }

  void start(const std::string& host, unsigned short port, int threads) {
    running.store(true);
    acc.reset(new tcp::acceptor(ioc));
    tcp::endpoint ep = tcp::endpoint(net::ip::make_address(host), port);
    boost::system::error_code ec;
    acc->open(ep.protocol(), ec);
    acc->set_option(net::socket_base::reuse_address(true), ec);
    acc->bind(ep, ec);
    acc->listen(net::socket_base::max_listen_connections, ec);
    std::cout << "rpc listening on " << host << ":" << port << "\n";
    do_accept();
    if (threads < 1) threads = 1;
    for (int i = 0; i < threads; ++i) workers.emplace_back([this]{ ioc.run(); });
  }

  void stop() {
    running.store(false);
    if (acc) acc->close();
    ioc.stop();
    for (auto& t : workers) if (t.joinable()) t.join();
    workers.clear();
  }
};

RpcServer::RpcServer() : impl_(new Impl) {}
RpcServer::~RpcServer() { impl_->stop(); }
void RpcServer::add(const std::string& m, Handler h) { impl_->add(m, std::move(h)); }
void RpcServer::start(const std::string& host, unsigned short port, int threads) { impl_->start(host, port, threads); }
void RpcServer::stop() { impl_->stop(); }

}
