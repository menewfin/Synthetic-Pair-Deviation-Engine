#pragma once

#include "core/types.h"
#include <memory>
#include <unordered_map>
#include <mutex>

namespace arbitrage {

// Forward declarations
class MarketDataManager;

class RiskManager {
public:
    explicit RiskManager(MarketDataManager* market_data);
    ~RiskManager() = default;
    
    // Risk limits configuration
    void set_max_portfolio_exposure(double max_exposure) {
        max_portfolio_exposure_ = max_exposure;
    }
    
    void set_position_limit(const Symbol& symbol, double limit) {
        position_limits_[symbol] = limit;
    }
    
    void set_exchange_limit(Exchange exchange, double limit) {
        exchange_limits_[exchange] = limit;
    }
    
    // Risk checks
    bool check_opportunity_risk(const ArbitrageOpportunity& opportunity);
    bool check_position_limit(const Symbol& symbol, double size);
    bool check_exchange_exposure(Exchange exchange, double exposure);
    bool check_portfolio_risk();
    
    // Position management
    void add_position(const PositionInfo& position);
    void update_position(const Symbol& symbol, Exchange exchange, 
                        const PositionInfo& position);
    void close_position(const Symbol& symbol, Exchange exchange);
    
    // Risk metrics calculation
    RiskMetrics calculate_risk_metrics() const;
    double calculate_portfolio_var(double confidence_level = 0.95) const;
    double calculate_max_drawdown() const;
    double calculate_sharpe_ratio() const;
    
    // Correlation analysis
    double calculate_correlation(const Symbol& symbol1, const Symbol& symbol2) const;
    std::unordered_map<std::string, double> get_correlation_matrix() const;
    
    // Stress testing
    struct StressTestScenario {
        std::string name;
        std::unordered_map<Symbol, double> price_shocks;  // % changes
        double funding_rate_shock;
        double volatility_multiplier;
    };
    
    struct StressTestResult {
        std::string scenario_name;
        double portfolio_loss;
        double worst_position_loss;
        Symbol worst_position_symbol;
        bool breaches_limits;
    };
    
    std::vector<StressTestResult> run_stress_tests(
        const std::vector<StressTestScenario>& scenarios) const;
    
    // Real-time monitoring
    struct Alert {
        enum Type {
            POSITION_LIMIT_WARNING,
            EXCHANGE_EXPOSURE_WARNING,
            CORRELATION_RISK_WARNING,
            VAR_BREACH,
            DRAWDOWN_WARNING
        };
        
        Type type;
        std::string message;
        double severity;  // 0-1
        Timestamp timestamp;
    };
    
    std::vector<Alert> get_active_alerts() const;
    
    // Historical tracking
    void record_pnl(double pnl);
    std::vector<double> get_pnl_history() const { return pnl_history_; }
    
private:
    MarketDataManager* market_data_;
    
    // Risk limits
    double max_portfolio_exposure_;
    std::unordered_map<Symbol, double> position_limits_;
    std::unordered_map<Exchange, double> exchange_limits_;
    
    // Current positions
    struct PositionKey {
        Symbol symbol;
        Exchange exchange;
        
        bool operator==(const PositionKey& other) const {
            return symbol == other.symbol && exchange == other.exchange;
        }
    };
    
    struct PositionKeyHash {
        std::size_t operator()(const PositionKey& key) const {
            return std::hash<std::string>{}(key.symbol) ^ 
                   (std::hash<int>{}(static_cast<int>(key.exchange)) << 1);
        }
    };
    
    std::unordered_map<PositionKey, PositionInfo, PositionKeyHash> positions_;
    mutable std::mutex positions_mutex_;
    
    // Risk metrics cache
    mutable RiskMetrics cached_metrics_;
    mutable std::chrono::steady_clock::time_point metrics_cache_time_;
    
    // Historical data
    std::vector<double> pnl_history_;
    std::vector<double> returns_history_;
    
    // Alerts
    std::vector<Alert> active_alerts_;
    mutable std::mutex alerts_mutex_;
    
    // Helper methods
    double calculate_position_exposure(const PositionInfo& position) const;
    double calculate_total_exposure() const;
    void update_returns_history();
    void check_and_generate_alerts();
};

// VaR calculator using historical simulation
class VaRCalculator {
public:
    explicit VaRCalculator(size_t lookback_days = 30);
    
    void add_return(double daily_return);
    double calculate_var(double confidence_level = 0.95) const;
    double calculate_cvar(double confidence_level = 0.95) const;  // Conditional VaR
    
private:
    std::vector<double> returns_;
    size_t lookback_days_;
};

// Position sizing calculator
class PositionSizer {
public:
    // Kelly criterion for optimal sizing
    static double kelly_criterion(double win_probability, 
                                 double avg_win, 
                                 double avg_loss);
    
    // Risk parity sizing
    static std::unordered_map<Symbol, double> risk_parity_weights(
        const std::unordered_map<Symbol, double>& volatilities,
        double target_risk);
    
    // Maximum position size based on liquidity
    static double max_size_by_liquidity(const OrderBook& book, 
                                       double max_market_impact_bps);
};

} // namespace arbitrage