#pragma once

#include "core/types.h"
#include <map>
#include <vector>
#include <shared_mutex>
#include <immintrin.h>

namespace arbitrage {

class OrderBook {
public:
    OrderBook() = default;
    ~OrderBook() = default;
    
    // Update the order book
    void update(const std::vector<PriceLevel>& bids, 
                const std::vector<PriceLevel>& asks);
    
    // Get top of book
    bool get_best_bid(PriceLevel& level) const;
    bool get_best_ask(PriceLevel& level) const;
    
    // Get multiple levels
    std::vector<PriceLevel> get_bids(size_t depth = 10) const;
    std::vector<PriceLevel> get_asks(size_t depth = 10) const;
    
    // Calculate metrics
    Price get_mid_price() const;
    Price get_weighted_mid_price(size_t depth = 5) const;
    double get_spread() const;
    double get_spread_bps() const;
    double get_imbalance(size_t depth = 5) const;
    
    // SIMD-optimized VWAP calculation
    Price calculate_vwap(Side side, Quantity target_quantity) const;
    
    // Get book depth statistics
    struct DepthStats {
        double total_bid_volume;
        double total_ask_volume;
        double avg_bid_price;
        double avg_ask_price;
        size_t bid_levels;
        size_t ask_levels;
    };
    
    DepthStats get_depth_stats(size_t max_levels = 20) const;
    
    // Clear the book
    void clear();
    
    // Check if book is valid
    bool is_valid() const;
    
    // Get update timestamp
    Timestamp get_last_update() const { return last_update_; }
    
    // Thread-safe snapshot
    struct Snapshot {
        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;
        Timestamp timestamp;
    };
    
    Snapshot get_snapshot() const;
    
private:
    // Bid levels (price -> level) in descending order
    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    
    // Ask levels (price -> level) in ascending order
    std::map<Price, PriceLevel> asks_;
    
    // Last update timestamp
    Timestamp last_update_;
    
    // Thread safety
    mutable std::shared_mutex mutex_;
    
    // SIMD helper for VWAP calculation
    struct alignas(32) SimdPriceQuantity {
        __m256d prices;
        __m256d quantities;
    };
    
    Price calculate_vwap_simd(const std::vector<PriceLevel>& levels, 
                             Quantity target_quantity) const;
};

// Lock-free order book for ultra-low latency
template<size_t MaxLevels = 50>
class LockFreeOrderBook {
public:
    LockFreeOrderBook() {
        bid_count_.store(0);
        ask_count_.store(0);
        sequence_.store(0);
    }
    
    void update_bids(const PriceLevel* levels, size_t count) {
        size_t seq = sequence_.fetch_add(1) + 1;
        
        size_t actual_count = std::min(count, MaxLevels);
        for (size_t i = 0; i < actual_count; ++i) {
            bids_[i].store(levels[i]);
        }
        
        bid_count_.store(actual_count);
        bid_sequence_.store(seq);
    }
    
    void update_asks(const PriceLevel* levels, size_t count) {
        size_t seq = sequence_.fetch_add(1) + 1;
        
        size_t actual_count = std::min(count, MaxLevels);
        for (size_t i = 0; i < actual_count; ++i) {
            asks_[i].store(levels[i]);
        }
        
        ask_count_.store(actual_count);
        ask_sequence_.store(seq);
    }
    
    bool get_best_bid(PriceLevel& level) const {
        if (bid_count_.load() == 0) return false;
        level = bids_[0].load();
        return true;
    }
    
    bool get_best_ask(PriceLevel& level) const {
        if (ask_count_.load() == 0) return false;
        level = asks_[0].load();
        return true;
    }
    
    Price get_mid_price() const {
        PriceLevel bid, ask;
        if (get_best_bid(bid) && get_best_ask(ask)) {
            return (bid.price + ask.price) / 2.0;
        }
        return 0.0;
    }
    
private:
    alignas(64) std::array<std::atomic<PriceLevel>, MaxLevels> bids_;
    alignas(64) std::array<std::atomic<PriceLevel>, MaxLevels> asks_;
    
    alignas(64) std::atomic<size_t> bid_count_;
    alignas(64) std::atomic<size_t> ask_count_;
    alignas(64) std::atomic<uint64_t> bid_sequence_;
    alignas(64) std::atomic<uint64_t> ask_sequence_;
    alignas(64) std::atomic<uint64_t> sequence_;
};

} // namespace arbitrage