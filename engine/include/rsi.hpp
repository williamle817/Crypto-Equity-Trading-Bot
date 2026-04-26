#pragma once

class RSI {
public:
    explicit RSI(int period = 14);
    void   reset();
    double update(double price);
    bool   ready() const;
    double value() const;

private:
    int    period_;
    double avg_gain_, avg_loss_;
    double prev_price_;
    double rsi_;
    int    seen_;
};
