#pragma once
#include <string>

namespace QTC {

struct Zk {
  static bool prove_transfer(const std::string& note, std::string& proof);
  static bool verify_transfer(const std::string& proof);
};

} // namespace QTC
