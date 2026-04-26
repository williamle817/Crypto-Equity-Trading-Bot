#include "rsi.hpp"

RSI::RSI(int period)
    : period_(period), avg_gain_(0.0), avg_loss_(0.0),
      prev_price_(0.0), rsi_(50.0), seen_(0) {}

void RSI::reset() {
    avg_gain_ = avg_loss_ = prev_price_ = 0.0;
    rsi_  = 50.0;
    seen_ = 0;
}

double RSI::update(double price) {
    if (seen_ == 0) {
        prev_price_ = price;
        ++seen_;
        return 50.0;
    }

    double change = price - prev_price_;
    prev_price_   = price;
    double gain   = change > 0.0 ?  change : 0.0;
    double loss   = change < 0.0 ? -change : 0.0;

    if (seen_ <= period_) {
        avg_gain_ += gain;
        avg_loss_ += loss;
        if (seen_ == period_) {
            avg_gain_ /= period_;
            avg_loss_ /= period_;
        }
    } else {
        // Wilder's smoothing
        avg_gain_ = (avg_gain_ * (period_ - 1) + gain) / period_;
        avg_loss_ = (avg_loss_ * (period_ - 1) + loss) / period_;
    }
    ++seen_;

    if (avg_loss_ < 1e-12) { rsi_ = 100.0; }
    else { rsi_ = 100.0 - 100.0 / (1.0 + avg_gain_ / avg_loss_); }
    return rsi_;
}

bool   RSI::ready() const { return seen_ > period_; }
double RSI::value() const { return rsi_; }
