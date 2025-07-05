#include "bybit_websocket.h"
#include "core/utils.h"
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace arbitrage {

BybitWebSocket::BybitWebSocket(const ExchangeConfig& config)
    : ExchangeBase(Exchange::BYBIT, config) {
}

void BybitWebSocket::connect() {
    if (state_ == ConnectionState::CONNECTED || state_ == ConnectionState::CONNECTING) {
        return;
    }
    
    state_ = ConnectionState::CONNECTING;
    
    try {
        ws_client_->set_open_handler([this](WsConnection hdl) { on_open(hdl); });
        ws_client_->set_close_handler([this](WsConnection hdl) { on_close(hdl); });
        ws_client_->set_fail_handler([this](WsConnection hdl) { on_fail(hdl); });
        ws_client_->set_message_handler([this](WsConnection hdl, WsMessage msg) { 
            on_message(hdl, msg); 
        });
        
        websocketpp::lib::error_code ec;
        auto con = ws_client_->get_connection(constants::endpoints::BYBIT_WS_SPOT, ec);
        
        if (ec) {
            state_ = ConnectionState::ERROR;
            return;
        }
        
        ws_client_->connect(con);
        
        io_thread_ = std::make_unique<std::thread>([this]() {
            ws_client_->run();
        });
        
    } catch (const std::exception& e) {
        LOG_ERROR("Bybit connection failed: {}", e.what());
        state_ = ConnectionState::ERROR;
    }
}

void BybitWebSocket::disconnect() {
    if (state_ == ConnectionState::DISCONNECTED) return;
    
    state_ = ConnectionState::DISCONNECTED;
    
    try {
        ws_client_->close(connection_, websocketpp::close::status::normal, "Closing");
        ws_client_->stop();
        
        if (io_thread_ && io_thread_->joinable()) {
            io_thread_->join();
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Bybit disconnect error: {}", e.what());
    }
}

void BybitWebSocket::subscribe_orderbook(const Symbol& symbol, InstrumentType type) {
    std::string topic = get_topic(symbol, "orderbook.50");
    topic_symbol_map_[topic] = symbol;
    send_message(build_subscribe_message(topic));
}

void BybitWebSocket::subscribe_trades(const Symbol& symbol, InstrumentType type) {
    std::string topic = get_topic(symbol, "publicTrade");
    topic_symbol_map_[topic] = symbol;
    send_message(build_subscribe_message(topic));
}

void BybitWebSocket::subscribe_ticker(const Symbol& symbol, InstrumentType type) {
    std::string topic = get_topic(symbol, "tickers");
    topic_symbol_map_[topic] = symbol;
    send_message(build_subscribe_message(topic));
}

void BybitWebSocket::subscribe_funding_rate(const Symbol& symbol) {
    std::string topic = get_topic(symbol, "fundingRate");
    topic_symbol_map_[topic] = symbol;
    send_message(build_subscribe_message(topic));
}

void BybitWebSocket::unsubscribe_orderbook(const Symbol& symbol, InstrumentType type) {
    // Implement unsubscribe logic
}

void BybitWebSocket::unsubscribe_all() {
    topic_symbol_map_.clear();
}

void BybitWebSocket::on_message(WsConnection hdl, WsMessage msg) {
    messages_received_++;
    
    try {
        std::string payload = msg->get_payload();
        parse_message(payload);
    } catch (const std::exception& e) {
        LOG_ERROR("Bybit message processing error: {}", e.what());
    }
}

void BybitWebSocket::parse_message(const std::string& message) {
    rapidjson::Document doc;
    doc.Parse(message.c_str());
    
    if (doc.HasParseError()) {
        return;
    }
    
    if (doc.HasMember("topic") && doc.HasMember("data")) {
        std::string topic = doc["topic"].GetString();
        
        // Extract symbol from topic
        auto it = topic_symbol_map_.find(topic);
        if (it == topic_symbol_map_.end()) return;
        
        Symbol symbol = it->second;
        
        if (topic.find("orderbook") != std::string::npos) {
            // Parse orderbook data
            const auto& data = doc["data"];
            if (data.HasMember("b") && data.HasMember("a")) {
                std::vector<PriceLevel> bids, asks;
                
                const auto& b = data["b"];
                for (const auto& bid : b.GetArray()) {
                    if (bid.Size() >= 2) {
                        bids.emplace_back(
                            std::stod(bid[0].GetString()),
                            std::stod(bid[1].GetString()),
                            1
                        );
                    }
                }
                
                const auto& a = data["a"];
                for (const auto& ask : a.GetArray()) {
                    if (ask.Size() >= 2) {
                        asks.emplace_back(
                            std::stod(ask[0].GetString()),
                            std::stod(ask[1].GetString()),
                            1
                        );
                    }
                }
                
                update_orderbook(symbol, bids, asks);
            }
        } else if (topic.find("tickers") != std::string::npos) {
            // Parse ticker data
            const auto& data = doc["data"];
            
            MarketData md;
            md.symbol = symbol;
            md.exchange = Exchange::BYBIT;
            
            if (data.HasMember("bid1Price")) 
                md.bid_price = std::stod(data["bid1Price"].GetString());
            if (data.HasMember("ask1Price")) 
                md.ask_price = std::stod(data["ask1Price"].GetString());
            if (data.HasMember("lastPrice")) 
                md.last_price = std::stod(data["lastPrice"].GetString());
            if (data.HasMember("volume24h")) 
                md.volume_24h = std::stod(data["volume24h"].GetString());
            
            md.timestamp = utils::get_current_timestamp();
            update_market_data(md);
        }
    }
}

std::string BybitWebSocket::build_subscribe_message(const std::string& topic) const {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    
    doc.AddMember("op", "subscribe", allocator);
    
    rapidjson::Value args(rapidjson::kArrayType);
    args.PushBack(rapidjson::Value(topic.c_str(), allocator), allocator);
    doc.AddMember("args", args, allocator);
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    
    return buffer.GetString();
}

std::string BybitWebSocket::get_topic(const Symbol& symbol, const std::string& channel) const {
    return channel + "." + symbol;
}

} // namespace arbitrage