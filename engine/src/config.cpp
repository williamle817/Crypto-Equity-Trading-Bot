#include "config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;

static StrategyType parse_strategy(const std::string& s) {
    if (s == "RSI")       return StrategyType::RSI;
    if (s == "BOLLINGER") return StrategyType::BOLLINGER;
    if (s == "COMBINED")  return StrategyType::COMBINED;
    return StrategyType::EMA_CROSS;
}

Config Config::from_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot open config: " + path);
    json j;
    f >> j;

    Config cfg;
    if (j.contains("data_api"))         cfg.data_api         = j["data_api"];
    if (j.contains("granularity"))      cfg.granularity      = j["granularity"];
    if (j.contains("lookback"))         cfg.lookback         = j["lookback"];
    if (j.contains("ema_fast"))         cfg.ema_fast         = j["ema_fast"];
    if (j.contains("ema_slow"))         cfg.ema_slow         = j["ema_slow"];
    if (j.contains("rsi_period"))       cfg.rsi_period       = j["rsi_period"];
    if (j.contains("rsi_oversold"))     cfg.rsi_oversold     = j["rsi_oversold"];
    if (j.contains("rsi_overbought"))   cfg.rsi_overbought   = j["rsi_overbought"];
    if (j.contains("bb_period"))        cfg.bb_period        = j["bb_period"];
    if (j.contains("bb_std"))           cfg.bb_std           = j["bb_std"];
    if (j.contains("strategy"))        cfg.strategy         = parse_strategy(j["strategy"]);
    if (j.contains("sandbox"))          cfg.sandbox          = j["sandbox"];
    if (j.contains("dry_run"))          cfg.dry_run          = j["dry_run"];
    if (j.contains("max_daily_loss"))   cfg.max_daily_loss   = j["max_daily_loss"];
    if (j.contains("cooldown_seconds")) cfg.cooldown_seconds = j["cooldown_seconds"];
    if (j.contains("long_only"))        cfg.long_only        = j["long_only"];

    if (j.contains("assets")) {
        for (const auto& a : j["assets"]) {
            AssetConfig ac;
            ac.product_id   = a.value("product_id",   "ETH-USD");
            ac.size         = a.value("size",          "0.01");
            ac.max_position = a.value("max_position",  0.05);
            cfg.assets.push_back(ac);
        }
    }
    return cfg;
}

Config Config::from_args(int argc, char* argv[]) {
    Config cfg;
    std::string config_file;

    // First pass: locate --config
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--config" && i + 1 < argc)
            config_file = argv[++i];

    if (!config_file.empty())
        cfg = Config::from_file(config_file);

    // Second pass: CLI overrides on top of any loaded config
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto nxt = [&]() -> std::string {
            if (i + 1 < argc) return std::string(argv[++i]);
            throw std::runtime_error("Missing value for " + a);
        };
        if      (a == "--config")      { ++i; }
        else if (a == "--api")         cfg.data_api         = nxt();
        else if (a == "--granularity") cfg.granularity      = nxt();
        else if (a == "--lookback")    cfg.lookback         = std::stoi(nxt());
        else if (a == "--fast")        cfg.ema_fast         = std::stoi(nxt());
        else if (a == "--slow")        cfg.ema_slow         = std::stoi(nxt());
        else if (a == "--strategy")    cfg.strategy         = parse_strategy(nxt());
        else if (a == "--sandbox")     cfg.sandbox          = (nxt() == "true");
        else if (a == "--dry-run")     cfg.dry_run          = (nxt() == "true");
        else if (a == "--product") {
            std::string pid = nxt();
            if (cfg.assets.empty()) cfg.assets.push_back({pid, "0.01", 0.05});
            else                    cfg.assets[0].product_id = pid;
        }
        else if (a == "--size") {
            std::string sz = nxt();
            if (cfg.assets.empty()) cfg.assets.push_back({"ETH-USD", sz, 0.05});
            else                    cfg.assets[0].size = sz;
        }
        else if (a == "-h" || a == "--help") {
            std::cout <<
                "Usage: ethbot [--config FILE] [--api URL] [--granularity STR]\n"
                "              [--lookback N] [--fast N] [--slow N]\n"
                "              [--strategy EMA_CROSS|RSI|BOLLINGER|COMBINED]\n"
                "              [--product SYMBOL] [--size SIZE]\n"
                "              [--sandbox true|false] [--dry-run true|false]\n";
            std::exit(0);
        }
    }

    if (cfg.assets.empty())
        cfg.assets.push_back({"ETH-USD", "0.01", 0.05});
    return cfg;
}

void Config::validate() const {
    if (ema_fast <= 0 || ema_slow <= 0 || ema_fast >= ema_slow)
        throw std::runtime_error("Require 0 < ema_fast < ema_slow");
    if (assets.empty())
        throw std::runtime_error("No assets configured");
}

void Config::print() const {
    std::cout << "Config: api="      << data_api
              << " strategy="        << strategy_name(strategy)
              << " assets="          << assets.size()
              << " dry_run="         << std::boolalpha << dry_run
              << " sandbox="         << sandbox << "\n";
    for (const auto& a : assets)
        std::cout << "  asset=" << a.product_id
                  << " size="   << a.size
                  << " max_pos=" << a.max_position << "\n";
}
