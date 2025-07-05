#pragma once

#include "core/types.h"
#include <memory>
#include <vector>

namespace arbitrage {

// Forward declarations
class MarketDataManager;

// Base class for synthetic pricing models
class SyntheticPricer {
public:
    explicit SyntheticPricer(MarketDataManager* market_data);
    virtual ~SyntheticPricer() = default;
    
    // Calculate synthetic price
    virtual Price calculate_synthetic_price(const Symbol& underlying,
                                          InstrumentType synthetic_type,
                                          Timestamp expiry = Timestamp{}) const = 0;
    
    // Calculate basis (synthetic - spot)
    virtual double calculate_basis(const Symbol& underlying,
                                  InstrumentType synthetic_type,
                                  Exchange exchange = Exchange::BINANCE) const;
    
    // Calculate implied funding rate
    virtual double calculate_implied_funding_rate(const Symbol& underlying,
                                                 Exchange exchange) const;
    
    // Identify arbitrage opportunities
    struct SyntheticArbitrage {
        Symbol symbol;
        InstrumentType spot_type;
        InstrumentType synthetic_type;
        Exchange spot_exchange;
        Exchange synthetic_exchange;
        
        Price spot_price;
        Price synthetic_price;
        Price fair_value;
        
        double basis_bps;
        double mispricing_bps;
        double expected_profit_bps;
        
        Quantity max_size;
        double funding_impact;
        double execution_risk;
    };
    
    std::vector<SyntheticArbitrage> find_arbitrage_opportunities(
        double min_profit_bps = 5.0) const;
    
protected:
    MarketDataManager* market_data_;
    
    // Risk-free rate (annualized)
    double get_risk_free_rate() const { return 0.05; }  // 5% default
    
    // Get funding rate for perpetual
    double get_funding_rate(const Symbol& symbol, Exchange exchange) const;
    
    // Calculate time to expiry in years
    double calculate_time_to_expiry(Timestamp expiry) const;
};

// Multi-leg synthetic construction
class MultiLegSyntheticPricer : public SyntheticPricer {
public:
    using SyntheticPricer::SyntheticPricer;
    
    // Synthetic construction definition
    struct SyntheticLeg {
        Symbol symbol;
        InstrumentType type;
        Side side;
        double weight;
        Exchange preferred_exchange;
    };
    
    struct SyntheticConstruction {
        std::string name;
        std::vector<SyntheticLeg> legs;
        InstrumentType target_type;
    };
    
    // Calculate synthetic price from multiple legs
    Price calculate_multi_leg_synthetic(const SyntheticConstruction& construction) const;
    
    // Find optimal leg combination
    SyntheticConstruction find_optimal_construction(const Symbol& target,
                                                   InstrumentType target_type) const;
    
    // Standard synthetic constructions
    static SyntheticConstruction spot_from_perpetual_funding();
    static SyntheticConstruction futures_from_spot_funding();
    static SyntheticConstruction calendar_spread(const Symbol& symbol, 
                                                Timestamp near_expiry,
                                                Timestamp far_expiry);
    
    Price calculate_synthetic_price(const Symbol& underlying,
                                  InstrumentType synthetic_type,
                                  Timestamp expiry = Timestamp{}) const override;
};

// Statistical arbitrage on synthetic spreads
class StatisticalSyntheticPricer : public SyntheticPricer {
public:
    using SyntheticPricer::SyntheticPricer;
    
    // Mean reversion parameters
    struct MeanReversionParams {
        double mean_spread;
        double std_deviation;
        double half_life_hours;
        double current_z_score;
        double sharpe_ratio;
    };
    
    // Calculate mean reversion parameters for a spread
    MeanReversionParams calculate_mean_reversion(const Symbol& symbol,
                                                InstrumentType type1,
                                                InstrumentType type2,
                                                size_t lookback_hours = 24) const;
    
    // Generate statistical arbitrage signals
    struct StatArbSignal {
        Symbol symbol;
        double z_score;
        double expected_reversion_bps;
        double confidence;
        Side recommended_side;
        Quantity recommended_size;
        double expected_holding_hours;
    };
    
    std::vector<StatArbSignal> generate_signals(double z_score_threshold = 2.0) const;
    
    // Cointegration analysis
    struct CointegrationResult {
        Symbol symbol1;
        Symbol symbol2;
        double beta;  // Hedge ratio
        double correlation;
        double adf_statistic;
        bool is_cointegrated;
    };
    
    CointegrationResult test_cointegration(const Symbol& symbol1,
                                          const Symbol& symbol2,
                                          size_t lookback_hours = 168) const;
    
    Price calculate_synthetic_price(const Symbol& underlying,
                                  InstrumentType synthetic_type,
                                  Timestamp expiry = Timestamp{}) const override;
};

} // namespace arbitrage