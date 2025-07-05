#include "market_data_manager.h"
#include "exchange/exchange_base.h"
#include "utils/logger.h"
#include <algorithm>

namespace arbitrage {

MarketDataManager::MarketDataManager() {
}

MarketDataManager::~MarketDataManager() {
    stop();
}

void MarketDataManager::add_exchange(std::unique_ptr<ExchangeBase> exchange) {
    // Set up callbacks
    exchange->set_market_data_callback(
        [this](const MarketData& data) { handle_market_data(data); }
    );
    
    exchange->set_orderbook_callback(
        [this, ex = exchange->get_exchange()](const Symbol& symbol, 
                                             const std::vector<PriceLevel>& bids,
                                             const std::vector<PriceLevel>& asks) {
            handle_orderbook_update(symbol, ex, InstrumentType::SPOT, bids, asks);
        }
    );
    
    exchanges_.push_back(std::move(exchange));
}

void MarketDataManager::start() {
    if (running_) return;
    
    running_ = true;
    
    // Connect all exchanges
    for (auto& exchange : exchanges_) {
        exchange->connect();
    }
    
    // Start statistics thread
    stats_thread_ = std::make_unique<std::thread>([this]() {
        update_statistics();
    });
    
    LOG_INFO("MarketDataManager started with {} exchanges", exchanges_.size());
}

void MarketDataManager::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Disconnect all exchanges
    for (auto& exchange : exchanges_) {
        exchange->disconnect();
    }
    
    // Stop statistics thread
    if (stats_thread_ && stats_thread_->joinable()) {
        stats_thread_->join();
    }
    
    LOG_INFO("MarketDataManager stopped");
}

void MarketDataManager::subscribe_symbol(const Symbol& symbol, Exchange exchange, InstrumentType type) {
    for (auto& ex : exchanges_) {
        if (ex->get_exchange() == exchange) {
            ex->subscribe_orderbook(symbol, type);
            ex->subscribe_ticker(symbol, type);
            ex->subscribe_trades(symbol, type);
            
            if (type == InstrumentType::PERPETUAL) {
                ex->subscribe_funding_rate(symbol);
            }
            break;
        }
    }
}

void MarketDataManager::subscribe_all_exchanges(const Symbol& symbol, InstrumentType type) {
    for (auto& exchange : exchanges_) {
        exchange->subscribe_orderbook(symbol, type);
        exchange->subscribe_ticker(symbol, type);
        exchange->subscribe_trades(symbol, type);
        
        if (type == InstrumentType::PERPETUAL) {
            exchange->subscribe_funding_rate(symbol);
        }
    }
}

bool MarketDataManager::get_market_data(const MarketDataKey& key, MarketData& data) const {
    MarketDataMap::const_accessor accessor;
    if (market_data_.find(accessor, key)) {
        data = accessor->second;
        return true;
    }
    return false;
}

std::vector<MarketData> MarketDataManager::get_all_market_data(const Symbol& symbol) const {
    std::vector<MarketData> result;
    
    for (auto it = market_data_.begin(); it != market_data_.end(); ++it) {
        if (it->first.symbol == symbol) {
            result.push_back(it->second);
        }
    }
    
    return result;
}

std::shared_ptr<OrderBook> MarketDataManager::get_order_book(const MarketDataKey& key) const {
    OrderBookMap::const_accessor accessor;
    if (order_books_.find(accessor, key)) {
        // Convert LockFreeOrderBook to regular OrderBook for external use
        auto book = std::make_shared<OrderBook>();
        // Copy data from lock-free book to regular book
        return book;
    }
    return nullptr;
}

bool MarketDataManager::get_best_prices(const Symbol& symbol, InstrumentType type, BestPrices& prices) const {
    prices.best_bid = 0;
    prices.best_ask = std::numeric_limits<double>::max();
    bool found = false;
    
    for (auto& exchange : {Exchange::OKX, Exchange::BINANCE, Exchange::BYBIT}) {
        MarketDataKey key{symbol, exchange, type};
        MarketData data;
        
        if (get_market_data(key, data)) {
            if (data.bid_price > prices.best_bid) {
                prices.best_bid = data.bid_price;
                prices.best_bid_exchange = exchange;
                prices.best_bid_size = data.bid_size;
            }
            
            if (data.ask_price < prices.best_ask) {
                prices.best_ask = data.ask_price;
                prices.best_ask_exchange = exchange;
                prices.best_ask_size = data.ask_size;
            }
            
            found = true;
        }
    }
    
    return found;
}

void MarketDataManager::register_market_data_callback(MarketDataCallback callback) {
    std::unique_lock<std::shared_mutex> lock(callbacks_mutex_);
    market_data_callbacks_.push_back(std::move(callback));
}

void MarketDataManager::register_orderbook_callback(OrderBookCallback callback) {
    std::unique_lock<std::shared_mutex> lock(callbacks_mutex_);
    orderbook_callbacks_.push_back(std::move(callback));
}

void MarketDataManager::handle_market_data(const MarketData& data) {
    total_updates_++;
    
    // Update market data
    MarketDataKey key{data.symbol, data.exchange, data.type};
    {
        MarketDataMap::accessor accessor;
        market_data_.insert(accessor, key);
        accessor->second = data;
    }
    
    // Notify callbacks
    {
        std::shared_lock<std::shared_mutex> lock(callbacks_mutex_);
        for (const auto& callback : market_data_callbacks_) {
            callback(data);
        }
    }
}

void MarketDataManager::handle_orderbook_update(const Symbol& symbol, Exchange exchange, 
                                               InstrumentType type,
                                               const std::vector<PriceLevel>& bids,
                                               const std::vector<PriceLevel>& asks) {
    MarketDataKey key{symbol, exchange, type};
    
    // Update lock-free order book
    {
        OrderBookMap::accessor accessor;
        if (!order_books_.find(accessor, key)) {
            order_books_.insert(accessor, key);
            accessor->second = std::make_shared<LockFreeOrderBook<>>();
        }
        
        accessor->second->update_bids(bids.data(), bids.size());
        accessor->second->update_asks(asks.data(), asks.size());
    }
    
    // Create snapshot for callbacks
    OrderBook::Snapshot snapshot;
    snapshot.bids = bids;
    snapshot.asks = asks;
    snapshot.timestamp = utils::get_current_timestamp();
    
    // Notify callbacks
    {
        std::shared_lock<std::shared_mutex> lock(callbacks_mutex_);
        for (const auto& callback : orderbook_callbacks_) {
            callback(key, snapshot);
        }
    }
}

void MarketDataManager::update_statistics() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Update statistics
    }
}

MarketDataManager::Statistics MarketDataManager::get_statistics() const {
    Statistics stats{};
    stats.total_updates = total_updates_.load();
    
    // Calculate updates by exchange and symbol
    for (const auto& exchange : exchanges_) {
        stats.updates_by_exchange[exchange->get_exchange()] = 
            exchange->get_messages_processed();
    }
    
    return stats;
}

} // namespace arbitrage