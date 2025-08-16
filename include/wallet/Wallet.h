// include/wallet/Wallet.h
#pragma once
#include <string>
#include <vector>

namespace QTC {

class Wallet {
public:
  static std::string Create();
  static const std::vector<std::string>& All();
};

}
