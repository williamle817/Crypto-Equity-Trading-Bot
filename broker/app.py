"""
Multi-asset trading data API
  - Crypto  : Coinbase Advanced Trade (ETH, BTC, SOL, AVAX, LINK, ...)
  - Equities: Yahoo Finance           (SPY, QQQ, AAPL, TSLA, NVDA, GLD, ...)
"""
from __future__ import annotations

from datetime import datetime, timedelta, timezone
from typing import Optional

import httpx
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

try:
    import pandas as pd
    import yfinance as yf
    _YFINANCE = True
except ImportError:
    _YFINANCE = False

app = FastAPI(title="Trading Data API", version="2.0")

COINBASE_BASE = "https://api.coinbase.com/api/v3/brokerage/market"

CRYPTO_ASSETS: dict[str, str] = {
    "ETH-USD":  "Ethereum",
    "BTC-USD":  "Bitcoin",
    "SOL-USD":  "Solana",
    "AVAX-USD": "Avalanche",
    "LINK-USD": "Chainlink",
}

EQUITY_ASSETS: dict[str, str] = {
    "SPY":  "S&P 500 ETF",
    "QQQ":  "Nasdaq-100 ETF",
    "GLD":  "Gold ETF",
    "AAPL": "Apple Inc.",
    "MSFT": "Microsoft Corp.",
    "NVDA": "NVIDIA Corp.",
    "TSLA": "Tesla Inc.",
}

# Maps our granularity strings to yfinance interval codes
_YF_INTERVAL: dict[str, str] = {
    "ONE_MINUTE":     "1m",
    "FIVE_MINUTE":    "5m",
    "FIFTEEN_MINUTE": "15m",
    "ONE_HOUR":       "1h",
    "ONE_DAY":        "1d",
}

# yfinance period for each interval (max lookback yfinance supports)
_YF_PERIOD: dict[str, str] = {
    "1m":  "7d",
    "5m":  "60d",
    "15m": "60d",
    "1h":  "730d",
    "1d":  "max",
}


# ── models ────────────────────────────────────────────────────────────────────

class CandleResp(BaseModel):
    candles: list


class IndicatorsResp(BaseModel):
    ema_fast:    Optional[float] = None
    ema_slow:    Optional[float] = None
    rsi:         Optional[float] = None
    bb_upper:    Optional[float] = None
    bb_middle:   Optional[float] = None
    bb_lower:    Optional[float] = None
    bb_pct_b:    Optional[float] = None
    macd:        Optional[float] = None
    macd_signal: Optional[float] = None
    macd_hist:   Optional[float] = None


# ── asset directory ───────────────────────────────────────────────────────────

@app.get("/assets")
async def list_assets():
    return {
        "crypto":   CRYPTO_ASSETS,
        "equities": EQUITY_ASSETS if _YFINANCE else {},
        "yfinance_available": _YFINANCE,
    }


@app.get("/health")
async def health():
    return {"status": "ok", "yfinance": _YFINANCE}


# ── candles ───────────────────────────────────────────────────────────────────

@app.get("/candles", response_model=CandleResp)
async def candles(
    product_id:      str = "ETH-USD",
    granularity:     str = "ONE_MINUTE",
    lookback_minutes: int = 300,
):
    if product_id in EQUITY_ASSETS:
        return await _equity_candles(product_id, granularity, lookback_minutes)
    return await _crypto_candles(product_id, granularity, lookback_minutes)


async def _crypto_candles(product_id: str, granularity: str, lookback_minutes: int) -> dict:
    end   = datetime.now(timezone.utc)
    start = end - timedelta(minutes=lookback_minutes)
    url   = f"{COINBASE_BASE}/products/{product_id}/candles"
    params = {
        "start":       int(start.timestamp()),
        "end":         int(end.timestamp()),
        "granularity": granularity,
        "limit":       350,
    }
    async with httpx.AsyncClient(timeout=15) as client:
        r = await client.get(url, params=params, headers={"cache-control": "no-cache"})
        r.raise_for_status()
        return r.json()


async def _equity_candles(product_id: str, granularity: str, lookback_minutes: int) -> dict:
    if not _YFINANCE:
        raise HTTPException(status_code=503, detail="yfinance not installed")
    interval = _YF_INTERVAL.get(granularity, "1m")
    period   = _YF_PERIOD.get(interval, "7d")

    import asyncio
    loop = asyncio.get_event_loop()
    hist = await loop.run_in_executor(
        None,
        lambda: yf.Ticker(product_id).history(period=period, interval=interval)
    )
    if hist.empty:
        raise HTTPException(status_code=404, detail=f"No data for {product_id}")

    # Keep only the most recent `lookback_minutes` rows
    hist = hist.tail(lookback_minutes)

    # Normalise to the same shape Coinbase returns (all values as strings)
    out = []
    for ts, row in hist.iterrows():
        out.append({
            "start":  str(int(ts.timestamp())),
            "open":   str(round(float(row["Open"]),   6)),
            "high":   str(round(float(row["High"]),   6)),
            "low":    str(round(float(row["Low"]),    6)),
            "close":  str(round(float(row["Close"]),  6)),
            "volume": str(round(float(row["Volume"]), 2)),
        })
    return {"candles": out}


# ── ticker ────────────────────────────────────────────────────────────────────

@app.get("/ticker")
async def ticker(product_id: str = "ETH-USD"):
    if product_id in EQUITY_ASSETS:
        return await _equity_ticker(product_id)
    return await _crypto_ticker(product_id)


async def _crypto_ticker(product_id: str) -> dict:
    url = f"{COINBASE_BASE}/products/{product_id}/ticker"
    async with httpx.AsyncClient(timeout=10) as client:
        r = await client.get(url, headers={"cache-control": "no-cache"})
        r.raise_for_status()
        return r.json()


async def _equity_ticker(product_id: str) -> dict:
    if not _YFINANCE:
        raise HTTPException(status_code=503, detail="yfinance not installed")
    import asyncio
    loop = asyncio.get_event_loop()
    info = await loop.run_in_executor(
        None, lambda: yf.Ticker(product_id).fast_info
    )
    return {
        "price":        getattr(info, "last_price",              None),
        "prev_close":   getattr(info, "previous_close",          None),
        "market_cap":   getattr(info, "market_cap",              None),
        "volume_avg":   getattr(info, "three_month_average_volume", None),
    }


# ── indicators ────────────────────────────────────────────────────────────────

@app.get("/indicators", response_model=IndicatorsResp)
async def indicators(
    product_id:      str   = "ETH-USD",
    granularity:     str   = "ONE_MINUTE",
    lookback_minutes: int  = 300,
    fast:            int   = 12,
    slow:            int   = 26,
    rsi_period:      int   = 14,
    bb_period:       int   = 20,
    bb_std:          float = 2.0,
):
    if not _YFINANCE:
        raise HTTPException(status_code=503,
                            detail="pandas/yfinance required for /indicators")

    resp    = await candles(product_id, granularity, lookback_minutes)
    raw     = resp["candles"] if isinstance(resp, dict) else resp.candles
    if not raw:
        raise HTTPException(status_code=404, detail="No candle data")

    closes = pd.Series([float(c["close"]) for c in raw])
    return _compute_indicators(closes, fast, slow, rsi_period, bb_period, bb_std)


def _compute_indicators(
    closes: "pd.Series",
    fast: int   = 12,
    slow: int   = 26,
    rsi_period: int   = 14,
    bb_period:  int   = 20,
    bb_std:     float = 2.0,
) -> dict:
    s = closes

    ema_fast_s = s.ewm(span=fast, adjust=False).mean()
    ema_slow_s = s.ewm(span=slow, adjust=False).mean()

    # MACD
    macd_line   = ema_fast_s - ema_slow_s
    macd_sig    = macd_line.ewm(span=9, adjust=False).mean()
    macd_hist   = macd_line - macd_sig

    # RSI (Wilder's method via EWM)
    delta  = s.diff()
    gain   = delta.clip(lower=0).ewm(alpha=1 / rsi_period, adjust=False).mean()
    loss   = (-delta.clip(upper=0)).ewm(alpha=1 / rsi_period, adjust=False).mean()
    rs     = gain / loss.replace(0, float("nan"))
    rsi    = 100 - 100 / (1 + rs)

    # Bollinger Bands
    rm     = s.rolling(bb_period).mean()
    rs_std = s.rolling(bb_period).std(ddof=0)
    upper  = rm + bb_std * rs_std
    lower  = rm - bb_std * rs_std
    bw     = upper - lower
    pct_b  = (s - lower) / bw.replace(0, float("nan"))

    def _f(series) -> Optional[float]:
        v = series.iloc[-1]
        return round(float(v), 6) if pd.notna(v) else None

    return {
        "ema_fast":    _f(ema_fast_s),
        "ema_slow":    _f(ema_slow_s),
        "rsi":         _f(rsi),
        "bb_upper":    _f(upper),
        "bb_middle":   _f(rm),
        "bb_lower":    _f(lower),
        "bb_pct_b":    _f(pct_b),
        "macd":        _f(macd_line),
        "macd_signal": _f(macd_sig),
        "macd_hist":   _f(macd_hist),
    }
