#include "perpetual_pricer.h"
#include "market_data/market_data_manager.h"

namespace arbitrage {

double PerpetualPricer::calculate_fair_funding_rate(const Symbol& underlying,
                                                   Exchange exchange) const {
    // Get spot and perpetual prices
    MarketDataKey spot_key{underlying, exchange, InstrumentType::SPOT};
    MarketDataKey perp_key{underlying, exchange, InstrumentType::PERPETUAL};
    
    MarketData spot_data, perp_data;
    
    if (market_data_->get_market_data(spot_key, spot_data) &&
        market_data_->get_market_data(perp_key, perp_data)) {
        
        // Calculate basis
        double basis = (perp_data.mid_price() - spot_data.mid_price()) / spot_data.mid_price();
        
        // Fair funding rate = basis * 3 (for 8-hour funding periods)
        return basis * 3;
    }
    
    return 0.0;
}

std::vector<PerpetualPricer::FundingArbitrage> 
PerpetualPricer::find_funding_arbitrage(double min_spread_bps) const {
    std::vector<FundingArbitrage> opportunities;
    
    std::vector<Symbol> symbols = {"BTC-USDT", "ETH-USDT", "SOL-USDT"};
    
    for (const auto& symbol : symbols) {
        // Compare funding rates across exchanges
        std::vector<std::pair<Exchange, double>> funding_rates;
        
        for (auto exchange : {Exchange::OKX, Exchange::BINANCE, Exchange::BYBIT}) {
            double rate = get_funding_rate(symbol, exchange);
            funding_rates.push_back({exchange, rate});
        }
        
        // Find max and min funding rates
        auto max_it = std::max_element(funding_rates.begin(), funding_rates.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        auto min_it = std::min_element(funding_rates.begin(), funding_rates.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        
        double spread_bps = (max_it->second - min_it->second) * 10000;
        
        if (spread_bps > min_spread_bps) {
            FundingArbitrage arb;
            arb.symbol = symbol;
            arb.long_exchange = min_it->first;   // Long where funding is lowest
            arb.short_exchange = max_it->first;  // Short where funding is highest
            arb.long_funding_rate = min_it->second;
            arb.short_funding_rate = max_it->second;
            arb.funding_spread = max_it->second - min_it->second;
            arb.annualized_return = arb.funding_spread * 365 * 3;  // 3 periods per day
            
            // Get required capital based on position sizes
            MarketDataKey key{symbol, arb.long_exchange, InstrumentType::PERPETUAL};
            MarketData data;
            if (market_data_->get_market_data(key, data)) {
                arb.required_capital = data.mid_price() * 2;  // Need capital for both legs
            }
            
            opportunities.push_back(arb);
        }
    }
    
    return opportunities;
}

Price PerpetualPricer::calculate_synthetic_spot(const Symbol& underlying,
                                               Exchange exchange,
                                               double holding_period_hours) const {
    // Get perpetual price
    MarketDataKey perp_key{underlying, exchange, InstrumentType::PERPETUAL};
    MarketData perp_data;
    
    if (!market_data_->get_market_data(perp_key, perp_data)) {
        return 0.0;
    }
    
    // Get funding rate
    double funding_rate = get_funding_rate(underlying, exchange);
    
    // Adjust for funding over holding period
    double periods = holding_period_hours / 8.0;  // 8-hour funding periods
    double adjustment = 1.0 - (funding_rate * periods);
    
    return perp_data.mid_price() * adjustment;
}

Price PerpetualPricer::calculate_synthetic_price(const Symbol& underlying,
                                                InstrumentType synthetic_type,
                                                Timestamp expiry) const {
    if (synthetic_type == InstrumentType::SPOT) {
        // Find best synthetic spot across exchanges
        Price best_price = 0.0;
        
        for (auto exchange : {Exchange::OKX, Exchange::BINANCE, Exchange::BYBIT}) {
            Price synthetic = calculate_synthetic_spot(underlying, exchange);
            if (synthetic > best_price) {
                best_price = synthetic;
            }
        }
        
        return best_price;
    }
    
    return 0.0;
}

} // namespace arbitrage