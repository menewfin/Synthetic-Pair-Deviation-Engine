#pragma once

#include "exchange/exchange_base.h"
#include <rapidjson/document.h>
#include <unordered_map>

namespace arbitrage {

class BybitWebSocket : public ExchangeBase {
public:
    explicit BybitWebSocket(const ExchangeConfig& config);
    ~BybitWebSocket() override = default;
    
    void connect() override;
    void disconnect() override;
    
    void subscribe_orderbook(const Symbol& symbol, InstrumentType type) override;
    void subscribe_trades(const Symbol& symbol, InstrumentType type) override;
    void subscribe_ticker(const Symbol& symbol, InstrumentType type) override;
    void subscribe_funding_rate(const Symbol& symbol) override;
    
    void unsubscribe_orderbook(const Symbol& symbol, InstrumentType type) override;
    void unsubscribe_all() override;
    
protected:
    void on_message(WsConnection hdl, WsMessage msg) override;
    void parse_message(const std::string& message) override;
    
private:
    std::string build_subscribe_message(const std::string& topic) const;
    std::string get_topic(const Symbol& symbol, const std::string& channel) const;
    
    std::unordered_map<std::string, Symbol> topic_symbol_map_;
    std::unique_ptr<std::thread> io_thread_;
};

} // namespace arbitrage