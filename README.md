# Crypto & Equity Trading Bot

A quantitative trading system with a **C++17 engine** and a **Python data API** (FastAPI).  
Supports multiple assets, four trading strategies, configurable risk management, and per-asset performance metrics.

```
┌──────────────────────────────┐    HTTP/JSON    ┌────────────────────────────────┐
│        C++17 Engine          │◄───────────────►│     Python Data API (FastAPI)  │
│                              │                 │                                │
│  Indicators                  │                 │  Data Sources                  │
│    EMA (fast / slow)         │                 │    Coinbase (crypto)           │
│    RSI (Wilder's method)     │                 │    Yahoo Finance (ETF/equity)  │
│    Bollinger Bands           │                 │                                │
│                              │                 │  Endpoints                     │
│  Strategies                  │                 │    GET /candles                │
│    EMA Crossover             │                 │    GET /ticker                 │
│    RSI Mean-Reversion        │                 │    GET /indicators             │
│    Bollinger Breakout        │                 │    GET /assets                 │
│    Combined (majority vote)  │                 │    GET /health                 │
│                              │                 │                                │
│  Risk Management             │                 │  Server-side Indicators        │
│    Per-asset position cap    │                 │    EMA, RSI, MACD              │
│    Daily loss limit          │                 │    Bollinger Bands + %B        │
│    Trade cooldown            │                 │                                │
│    Long-only enforcement     │                 │                                │
│                              │                 │                                │
│  Performance Metrics         │                 │                                │
│    Sharpe Ratio              │                 │                                │
│    Max Drawdown              │                 │                                │
│    Win Rate / Profit Factor  │                 │                                │
│    Avg Win / Avg Loss        │                 │                                │
└──────────────────────────────┘                 └────────────────────────────────┘
```

## Supported Assets

| Type    | Symbols                                  | Source         |
|---------|------------------------------------------|----------------|
| Crypto  | ETH-USD, BTC-USD, SOL-USD, AVAX-USD, LINK-USD | Coinbase  |
| ETF     | SPY, QQQ, GLD                            | Yahoo Finance  |
| Equity  | AAPL, MSFT, NVDA, TSLA                   | Yahoo Finance  |

## Project Structure

```
eth-trading-bot/
├── broker/
│   ├── app.py              # FastAPI data server (multi-asset, indicators)
│   └── requirements.txt
├── engine/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── bollinger.hpp   # Bollinger Bands indicator
│   │   ├── config.hpp      # JSON + CLI configuration
│   │   ├── data_client.hpp # HTTP candle fetcher
│   │   ├── ema.hpp         # Exponential Moving Average
│   │   ├── logger.hpp      # CSV trade logger
│   │   ├── metrics.hpp     # Performance metrics (Sharpe, drawdown, ...)
│   │   ├── order_placer.hpp
│   │   ├── risk.hpp        # Risk limits and position tracking
│   │   ├── rsi.hpp         # RSI indicator (Wilder's method)
│   │   └── strategy.hpp    # Signal functions + StrategyType enum
│   └── src/
│       ├── bollinger.cpp
│       ├── config.cpp
│       ├── data_client.cpp
│       ├── ema.cpp
│       ├── http.cpp
│       ├── logger.cpp
│       ├── main.cpp        # Multi-asset main loop
│       ├── metrics.cpp
│       ├── order_placer.cpp
│       ├── risk.cpp
│       ├── rsi.cpp
│       └── strategy.cpp
├── config.json             # Example multi-asset configuration
└── logs/                   # Per-asset CSV logs (auto-created)
```

## Build

**Prerequisites:** CMake ≥ 3.15, libcurl (e.g. `vcpkg install curl` on Windows)

```bash
cd engine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

`nlohmann/json` is fetched automatically via `FetchContent`.

## Run

**1. Start the data API:**
```bash
cd broker
pip install -r requirements.txt
uvicorn app:app --port 8000
```

**2. Single-asset dry-run (backwards-compatible CLI):**
```bash
./engine/build/ethbot --product ETH-USD --strategy EMA_CROSS --dry-run true
```

**3. Multi-asset from config file:**
```bash
./engine/build/ethbot --config config.json
```

**4. Override strategy at the command line:**
```bash
./engine/build/ethbot --config config.json --strategy RSI
```

## Configuration

`config.json` drives the full run; any CLI flag overrides its JSON counterpart.

```json
{
  "strategy": "COMBINED",
  "dry_run":  true,
  "assets": [
    { "product_id": "ETH-USD", "size": "0.01",   "max_position": 0.05  },
    { "product_id": "BTC-USD", "size": "0.0003", "max_position": 0.001 },
    { "product_id": "SPY",     "size": "1",      "max_position": 5.0   }
  ]
}
```

| Field                       | Default    | Description                                     |
|-----------------------------|------------|-------------------------------------------------|
| `strategy`                  | `EMA_CROSS`| `EMA_CROSS` / `RSI` / `BOLLINGER` / `COMBINED` |
| `ema_fast` / `ema_slow`     | `12` / `26`| EMA periods                                     |
| `rsi_period`                | `14`       | RSI lookback                                    |
| `rsi_oversold/overbought`   | `30` / `70`| RSI signal thresholds                           |
| `bb_period` / `bb_std`      | `20` / `2.0`| Bollinger Bands parameters                     |
| `max_daily_loss`            | `50.0`     | USD loss cap; trading halts when reached        |
| `cooldown_seconds`          | `60`       | Minimum seconds between fills                   |
| `long_only`                 | `true`     | Disallow short positions                        |

## Strategies

| Strategy    | Signal logic                                                 |
|-------------|--------------------------------------------------------------|
| `EMA_CROSS` | BUY when fast EMA crosses above slow; SELL on cross below    |
| `RSI`       | BUY when RSI < oversold; SELL when RSI > overbought          |
| `BOLLINGER` | BUY when %B < 5%; SELL when %B > 95%                        |
| `COMBINED`  | Majority vote across all three — requires ≥ 2-of-3 agreement |

## Sample Output

```
Config: api=http://localhost:8000 strategy=COMBINED assets=2 dry_run=true sandbox=true
  asset=ETH-USD size=0.01 max_pos=0.05
  asset=BTC-USD size=0.0003 max_pos=0.001

=== ETH-USD | strategy=COMBINED ===
Fetched 300 candles
BUY  0.01 ETH-USD @ 3241.50  pos=0.010  pnl=0.0000
SELL 0.01 ETH-USD @ 3289.20  pos=0.000  pnl=0.4770

--- Performance: ETH-USD ---
  Total Trades  : 8
  Win Rate      : 62.50%
  Total PnL     : $1.2340
  Profit Factor : 1.8700
  Avg Win       : $0.5200
  Avg Loss      : $0.2800
  Max Drawdown  : 2.14%
  Sharpe Ratio  : 1.3400
-------------------------------
```

Logs are written per-asset to `logs/ETH_USD.csv`, `logs/BTC_USD.csv`, etc.

## Notes

- **Dry-run mode** (`dry_run: true`) simulates fills locally — no real orders are placed.
- **Live trading** requires a valid Coinbase Advanced Trade API key with HMAC/JWT signing (not yet implemented).
- Yahoo Finance intraday data is limited to the past 60 days for intervals shorter than 1 hour.
