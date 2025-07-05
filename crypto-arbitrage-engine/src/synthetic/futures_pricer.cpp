#include "futures_pricer.h"
#include "market_data/market_data_manager.h"
#include <cmath>

namespace arbitrage {

Price FuturesPricer::calculate_futures_fair_value(const Symbol& underlying,
                                                 Timestamp expiry,
                                                 double interest_rate,
                                                 double storage_cost) const {
    // Get spot price
    MarketDataManager::BestPrices spot_prices;
    if (!market_data_->get_best_prices(underlying, InstrumentType::SPOT, spot_prices)) {
        return 0.0;
    }
    
    // Calculate time to expiry
    double time_to_expiry = calculate_time_to_expiry(expiry);
    
    // F = S * e^((r + c) * T)
    // where r = risk-free rate, c = storage cost, T = time to expiry
    return spot_prices.best_bid * std::exp((interest_rate + storage_cost) * time_to_expiry);
}

double FuturesPricer::calculate_implied_rate(const Symbol& underlying,
                                            Price futures_price,
                                            Price spot_price,
                                            Timestamp expiry) const {
    if (spot_price <= 0) return 0.0;
    
    double time_to_expiry = calculate_time_to_expiry(expiry);
    if (time_to_expiry <= 0) return 0.0;
    
    // r = ln(F/S) / T
    return std::log(futures_price / spot_price) / time_to_expiry;
}

std::vector<FuturesPricer::CalendarSpread> 
FuturesPricer::find_calendar_spreads(double min_profit_bps) const {
    std::vector<CalendarSpread> spreads;
    
    // Simplified implementation - would need actual futures data
    // This is a placeholder for the calendar spread detection logic
    
    return spreads;
}

Price FuturesPricer::calculate_synthetic_price(const Symbol& underlying,
                                              InstrumentType synthetic_type,
                                              Timestamp expiry) const {
    if (synthetic_type == InstrumentType::FUTURES) {
        return calculate_futures_fair_value(underlying, expiry);
    }
    
    // Calculate synthetic spot from futures
    if (synthetic_type == InstrumentType::SPOT) {
        MarketDataManager::BestPrices futures_prices;
        if (market_data_->get_best_prices(underlying, InstrumentType::FUTURES, futures_prices)) {
            double time_to_expiry = calculate_time_to_expiry(expiry);
            double interest_rate = get_risk_free_rate();
            
            // S = F * e^(-r * T)
            return futures_prices.best_bid * std::exp(-interest_rate * time_to_expiry);
        }
    }
    
    return 0.0;
}

} // namespace arbitrage