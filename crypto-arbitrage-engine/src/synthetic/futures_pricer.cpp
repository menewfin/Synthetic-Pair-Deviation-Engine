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
    
    // Common cryptocurrency futures symbols with expiry dates
    std::vector<Symbol> symbols = {"BTC-USDT", "ETH-USDT", "SOL-USDT"};
    
    // Typical quarterly expiry timestamps (simplified for demonstration)
    auto now = std::chrono::system_clock::now();
    std::vector<Timestamp> expiries = {
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            (now + std::chrono::hours(24 * 30)).time_since_epoch()),  // 1 month
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            (now + std::chrono::hours(24 * 90)).time_since_epoch()),  // 3 months
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            (now + std::chrono::hours(24 * 180)).time_since_epoch())  // 6 months
    };
    
    for (const auto& symbol : symbols) {
        // Get spot price for theoretical calculation
        MarketDataManager::BestPrices spot_prices;
        if (!market_data_->get_best_prices(symbol, InstrumentType::SPOT, spot_prices)) {
            continue;
        }
        
        double spot_price = (spot_prices.best_bid + spot_prices.best_ask) / 2.0;
        
        // Check all calendar spread combinations
        for (size_t i = 0; i < expiries.size() - 1; ++i) {
            for (size_t j = i + 1; j < expiries.size(); ++j) {
                Timestamp near_expiry = expiries[i];
                Timestamp far_expiry = expiries[j];
                
                // Calculate theoretical prices
                double near_theoretical = calculate_futures_fair_value(symbol, near_expiry);
                double far_theoretical = calculate_futures_fair_value(symbol, far_expiry);
                
                if (near_theoretical <= 0 || far_theoretical <= 0) continue;
                
                // Calculate theoretical spread
                double theoretical_spread = far_theoretical - near_theoretical;
                
                // Try to get actual market prices (simplified - in practice would check multiple exchanges)
                MarketDataKey near_key{symbol, Exchange::BINANCE, InstrumentType::FUTURES};
                MarketDataKey far_key{symbol, Exchange::BINANCE, InstrumentType::FUTURES};
                
                MarketData near_data, far_data;
                bool has_near = market_data_->get_market_data(near_key, near_data);
                bool has_far = market_data_->get_market_data(far_key, far_data);
                
                if (has_near && has_far) {
                    double near_market = near_data.mid_price();
                    double far_market = far_data.mid_price();
                    double market_spread = far_market - near_market;
                    
                    // Calculate mispricing
                    double spread_diff = market_spread - theoretical_spread;
                    double mispricing_bps = (spread_diff / spot_price) * 10000;
                    
                    if (std::abs(mispricing_bps) > min_profit_bps) {
                        CalendarSpread spread;
                        spread.symbol = symbol;
                        spread.near_expiry = near_expiry;
                        spread.far_expiry = far_expiry;
                        spread.near_price = near_market;
                        spread.far_price = far_market;
                        spread.spread = market_spread;
                        spread.theoretical_spread = theoretical_spread;
                        spread.mispricing_bps = mispricing_bps;
                        
                        spreads.push_back(spread);
                    }
                }
                else {
                    // If no actual futures data available, create synthetic opportunity
                    // based on theoretical mispricing with perpetuals
                    MarketDataKey perp_key{symbol, Exchange::BINANCE, InstrumentType::PERPETUAL};
                    MarketData perp_data;
                    
                    if (market_data_->get_market_data(perp_key, perp_data)) {
                        // Use perpetual as proxy for far futures
                        double perp_price = perp_data.mid_price();
                        double near_synthetic = calculate_futures_fair_value(symbol, near_expiry);
                        
                        if (near_synthetic > 0) {
                            double synthetic_spread = perp_price - near_synthetic;
                            double spread_diff = synthetic_spread - theoretical_spread;
                            double mispricing_bps = (spread_diff / spot_price) * 10000;
                            
                            if (std::abs(mispricing_bps) > min_profit_bps) {
                                CalendarSpread spread;
                                spread.symbol = symbol;
                                spread.near_expiry = near_expiry;
                                spread.far_expiry = far_expiry;
                                spread.near_price = near_synthetic;
                                spread.far_price = perp_price;
                                spread.spread = synthetic_spread;
                                spread.theoretical_spread = theoretical_spread;
                                spread.mispricing_bps = mispricing_bps;
                                
                                spreads.push_back(spread);
                            }
                        }
                    }
                }
            }
        }
    }
    
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