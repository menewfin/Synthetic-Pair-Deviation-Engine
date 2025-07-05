#include "synthetic_pricer.h"
#include "market_data/market_data_manager.h"
#include "core/utils.h"

namespace arbitrage {

SyntheticPricer::SyntheticPricer(MarketDataManager* market_data)
    : market_data_(market_data) {
}

double SyntheticPricer::calculate_basis(const Symbol& underlying,
                                       InstrumentType synthetic_type,
                                       Exchange exchange) const {
    MarketDataKey spot_key{underlying, exchange, InstrumentType::SPOT};
    MarketDataKey synthetic_key{underlying, exchange, synthetic_type};
    
    MarketData spot_data, synthetic_data;
    
    if (market_data_->get_market_data(spot_key, spot_data) &&
        market_data_->get_market_data(synthetic_key, synthetic_data)) {
        
        double basis = synthetic_data.mid_price() - spot_data.mid_price();
        return (basis / spot_data.mid_price()) * 10000;  // Return in bps
    }
    
    return 0.0;
}

double SyntheticPricer::calculate_implied_funding_rate(const Symbol& underlying,
                                                      Exchange exchange) const {
    // Calculate implied funding from perpetual-spot basis
    double basis_bps = calculate_basis(underlying, InstrumentType::PERPETUAL, exchange);
    
    // Annualized funding rate = basis * 365 * 3 (3 funding periods per day)
    return basis_bps * 365 * 3 / 10000;
}

std::vector<SyntheticPricer::SyntheticArbitrage> 
SyntheticPricer::find_arbitrage_opportunities(double min_profit_bps) const {
    std::vector<SyntheticArbitrage> opportunities;
    
    // Check common symbols
    std::vector<Symbol> symbols = {"BTC-USDT", "ETH-USDT", "SOL-USDT"};
    
    for (const auto& symbol : symbols) {
        // Check spot vs perpetual arbitrage
        for (auto spot_exchange : {Exchange::OKX, Exchange::BINANCE, Exchange::BYBIT}) {
            for (auto perp_exchange : {Exchange::OKX, Exchange::BINANCE, Exchange::BYBIT}) {
                MarketDataKey spot_key{symbol, spot_exchange, InstrumentType::SPOT};
                MarketDataKey perp_key{symbol, perp_exchange, InstrumentType::PERPETUAL};
                
                MarketData spot_data, perp_data;
                
                if (market_data_->get_market_data(spot_key, spot_data) &&
                    market_data_->get_market_data(perp_key, perp_data)) {
                    
                    // Calculate synthetic spot from perpetual
                    Price synthetic_spot = calculate_synthetic_price(symbol, 
                                                                   InstrumentType::SPOT);
                    
                    // Calculate mispricing
                    double mispricing = synthetic_spot - spot_data.mid_price();
                    double mispricing_bps = (mispricing / spot_data.mid_price()) * 10000;
                    
                    if (std::abs(mispricing_bps) > min_profit_bps) {
                        SyntheticArbitrage arb;
                        arb.symbol = symbol;
                        arb.spot_type = InstrumentType::SPOT;
                        arb.synthetic_type = InstrumentType::PERPETUAL;
                        arb.spot_exchange = spot_exchange;
                        arb.synthetic_exchange = perp_exchange;
                        arb.spot_price = spot_data.mid_price();
                        arb.synthetic_price = perp_data.mid_price();
                        arb.fair_value = synthetic_spot;
                        arb.basis_bps = calculate_basis(symbol, InstrumentType::PERPETUAL, perp_exchange);
                        arb.mispricing_bps = mispricing_bps;
                        arb.expected_profit_bps = std::abs(mispricing_bps) - 10;  // Subtract fees
                        arb.max_size = std::min(spot_data.bid_size, perp_data.ask_size);
                        arb.funding_impact = get_funding_rate(symbol, perp_exchange);
                        arb.execution_risk = 0.3;  // Simplified risk score
                        
                        opportunities.push_back(arb);
                    }
                }
            }
        }
    }
    
    return opportunities;
}

double SyntheticPricer::get_funding_rate(const Symbol& symbol, Exchange exchange) const {
    MarketDataKey key{symbol, exchange, InstrumentType::PERPETUAL};
    MarketData data;
    
    if (market_data_->get_market_data(key, data)) {
        return data.funding_rate;
    }
    
    return 0.0;
}

double SyntheticPricer::calculate_time_to_expiry(Timestamp expiry) const {
    auto now = utils::get_current_timestamp();
    auto diff = expiry - now;
    auto days = std::chrono::duration_cast<std::chrono::hours>(diff).count() / 24.0;
    return days / 365.25;
}

// MultiLegSyntheticPricer implementation

Price MultiLegSyntheticPricer::calculate_synthetic_price(const Symbol& underlying,
                                                        InstrumentType synthetic_type,
                                                        Timestamp expiry) const {
    // Simple implementation - calculate synthetic spot from perpetual
    if (synthetic_type == InstrumentType::SPOT) {
        // Get perpetual price
        MarketDataManager::BestPrices perp_prices;
        if (market_data_->get_best_prices(underlying, InstrumentType::PERPETUAL, perp_prices)) {
            // Adjust for funding rate
            double funding_rate = get_funding_rate(underlying, perp_prices.best_bid_exchange);
            double adjustment = 1.0 - (funding_rate / 365.0 / 3.0);  // Daily funding / 3
            
            return perp_prices.best_bid * adjustment;
        }
    }
    
    return 0.0;
}

Price MultiLegSyntheticPricer::calculate_multi_leg_synthetic(
    const SyntheticConstruction& construction) const {
    
    Price synthetic_price = 0.0;
    
    for (const auto& leg : construction.legs) {
        MarketDataKey key{leg.symbol, leg.preferred_exchange, leg.type};
        MarketData data;
        
        if (market_data_->get_market_data(key, data)) {
            Price leg_price = (leg.side == Side::BUY) ? data.ask_price : data.bid_price;
            synthetic_price += leg_price * leg.weight;
        }
    }
    
    return synthetic_price;
}

MultiLegSyntheticPricer::SyntheticConstruction 
MultiLegSyntheticPricer::spot_from_perpetual_funding() {
    SyntheticConstruction construction;
    construction.name = "Synthetic Spot from Perpetual";
    construction.target_type = InstrumentType::SPOT;
    
    // Long perpetual + short funding = synthetic spot
    construction.legs.push_back({
        "", // Symbol to be filled
        InstrumentType::PERPETUAL,
        Side::BUY,
        1.0,
        Exchange::BINANCE
    });
    
    return construction;
}

// StatisticalSyntheticPricer implementation

Price StatisticalSyntheticPricer::calculate_synthetic_price(const Symbol& underlying,
                                                           InstrumentType synthetic_type,
                                                           Timestamp expiry) const {
    // Use statistical model to predict fair value
    // Simplified implementation
    return MultiLegSyntheticPricer(market_data_).calculate_synthetic_price(
        underlying, synthetic_type, expiry
    );
}

StatisticalSyntheticPricer::MeanReversionParams 
StatisticalSyntheticPricer::calculate_mean_reversion(const Symbol& symbol,
                                                     InstrumentType type1,
                                                     InstrumentType type2,
                                                     size_t lookback_hours) const {
    MeanReversionParams params{};
    
    // Simplified implementation
    params.mean_spread = 0.0;
    params.std_deviation = 10.0;  // 10 bps
    params.half_life_hours = 4.0;
    params.current_z_score = 0.0;
    params.sharpe_ratio = 1.5;
    
    // Calculate current spread
    MarketDataManager::BestPrices prices1, prices2;
    if (market_data_->get_best_prices(symbol, type1, prices1) &&
        market_data_->get_best_prices(symbol, type2, prices2)) {
        
        double spread = prices1.best_bid - prices2.best_ask;
        params.current_z_score = spread / params.std_deviation;
    }
    
    return params;
}

} // namespace arbitrage