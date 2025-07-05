#include "binance_websocket.h"
#include "core/utils.h"
#include <algorithm>
#include <cctype>

namespace arbitrage {

BinanceWebSocket::BinanceWebSocket(const ExchangeConfig& config)
    : ExchangeBase(Exchange::BINANCE, config)
    , use_combined_streams_(true) {
    ws_spot_endpoint_ = constants::endpoints::BINANCE_WS_SPOT;
    ws_futures_endpoint_ = constants::endpoints::BINANCE_WS_FUTURES;
    rest_endpoint_ = "https://api.binance.com";
}

void BinanceWebSocket::connect() {
    if (state_ == ConnectionState::CONNECTED || state_ == ConnectionState::CONNECTING) {
        LOG_WARN("Binance WebSocket already connected or connecting");
        return;
    }
    
    state_ = ConnectionState::CONNECTING;
    
    try {
        // Set handlers
        ws_client_->set_open_handler([this](WsConnection hdl) { on_open(hdl); });
        ws_client_->set_close_handler([this](WsConnection hdl) { on_close(hdl); });
        ws_client_->set_fail_handler([this](WsConnection hdl) { on_fail(hdl); });
        ws_client_->set_message_handler([this](WsConnection hdl, WsMessage msg) { 
            on_message(hdl, msg); 
        });
        
        // Build connection URL with streams
        std::string url = ws_spot_endpoint_;
        if (use_combined_streams_ && !active_streams_.empty()) {
            url = build_combined_stream_url(std::vector<std::string>(
                active_streams_.begin(), active_streams_.end()
            ));
        }
        
        // Create connection
        websocketpp::lib::error_code ec;
        auto con = ws_client_->get_connection(url, ec);
        
        if (ec) {
            LOG_ERROR("Binance connection creation failed: {}", ec.message());
            state_ = ConnectionState::ERROR;
            return;
        }
        
        // Connect
        ws_client_->connect(con);
        
        // Run io_context in separate thread
        io_thread_ = std::make_unique<std::thread>([this]() {
            ws_client_->run();
        });
        
    } catch (const std::exception& e) {
        LOG_ERROR("Binance connection failed: {}", e.what());
        state_ = ConnectionState::ERROR;
    }
}

void BinanceWebSocket::disconnect() {
    if (state_ == ConnectionState::DISCONNECTED) return;
    
    state_ = ConnectionState::DISCONNECTED;
    
    try {
        ws_client_->close(connection_, websocketpp::close::status::normal, "Closing");
        ws_client_->stop();
        
        if (io_thread_ && io_thread_->joinable()) {
            io_thread_->join();
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Binance disconnect error: {}", e.what());
    }
}

void BinanceWebSocket::subscribe_orderbook(const Symbol& symbol, InstrumentType type) {
    std::string stream = get_stream_name(symbol, "depth20@100ms");
    active_streams_.insert(stream);
    stream_symbol_map_[stream] = symbol;
    
    // Request snapshot for proper order book initialization
    request_depth_snapshot(symbol);
    
    if (state_ == ConnectionState::CONNECTED) {
        update_subscription_url();
    }
}

void BinanceWebSocket::subscribe_trades(const Symbol& symbol, InstrumentType type) {
    std::string stream = get_stream_name(symbol, "trade");
    active_streams_.insert(stream);
    stream_symbol_map_[stream] = symbol;
    
    if (state_ == ConnectionState::CONNECTED) {
        update_subscription_url();
    }
}

void BinanceWebSocket::subscribe_ticker(const Symbol& symbol, InstrumentType type) {
    std::string stream = get_stream_name(symbol, "ticker");
    active_streams_.insert(stream);
    stream_symbol_map_[stream] = symbol;
    
    if (state_ == ConnectionState::CONNECTED) {
        update_subscription_url();
    }
}

void BinanceWebSocket::subscribe_funding_rate(const Symbol& symbol) {
    std::string stream = get_stream_name(symbol, "markPrice@1s");
    active_streams_.insert(stream);
    stream_symbol_map_[stream] = symbol;
    
    if (state_ == ConnectionState::CONNECTED) {
        update_subscription_url();
    }
}

void BinanceWebSocket::unsubscribe_orderbook(const Symbol& symbol, InstrumentType type) {
    std::string stream = get_stream_name(symbol, "depth20@100ms");
    active_streams_.erase(stream);
    stream_symbol_map_.erase(stream);
    
    if (state_ == ConnectionState::CONNECTED) {
        update_subscription_url();
    }
}

void BinanceWebSocket::unsubscribe_all() {
    active_streams_.clear();
    stream_symbol_map_.clear();
    depth_cache_.clear();
    
    if (state_ == ConnectionState::CONNECTED) {
        disconnect();
    }
}

void BinanceWebSocket::on_message(WsConnection hdl, WsMessage msg) {
    messages_received_++;
    
    try {
        std::string payload = msg->get_payload();
        parse_message(payload);
    } catch (const std::exception& e) {
        LOG_ERROR("Binance message processing error: {}", e.what());
    }
}

void BinanceWebSocket::parse_message(const std::string& message) {
    rapidjson::Document doc;
    doc.Parse(message.c_str());
    
    if (doc.HasParseError()) {
        LOG_ERROR("Binance JSON parse error: {}", message);
        return;
    }
    
    // Binance sends different message formats
    if (doc.HasMember("stream") && doc.HasMember("data")) {
        // Combined stream format
        std::string stream = doc["stream"].GetString();
        const auto& data = doc["data"];
        
        if (stream.find("depth") != std::string::npos) {
            parse_depth_update(data);
        } else if (stream.find("trade") != std::string::npos) {
            parse_trade_update(data);
        } else if (stream.find("ticker") != std::string::npos) {
            parse_ticker_update(data);
        } else if (stream.find("markPrice") != std::string::npos) {
            parse_mark_price_update(data);
        }
    } else {
        // Direct stream format
        if (doc.HasMember("e")) {
            std::string event_type = doc["e"].GetString();
            
            if (event_type == "depthUpdate") {
                parse_depth_update(doc);
            } else if (event_type == "trade") {
                parse_trade_update(doc);
            } else if (event_type == "24hrTicker") {
                parse_ticker_update(doc);
            } else if (event_type == "markPriceUpdate") {
                parse_mark_price_update(doc);
            }
        }
    }
}

void BinanceWebSocket::parse_depth_update(const rapidjson::Document& doc) {
    if (!doc.HasMember("s") || !doc.HasMember("b") || !doc.HasMember("a")) {
        return;
    }
    
    Symbol symbol = doc["s"].GetString();
    
    // Check if we have initialized depth cache
    auto& cache = depth_cache_[symbol];
    if (!cache.initialized) {
        // Need snapshot first
        request_depth_snapshot(symbol);
        return;
    }
    
    // Update bids
    const auto& bids = doc["b"];
    for (const auto& bid : bids.GetArray()) {
        if (bid.Size() >= 2) {
            Price price = std::stod(bid[0].GetString());
            Quantity qty = std::stod(bid[1].GetString());
            
            if (qty > 0) {
                cache.bids[price] = qty;
            } else {
                cache.bids.erase(price);
            }
        }
    }
    
    // Update asks
    const auto& asks = doc["a"];
    for (const auto& ask : asks.GetArray()) {
        if (ask.Size() >= 2) {
            Price price = std::stod(ask[0].GetString());
            Quantity qty = std::stod(ask[1].GetString());
            
            if (qty > 0) {
                cache.asks[price] = qty;
            } else {
                cache.asks.erase(price);
            }
        }
    }
    
    // Convert to vectors for callback
    std::vector<PriceLevel> bid_levels, ask_levels;
    
    size_t count = 0;
    for (const auto& [price, qty] : cache.bids) {
        bid_levels.emplace_back(price, qty, 1);
        if (++count >= constants::MAX_ORDER_BOOK_DEPTH) break;
    }
    
    count = 0;
    for (const auto& [price, qty] : cache.asks) {
        ask_levels.emplace_back(price, qty, 1);
        if (++count >= constants::MAX_ORDER_BOOK_DEPTH) break;
    }
    
    update_orderbook(symbol, bid_levels, ask_levels);
}

void BinanceWebSocket::parse_trade_update(const rapidjson::Document& doc) {
    if (!doc.HasMember("s") || !doc.HasMember("p") || !doc.HasMember("q")) {
        return;
    }
    
    MarketData md;
    md.symbol = doc["s"].GetString();
    md.exchange = Exchange::BINANCE;
    md.last_price = std::stod(doc["p"].GetString());
    md.volume_24h = std::stod(doc["q"].GetString());
    md.timestamp = std::chrono::milliseconds(doc["T"].GetInt64());
    
    update_market_data(md);
}

void BinanceWebSocket::parse_ticker_update(const rapidjson::Document& doc) {
    if (!doc.HasMember("s")) return;
    
    MarketData md;
    md.symbol = doc["s"].GetString();
    md.exchange = Exchange::BINANCE;
    
    if (doc.HasMember("b")) md.bid_price = std::stod(doc["b"].GetString());
    if (doc.HasMember("a")) md.ask_price = std::stod(doc["a"].GetString());
    if (doc.HasMember("B")) md.bid_size = std::stod(doc["B"].GetString());
    if (doc.HasMember("A")) md.ask_size = std::stod(doc["A"].GetString());
    if (doc.HasMember("c")) md.last_price = std::stod(doc["c"].GetString());
    if (doc.HasMember("v")) md.volume_24h = std::stod(doc["v"].GetString());
    
    md.timestamp = utils::get_current_timestamp();
    
    update_market_data(md);
}

void BinanceWebSocket::parse_mark_price_update(const rapidjson::Document& doc) {
    if (!doc.HasMember("s") || !doc.HasMember("r")) return;
    
    MarketData md;
    md.symbol = doc["s"].GetString();
    md.exchange = Exchange::BINANCE;
    md.type = InstrumentType::PERPETUAL;
    md.funding_rate = std::stod(doc["r"].GetString());
    md.timestamp = std::chrono::milliseconds(doc["T"].GetInt64());
    
    update_market_data(md);
}

std::string BinanceWebSocket::get_stream_name(const Symbol& symbol, const std::string& stream_type) const {
    // Convert symbol to lowercase for Binance stream names
    std::string lower_symbol = symbol;
    std::transform(lower_symbol.begin(), lower_symbol.end(), lower_symbol.begin(), ::tolower);
    
    return lower_symbol + "@" + stream_type;
}

std::string BinanceWebSocket::build_combined_stream_url(const std::vector<std::string>& streams) const {
    std::string url = ws_spot_endpoint_ + "/stream?streams=";
    
    for (size_t i = 0; i < streams.size(); ++i) {
        if (i > 0) url += "/";
        url += streams[i];
    }
    
    return url;
}

void BinanceWebSocket::update_subscription_url() {
    // Binance requires reconnection to update streams
    // In production, implement proper stream management
    LOG_INFO("Binance stream update required - reconnection needed");
}

void BinanceWebSocket::request_depth_snapshot(const Symbol& symbol) {
    // In production, implement REST API call to get order book snapshot
    // For now, mark as initialized
    depth_cache_[symbol].initialized = true;
    LOG_INFO("Requested depth snapshot for {}", symbol);
}

void BinanceWebSocket::parse_depth_snapshot(const Symbol& symbol, const std::string& response) {
    // Parse REST API depth snapshot response
    // Update depth_cache_[symbol] with snapshot data
}

} // namespace arbitrage