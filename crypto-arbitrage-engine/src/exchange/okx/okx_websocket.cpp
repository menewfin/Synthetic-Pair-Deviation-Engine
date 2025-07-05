#include "okx_websocket.h"
#include "core/utils.h"
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <sstream>

namespace arbitrage {

OKXWebSocket::OKXWebSocket(const ExchangeConfig& config)
    : ExchangeBase(Exchange::OKX, config) {
    ws_public_endpoint_ = constants::endpoints::OKX_WS_PUBLIC;
    ws_business_endpoint_ = constants::endpoints::OKX_WS_BUSINESS;
}

void OKXWebSocket::connect() {
    if (state_ == ConnectionState::CONNECTED || state_ == ConnectionState::CONNECTING) {
        LOG_WARN("OKX WebSocket already connected or connecting");
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
        
        // Create connection
        websocketpp::lib::error_code ec;
        auto con = ws_client_->get_connection(ws_public_endpoint_, ec);
        
        if (ec) {
            LOG_ERROR("OKX connection creation failed: {}", ec.message());
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
        LOG_ERROR("OKX connection failed: {}", e.what());
        state_ = ConnectionState::ERROR;
    }
}

void OKXWebSocket::disconnect() {
    if (state_ == ConnectionState::DISCONNECTED) return;
    
    state_ = ConnectionState::DISCONNECTED;
    
    try {
        ws_client_->close(connection_, websocketpp::close::status::normal, "Closing");
        ws_client_->stop();
        
        if (io_thread_ && io_thread_->joinable()) {
            io_thread_->join();
        }
    } catch (const std::exception& e) {
        LOG_ERROR("OKX disconnect error: {}", e.what());
    }
}

void OKXWebSocket::subscribe_orderbook(const Symbol& symbol, InstrumentType type) {
    std::string inst_id = get_inst_id(symbol, type);
    std::string message = build_subscribe_message(constants::channels::OKX_ORDERBOOK, inst_id);
    
    pending_subscriptions_.insert(inst_id + ":" + constants::channels::OKX_ORDERBOOK);
    send_message(message);
}

void OKXWebSocket::subscribe_trades(const Symbol& symbol, InstrumentType type) {
    std::string inst_id = get_inst_id(symbol, type);
    std::string message = build_subscribe_message(constants::channels::OKX_TRADES, inst_id);
    
    pending_subscriptions_.insert(inst_id + ":" + constants::channels::OKX_TRADES);
    send_message(message);
}

void OKXWebSocket::subscribe_ticker(const Symbol& symbol, InstrumentType type) {
    std::string inst_id = get_inst_id(symbol, type);
    std::string message = build_subscribe_message(constants::channels::OKX_TICKER, inst_id);
    
    pending_subscriptions_.insert(inst_id + ":" + constants::channels::OKX_TICKER);
    send_message(message);
}

void OKXWebSocket::subscribe_funding_rate(const Symbol& symbol) {
    std::string inst_id = get_inst_id(symbol, InstrumentType::PERPETUAL);
    std::string message = build_subscribe_message(constants::channels::OKX_FUNDING_RATE, inst_id);
    
    pending_subscriptions_.insert(inst_id + ":" + constants::channels::OKX_FUNDING_RATE);
    send_message(message);
}

void OKXWebSocket::unsubscribe_orderbook(const Symbol& symbol, InstrumentType type) {
    std::string inst_id = get_inst_id(symbol, type);
    std::string message = build_unsubscribe_message(constants::channels::OKX_ORDERBOOK, inst_id);
    
    send_message(message);
    subscriptions_.erase(inst_id + ":" + constants::channels::OKX_ORDERBOOK);
}

void OKXWebSocket::unsubscribe_all() {
    for (const auto& [key, sub] : subscriptions_) {
        std::string message = build_unsubscribe_message(sub.channel, sub.inst_id);
        send_message(message);
    }
    subscriptions_.clear();
}

void OKXWebSocket::on_message(WsConnection hdl, WsMessage msg) {
    messages_received_++;
    
    try {
        std::string payload = msg->get_payload();
        parse_message(payload);
    } catch (const std::exception& e) {
        LOG_ERROR("OKX message processing error: {}", e.what());
    }
}

void OKXWebSocket::parse_message(const std::string& message) {
    rapidjson::Document doc;
    doc.Parse(message.c_str());
    
    if (doc.HasParseError()) {
        LOG_ERROR("OKX JSON parse error: {}", message);
        return;
    }
    
    // Handle different message types
    if (doc.HasMember("event")) {
        std::string event = doc["event"].GetString();
        
        if (event == "subscribe") {
            if (doc.HasMember("arg")) {
                const auto& arg = doc["arg"];
                if (arg.HasMember("channel") && arg.HasMember("instId")) {
                    std::string channel = arg["channel"].GetString();
                    std::string inst_id = arg["instId"].GetString();
                    LOG_INFO("OKX subscribed to {} for {}", channel, inst_id);
                }
            }
        } else if (event == "error") {
            std::string msg = doc.HasMember("msg") ? doc["msg"].GetString() : "Unknown error";
            handle_error(msg);
        }
        return;
    }
    
    // Handle data messages
    if (doc.HasMember("arg") && doc.HasMember("data")) {
        const auto& arg = doc["arg"];
        const auto& data = doc["data"];
        
        if (!arg.HasMember("channel")) return;
        
        std::string channel = arg["channel"].GetString();
        
        if (channel == constants::channels::OKX_ORDERBOOK) {
            parse_orderbook_message(data);
        } else if (channel == constants::channels::OKX_TRADES) {
            parse_trades_message(data);
        } else if (channel == constants::channels::OKX_TICKER) {
            parse_ticker_message(data);
        } else if (channel == constants::channels::OKX_FUNDING_RATE) {
            parse_funding_rate_message(data);
        }
    }
}

void OKXWebSocket::parse_orderbook_message(const rapidjson::Value& data) {
    if (!data.IsArray() || data.Empty()) return;
    
    for (const auto& item : data.GetArray()) {
        if (!item.HasMember("instId") || !item.HasMember("asks") || !item.HasMember("bids")) {
            continue;
        }
        
        std::string inst_id = item["instId"].GetString();
        
        std::vector<PriceLevel> bids, asks;
        
        // Parse bids
        const auto& bids_array = item["bids"];
        for (const auto& bid : bids_array.GetArray()) {
            if (bid.IsArray() && bid.Size() >= 2) {
                Price price = std::stod(bid[0].GetString());
                Quantity qty = std::stod(bid[1].GetString());
                uint32_t count = bid.Size() >= 4 ? bid[3].GetUint() : 1;
                bids.emplace_back(price, qty, count);
            }
        }
        
        // Parse asks
        const auto& asks_array = item["asks"];
        for (const auto& ask : asks_array.GetArray()) {
            if (ask.IsArray() && ask.Size() >= 2) {
                Price price = std::stod(ask[0].GetString());
                Quantity qty = std::stod(ask[1].GetString());
                uint32_t count = ask.Size() >= 4 ? ask[3].GetUint() : 1;
                asks.emplace_back(price, qty, count);
            }
        }
        
        // Update order book
        update_orderbook(inst_id, bids, asks);
    }
}

void OKXWebSocket::parse_trades_message(const rapidjson::Value& data) {
    if (!data.IsArray()) return;
    
    for (const auto& item : data.GetArray()) {
        if (!item.HasMember("instId") || !item.HasMember("px") || 
            !item.HasMember("sz") || !item.HasMember("side")) {
            continue;
        }
        
        MarketData md;
        md.symbol = item["instId"].GetString();
        md.exchange = Exchange::OKX;
        md.last_price = std::stod(item["px"].GetString());
        md.volume_24h = std::stod(item["sz"].GetString());
        md.timestamp = std::chrono::milliseconds(item["ts"].GetInt64());
        
        update_market_data(md);
    }
}

void OKXWebSocket::parse_ticker_message(const rapidjson::Value& data) {
    if (!data.IsArray()) return;
    
    for (const auto& item : data.GetArray()) {
        if (!item.HasMember("instId") || !item.HasMember("bidPx") || 
            !item.HasMember("askPx") || !item.HasMember("bidSz") || 
            !item.HasMember("askSz")) {
            continue;
        }
        
        MarketData md;
        md.symbol = item["instId"].GetString();
        md.exchange = Exchange::OKX;
        md.bid_price = std::stod(item["bidPx"].GetString());
        md.ask_price = std::stod(item["askPx"].GetString());
        md.bid_size = std::stod(item["bidSz"].GetString());
        md.ask_size = std::stod(item["askSz"].GetString());
        
        if (item.HasMember("last")) {
            md.last_price = std::stod(item["last"].GetString());
        }
        
        if (item.HasMember("vol24h")) {
            md.volume_24h = std::stod(item["vol24h"].GetString());
        }
        
        md.timestamp = std::chrono::milliseconds(item["ts"].GetInt64());
        
        update_market_data(md);
    }
}

void OKXWebSocket::parse_funding_rate_message(const rapidjson::Value& data) {
    if (!data.IsArray()) return;
    
    for (const auto& item : data.GetArray()) {
        if (!item.HasMember("instId") || !item.HasMember("fundingRate")) {
            continue;
        }
        
        MarketData md;
        md.symbol = item["instId"].GetString();
        md.exchange = Exchange::OKX;
        md.type = InstrumentType::PERPETUAL;
        md.funding_rate = std::stod(item["fundingRate"].GetString());
        md.timestamp = std::chrono::milliseconds(item["fundingTime"].GetInt64());
        
        update_market_data(md);
    }
}

std::string OKXWebSocket::get_inst_type(InstrumentType type) const {
    switch (type) {
        case InstrumentType::SPOT: return "SPOT";
        case InstrumentType::PERPETUAL: return "SWAP";
        case InstrumentType::FUTURES: return "FUTURES";
        case InstrumentType::OPTION: return "OPTION";
        default: return "SPOT";
    }
}

std::string OKXWebSocket::get_inst_id(const Symbol& symbol, InstrumentType type) const {
    // OKX uses different formats for different instrument types
    // This is a simplified version - real implementation would need proper mapping
    return symbol;
}

std::string OKXWebSocket::build_subscribe_message(const std::string& channel, 
                                                  const std::string& inst_id) const {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    
    doc.AddMember("op", "subscribe", allocator);
    
    rapidjson::Value args(rapidjson::kArrayType);
    rapidjson::Value arg(rapidjson::kObjectType);
    
    arg.AddMember("channel", rapidjson::Value(channel.c_str(), allocator), allocator);
    arg.AddMember("instId", rapidjson::Value(inst_id.c_str(), allocator), allocator);
    
    args.PushBack(arg, allocator);
    doc.AddMember("args", args, allocator);
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    
    return buffer.GetString();
}

std::string OKXWebSocket::build_unsubscribe_message(const std::string& channel,
                                                    const std::string& inst_id) const {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    
    doc.AddMember("op", "unsubscribe", allocator);
    
    rapidjson::Value args(rapidjson::kArrayType);
    rapidjson::Value arg(rapidjson::kObjectType);
    
    arg.AddMember("channel", rapidjson::Value(channel.c_str(), allocator), allocator);
    arg.AddMember("instId", rapidjson::Value(inst_id.c_str(), allocator), allocator);
    
    args.PushBack(arg, allocator);
    doc.AddMember("args", args, allocator);
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    
    return buffer.GetString();
}

} // namespace arbitrage