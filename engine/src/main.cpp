#include "config.hpp"
#include "data_client.hpp"
#include "ema.hpp"
#include "rsi.hpp"
#include "bollinger.hpp"
#include "strategy.hpp"
#include "order_placer.hpp"
#include "risk.hpp"
#include "logger.hpp"
#include "metrics.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>
#include <cstdint>

// ── helpers ──────────────────────────────────────────────────────────────────

static SigStr sigstr_from(Signal s) {
    if (s == Signal::BUY)  return SigStr::Buy;
    if (s == Signal::SELL) return SigStr::Sell;
    return SigStr::Hold;
}

static Side side_from(Signal s) {
    return (s == Signal::SELL) ? Side::Sell : Side::Buy;
}

// ── per-asset strategy run ───────────────────────────────────────────────────

static void run_asset(const Config& cfg, const AssetConfig& asset) {
    std::cout << "\n=== " << asset.product_id
              << " | strategy=" << strategy_name(cfg.strategy) << " ===\n";

    auto candles = fetch_candles_local(cfg.data_api, asset.product_id,
                                       cfg.granularity, cfg.lookback);
    if (candles.empty()) {
        std::cerr << "No candles fetched for " << asset.product_id << "\n";
        return;
    }
    std::cout << "Fetched " << candles.size() << " candles\n";

    EMA      ema_f(cfg.ema_fast), ema_s(cfg.ema_slow);
    RSI      rsi_ind(cfg.rsi_period);
    Bollinger bb(cfg.bb_period, cfg.bb_std);
    CrossState cs{};

    RiskLimits limits;
    limits.max_position_abs = asset.max_position;
    limits.max_daily_loss   = cfg.max_daily_loss;
    limits.cooldown_seconds = cfg.cooldown_seconds;
    limits.long_only        = cfg.long_only;

    RiskState rstate{};
    MetricsTracker metrics;

    // One CSV log per asset: logs/ETH_USD.csv
    std::string log_name = asset.product_id;
    std::replace(log_name.begin(), log_name.end(), '-', '_');
    logger_init("logs/" + log_name + ".csv");

    auto now_ts = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    for (const auto& k : candles) {
        double f  = ema_f.update(k.close);
        double s  = ema_s.update(k.close);
        double rv = rsi_ind.update(k.close);
        auto   bb_val = bb.update(k.close);

        metrics.record_equity_point(rstate.realized_pnl);

        if (!(ema_f.ready() && ema_s.ready())) continue;

        // ── signal selection ──────────────────────────────────────────────
        bool indicators_ready = rsi_ind.ready() && bb.ready();
        Signal sig = Signal::HOLD;

        switch (cfg.strategy) {
            case StrategyType::EMA_CROSS:
                sig = crossover_signal(cs, f, s);
                break;
            case StrategyType::RSI:
                if (indicators_ready)
                    sig = rsi_signal(rv, cfg.rsi_oversold, cfg.rsi_overbought);
                break;
            case StrategyType::BOLLINGER:
                if (indicators_ready)
                    sig = bollinger_signal(k.close, bb_val.lower, bb_val.upper, bb_val.pct_b);
                break;
            case StrategyType::COMBINED:
                if (indicators_ready) {
                    Signals sigs{
                        crossover_signal(cs, f, s),
                        rsi_signal(rv, cfg.rsi_oversold, cfg.rsi_overbought),
                        bollinger_signal(k.close, bb_val.lower, bb_val.upper, bb_val.pct_b)
                    };
                    sig = combined_signal(sigs);
                } else {
                    sig = crossover_signal(cs, f, s); // fallback while warming up
                }
                break;
        }

        SigStr sigstr = sigstr_from(sig);

        if (sig == Signal::HOLD) {
            logger_row(k.start, k.close, f, s, sigstr, "NONE",
                       rstate.position_eth, rstate.realized_pnl, "");
            continue;
        }

        // ── risk gate ─────────────────────────────────────────────────────
        Side        side = side_from(sig);
        double      sz   = std::stod(asset.size);
        std::string reason;
        bool ok = allow_order(limits, rstate, side, sz, k.close,
                              now_ts + k.start, reason);

        if (!ok) {
            logger_row(k.start, k.close, f, s, sigstr, "BLOCKED",
                       rstate.position_eth, rstate.realized_pnl, reason);
            continue;
        }

        std::string action = (sig == Signal::BUY) ? "BUY" : "SELL";

        if (cfg.dry_run) {
            on_fill(rstate, side, sz, k.close, k.start);
            logger_row(k.start, k.close, f, s, sigstr, action,
                       rstate.position_eth, rstate.realized_pnl, "DRY-RUN");
            metrics.record_fill(k.start, side, sz, k.close, rstate.realized_pnl);
            std::cout << action << " " << asset.size << " " << asset.product_id
                      << " @ " << k.close
                      << " pos=" << rstate.position_eth
                      << " pnl=" << rstate.realized_pnl << "\n";
        } else {
            auto resp = place_market_order(asset.product_id, cfg.sandbox,
                                           action, asset.size, "");
            on_fill(rstate, side, sz, k.close, k.start);
            logger_row(k.start, k.close, f, s, sigstr, action,
                       rstate.position_eth, rstate.realized_pnl, "LIVE");
            metrics.record_fill(k.start, side, sz, k.close, rstate.realized_pnl);
            std::cout << "LIVE " << action << " resp: " << resp.dump() << "\n";
        }
    }

    metrics.print_summary(asset.product_id);
}

// ── entry point ──────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    try {
        Config cfg = Config::from_args(argc, argv);
        cfg.validate();
        cfg.print();

        for (const auto& asset : cfg.assets)
            run_asset(cfg, asset);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}
