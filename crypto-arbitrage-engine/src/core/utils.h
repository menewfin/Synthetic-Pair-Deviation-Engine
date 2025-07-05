#pragma once

#include <string>
#include <chrono>
#include <immintrin.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include "types.h"
#include "constants.h"

namespace arbitrage {
namespace utils {

// Time utilities
inline Timestamp get_current_timestamp() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    );
}

inline uint64_t timestamp_to_microseconds(const Timestamp& ts) {
    return std::chrono::duration_cast<std::chrono::microseconds>(ts).count();
}

inline std::string timestamp_to_string(const Timestamp& ts) {
    auto time_t = std::chrono::duration_cast<std::chrono::milliseconds>(ts).count();
    auto seconds = time_t / 1000;
    auto milliseconds = time_t % 1000;
    
    std::stringstream ss;
    ss << seconds << "." << std::setfill('0') << std::setw(3) << milliseconds;
    return ss.str();
}

// String utilities
inline std::string exchange_to_string(Exchange exchange) {
    switch (exchange) {
        case Exchange::OKX: return "OKX";
        case Exchange::BINANCE: return "Binance";
        case Exchange::BYBIT: return "Bybit";
        default: return "Unknown";
    }
}

inline std::string instrument_type_to_string(InstrumentType type) {
    switch (type) {
        case InstrumentType::SPOT: return "SPOT";
        case InstrumentType::PERPETUAL: return "PERPETUAL";
        case InstrumentType::FUTURES: return "FUTURES";
        case InstrumentType::OPTION: return "OPTION";
        default: return "Unknown";
    }
}

inline std::string side_to_string(Side side) {
    return side == Side::BUY ? "BUY" : "SELL";
}

// Mathematical utilities
inline bool is_approximately_equal(double a, double b, double epsilon = constants::math::EPSILON) {
    return std::abs(a - b) < epsilon;
}

inline double round_to_tick_size(double price, double tick_size) {
    return std::round(price / tick_size) * tick_size;
}

inline double calculate_percentage_change(double from, double to) {
    if (std::abs(from) < constants::math::EPSILON) {
        return 0.0;
    }
    return ((to - from) / from) * 100.0;
}

// SIMD utilities for price calculations
inline __m256d calculate_weighted_prices_simd(const __m256d& prices, const __m256d& weights) {
    return _mm256_mul_pd(prices, weights);
}

inline double sum_simd_result(const __m256d& vec) {
    // Horizontal add
    __m128d low = _mm256_castpd256_pd128(vec);
    __m128d high = _mm256_extractf128_pd(vec, 1);
    __m128d sum = _mm_add_pd(low, high);
    
    // Final reduction
    __m128d shuffle = _mm_shuffle_pd(sum, sum, 1);
    __m128d result = _mm_add_sd(sum, shuffle);
    
    return _mm_cvtsd_f64(result);
}

// Memory alignment utilities
template<typename T>
inline T* aligned_alloc(size_t count, size_t alignment = constants::simd::ALIGNMENT) {
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, count * sizeof(T)) != 0) {
        return nullptr;
    }
    return static_cast<T*>(ptr);
}

template<typename T>
inline void aligned_free(T* ptr) {
    free(ptr);
}

// Risk calculation utilities
inline double calculate_var(const std::vector<double>& returns, double confidence_level) {
    if (returns.empty()) return 0.0;
    
    std::vector<double> sorted_returns = returns;
    std::sort(sorted_returns.begin(), sorted_returns.end());
    
    size_t index = static_cast<size_t>((1.0 - confidence_level) * sorted_returns.size());
    return -sorted_returns[index];
}

inline double calculate_sharpe_ratio(double avg_return, double std_dev, double risk_free_rate = 0.02) {
    if (std_dev < constants::math::EPSILON) return 0.0;
    return (avg_return - risk_free_rate) / std_dev;
}

// Order book utilities
inline double calculate_book_imbalance(const std::vector<PriceLevel>& bids, 
                                      const std::vector<PriceLevel>& asks) {
    if (bids.empty() || asks.empty()) return 0.0;
    
    double bid_volume = 0.0;
    double ask_volume = 0.0;
    
    size_t levels = std::min(size_t(5), std::min(bids.size(), asks.size()));
    
    for (size_t i = 0; i < levels; ++i) {
        bid_volume += bids[i].quantity;
        ask_volume += asks[i].quantity;
    }
    
    double total_volume = bid_volume + ask_volume;
    if (total_volume < constants::math::EPSILON) return 0.0;
    
    return (bid_volume - ask_volume) / total_volume;
}

inline double calculate_weighted_mid_price(const std::vector<PriceLevel>& bids,
                                          const std::vector<PriceLevel>& asks) {
    if (bids.empty() || asks.empty()) return 0.0;
    
    double bid_weight = bids[0].quantity;
    double ask_weight = asks[0].quantity;
    double total_weight = bid_weight + ask_weight;
    
    if (total_weight < constants::math::EPSILON) {
        return (bids[0].price + asks[0].price) / 2.0;
    }
    
    return (bids[0].price * ask_weight + asks[0].price * bid_weight) / total_weight;
}

// Synthetic pricing utilities
inline double calculate_futures_fair_value(double spot_price, double interest_rate, 
                                          double dividend_yield, double time_to_expiry) {
    return spot_price * std::exp((interest_rate - dividend_yield) * time_to_expiry);
}

inline double calculate_perpetual_basis(double perpetual_price, double spot_price) {
    if (spot_price < constants::math::EPSILON) return 0.0;
    return (perpetual_price - spot_price) / spot_price;
}

inline double calculate_funding_pnl(double position_size, double funding_rate, double hours) {
    // Most exchanges use 8-hour funding intervals
    return position_size * funding_rate * (hours / 8.0);
}

// Execution utilities
inline double calculate_slippage(double expected_price, double actual_price, Side side) {
    double slippage_bps = 0.0;
    
    if (side == Side::BUY) {
        slippage_bps = (actual_price - expected_price) / expected_price * 10000;
    } else {
        slippage_bps = (expected_price - actual_price) / expected_price * 10000;
    }
    
    return slippage_bps;
}

inline double calculate_execution_cost(double notional, double fee_bps, double slippage_bps) {
    return notional * (fee_bps + slippage_bps) / 10000;
}

// Statistics utilities
template<typename T>
inline double calculate_mean(const std::vector<T>& values) {
    if (values.empty()) return 0.0;
    
    double sum = 0.0;
    for (const auto& val : values) {
        sum += static_cast<double>(val);
    }
    return sum / values.size();
}

template<typename T>
inline double calculate_std_dev(const std::vector<T>& values) {
    if (values.size() < 2) return 0.0;
    
    double mean = calculate_mean(values);
    double sum_sq_diff = 0.0;
    
    for (const auto& val : values) {
        double diff = static_cast<double>(val) - mean;
        sum_sq_diff += diff * diff;
    }
    
    return std::sqrt(sum_sq_diff / (values.size() - 1));
}

// Performance measurement utilities
class ScopedTimer {
private:
    std::chrono::high_resolution_clock::time_point start_;
    std::function<void(uint64_t)> callback_;
    
public:
    explicit ScopedTimer(std::function<void(uint64_t)> callback)
        : start_(std::chrono::high_resolution_clock::now())
        , callback_(std::move(callback)) {}
    
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
        if (callback_) {
            callback_(duration);
        }
    }
};

// ID generation
inline std::string generate_opportunity_id(const std::string& strategy, 
                                          const Timestamp& timestamp) {
    std::stringstream ss;
    ss << strategy << "_" << timestamp_to_microseconds(timestamp);
    return ss.str();
}

} // namespace utils
} // namespace arbitrage