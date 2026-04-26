#include "order_placer.hpp"
#include "http.hpp"
#include <vector>

nlohmann::json place_market_order(const std::string& product_id,
                                  bool               sandbox,
                                  const std::string& side,
                                  const std::string& base_size,
                                  const std::string& bearer_token) {
    const std::string host = sandbox ? "https://api-sandbox.coinbase.com"
                                     : "https://api.coinbase.com";
    const std::string url  = host + "/api/v3/brokerage/orders";

    nlohmann::json body = {
        {"client_order_id", "client_" + side + "_" + product_id},
        {"product_id",      product_id},
        {"side",            side},
        {"order_configuration", {
            {"market_market_ioc", {{"base_size", base_size}}}
        }}
    };

    std::vector<std::string> headers = {"Content-Type: application/json"};
    if (!bearer_token.empty())
        headers.push_back("Authorization: Bearer " + bearer_token);

    // NOTE: Coinbase Advanced Trade requires HMAC-signed JWT.
    // Keep --dry-run true until authentication is wired up.
    auto resp = http_post(url, body.dump(), headers);
    return nlohmann::json::parse(resp);
}
