#pragma once
#include <string>
#include <nlohmann/json.hpp>

nlohmann::json place_market_order(const std::string& product_id,
                                  bool               sandbox,
                                  const std::string& side,
                                  const std::string& base_size,
                                  const std::string& bearer_token);
