#pragma once
#include <deque>

struct BollingerBands {
    double upper     = 0.0;
    double middle    = 0.0;
    double lower     = 0.0;
    double bandwidth = 0.0;
    double pct_b     = 0.5; // (price - lower) / (upper - lower); <0.05 = oversold, >0.95 = overbought
};

class Bollinger {
public:
    explicit Bollinger(int period = 20, double std_dev = 2.0);
    void          reset();
    BollingerBands update(double price);
    bool           ready() const;
    BollingerBands value() const;

private:
    int    period_;
    double std_dev_;
    std::deque<double> window_;
    BollingerBands bands_;
};
