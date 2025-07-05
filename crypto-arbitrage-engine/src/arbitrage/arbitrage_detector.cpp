#include "arbitrage_detector.h"
#include "market_data/market_data_manager.h"
#include "synthetic/synthetic_pricer.h"
#include "synthetic/futures_pricer.h"
#include "synthetic/perpetual_pricer.h"
#include "risk/risk_manager.h"
#include "utils/logger.h"
#include "core/utils.h"

namespace arbitrage {

ArbitrageDetector::ArbitrageDetector(MarketDataManager* market_data,
                                   RiskManager* risk_manager)
    : market_data_(market_data)
    , risk_manager_(risk_manager)
    , min_profit_threshold_(constants::MIN_PROFIT_THRESHOLD_DEFAULT)
    , max_position_size_(constants::MAX_POSITION_SIZE_USD)
    , opportunity_ttl_ms_(500) {
    
    // Initialize synthetic pricers
    multi_leg_pricer_ = std::make_unique<MultiLegSyntheticPricer>(market_data_);
    stat_arb_pricer_ = std::make_unique<StatisticalSyntheticPricer>(market_data_);
    futures_pricer_ = std::make_unique<FuturesPricer>(market_data_);
    perpetual_pricer_ = std::make_unique<PerpetualPricer>(market_data_);
}

ArbitrageDetector::~ArbitrageDetector() {
    stop();
}

void ArbitrageDetector::start() {
    if (running_) return;
    
    running_ = true;
    detection_thread_ = std::make_unique<std::thread>([this]() {
        detection_loop();
    });
    
    LOG_INFO("ArbitrageDetector started");
}

void ArbitrageDetector::stop() {
    if (!running_) return;
    
    running_ = false;
    
    if (detection_thread_ && detection_thread_->joinable()) {
        detection_thread_->join();
    }
    
    LOG_INFO("ArbitrageDetector stopped");
}

void ArbitrageDetector::register_opportunity_callback(OpportunityCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    callbacks_.push_back(std::move(callback));
}

std::vector<ArbitrageOpportunity> ArbitrageDetector::get_current_opportunities() const {
    std::lock_guard<std::mutex> lock(opportunities_mutex_);
    return current_opportunities_;
}

void ArbitrageDetector::detection_loop() {
    while (running_) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Run different detection algorithms
        detect_spot_arbitrage();
        detect_synthetic_arbitrage();
        detect_funding_arbitrage();
        
        // Cleanup expired opportunities
        cleanup_expired_opportunities();
        
        // Sleep to maintain detection frequency
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        auto sleep_time = std::chrono::milliseconds(100) - elapsed;
        if (sleep_time.count() > 0) {
            std::this_thread::sleep_for(sleep_time);
        }
    }
}

void ArbitrageDetector::detect_spot_arbitrage() {
    std::vector<Symbol> symbols = {"BTC-USDT", "ETH-USDT", "SOL-USDT"};
    
    for (const auto& symbol : symbols) {
        MarketDataManager::BestPrices best_prices;
        
        if (!market_data_->get_best_prices(symbol, InstrumentType::SPOT, best_prices)) {
            continue;
        }
        
        // Check if there's an arbitrage opportunity
        if (best_prices.best_bid_exchange != best_prices.best_ask_exchange) {
            double spread = best_prices.best_bid - best_prices.best_ask;
            double spread_bps = (spread / best_prices.best_ask) * 10000;
            
            // Account for fees (simplified)
            double net_profit_bps = spread_bps - constants::TAKER_FEE_BPS * 2;
            
            if (net_profit_bps > min_profit_threshold_) {
                // Get full market data for both exchanges
                MarketDataKey buy_key{symbol, best_prices.best_ask_exchange, InstrumentType::SPOT};
                MarketDataKey sell_key{symbol, best_prices.best_bid_exchange, InstrumentType::SPOT};
                
                MarketData buy_data, sell_data;
                if (market_data_->get_market_data(buy_key, buy_data) &&
                    market_data_->get_market_data(sell_key, sell_data)) {
                    
                    auto opportunity = create_spot_opportunity(
                        symbol,
                        best_prices.best_ask_exchange,
                        best_prices.best_bid_exchange,
                        buy_data,
                        sell_data
                    );
                    
                    {
                        std::lock_guard<std::mutex> lock(opportunities_mutex_);
                        current_opportunities_.push_back(opportunity);
                    }
                    
                    notify_callbacks(opportunity);
                    total_opportunities_++;
                }
            }
        }
    }
}

void ArbitrageDetector::detect_synthetic_arbitrage() {
    // Get synthetic arbitrage opportunities from pricers
    auto synthetic_arbs = multi_leg_pricer_->find_arbitrage_opportunities(min_profit_threshold_);
    
    for (const auto& arb : synthetic_arbs) {
        ArbitrageOpportunity opportunity;
        opportunity.id = utils::generate_opportunity_id("SYNTHETIC", utils::get_current_timestamp());
        opportunity.timestamp = utils::get_current_timestamp();
        
        // Create legs
        opportunity.legs.push_back({
            arb.symbol,
            arb.spot_exchange,
            Side::BUY,
            arb.spot_price,
            arb.max_size,
            arb.spot_type,
            false
        });
        
        opportunity.legs.push_back({
            arb.symbol,
            arb.synthetic_exchange,
            Side::SELL,
            arb.synthetic_price,
            arb.max_size,
            arb.synthetic_type,
            true
        });
        
        opportunity.expected_profit = arb.expected_profit_bps / 10000 * arb.spot_price * arb.max_size;
        opportunity.profit_percentage = arb.expected_profit_bps / 100;
        opportunity.required_capital = arb.spot_price * arb.max_size;
        opportunity.execution_risk = arb.execution_risk;
        opportunity.funding_risk = arb.funding_impact;
        opportunity.liquidity_score = 0.8;
        opportunity.ttl_ms = opportunity_ttl_ms_;
        opportunity.is_executable = true;
        
        {
            std::lock_guard<std::mutex> lock(opportunities_mutex_);
            current_opportunities_.push_back(opportunity);
        }
        
        notify_callbacks(opportunity);
        total_opportunities_++;
    }
}

void ArbitrageDetector::detect_funding_arbitrage() {
    auto perp_pricer = static_cast<PerpetualPricer*>(perpetual_pricer_.get());
    auto funding_arbs = perp_pricer->find_funding_arbitrage(min_profit_threshold_);
    
    for (const auto& arb : funding_arbs) {
        ArbitrageOpportunity opportunity;
        opportunity.id = utils::generate_opportunity_id("FUNDING", utils::get_current_timestamp());
        opportunity.timestamp = utils::get_current_timestamp();
        
        // Create legs for funding arbitrage
        opportunity.legs.push_back({
            arb.symbol,
            arb.long_exchange,
            Side::BUY,
            0.0,  // Price will be filled from market data
            1.0,  // Normalized size
            InstrumentType::PERPETUAL,
            false
        });
        
        opportunity.legs.push_back({
            arb.symbol,
            arb.short_exchange,
            Side::SELL,
            0.0,  // Price will be filled from market data
            1.0,  // Normalized size
            InstrumentType::PERPETUAL,
            false
        });
        
        opportunity.expected_profit = arb.funding_spread * arb.required_capital;
        opportunity.profit_percentage = arb.annualized_return;
        opportunity.required_capital = arb.required_capital;
        opportunity.funding_risk = arb.funding_spread;
        opportunity.ttl_ms = 28800000;  // 8 hours for funding period
        opportunity.is_executable = true;
        
        {
            std::lock_guard<std::mutex> lock(opportunities_mutex_);
            current_opportunities_.push_back(opportunity);
        }
        
        notify_callbacks(opportunity);
        total_opportunities_++;
    }
}

ArbitrageOpportunity ArbitrageDetector::create_spot_opportunity(
    const Symbol& symbol,
    Exchange buy_exchange,
    Exchange sell_exchange,
    const MarketData& buy_data,
    const MarketData& sell_data) {
    
    ArbitrageOpportunity opportunity;
    opportunity.id = utils::generate_opportunity_id("SPOT", utils::get_current_timestamp());
    opportunity.timestamp = utils::get_current_timestamp();
    
    // Calculate opportunity details
    double max_quantity = std::min(buy_data.ask_size, sell_data.bid_size);
    double buy_price = buy_data.ask_price;
    double sell_price = sell_data.bid_price;
    
    // Create legs
    opportunity.legs.push_back({
        symbol,
        buy_exchange,
        Side::BUY,
        buy_price,
        max_quantity,
        InstrumentType::SPOT,
        false
    });
    
    opportunity.legs.push_back({
        symbol,
        sell_exchange,
        Side::SELL,
        sell_price,
        max_quantity,
        InstrumentType::SPOT,
        false
    });
    
    // Calculate profitability
    double gross_profit = (sell_price - buy_price) * max_quantity;
    double fees = (buy_price + sell_price) * max_quantity * constants::TAKER_FEE_BPS / 10000;
    
    opportunity.expected_profit = gross_profit - fees;
    opportunity.profit_percentage = (opportunity.expected_profit / (buy_price * max_quantity)) * 100;
    opportunity.required_capital = buy_price * max_quantity;
    
    // Risk metrics
    opportunity.execution_risk = calculate_execution_risk(opportunity);
    opportunity.funding_risk = 0.0;  // No funding for spot
    opportunity.liquidity_score = 0.9;  // High for spot
    
    opportunity.ttl_ms = opportunity_ttl_ms_;
    opportunity.is_executable = opportunity.expected_profit > 0 && 
                               opportunity.required_capital <= max_position_size_;
    
    return opportunity;
}

void ArbitrageDetector::notify_callbacks(const ArbitrageOpportunity& opportunity) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    
    for (const auto& callback : callbacks_) {
        try {
            callback(opportunity);
        } catch (const std::exception& e) {
            LOG_ERROR("Callback error: {}", e.what());
        }
    }
}

void ArbitrageDetector::cleanup_expired_opportunities() {
    std::lock_guard<std::mutex> lock(opportunities_mutex_);
    
    auto now = utils::get_current_timestamp();
    
    auto new_end = std::remove_if(current_opportunities_.begin(), 
                                 current_opportunities_.end(),
                                 [now, this](const ArbitrageOpportunity& opp) {
        auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - opp.timestamp
        ).count();
        
        if (age_ms > opp.ttl_ms) {
            expired_opportunities_++;
            return true;
        }
        return false;
    });
    
    current_opportunities_.erase(new_end, current_opportunities_.end());
}

double ArbitrageDetector::calculate_execution_risk(const ArbitrageOpportunity& opportunity) const {
    // Simplified risk calculation
    double risk = 0.0;
    
    // Add risk for cross-exchange execution
    bool cross_exchange = false;
    Exchange first_exchange = opportunity.legs[0].exchange;
    for (const auto& leg : opportunity.legs) {
        if (leg.exchange != first_exchange) {
            cross_exchange = true;
            break;
        }
    }
    
    if (cross_exchange) {
        risk += 0.3;
    }
    
    // Add risk for synthetic instruments
    for (const auto& leg : opportunity.legs) {
        if (leg.is_synthetic) {
            risk += 0.2;
        }
    }
    
    return std::min(risk, 1.0);
}

ArbitrageDetector::Statistics ArbitrageDetector::get_statistics() const {
    Statistics stats{};
    stats.opportunities_detected = total_opportunities_.load();
    stats.opportunities_expired = expired_opportunities_.load();
    
    // Calculate average profit and other metrics
    std::lock_guard<std::mutex> lock(opportunities_mutex_);
    
    if (!current_opportunities_.empty()) {
        double total_profit_bps = 0.0;
        for (const auto& opp : current_opportunities_) {
            total_profit_bps += opp.profit_percentage * 100;
            stats.total_profit_potential += opp.expected_profit;
        }
        stats.avg_profit_bps = total_profit_bps / current_opportunities_.size();
    }
    
    return stats;
}

} // namespace arbitrage