#pragma once

#include "synthetic_pricer.h"

namespace arbitrage {

class PerpetualPricer : public SyntheticPricer {
public:
    using SyntheticPricer::SyntheticPricer;
    
    // Calculate fair funding rate based on spot-perp basis
    double calculate_fair_funding_rate(const Symbol& underlying,
                                      Exchange exchange) const;
    
    // Funding arbitrage opportunities
    struct FundingArbitrage {
        Symbol symbol;
        Exchange long_exchange;   // Exchange to long perpetual
        Exchange short_exchange;  // Exchange to short perpetual
        double long_funding_rate;
        double short_funding_rate;
        double funding_spread;
        double annualized_return;
        double required_capital;
    };
    
    std::vector<FundingArbitrage> find_funding_arbitrage(
        double min_spread_bps = 10.0) const;
    
    // Calculate synthetic spot using perpetual and funding
    Price calculate_synthetic_spot(const Symbol& underlying,
                                  Exchange exchange,
                                  double holding_period_hours = 8.0) const;
    
    Price calculate_synthetic_price(const Symbol& underlying,
                                  InstrumentType synthetic_type,
                                  Timestamp expiry = Timestamp{}) const override;
};

} // namespace arbitrage