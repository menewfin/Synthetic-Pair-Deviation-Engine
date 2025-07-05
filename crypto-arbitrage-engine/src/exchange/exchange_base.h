#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <chrono>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include "core/types.h"
#include "core/constants.h"
#include "utils/logger.h"

namespace arbitrage {

// Forward declarations
class MarketDataManager;

// WebSocket client type
using WsClient = websocketpp::client<websocketpp::config::asio_tls_client>;
using WsMessage = websocketpp::config::asio_client::message_type::ptr;
using WsConnection = websocketpp::connection_hdl;

// Exchange connection state
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    ERROR
};

// Callback types
using MarketDataCallback = std::function<void(const MarketData&)>;
using OrderBookCallback = std::function<void(const Symbol&, const std::vector<PriceLevel>&, const std::vector<PriceLevel>&)>;
using ErrorCallback = std::function<void(const std::string&)>;

class ExchangeBase {
public:
    ExchangeBase(Exchange exchange, const ExchangeConfig& config);
    virtual ~ExchangeBase();
    
    // Connection management
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual void reconnect();
    virtual bool is_connected() const { return state_ == ConnectionState::CONNECTED; }
    
    // Subscribe to market data
    virtual void subscribe_orderbook(const Symbol& symbol, InstrumentType type) = 0;
    virtual void subscribe_trades(const Symbol& symbol, InstrumentType type) = 0;
    virtual void subscribe_ticker(const Symbol& symbol, InstrumentType type) = 0;
    virtual void subscribe_funding_rate(const Symbol& symbol) = 0;
    
    // Unsubscribe from market data
    virtual void unsubscribe_orderbook(const Symbol& symbol, InstrumentType type) = 0;
    virtual void unsubscribe_all() = 0;
    
    // Callbacks
    void set_market_data_callback(MarketDataCallback callback) {
        market_data_callback_ = std::move(callback);
    }
    
    void set_orderbook_callback(OrderBookCallback callback) {
        orderbook_callback_ = std::move(callback);
    }
    
    void set_error_callback(ErrorCallback callback) {
        error_callback_ = std::move(callback);
    }
    
    // Getters
    Exchange get_exchange() const { return exchange_; }
    const std::string& get_name() const { return config_.name; }
    ConnectionState get_state() const { return state_.load(); }
    
    // Statistics
    uint64_t get_messages_received() const { return messages_received_.load(); }
    uint64_t get_messages_processed() const { return messages_processed_.load(); }
    uint64_t get_reconnect_count() const { return reconnect_count_.load(); }
    
protected:
    // WebSocket handlers
    virtual void on_open(WsConnection hdl);
    virtual void on_close(WsConnection hdl);
    virtual void on_message(WsConnection hdl, WsMessage msg) = 0;
    virtual void on_fail(WsConnection hdl);
    virtual void on_error(WsConnection hdl, const std::error_code& ec);
    
    // Helper methods
    virtual void send_message(const std::string& message);
    virtual void send_ping();
    virtual void handle_pong();
    
    // Parse message (to be implemented by derived classes)
    virtual void parse_message(const std::string& message) = 0;
    
    // Update market data
    void update_market_data(const MarketData& data);
    void update_orderbook(const Symbol& symbol, 
                         const std::vector<PriceLevel>& bids,
                         const std::vector<PriceLevel>& asks);
    
    // Error handling
    void handle_error(const std::string& error);
    
    // Member variables
    Exchange exchange_;
    ExchangeConfig config_;
    std::atomic<ConnectionState> state_;
    
    // WebSocket client
    std::unique_ptr<WsClient> ws_client_;
    WsConnection connection_;
    
    // Callbacks
    MarketDataCallback market_data_callback_;
    OrderBookCallback orderbook_callback_;
    ErrorCallback error_callback_;
    
    // Statistics
    std::atomic<uint64_t> messages_received_{0};
    std::atomic<uint64_t> messages_processed_{0};
    std::atomic<uint64_t> reconnect_count_{0};
    
    // Timing
    std::chrono::steady_clock::time_point last_heartbeat_;
    std::chrono::steady_clock::time_point last_message_;
    
    // Thread safety
    mutable std::mutex connection_mutex_;
    
private:
    // Heartbeat timer
    void start_heartbeat_timer();
    void stop_heartbeat_timer();
    void check_heartbeat();
    
    std::unique_ptr<std::thread> heartbeat_thread_;
    std::atomic<bool> heartbeat_running_{false};
};

} // namespace arbitrage