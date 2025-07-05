#pragma once

#include "core/types.h"
#include <memory>
#include <vector>
#include <functional>

namespace arbitrage {

// Forward declarations
class MarketDataManager;
class SyntheticPricer;
class RiskManager;

class ArbitrageDetector {
public:
    ArbitrageDetector(MarketDataManager* market_data,
                     RiskManager* risk_manager);
    ~ArbitrageDetector();
    
    // Configure detection parameters
    void set_min_profit_threshold(double min_profit_bps) {
        min_profit_threshold_ = min_profit_bps;
    }
    
    void set_max_position_size(double max_size_usd) {
        max_position_size_ = max_size_usd;
    }
    
    // Start/stop detection
    void start();
    void stop();
    
    // Register callback for opportunities
    using OpportunityCallback = std::function<void(const ArbitrageOpportunity&)>;
    void register_opportunity_callback(OpportunityCallback callback);
    
    // Get current opportunities
    std::vector<ArbitrageOpportunity> get_current_opportunities() const;
    
    // Detection methods
    void detect_spot_arbitrage();
    void detect_synthetic_arbitrage();
    void detect_triangular_arbitrage();
    void detect_funding_arbitrage();
    
    // Statistics
    struct Statistics {
        uint64_t opportunities_detected;
        uint64_t opportunities_expired;
        double avg_profit_bps;
        double total_profit_potential;
        std::unordered_map<std::string, uint64_t> opportunities_by_type;
    };
    
    Statistics get_statistics() const;
    
private:
    MarketDataManager* market_data_;
    RiskManager* risk_manager_;
    
    // Synthetic pricers
    std::unique_ptr<SyntheticPricer> multi_leg_pricer_;
    std::unique_ptr<SyntheticPricer> stat_arb_pricer_;
    std::unique_ptr<SyntheticPricer> futures_pricer_;
    std::unique_ptr<SyntheticPricer> perpetual_pricer_;
    
    // Detection parameters
    double min_profit_threshold_;
    double max_position_size_;
    uint32_t opportunity_ttl_ms_;
    
    // Current opportunities
    std::vector<ArbitrageOpportunity> current_opportunities_;
    mutable std::mutex opportunities_mutex_;
    
    // Callbacks
    std::vector<OpportunityCallback> callbacks_;
    mutable std::mutex callbacks_mutex_;
    
    // Detection thread
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> detection_thread_;
    
    // Statistics
    std::atomic<uint64_t> total_opportunities_{0};
    std::atomic<uint64_t> expired_opportunities_{0};
    
    // Detection loop
    void detection_loop();
    
    // Helper methods
    ArbitrageOpportunity create_spot_opportunity(
        const Symbol& symbol,
        Exchange buy_exchange,
        Exchange sell_exchange,
        const MarketData& buy_data,
        const MarketData& sell_data);
    
    ArbitrageOpportunity create_synthetic_opportunity(
        const Symbol& symbol,
        const std::vector<ArbitrageOpportunity::Leg>& legs,
        double expected_profit);
    
    void notify_callbacks(const ArbitrageOpportunity& opportunity);
    void cleanup_expired_opportunities();
    
    // Profit calculation
    double calculate_net_profit(const ArbitrageOpportunity& opportunity) const;
    double calculate_execution_risk(const ArbitrageOpportunity& opportunity) const;
};

} // namespace arbitrage