#pragma once
#include <string>
#include <cstdint>
#include <memory>
#include <boost/property_tree/ptree.hpp>

namespace QTC {

class Transaction {
public:
  Transaction(const std::string& from, const std::string& to, uint64_t amount, uint64_t fee);

  const std::string& getId() const;
  const std::string& getFrom() const;
  const std::string& getTo() const;
  uint64_t getAmount() const;
  uint64_t getFee() const;
  uint64_t getTimestamp() const;

  boost::property_tree::ptree toPtree() const;
  static std::unique_ptr<Transaction> fromPtree(const boost::property_tree::ptree& t);

private:
  std::string id_;
  std::string from_;
  std::string to_;
  uint64_t amount_{0};
  uint64_t fee_{0};
  uint64_t ts_{0};

  void computeId();
};

}
