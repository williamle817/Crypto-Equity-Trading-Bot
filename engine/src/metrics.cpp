#include "metrics.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <numeric>
#include <algorithm>

void MetricsTracker::record_fill(long ts, Side side, double size, double price, double cumulative_pnl) {
    fills_.push_back({ts, side, size, price, cumulative_pnl});
}

void MetricsTracker::record_equity_point(double realized_pnl) {
    equity_series_.push_back(realized_pnl);
}

PerformanceMetrics MetricsTracker::compute() const {
    PerformanceMetrics m;
    if (fills_.empty()) return m;

    m.total_trades = static_cast<int>(fills_.size());
    m.total_pnl    = fills_.back().cumulative_pnl;

    double prev_pnl   = 0.0;
    double gross_win  = 0.0;
    double gross_loss = 0.0;
    int    sell_count = 0;

    for (const auto& f : fills_) {
        if (f.side != Side::Sell) continue;
        ++sell_count;
        double delta = f.cumulative_pnl - prev_pnl;
        prev_pnl = f.cumulative_pnl;
        if (delta > 0.0) { gross_win  += delta; ++m.winning_trades; m.avg_win  += delta; }
        else             { gross_loss += -delta;                     m.avg_loss += -delta; }
    }

    m.win_rate      = sell_count > 0 ? static_cast<double>(m.winning_trades) / sell_count : 0.0;
    m.profit_factor = gross_loss > 1e-12 ? gross_win / gross_loss : (gross_win > 0 ? 999.0 : 0.0);
    int loss_count  = sell_count - m.winning_trades;
    m.avg_win  = m.winning_trades > 0 ? m.avg_win  / m.winning_trades : 0.0;
    m.avg_loss = loss_count       > 0 ? m.avg_loss / loss_count       : 0.0;

    if (equity_series_.size() > 1) {
        // Max drawdown
        double peak = equity_series_[0];
        for (double e : equity_series_) {
            if (e > peak) peak = e;
            double dd = peak > 1e-12 ? (peak - e) / peak : (peak - e);
            if (dd > m.max_drawdown) m.max_drawdown = dd;
        }

        // Sharpe ratio from equity differences; annualised assuming 1-min candles
        std::vector<double> rets;
        rets.reserve(equity_series_.size() - 1);
        for (size_t i = 1; i < equity_series_.size(); ++i)
            rets.push_back(equity_series_[i] - equity_series_[i - 1]);

        double mean = std::accumulate(rets.begin(), rets.end(), 0.0) / static_cast<double>(rets.size());
        double var  = 0.0;
        for (double r : rets) var += (r - mean) * (r - mean);
        var /= static_cast<double>(rets.size());

        const double ANNUALIZE = std::sqrt(252.0 * 390.0); // sqrt(trading mins/year)
        m.sharpe_ratio = var > 1e-24 ? mean / std::sqrt(var) * ANNUALIZE : 0.0;
    }

    return m;
}

void MetricsTracker::print_summary(const std::string& asset_id) const {
    auto m = compute();
    std::cout << "\n--- Performance: " << (asset_id.empty() ? "summary" : asset_id) << " ---\n"
              << std::fixed << std::setprecision(4)
              << "  Total Trades  : " << m.total_trades         << "\n"
              << "  Win Rate      : " << m.win_rate * 100.0     << "%\n"
              << "  Total PnL     : $" << m.total_pnl           << "\n"
              << "  Profit Factor : " << m.profit_factor        << "\n"
              << "  Avg Win       : $" << m.avg_win             << "\n"
              << "  Avg Loss      : $" << m.avg_loss            << "\n"
              << "  Max Drawdown  : " << m.max_drawdown * 100.0 << "%\n"
              << "  Sharpe Ratio  : " << m.sharpe_ratio         << "\n"
              << "-------------------------------\n";
}
