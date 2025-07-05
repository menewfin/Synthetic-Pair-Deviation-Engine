#include "risk_manager.h"
#include "market_data/market_data_manager.h"
#include "core/utils.h"
#include "utils/logger.h"
#include <algorithm>
#include <numeric>

namespace arbitrage {

RiskManager::RiskManager(MarketDataManager* market_data)
    : market_data_(market_data)
    , max_portfolio_exposure_(constants::MAX_PORTFOLIO_EXPOSURE) {
    
    // Set default position limits
    position_limits_["BTC-USDT"] = 10.0;
    position_limits_["ETH-USDT"] = 100.0;
    position_limits_["SOL-USDT"] = 1000.0;
    
    // Set default exchange limits
    exchange_limits_[Exchange::OKX] = 300000.0;
    exchange_limits_[Exchange::BINANCE] = 400000.0;
    exchange_limits_[Exchange::BYBIT] = 300000.0;
}

bool RiskManager::check_opportunity_risk(const ArbitrageOpportunity& opportunity) {
    // Check execution risk
    if (opportunity.execution_risk > 0.7) {
        LOG_WARN("Opportunity {} rejected - high execution risk: {}", 
                opportunity.id, opportunity.execution_risk);
        return false;
    }
    
    // Check funding risk for perpetuals
    if (opportunity.funding_risk > constants::MAX_FUNDING_RATE_EXPOSURE) {
        LOG_WARN("Opportunity {} rejected - high funding risk: {}", 
                opportunity.id, opportunity.funding_risk);
        return false;
    }
    
    // Check liquidity
    if (opportunity.liquidity_score < constants::MIN_LIQUIDITY_SCORE) {
        LOG_WARN("Opportunity {} rejected - low liquidity: {}", 
                opportunity.id, opportunity.liquidity_score);
        return false;
    }
    
    // Check position limits for each leg
    for (const auto& leg : opportunity.legs) {
        if (!check_position_limit(leg.symbol, leg.quantity)) {
            return false;
        }
    }
    
    // Check total portfolio exposure
    double additional_exposure = opportunity.required_capital;
    double current_exposure = calculate_total_exposure();
    
    if (current_exposure + additional_exposure > max_portfolio_exposure_) {
        LOG_WARN("Opportunity {} rejected - would exceed portfolio limit", opportunity.id);
        return false;
    }
    
    return true;
}

bool RiskManager::check_position_limit(const Symbol& symbol, double size) {
    auto it = position_limits_.find(symbol);
    if (it == position_limits_.end()) {
        // Use default limit
        return size <= 50000.0;
    }
    
    // Check current position + new size
    std::lock_guard<std::mutex> lock(positions_mutex_);
    double current_position = 0.0;
    
    for (const auto& [key, position] : positions_) {
        if (key.symbol == symbol) {
            current_position += position.quantity;
        }
    }
    
    return (current_position + size) <= it->second;
}

bool RiskManager::check_exchange_exposure(Exchange exchange, double exposure) {
    auto it = exchange_limits_.find(exchange);
    if (it == exchange_limits_.end()) {
        return true;
    }
    
    // Calculate current exchange exposure
    std::lock_guard<std::mutex> lock(positions_mutex_);
    double current_exposure = 0.0;
    
    for (const auto& [key, position] : positions_) {
        if (key.exchange == exchange) {
            current_exposure += calculate_position_exposure(position);
        }
    }
    
    return (current_exposure + exposure) <= it->second;
}

bool RiskManager::check_portfolio_risk() {
    RiskMetrics metrics = calculate_risk_metrics();
    
    // Check VaR
    if (metrics.portfolio_var > max_portfolio_exposure_ * 0.1) {  // 10% of max exposure
        LOG_WARN("Portfolio VaR exceeds limit: {}", metrics.portfolio_var);
        return false;
    }
    
    // Check correlation risk
    if (metrics.correlation_risk > constants::MAX_CORRELATION_RISK) {
        LOG_WARN("Portfolio correlation risk too high: {}", metrics.correlation_risk);
        return false;
    }
    
    return true;
}

void RiskManager::add_position(const PositionInfo& position) {
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    PositionKey key{position.symbol, position.exchange};
    positions_[key] = position;
    
    LOG_INFO("Added position: {} {} @ {} on {}", 
            position.side == Side::BUY ? "BUY" : "SELL",
            position.quantity,
            position.average_price,
            utils::exchange_to_string(position.exchange));
}

void RiskManager::update_position(const Symbol& symbol, Exchange exchange, 
                                 const PositionInfo& position) {
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    PositionKey key{symbol, exchange};
    positions_[key] = position;
}

void RiskManager::close_position(const Symbol& symbol, Exchange exchange) {
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    PositionKey key{symbol, exchange};
    auto it = positions_.find(key);
    
    if (it != positions_.end()) {
        // Record P&L
        double pnl = it->second.unrealized_pnl();
        record_pnl(pnl);
        
        positions_.erase(it);
        
        LOG_INFO("Closed position: {} on {} - P&L: {}", 
                symbol, utils::exchange_to_string(exchange), pnl);
    }
}

RiskMetrics RiskManager::calculate_risk_metrics() const {
    // Check cache
    auto now = std::chrono::steady_clock::now();
    if (now - metrics_cache_time_ < std::chrono::seconds(5)) {
        return cached_metrics_;
    }
    
    RiskMetrics metrics{};
    
    // Calculate portfolio VaR
    metrics.portfolio_var = calculate_portfolio_var();
    
    // Calculate max drawdown
    metrics.max_drawdown = calculate_max_drawdown();
    
    // Calculate Sharpe ratio
    metrics.sharpe_ratio = calculate_sharpe_ratio();
    
    // Calculate correlation risk (simplified)
    metrics.correlation_risk = 0.5;  // Placeholder
    
    // Calculate funding rate exposure
    std::lock_guard<std::mutex> lock(positions_mutex_);
    double total_perp_exposure = 0.0;
    
    for (const auto& [key, position] : positions_) {
        if (position.type == InstrumentType::PERPETUAL) {
            total_perp_exposure += calculate_position_exposure(position);
        }
    }
    
    metrics.funding_rate_exposure = total_perp_exposure / calculate_total_exposure();
    
    // Update cache
    cached_metrics_ = metrics;
    metrics_cache_time_ = now;
    
    return metrics;
}

double RiskManager::calculate_portfolio_var(double confidence_level) const {
    if (returns_history_.empty()) {
        return 0.0;
    }
    
    // Sort returns
    std::vector<double> sorted_returns = returns_history_;
    std::sort(sorted_returns.begin(), sorted_returns.end());
    
    // Calculate VaR at confidence level
    size_t index = static_cast<size_t>((1.0 - confidence_level) * sorted_returns.size());
    
    double var_percentage = -sorted_returns[index];
    
    // Apply to current portfolio value
    double portfolio_value = calculate_total_exposure();
    
    return portfolio_value * var_percentage;
}

double RiskManager::calculate_max_drawdown() const {
    if (pnl_history_.empty()) {
        return 0.0;
    }
    
    double peak = 0.0;
    double max_drawdown = 0.0;
    double cumulative_pnl = 0.0;
    
    for (double pnl : pnl_history_) {
        cumulative_pnl += pnl;
        
        if (cumulative_pnl > peak) {
            peak = cumulative_pnl;
        }
        
        double drawdown = (peak - cumulative_pnl) / peak;
        max_drawdown = std::max(max_drawdown, drawdown);
    }
    
    return max_drawdown;
}

double RiskManager::calculate_sharpe_ratio() const {
    if (returns_history_.size() < 2) {
        return 0.0;
    }
    
    double mean_return = utils::calculate_mean(returns_history_);
    double std_dev = utils::calculate_std_dev(returns_history_);
    
    if (std_dev < constants::math::EPSILON) {
        return 0.0;
    }
    
    // Annualize
    double annual_return = mean_return * 365;
    double annual_std_dev = std_dev * std::sqrt(365);
    
    // Risk-free rate (simplified)
    double risk_free_rate = 0.02;
    
    return (annual_return - risk_free_rate) / annual_std_dev;
}

void RiskManager::record_pnl(double pnl) {
    pnl_history_.push_back(pnl);
    
    // Update returns history
    double portfolio_value = calculate_total_exposure();
    if (portfolio_value > 0) {
        double daily_return = pnl / portfolio_value;
        returns_history_.push_back(daily_return);
    }
    
    // Keep only recent history
    if (pnl_history_.size() > 1000) {
        pnl_history_.erase(pnl_history_.begin());
    }
    
    if (returns_history_.size() > constants::VAR_LOOKBACK_DAYS) {
        returns_history_.erase(returns_history_.begin());
    }
}

double RiskManager::calculate_position_exposure(const PositionInfo& position) const {
    return position.quantity * position.current_price;
}

double RiskManager::calculate_total_exposure() const {
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    double total = 0.0;
    for (const auto& [key, position] : positions_) {
        total += calculate_position_exposure(position);
    }
    
    return total;
}

// VaRCalculator implementation

VaRCalculator::VaRCalculator(size_t lookback_days)
    : lookback_days_(lookback_days) {
}

void VaRCalculator::add_return(double daily_return) {
    returns_.push_back(daily_return);
    
    // Keep only lookback period
    if (returns_.size() > lookback_days_) {
        returns_.erase(returns_.begin());
    }
}

double VaRCalculator::calculate_var(double confidence_level) const {
    if (returns_.empty()) return 0.0;
    
    std::vector<double> sorted = returns_;
    std::sort(sorted.begin(), sorted.end());
    
    size_t index = static_cast<size_t>((1.0 - confidence_level) * sorted.size());
    return -sorted[index];
}

double VaRCalculator::calculate_cvar(double confidence_level) const {
    if (returns_.empty()) return 0.0;
    
    std::vector<double> sorted = returns_;
    std::sort(sorted.begin(), sorted.end());
    
    size_t cutoff_index = static_cast<size_t>((1.0 - confidence_level) * sorted.size());
    
    double sum = 0.0;
    for (size_t i = 0; i <= cutoff_index; ++i) {
        sum += sorted[i];
    }
    
    return -sum / (cutoff_index + 1);
}

// PositionSizer implementation

double PositionSizer::kelly_criterion(double win_probability, 
                                     double avg_win, 
                                     double avg_loss) {
    if (avg_loss <= 0) return 0.0;
    
    double win_loss_ratio = avg_win / avg_loss;
    double kelly_fraction = (win_probability * win_loss_ratio - (1 - win_probability)) / win_loss_ratio;
    
    // Apply Kelly fraction with safety factor
    return std::max(0.0, std::min(0.25, kelly_fraction * 0.5));  // Max 25% of capital
}

} // namespace arbitrage