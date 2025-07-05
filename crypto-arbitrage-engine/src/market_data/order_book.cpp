#include "order_book.h"
#include "core/utils.h"
#include <algorithm>

namespace arbitrage {

void OrderBook::update(const std::vector<PriceLevel>& bids, 
                      const std::vector<PriceLevel>& asks) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Clear and update bids
    bids_.clear();
    for (const auto& bid : bids) {
        bids_[bid.price] = bid;
    }
    
    // Clear and update asks
    asks_.clear();
    for (const auto& ask : asks) {
        asks_[ask.price] = ask;
    }
    
    last_update_ = utils::get_current_timestamp();
}

bool OrderBook::get_best_bid(PriceLevel& level) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (bids_.empty()) return false;
    
    level = bids_.begin()->second;
    return true;
}

bool OrderBook::get_best_ask(PriceLevel& level) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (asks_.empty()) return false;
    
    level = asks_.begin()->second;
    return true;
}

std::vector<PriceLevel> OrderBook::get_bids(size_t depth) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<PriceLevel> result;
    result.reserve(depth);
    
    size_t count = 0;
    for (const auto& [price, level] : bids_) {
        result.push_back(level);
        if (++count >= depth) break;
    }
    
    return result;
}

std::vector<PriceLevel> OrderBook::get_asks(size_t depth) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<PriceLevel> result;
    result.reserve(depth);
    
    size_t count = 0;
    for (const auto& [price, level] : asks_) {
        result.push_back(level);
        if (++count >= depth) break;
    }
    
    return result;
}

Price OrderBook::get_mid_price() const {
    PriceLevel bid, ask;
    
    if (get_best_bid(bid) && get_best_ask(ask)) {
        return (bid.price + ask.price) / 2.0;
    }
    
    return 0.0;
}

Price OrderBook::get_weighted_mid_price(size_t depth) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (bids_.empty() || asks_.empty()) return 0.0;
    
    double total_bid_value = 0.0;
    double total_bid_quantity = 0.0;
    double total_ask_value = 0.0;
    double total_ask_quantity = 0.0;
    
    size_t count = 0;
    for (const auto& [price, level] : bids_) {
        total_bid_value += level.price * level.quantity;
        total_bid_quantity += level.quantity;
        if (++count >= depth) break;
    }
    
    count = 0;
    for (const auto& [price, level] : asks_) {
        total_ask_value += level.price * level.quantity;
        total_ask_quantity += level.quantity;
        if (++count >= depth) break;
    }
    
    if (total_bid_quantity <= 0 || total_ask_quantity <= 0) {
        return get_mid_price();
    }
    
    double bid_vwap = total_bid_value / total_bid_quantity;
    double ask_vwap = total_ask_value / total_ask_quantity;
    
    // Weight by inverse spread
    double total_weight = total_bid_quantity + total_ask_quantity;
    return (bid_vwap * total_ask_quantity + ask_vwap * total_bid_quantity) / total_weight;
}

double OrderBook::get_spread() const {
    PriceLevel bid, ask;
    
    if (get_best_bid(bid) && get_best_ask(ask)) {
        return ask.price - bid.price;
    }
    
    return 0.0;
}

double OrderBook::get_spread_bps() const {
    PriceLevel bid, ask;
    
    if (get_best_bid(bid) && get_best_ask(ask)) {
        double mid = (bid.price + ask.price) / 2.0;
        if (mid > 0) {
            return (ask.price - bid.price) / mid * 10000;
        }
    }
    
    return 0.0;
}

double OrderBook::get_imbalance(size_t depth) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    double total_bid_quantity = 0.0;
    double total_ask_quantity = 0.0;
    
    size_t count = 0;
    for (const auto& [price, level] : bids_) {
        total_bid_quantity += level.quantity;
        if (++count >= depth) break;
    }
    
    count = 0;
    for (const auto& [price, level] : asks_) {
        total_ask_quantity += level.quantity;
        if (++count >= depth) break;
    }
    
    double total = total_bid_quantity + total_ask_quantity;
    if (total <= 0) return 0.0;
    
    return (total_bid_quantity - total_ask_quantity) / total;
}

Price OrderBook::calculate_vwap(Side side, Quantity target_quantity) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (side == Side::BUY) {
        std::vector<PriceLevel> levels;
        for (const auto& [price, level] : asks_) {
            levels.push_back(level);
        }
        return calculate_vwap_simd(levels, target_quantity);
    } else {
        std::vector<PriceLevel> levels;
        for (const auto& [price, level] : bids_) {
            levels.push_back(level);
        }
        return calculate_vwap_simd(levels, target_quantity);
    }
}

Price OrderBook::calculate_vwap_simd(const std::vector<PriceLevel>& levels, 
                                    Quantity target_quantity) const {
    if (levels.empty() || target_quantity <= 0) return 0.0;
    
    double total_value = 0.0;
    double total_quantity = 0.0;
    
    // Process 4 levels at a time using AVX
    size_t i = 0;
    for (; i + 3 < levels.size() && total_quantity < target_quantity; i += 4) {
        __m256d prices = _mm256_set_pd(
            levels[i+3].price, levels[i+2].price, 
            levels[i+1].price, levels[i].price
        );
        
        __m256d quantities = _mm256_set_pd(
            levels[i+3].quantity, levels[i+2].quantity,
            levels[i+1].quantity, levels[i].quantity
        );
        
        // Check if we would exceed target
        double batch_quantity = levels[i].quantity + levels[i+1].quantity + 
                               levels[i+2].quantity + levels[i+3].quantity;
        
        if (total_quantity + batch_quantity <= target_quantity) {
            __m256d values = _mm256_mul_pd(prices, quantities);
            
            // Sum the results
            double value_array[4];
            double qty_array[4];
            _mm256_storeu_pd(value_array, values);
            _mm256_storeu_pd(qty_array, quantities);
            
            for (int j = 0; j < 4; ++j) {
                total_value += value_array[j];
                total_quantity += qty_array[j];
            }
        } else {
            // Process individually
            break;
        }
    }
    
    // Process remaining levels
    for (; i < levels.size() && total_quantity < target_quantity; ++i) {
        double remaining = target_quantity - total_quantity;
        double qty = std::min(remaining, levels[i].quantity);
        
        total_value += levels[i].price * qty;
        total_quantity += qty;
    }
    
    return total_quantity > 0 ? total_value / total_quantity : 0.0;
}

OrderBook::DepthStats OrderBook::get_depth_stats(size_t max_levels) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    DepthStats stats{};
    
    size_t count = 0;
    for (const auto& [price, level] : bids_) {
        stats.total_bid_volume += level.quantity;
        stats.avg_bid_price += price * level.quantity;
        stats.bid_levels++;
        if (++count >= max_levels) break;
    }
    
    if (stats.total_bid_volume > 0) {
        stats.avg_bid_price /= stats.total_bid_volume;
    }
    
    count = 0;
    for (const auto& [price, level] : asks_) {
        stats.total_ask_volume += level.quantity;
        stats.avg_ask_price += price * level.quantity;
        stats.ask_levels++;
        if (++count >= max_levels) break;
    }
    
    if (stats.total_ask_volume > 0) {
        stats.avg_ask_price /= stats.total_ask_volume;
    }
    
    return stats;
}

void OrderBook::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bids_.clear();
    asks_.clear();
}

bool OrderBook::is_valid() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (bids_.empty() || asks_.empty()) return false;
    
    // Check if best bid is less than best ask
    return bids_.begin()->first < asks_.begin()->first;
}

OrderBook::Snapshot OrderBook::get_snapshot() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    Snapshot snapshot;
    snapshot.timestamp = last_update_;
    
    for (const auto& [price, level] : bids_) {
        snapshot.bids.push_back(level);
    }
    
    for (const auto& [price, level] : asks_) {
        snapshot.asks.push_back(level);
    }
    
    return snapshot;
}

} // namespace arbitrage