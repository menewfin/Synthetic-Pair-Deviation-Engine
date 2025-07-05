#pragma once

#include "synthetic_pricer.h"

namespace arbitrage {

class FuturesPricer : public SyntheticPricer {
public:
    using SyntheticPricer::SyntheticPricer;
    
    // Calculate theoretical futures price using cost of carry
    Price calculate_futures_fair_value(const Symbol& underlying,
                                      Timestamp expiry,
                                      double interest_rate = 0.05,
                                      double storage_cost = 0.0) const;
    
    // Calculate implied interest rate from futures basis
    double calculate_implied_rate(const Symbol& underlying,
                                 Price futures_price,
                                 Price spot_price,
                                 Timestamp expiry) const;
    
    // Calendar spread arbitrage
    struct CalendarSpread {
        Symbol symbol;
        Timestamp near_expiry;
        Timestamp far_expiry;
        Price near_price;
        Price far_price;
        double spread;
        double theoretical_spread;
        double mispricing_bps;
    };
    
    std::vector<CalendarSpread> find_calendar_spreads(double min_profit_bps = 5.0) const;
    
    Price calculate_synthetic_price(const Symbol& underlying,
                                  InstrumentType synthetic_type,
                                  Timestamp expiry = Timestamp{}) const override;
};

} // namespace arbitrage