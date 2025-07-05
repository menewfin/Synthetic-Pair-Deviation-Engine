#include "exchange_base.h"
#include <thread>
#include <chrono>

namespace arbitrage {

ExchangeBase::ExchangeBase(Exchange exchange, const ExchangeConfig& config)
    : exchange_(exchange)
    , config_(config)
    , state_(ConnectionState::DISCONNECTED)
    , last_heartbeat_(std::chrono::steady_clock::now())
    , last_message_(std::chrono::steady_clock::now()) {
    
    // Initialize WebSocket client
    ws_client_ = std::make_unique<WsClient>();
    
    // Set up logging
    ws_client_->set_access_channels(websocketpp::log::alevel::none);
    ws_client_->set_error_channels(websocketpp::log::elevel::none);
    
    // Initialize ASIO
    ws_client_->init_asio();
    
    // Set up TLS handler
    ws_client_->set_tls_init_handler([](websocketpp::connection_hdl) {
        return websocketpp::lib::make_shared<boost::asio::ssl::context>(
            boost::asio::ssl::context::tlsv12_client
        );
    });
}

ExchangeBase::~ExchangeBase() {
    stop_heartbeat_timer();
    disconnect();
}

void ExchangeBase::reconnect() {
    LOG_INFO("Reconnecting to {} exchange", config_.name);
    
    disconnect();
    
    std::this_thread::sleep_for(
        std::chrono::milliseconds(config_.reconnect_interval_ms)
    );
    
    reconnect_count_++;
    connect();
}

void ExchangeBase::on_open(WsConnection hdl) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    connection_ = hdl;
    state_ = ConnectionState::CONNECTED;
    last_heartbeat_ = std::chrono::steady_clock::now();
    last_message_ = std::chrono::steady_clock::now();
    
    LOG_INFO("{} WebSocket connection established", config_.name);
    
    // Start heartbeat timer
    start_heartbeat_timer();
}

void ExchangeBase::on_close(WsConnection hdl) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    state_ = ConnectionState::DISCONNECTED;
    LOG_INFO("{} WebSocket connection closed", config_.name);
    
    stop_heartbeat_timer();
    
    // Attempt reconnection if not manually disconnected
    if (reconnect_count_ < constants::MAX_RECONNECT_ATTEMPTS) {
        state_ = ConnectionState::RECONNECTING;
        std::thread([this]() { reconnect(); }).detach();
    }
}

void ExchangeBase::on_fail(WsConnection hdl) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    state_ = ConnectionState::ERROR;
    LOG_ERROR("{} WebSocket connection failed", config_.name);
    
    handle_error("WebSocket connection failed");
    
    // Attempt reconnection
    if (reconnect_count_ < constants::MAX_RECONNECT_ATTEMPTS) {
        state_ = ConnectionState::RECONNECTING;
        std::thread([this]() { reconnect(); }).detach();
    }
}

void ExchangeBase::on_error(WsConnection hdl, const std::error_code& ec) {
    LOG_ERROR("{} WebSocket error: {}", config_.name, ec.message());
    handle_error(ec.message());
}

void ExchangeBase::send_message(const std::string& message) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (state_ != ConnectionState::CONNECTED) {
        LOG_WARN("Cannot send message - not connected to {}", config_.name);
        return;
    }
    
    try {
        ws_client_->send(connection_, message, websocketpp::frame::opcode::text);
        LOG_DEBUG("{} sent: {}", config_.name, message);
    } catch (const std::exception& e) {
        LOG_ERROR("{} failed to send message: {}", config_.name, e.what());
        handle_error(std::string("Send failed: ") + e.what());
    }
}

void ExchangeBase::send_ping() {
    if (state_ != ConnectionState::CONNECTED) return;
    
    try {
        ws_client_->ping(connection_, "ping");
        last_heartbeat_ = std::chrono::steady_clock::now();
    } catch (const std::exception& e) {
        LOG_ERROR("{} failed to send ping: {}", config_.name, e.what());
    }
}

void ExchangeBase::handle_pong() {
    last_heartbeat_ = std::chrono::steady_clock::now();
}

void ExchangeBase::update_market_data(const MarketData& data) {
    messages_processed_++;
    last_message_ = std::chrono::steady_clock::now();
    
    if (market_data_callback_) {
        market_data_callback_(data);
    }
}

void ExchangeBase::update_orderbook(const Symbol& symbol,
                                   const std::vector<PriceLevel>& bids,
                                   const std::vector<PriceLevel>& asks) {
    messages_processed_++;
    last_message_ = std::chrono::steady_clock::now();
    
    if (orderbook_callback_) {
        orderbook_callback_(symbol, bids, asks);
    }
}

void ExchangeBase::handle_error(const std::string& error) {
    LOG_ERROR("{} error: {}", config_.name, error);
    
    if (error_callback_) {
        error_callback_(error);
    }
}

void ExchangeBase::start_heartbeat_timer() {
    heartbeat_running_ = true;
    
    heartbeat_thread_ = std::make_unique<std::thread>([this]() {
        while (heartbeat_running_) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(config_.heartbeat_interval_ms)
            );
            
            if (heartbeat_running_) {
                check_heartbeat();
            }
        }
    });
}

void ExchangeBase::stop_heartbeat_timer() {
    heartbeat_running_ = false;
    
    if (heartbeat_thread_ && heartbeat_thread_->joinable()) {
        heartbeat_thread_->join();
    }
}

void ExchangeBase::check_heartbeat() {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_heartbeat_
    ).count();
    
    if (time_since_last > config_.heartbeat_interval_ms * 2) {
        LOG_WARN("{} heartbeat timeout - reconnecting", config_.name);
        reconnect();
    } else {
        send_ping();
    }
}

} // namespace arbitrage