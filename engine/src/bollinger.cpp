#include "bollinger.hpp"
#include <cmath>

Bollinger::Bollinger(int period, double std_dev)
    : period_(period), std_dev_(std_dev) {}

void Bollinger::reset() {
    window_.clear();
    bands_ = BollingerBands{};
}

BollingerBands Bollinger::update(double price) {
    window_.push_back(price);
    if (static_cast<int>(window_.size()) > period_) window_.pop_front();

    if (static_cast<int>(window_.size()) < period_) {
        bands_ = {price, price, price, 0.0, 0.5};
        return bands_;
    }

    double sum = 0.0;
    for (double v : window_) sum += v;
    double mean = sum / period_;

    double sq = 0.0;
    for (double v : window_) sq += (v - mean) * (v - mean);
    double sd = std::sqrt(sq / period_);

    bands_.middle    = mean;
    bands_.upper     = mean + std_dev_ * sd;
    bands_.lower     = mean - std_dev_ * sd;
    double bw        = bands_.upper - bands_.lower;
    bands_.bandwidth = bw > 1e-12 ? bw / mean : 0.0;
    bands_.pct_b     = bw > 1e-12 ? (price - bands_.lower) / bw : 0.5;
    return bands_;
}

bool           Bollinger::ready() const { return static_cast<int>(window_.size()) >= period_; }
BollingerBands Bollinger::value() const { return bands_; }
