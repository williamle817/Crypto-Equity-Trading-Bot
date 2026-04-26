#pragma once
#include <string>

enum class Signal       { HOLD, BUY, SELL };
enum class StrategyType { EMA_CROSS, RSI, BOLLINGER, COMBINED };

struct CrossState {
    bool   have_prev = false;
    double prev_diff = 0.0;
};

// All three signals are combined in COMBINED mode
struct Signals {
    Signal ema_cross = Signal::HOLD;
    Signal rsi       = Signal::HOLD;
    Signal bollinger = Signal::HOLD;
};

// ── EMA crossover ────────────────────────────────────────────────────────────
inline Signal crossover_signal(CrossState& st, double fast, double slow) {
    double diff = fast - slow;
    Signal s    = Signal::HOLD;
    if (st.have_prev) {
        if (st.prev_diff <= 0.0 && diff > 0.0) s = Signal::BUY;
        else if (st.prev_diff >= 0.0 && diff < 0.0) s = Signal::SELL;
    }
    st.have_prev = true;
    st.prev_diff = diff;
    return s;
}

// ── RSI mean-reversion ───────────────────────────────────────────────────────
inline Signal rsi_signal(double rsi, double oversold = 30.0, double overbought = 70.0) {
    if (rsi < oversold)   return Signal::BUY;
    if (rsi > overbought) return Signal::SELL;
    return Signal::HOLD;
}

// ── Bollinger Bands mean-reversion ───────────────────────────────────────────
// pct_b = (price - lower) / (upper - lower); uses extreme bands to avoid noise
inline Signal bollinger_signal(double /*price*/, double /*lower*/, double /*upper*/, double pct_b) {
    if (pct_b < 0.05) return Signal::BUY;
    if (pct_b > 0.95) return Signal::SELL;
    return Signal::HOLD;
}

// ── Combined: majority vote across all three signals ─────────────────────────
inline Signal combined_signal(const Signals& s) {
    int buy = 0, sell = 0;
    auto tally = [&](Signal sig) {
        if (sig == Signal::BUY)  ++buy;
        if (sig == Signal::SELL) ++sell;
    };
    tally(s.ema_cross);
    tally(s.rsi);
    tally(s.bollinger);
    if (buy  > sell) return Signal::BUY;
    if (sell > buy)  return Signal::SELL;
    return Signal::HOLD;
}

inline const char* to_str(Signal s) {
    switch (s) {
        case Signal::BUY:  return "BUY";
        case Signal::SELL: return "SELL";
        default:           return "HOLD";
    }
}

inline const char* strategy_name(StrategyType t) {
    switch (t) {
        case StrategyType::EMA_CROSS: return "EMA_CROSS";
        case StrategyType::RSI:       return "RSI";
        case StrategyType::BOLLINGER: return "BOLLINGER";
        case StrategyType::COMBINED:  return "COMBINED";
        default:                      return "UNKNOWN";
    }
}
