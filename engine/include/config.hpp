#pragma once
#include "strategy.hpp"
#include <string>
#include <vector>

struct AssetConfig {
    std::string product_id   = "ETH-USD";
    std::string size         = "0.01";
    double      max_position = 0.05;
};

struct Config {
    std::string  data_api         = "http://localhost:8000";
    std::string  granularity      = "ONE_MINUTE";
    int          lookback         = 300;
    int          ema_fast         = 12;
    int          ema_slow         = 26;
    int          rsi_period       = 14;
    double       rsi_oversold     = 30.0;
    double       rsi_overbought   = 70.0;
    int          bb_period        = 20;
    double       bb_std           = 2.0;
    StrategyType strategy         = StrategyType::EMA_CROSS;
    bool         sandbox          = true;
    bool         dry_run          = true;
    double       max_daily_loss   = 50.0;
    int          cooldown_seconds = 60;
    bool         long_only        = true;
    std::vector<AssetConfig> assets;

    static Config from_file(const std::string& path);
    static Config from_args(int argc, char* argv[]);
    void validate() const;
    void print() const;
};
