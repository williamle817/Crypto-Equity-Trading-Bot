#pragma once
#include "risk.hpp"
#include <vector>
#include <string>

struct PerformanceMetrics {
    int    total_trades   = 0;
    int    winning_trades = 0;
    double win_rate       = 0.0;
    double total_pnl      = 0.0;
    double max_drawdown   = 0.0;
    double sharpe_ratio   = 0.0;
    double profit_factor  = 0.0;
    double avg_win        = 0.0;
    double avg_loss       = 0.0;
};

class MetricsTracker {
public:
    // Call on every fill; cumulative_pnl = RiskState::realized_pnl after on_fill()
    void record_fill(long ts, Side side, double size, double price, double cumulative_pnl);
    // Call every candle to build equity curve (pass realized_pnl)
    void record_equity_point(double realized_pnl);

    PerformanceMetrics compute() const;
    void print_summary(const std::string& asset_id = "") const;

private:
    struct Fill {
        long   ts;
        Side   side;
        double size, price, cumulative_pnl;
    };
    std::vector<Fill>   fills_;
    std::vector<double> equity_series_;
};
