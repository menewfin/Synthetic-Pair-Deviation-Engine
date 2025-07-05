#pragma once

#include <chrono>
#include <string>

namespace arbitrage {
namespace constants {

// System constants
constexpr size_t MAX_ORDER_BOOK_DEPTH = 50;
constexpr size_t MARKET_DATA_BUFFER_SIZE = 10000;
constexpr size_t MAX_SYMBOLS_PER_EXCHANGE = 100;
constexpr size_t DEFAULT_THREAD_POOL_SIZE = 16;

// Timing constants
constexpr auto HEARTBEAT_INTERVAL = std::chrono::seconds(30);
constexpr auto RECONNECT_DELAY = std::chrono::seconds(5);
constexpr auto MAX_RECONNECT_ATTEMPTS = 10;
constexpr auto OPPORTUNITY_TTL_DEFAULT = std::chrono::milliseconds(500);
constexpr auto DETECTION_LATENCY_TARGET = std::chrono::microseconds(10000); // 10ms

// Trading constants
constexpr double MIN_PROFIT_THRESHOLD_DEFAULT = 0.001;  // 0.1%
constexpr double MAX_POSITION_SIZE_USD = 100000.0;
constexpr double MAX_PORTFOLIO_EXPOSURE = 1000000.0;
constexpr double EXECUTION_SLIPPAGE_BPS = 5.0;         // 0.05%
constexpr double MAKER_FEE_BPS = 2.0;                  // 0.02%
constexpr double TAKER_FEE_BPS = 4.0;                  // 0.04%

// Risk constants
constexpr double MAX_CORRELATION_RISK = 0.8;
constexpr double MAX_FUNDING_RATE_EXPOSURE = 0.01;     // 1% daily
constexpr double MIN_LIQUIDITY_SCORE = 0.7;
constexpr double VAR_CONFIDENCE_LEVEL = 0.95;
constexpr size_t VAR_LOOKBACK_DAYS = 30;

// Performance constants
constexpr size_t METRICS_UPDATE_INTERVAL_MS = 1000;
constexpr size_t PERFORMANCE_SAMPLE_SIZE = 1000;
constexpr double CPU_USAGE_WARNING_THRESHOLD = 80.0;
constexpr size_t MEMORY_USAGE_WARNING_MB = 1500;

// Exchange WebSocket endpoints
namespace endpoints {
    // OKX
    constexpr const char* OKX_WS_PUBLIC = "wss://ws.okx.com:8443/ws/v5/public";
    constexpr const char* OKX_WS_BUSINESS = "wss://ws.okx.com:8443/ws/v5/business";
    
    // Binance
    constexpr const char* BINANCE_WS_SPOT = "wss://stream.binance.com:9443/ws";
    constexpr const char* BINANCE_WS_FUTURES = "wss://fstream.binance.com/ws";
    
    // Bybit
    constexpr const char* BYBIT_WS_SPOT = "wss://stream.bybit.com/v5/public/spot";
    constexpr const char* BYBIT_WS_LINEAR = "wss://stream.bybit.com/v5/public/linear";
    constexpr const char* BYBIT_WS_INVERSE = "wss://stream.bybit.com/v5/public/inverse";
}

// Message types
namespace messages {
    constexpr const char* SUBSCRIBE = "subscribe";
    constexpr const char* UNSUBSCRIBE = "unsubscribe";
    constexpr const char* PING = "ping";
    constexpr const char* PONG = "pong";
    constexpr const char* ERROR = "error";
}

// Channel names
namespace channels {
    // OKX
    constexpr const char* OKX_ORDERBOOK = "books5";
    constexpr const char* OKX_TRADES = "trades";
    constexpr const char* OKX_TICKER = "tickers";
    constexpr const char* OKX_FUNDING_RATE = "funding-rate";
    
    // Binance
    constexpr const char* BINANCE_DEPTH = "depth";
    constexpr const char* BINANCE_TRADE = "trade";
    constexpr const char* BINANCE_TICKER = "ticker";
    constexpr const char* BINANCE_MARK_PRICE = "markPrice";
    
    // Bybit
    constexpr const char* BYBIT_ORDERBOOK = "orderbook";
    constexpr const char* BYBIT_TRADE = "publicTrade";
    constexpr const char* BYBIT_TICKER = "tickers";
    constexpr const char* BYBIT_FUNDING = "fundingRate";
}

// SIMD constants
namespace simd {
    constexpr size_t ALIGNMENT = 32;  // AVX2 alignment
    constexpr size_t VECTOR_SIZE = 4; // Process 4 doubles at once
}

// Memory pool constants
namespace memory {
    constexpr size_t SMALL_BLOCK_SIZE = 64;
    constexpr size_t MEDIUM_BLOCK_SIZE = 512;
    constexpr size_t LARGE_BLOCK_SIZE = 4096;
    constexpr size_t INITIAL_POOL_SIZE = 1000;
}

// Logging constants
namespace logging {
    constexpr const char* DEFAULT_LOG_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v";
    constexpr const char* DEFAULT_LOG_FILE = "arbitrage_engine.log";
    constexpr size_t MAX_LOG_FILE_SIZE = 100 * 1024 * 1024; // 100MB
    constexpr size_t MAX_LOG_FILES = 10;
}

// Mathematical constants
namespace math {
    constexpr double EPSILON = 1e-9;
    constexpr double BASIS_POINT = 0.0001;
    constexpr double ANNUALIZATION_FACTOR = 365.25;
}

} // namespace constants
} // namespace arbitrage