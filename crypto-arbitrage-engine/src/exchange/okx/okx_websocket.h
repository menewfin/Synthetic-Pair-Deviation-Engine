#pragma once

#include "exchange/exchange_base.h"
#include <rapidjson/document.h>
#include <unordered_map>
#include <unordered_set>

namespace arbitrage {

class OKXWebSocket : public ExchangeBase {
public:
    explicit OKXWebSocket(const ExchangeConfig& config);
    ~OKXWebSocket() override = default;
    
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
    void parse_orderbook_message(const rapidjson::Value& data);
    void parse_trades_message(const rapidjson::Value& data);
    void parse_ticker_message(const rapidjson::Value& data);
    void parse_funding_rate_message(const rapidjson::Value& data);
    
    // Helper methods
    std::string get_inst_type(InstrumentType type) const;
    std::string get_inst_id(const Symbol& symbol, InstrumentType type) const;
    std::string build_subscribe_message(const std::string& channel, 
                                       const std::string& inst_id) const;
    std::string build_unsubscribe_message(const std::string& channel,
                                         const std::string& inst_id) const;
    
    // Parse order book snapshot
    void parse_orderbook_snapshot(const rapidjson::Value& data,
                                 std::vector<PriceLevel>& bids,
                                 std::vector<PriceLevel>& asks);
    
    // Update order book with delta
    void update_orderbook_delta(const rapidjson::Value& data,
                               const std::string& inst_id);
    
    // Subscription tracking
    struct Subscription {
        std::string channel;
        std::string inst_id;
        InstrumentType inst_type;
    };
    
    std::unordered_map<std::string, Subscription> subscriptions_;
    std::unordered_set<std::string> pending_subscriptions_;
    
    // Order book cache for delta updates
    struct OrderBookCache {
        std::map<Price, PriceLevel, std::greater<Price>> bids;  // Descending order
        std::map<Price, PriceLevel> asks;                       // Ascending order
        uint64_t checksum;
        Timestamp last_update;
    };
    
    std::unordered_map<std::string, OrderBookCache> orderbook_cache_;
    
    // WebSocket endpoints
    std::string ws_public_endpoint_;
    std::string ws_business_endpoint_;
    
    // Thread for running io_context
    std::unique_ptr<std::thread> io_thread_;
};

} // namespace arbitrage