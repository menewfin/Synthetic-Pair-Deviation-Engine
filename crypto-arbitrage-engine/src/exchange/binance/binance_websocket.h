#pragma once

#include "exchange/exchange_base.h"
#include <rapidjson/document.h>
#include <unordered_map>
#include <unordered_set>

namespace arbitrage {

class BinanceWebSocket : public ExchangeBase {
public:
    explicit BinanceWebSocket(const ExchangeConfig& config);
    ~BinanceWebSocket() override = default;
    
    // Connection management
    void connect() override;
    void disconnect() override;
    
    // Subscribe to market data
    void subscribe_orderbook(const Symbol& symbol, InstrumentType type) override;
    void subscribe_trades(const Symbol& symbol, InstrumentType type) override;
    void subscribe_ticker(const Symbol& symbol, InstrumentType type) override;
    void subscribe_funding_rate(const Symbol& symbol) override;
    
    // Unsubscribe from market data
    void unsubscribe_orderbook(const Symbol& symbol, InstrumentType type) override;
    void unsubscribe_all() override;
    
protected:
    // WebSocket message handler
    void on_message(WsConnection hdl, WsMessage msg) override;
    
    // Parse message
    void parse_message(const std::string& message) override;
    
private:
    // Message parsers
    void parse_depth_update(const rapidjson::Document& doc);
    void parse_trade_update(const rapidjson::Document& doc);
    void parse_ticker_update(const rapidjson::Document& doc);
    void parse_mark_price_update(const rapidjson::Document& doc);
    
    // Helper methods
    std::string get_stream_name(const Symbol& symbol, const std::string& stream_type) const;
    std::string build_combined_stream_url(const std::vector<std::string>& streams) const;
    void update_subscription_url();
    
    // Parse depth snapshot from REST API
    void request_depth_snapshot(const Symbol& symbol);
    void parse_depth_snapshot(const Symbol& symbol, const std::string& response);
    
    // Subscription management
    std::unordered_set<std::string> active_streams_;
    std::unordered_map<std::string, Symbol> stream_symbol_map_;
    
    // Order book management
    struct DepthCache {
        std::map<Price, Quantity, std::greater<Price>> bids;
        std::map<Price, Quantity> asks;
        uint64_t last_update_id;
        bool initialized;
    };
    
    std::unordered_map<Symbol, DepthCache> depth_cache_;
    
    // Endpoints
    std::string ws_spot_endpoint_;
    std::string ws_futures_endpoint_;
    std::string rest_endpoint_;
    
    // Connection management
    std::unique_ptr<std::thread> io_thread_;
    bool use_combined_streams_;
    
    // Stream limits
    static constexpr size_t MAX_STREAMS_PER_CONNECTION = 200;
};

} // namespace arbitrage