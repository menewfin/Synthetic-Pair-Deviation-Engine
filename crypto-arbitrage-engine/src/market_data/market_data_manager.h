#pragma once

#include "core/types.h"
#include "order_book.h"
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <tbb/concurrent_hash_map.h>

namespace arbitrage {

// Forward declarations
class ExchangeBase;

// Market data key combining symbol and exchange
struct MarketDataKey {
    Symbol symbol;
    Exchange exchange;
    InstrumentType type;
    
    bool operator==(const MarketDataKey& other) const {
        return symbol == other.symbol && 
               exchange == other.exchange && 
               type == other.type;
    }
};

// Hash function for MarketDataKey
struct MarketDataKeyHash {
    std::size_t operator()(const MarketDataKey& key) const {
        std::size_t h1 = std::hash<std::string>{}(key.symbol);
        std::size_t h2 = std::hash<int>{}(static_cast<int>(key.exchange));
        std::size_t h3 = std::hash<int>{}(static_cast<int>(key.type));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class MarketDataManager {
public:
    MarketDataManager();
    ~MarketDataManager();
    
    // Initialize with exchanges
    void add_exchange(std::unique_ptr<ExchangeBase> exchange);
    
    // Start/stop data collection
    void start();
    void stop();
    
    // Subscribe to market data
    void subscribe_symbol(const Symbol& symbol, Exchange exchange, InstrumentType type);
    void subscribe_all_exchanges(const Symbol& symbol, InstrumentType type);
    
    // Get market data
    bool get_market_data(const MarketDataKey& key, MarketData& data) const;
    std::vector<MarketData> get_all_market_data(const Symbol& symbol) const;
    
    // Get order book
    std::shared_ptr<OrderBook> get_order_book(const MarketDataKey& key) const;
    
    // Get best prices across exchanges
    struct BestPrices {
        Price best_bid;
        Price best_ask;
        Exchange best_bid_exchange;
        Exchange best_ask_exchange;
        Quantity best_bid_size;
        Quantity best_ask_size;
    };
    
    bool get_best_prices(const Symbol& symbol, InstrumentType type, BestPrices& prices) const;
    
    // Calculate synthetic prices
    Price calculate_synthetic_price(const SyntheticInstrument& synthetic) const;
    
    // Market data callbacks
    using MarketDataCallback = std::function<void(const MarketData&)>;
    using OrderBookCallback = std::function<void(const MarketDataKey&, const OrderBook::Snapshot&)>;
    
    void register_market_data_callback(MarketDataCallback callback);
    void register_orderbook_callback(OrderBookCallback callback);
    
    // Statistics
    struct Statistics {
        uint64_t total_updates;
        uint64_t updates_per_second;
        std::unordered_map<Exchange, uint64_t> updates_by_exchange;
        std::unordered_map<Symbol, uint64_t> updates_by_symbol;
    };
    
    Statistics get_statistics() const;
    
private:
    // Exchange connections
    std::vector<std::unique_ptr<ExchangeBase>> exchanges_;
    
    // Market data storage - using TBB concurrent hash map for lock-free access
    using MarketDataMap = tbb::concurrent_hash_map<MarketDataKey, MarketData, MarketDataKeyHash>;
    using OrderBookMap = tbb::concurrent_hash_map<MarketDataKey, std::shared_ptr<LockFreeOrderBook<>>, MarketDataKeyHash>;
    
    MarketDataMap market_data_;
    OrderBookMap order_books_;
    
    // Callbacks
    std::vector<MarketDataCallback> market_data_callbacks_;
    std::vector<OrderBookCallback> orderbook_callbacks_;
    mutable std::shared_mutex callbacks_mutex_;
    
    // Statistics
    std::atomic<uint64_t> total_updates_{0};
    std::atomic<bool> running_{false};
    
    // Handlers for exchange callbacks
    void handle_market_data(const MarketData& data);
    void handle_orderbook_update(const Symbol& symbol, Exchange exchange, InstrumentType type,
                                const std::vector<PriceLevel>& bids,
                                const std::vector<PriceLevel>& asks);
    
    // Update thread for statistics
    std::unique_ptr<std::thread> stats_thread_;
    void update_statistics();
};

// Aggregated market view across all exchanges
class AggregatedMarketView {
public:
    struct AggregatedData {
        Symbol symbol;
        InstrumentType type;
        
        // Best bid/ask across all exchanges
        Price best_bid;
        Price best_ask;
        Exchange best_bid_exchange;
        Exchange best_ask_exchange;
        
        // Volume-weighted average prices
        Price vwap_bid;
        Price vwap_ask;
        
        // Total volumes
        Quantity total_bid_volume;
        Quantity total_ask_volume;
        
        // Spread metrics
        double min_spread;
        double avg_spread;
        Exchange tightest_spread_exchange;
        
        // Liquidity metrics
        double total_liquidity;  // Sum of bid and ask volumes in USD
        double imbalance;        // (bid_vol - ask_vol) / total_vol
        
        Timestamp last_update;
    };
    
    explicit AggregatedMarketView(MarketDataManager* manager);
    
    // Get aggregated view for a symbol
    bool get_aggregated_data(const Symbol& symbol, InstrumentType type, 
                            AggregatedData& data) const;
    
    // Find arbitrage opportunities
    struct ArbitrageSignal {
        Symbol symbol;
        InstrumentType type;
        Exchange buy_exchange;
        Exchange sell_exchange;
        Price buy_price;
        Price sell_price;
        Quantity max_quantity;
        double profit_bps;
        double profit_usd;
    };
    
    std::vector<ArbitrageSignal> find_arbitrage_opportunities(
        double min_profit_bps = 10.0) const;
    
private:
    MarketDataManager* manager_;
    
    // Cache for aggregated data
    mutable tbb::concurrent_hash_map<std::pair<Symbol, InstrumentType>, AggregatedData> cache_;
    
    void update_aggregated_data(const Symbol& symbol, InstrumentType type) const;
};

} // namespace arbitrage