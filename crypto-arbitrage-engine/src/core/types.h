#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <array>

namespace arbitrage {

// Forward declarations
class OrderBook;
class Position;

// Basic types
using Price = double;
using Quantity = double;
using OrderId = uint64_t;
using Timestamp = std::chrono::nanoseconds;
using Symbol = std::string;

// Exchange identifiers
enum class Exchange {
    OKX,
    BINANCE,
    BYBIT
};

// Instrument types
enum class InstrumentType {
    SPOT,
    PERPETUAL,
    FUTURES,
    OPTION
};

// Order side
enum class Side {
    BUY,
    SELL
};

// Market data types
struct PriceLevel {
    Price price;
    Quantity quantity;
    uint32_t order_count;
    
    PriceLevel() = default;
    PriceLevel(Price p, Quantity q, uint32_t c = 1) 
        : price(p), quantity(q), order_count(c) {}
};

struct MarketData {
    Symbol symbol;
    Exchange exchange;
    InstrumentType type;
    Timestamp timestamp;
    Price bid_price;
    Price ask_price;
    Quantity bid_size;
    Quantity ask_size;
    Price last_price;
    Quantity volume_24h;
    Price funding_rate;  // For perpetuals
    Timestamp expiry;    // For futures
    
    Price mid_price() const { return (bid_price + ask_price) / 2.0; }
    Price spread() const { return ask_price - bid_price; }
};

// Synthetic instrument definition
struct SyntheticInstrument {
    std::string id;
    std::vector<std::pair<Symbol, double>> components;  // Symbol and weight
    InstrumentType type;
    
    Price calculate_price(const std::unordered_map<Symbol, MarketData>& market_data) const {
        Price synthetic_price = 0.0;
        for (const auto& [symbol, weight] : components) {
            auto it = market_data.find(symbol);
            if (it != market_data.end()) {
                synthetic_price += it->second.mid_price() * weight;
            }
        }
        return synthetic_price;
    }
};

// Arbitrage opportunity
struct ArbitrageOpportunity {
    std::string id;
    Timestamp timestamp;
    
    // Legs of the arbitrage
    struct Leg {
        Symbol symbol;
        Exchange exchange;
        Side side;
        Price price;
        Quantity quantity;
        InstrumentType type;
        bool is_synthetic;
    };
    
    std::vector<Leg> legs;
    
    // Profitability metrics
    Price expected_profit;
    double profit_percentage;
    Price required_capital;
    
    // Risk metrics
    double execution_risk;
    double funding_risk;
    double liquidity_score;
    
    // Execution parameters
    uint32_t ttl_ms;  // Time to live in milliseconds
    bool is_executable;
};

// Position information
struct PositionInfo {
    Symbol symbol;
    Exchange exchange;
    InstrumentType type;
    Side side;
    Quantity quantity;
    Price average_price;
    Price current_price;
    Timestamp entry_time;
    
    Price unrealized_pnl() const {
        return (current_price - average_price) * quantity * (side == Side::BUY ? 1 : -1);
    }
};

// Risk metrics
struct RiskMetrics {
    double portfolio_var;      // Value at Risk
    double max_drawdown;
    double sharpe_ratio;
    double correlation_risk;
    double funding_rate_exposure;
    double liquidity_risk;
    
    std::unordered_map<Symbol, double> position_limits;
    std::unordered_map<Exchange, double> exchange_limits;
};

// Performance metrics
struct PerformanceMetrics {
    // Latency metrics (in microseconds)
    std::atomic<uint64_t> avg_processing_latency{0};
    std::atomic<uint64_t> max_processing_latency{0};
    std::atomic<uint64_t> avg_detection_latency{0};
    
    // Throughput metrics
    std::atomic<uint64_t> messages_processed{0};
    std::atomic<uint64_t> opportunities_detected{0};
    std::atomic<uint64_t> opportunities_executed{0};
    
    // System metrics
    std::atomic<uint64_t> memory_usage_mb{0};
    std::atomic<double> cpu_usage_percent{0.0};
    
    // Business metrics
    std::atomic<double> total_pnl{0.0};
    std::atomic<uint64_t> profitable_trades{0};
    std::atomic<uint64_t> total_trades{0};
};

// Configuration structures
struct ExchangeConfig {
    std::string name;
    std::string ws_endpoint;
    std::string rest_endpoint;
    std::vector<std::string> symbols;
    std::vector<InstrumentType> instrument_types;
    uint32_t reconnect_interval_ms;
    uint32_t heartbeat_interval_ms;
};

struct ArbitrageConfig {
    double min_profit_threshold;      // Minimum profit percentage
    double max_position_size;         // Maximum position size in USD
    double max_portfolio_exposure;    // Maximum total exposure
    uint32_t opportunity_ttl_ms;      // Opportunity time-to-live
    double execution_slippage_bps;    // Expected slippage in basis points
};

struct SystemConfig {
    uint32_t thread_pool_size;
    uint32_t order_book_depth;
    uint32_t market_data_buffer_size;
    bool enable_simd_optimization;
    bool enable_memory_pooling;
    std::string log_level;
    std::string log_file;
};

// Aligned data structures for SIMD operations
alignas(32) struct AlignedPriceData {
    std::array<double, 4> prices;
    std::array<double, 4> quantities;
    
    void reset() {
        prices.fill(0.0);
        quantities.fill(0.0);
    }
};

// Thread-safe circular buffer for market data
template<typename T, size_t N>
class CircularBuffer {
private:
    alignas(64) std::array<T, N> buffer_;
    alignas(64) std::atomic<size_t> write_pos_{0};
    alignas(64) std::atomic<size_t> read_pos_{0};
    
public:
    bool push(const T& item) {
        size_t current_write = write_pos_.load(std::memory_order_relaxed);
        size_t next_write = (current_write + 1) % N;
        
        if (next_write == read_pos_.load(std::memory_order_acquire)) {
            return false;  // Buffer full
        }
        
        buffer_[current_write] = item;
        write_pos_.store(next_write, std::memory_order_release);
        return true;
    }
    
    bool pop(T& item) {
        size_t current_read = read_pos_.load(std::memory_order_relaxed);
        
        if (current_read == write_pos_.load(std::memory_order_acquire)) {
            return false;  // Buffer empty
        }
        
        item = buffer_[current_read];
        read_pos_.store((current_read + 1) % N, std::memory_order_release);
        return true;
    }
    
    size_t size() const {
        size_t write = write_pos_.load(std::memory_order_acquire);
        size_t read = read_pos_.load(std::memory_order_acquire);
        return (write >= read) ? (write - read) : (N - read + write);
    }
};

} // namespace arbitrage