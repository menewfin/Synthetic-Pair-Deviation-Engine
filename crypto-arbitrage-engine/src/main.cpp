#include <iostream>
#include <signal.h>
#include <rapidjson/document.h>
#include <fstream>

#include "utils/logger.h"
#include "utils/thread_pool.h"
#include "market_data/market_data_manager.h"
#include "exchange/okx/okx_websocket.h"
#include "exchange/binance/binance_websocket.h"
#include "exchange/bybit/bybit_websocket.h"
#include "arbitrage/arbitrage_detector.h"
#include "risk/risk_manager.h"
#include "performance/metrics_collector.h"

using namespace arbitrage;

// Global flag for shutdown
std::atomic<bool> g_shutdown{false};

// Signal handler
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LOG_INFO("Shutdown signal received");
        g_shutdown = true;
    }
}

// Load configuration from file
bool load_config(const std::string& config_file, 
                SystemConfig& system_config,
                ArbitrageConfig& arbitrage_config) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file: {}", config_file);
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    rapidjson::Document doc;
    doc.Parse(content.c_str());
    
    if (doc.HasParseError()) {
        LOG_ERROR("Failed to parse config file");
        return false;
    }
    
    // Load system config
    if (doc.HasMember("system")) {
        const auto& sys = doc["system"];
        
        if (sys.HasMember("thread_pool_size"))
            system_config.thread_pool_size = sys["thread_pool_size"].GetUint();
        if (sys.HasMember("order_book_depth"))
            system_config.order_book_depth = sys["order_book_depth"].GetUint();
        if (sys.HasMember("log_level"))
            system_config.log_level = sys["log_level"].GetString();
        if (sys.HasMember("log_file"))
            system_config.log_file = sys["log_file"].GetString();
    }
    
    // Load arbitrage config
    if (doc.HasMember("arbitrage")) {
        const auto& arb = doc["arbitrage"];
        
        if (arb.HasMember("min_profit_threshold"))
            arbitrage_config.min_profit_threshold = arb["min_profit_threshold"].GetDouble();
        if (arb.HasMember("max_position_size"))
            arbitrage_config.max_position_size = arb["max_position_size"].GetDouble();
    }
    
    return true;
}

// Load exchange configuration
std::vector<ExchangeConfig> load_exchange_config(const std::string& config_file) {
    std::vector<ExchangeConfig> configs;
    
    std::ifstream file(config_file);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open exchange config file: {}", config_file);
        return configs;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    rapidjson::Document doc;
    doc.Parse(content.c_str());
    
    if (doc.HasParseError() || !doc.HasMember("exchanges")) {
        LOG_ERROR("Failed to parse exchange config file");
        return configs;
    }
    
    const auto& exchanges = doc["exchanges"];
    for (const auto& exchange : exchanges.GetArray()) {
        if (!exchange.HasMember("enabled") || !exchange["enabled"].GetBool()) {
            continue;
        }
        
        ExchangeConfig config;
        config.name = exchange["name"].GetString();
        
        if (exchange.HasMember("ws_endpoints")) {
            const auto& endpoints = exchange["ws_endpoints"];
            if (endpoints.HasMember("public"))
                config.ws_endpoint = endpoints["public"].GetString();
        }
        
        if (exchange.HasMember("symbols")) {
            const auto& symbols = exchange["symbols"];
            if (symbols.HasMember("spot")) {
                for (const auto& symbol : symbols["spot"].GetArray()) {
                    config.symbols.push_back(symbol.GetString());
                }
            }
        }
        
        config.reconnect_interval_ms = exchange["reconnect_interval_ms"].GetUint();
        config.heartbeat_interval_ms = exchange["heartbeat_interval_ms"].GetUint();
        
        configs.push_back(config);
    }
    
    return configs;
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Load configuration
    std::string config_file = "config/config.json";
    std::string exchange_config_file = "config/exchanges.json";
    
    if (argc > 1) {
        config_file = argv[1];
    }
    if (argc > 2) {
        exchange_config_file = argv[2];
    }
    
    SystemConfig system_config;
    ArbitrageConfig arbitrage_config;
    
    if (!load_config(config_file, system_config, arbitrage_config)) {
        std::cerr << "Failed to load configuration" << std::endl;
        return 1;
    }
    
    // Initialize logger
    Logger::init(system_config.log_file, system_config.log_level);
    
    LOG_INFO("=== Crypto Arbitrage Engine Starting ===");
    LOG_INFO("Config: {}", config_file);
    LOG_INFO("Thread pool size: {}", system_config.thread_pool_size);
    LOG_INFO("Min profit threshold: {:.2f} bps", arbitrage_config.min_profit_threshold);
    
    try {
        // Initialize thread pool
        ThreadPool thread_pool(system_config.thread_pool_size);
        
        // Initialize market data manager
        auto market_data = std::make_unique<MarketDataManager>();
        
        // Load and add exchanges
        auto exchange_configs = load_exchange_config(exchange_config_file);
        
        for (const auto& config : exchange_configs) {
            LOG_INFO("Adding exchange: {}", config.name);
            
            if (config.name == "OKX") {
                market_data->add_exchange(std::make_unique<OKXWebSocket>(config));
            } else if (config.name == "BINANCE") {
                market_data->add_exchange(std::make_unique<BinanceWebSocket>(config));
            } else if (config.name == "BYBIT") {
                market_data->add_exchange(std::make_unique<BybitWebSocket>(config));
            }
        }
        
        // Initialize risk manager
        auto risk_manager = std::make_unique<RiskManager>(market_data.get());
        risk_manager->set_max_portfolio_exposure(arbitrage_config.max_portfolio_exposure);
        
        // Initialize arbitrage detector
        auto arbitrage_detector = std::make_unique<ArbitrageDetector>(
            market_data.get(), 
            risk_manager.get()
        );
        arbitrage_detector->set_min_profit_threshold(arbitrage_config.min_profit_threshold);
        arbitrage_detector->set_max_position_size(arbitrage_config.max_position_size);
        
        // Set up opportunity callback
        arbitrage_detector->register_opportunity_callback(
            [&risk_manager](const ArbitrageOpportunity& opportunity) {
                LOG_INFO("Arbitrage opportunity detected: {}", opportunity.id);
                LOG_INFO("  Type: {} arbitrage", 
                        opportunity.legs.size() == 2 ? "Simple" : "Complex");
                LOG_INFO("  Expected profit: ${:.2f} ({:.2f}%)", 
                        opportunity.expected_profit, 
                        opportunity.profit_percentage);
                LOG_INFO("  Required capital: ${:.2f}", opportunity.required_capital);
                LOG_INFO("  Execution risk: {:.2f}", opportunity.execution_risk);
                
                // Check risk before execution
                if (risk_manager->check_opportunity_risk(opportunity)) {
                    LOG_INFO("  Risk check: PASSED - Ready for execution");
                    GlobalMetrics::instance().increment_opportunities_executed();
                } else {
                    LOG_WARN("  Risk check: FAILED - Opportunity rejected");
                }
            }
        );
        
        // Start all components
        LOG_INFO("Starting market data collection...");
        market_data->start();
        
        // Subscribe to symbols
        std::vector<std::string> symbols = {"BTC-USDT", "ETH-USDT", "SOL-USDT"};
        for (const auto& symbol : symbols) {
            LOG_INFO("Subscribing to {} across all exchanges", symbol);
            market_data->subscribe_all_exchanges(symbol, InstrumentType::SPOT);
            market_data->subscribe_all_exchanges(symbol, InstrumentType::PERPETUAL);
        }
        
        // Wait for connections to establish
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        LOG_INFO("Starting arbitrage detection...");
        arbitrage_detector->start();
        
        // Main loop
        LOG_INFO("=== Engine Running ===");
        LOG_INFO("Press Ctrl+C to shutdown");
        
        auto last_stats_time = std::chrono::steady_clock::now();
        
        while (!g_shutdown) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Print statistics every 30 seconds
            auto now = std::chrono::steady_clock::now();
            if (now - last_stats_time > std::chrono::seconds(30)) {
                auto metrics = GlobalMetrics::instance().get_current_metrics();
                auto detector_stats = arbitrage_detector->get_statistics();
                auto risk_metrics = risk_manager->calculate_risk_metrics();
                
                LOG_INFO("=== Performance Update ===");
                LOG_INFO("Messages processed: {}", metrics.messages_processed);
                LOG_INFO("Opportunities detected: {}", detector_stats.opportunities_detected);
                LOG_INFO("Opportunities executed: {}", metrics.opportunities_executed);
                LOG_INFO("Average profit: {:.2f} bps", detector_stats.avg_profit_bps);
                LOG_INFO("Portfolio VaR: ${:.2f}", risk_metrics.portfolio_var);
                LOG_INFO("Memory usage: {} MB", metrics.memory_usage_mb);
                LOG_INFO("CPU usage: {:.1f}%", metrics.cpu_usage_percent);
                
                last_stats_time = now;
            }
        }
        
        // Shutdown sequence
        LOG_INFO("Shutting down...");
        
        arbitrage_detector->stop();
        market_data->stop();
        
        // Final statistics
        auto final_metrics = GlobalMetrics::instance().get_detailed_statistics();
        
        LOG_INFO("=== Final Statistics ===");
        LOG_INFO("Total runtime: {:.2f} hours", final_metrics.system.uptime_hours);
        LOG_INFO("Total opportunities: {}", final_metrics.business.total_trades);
        LOG_INFO("Win rate: {:.1f}%", final_metrics.business.win_rate * 100);
        LOG_INFO("Total P&L: ${:.2f}", final_metrics.business.total_profit);
        
        // Export metrics
        std::string metrics_json = GlobalMetrics::instance().export_json();
        std::ofstream metrics_file("metrics_final.json");
        metrics_file << metrics_json;
        metrics_file.close();
        
        LOG_INFO("Metrics exported to metrics_final.json");
        
    } catch (const std::exception& e) {
        LOG_CRITICAL("Fatal error: {}", e.what());
        return 1;
    }
    
    LOG_INFO("=== Shutdown Complete ===");
    
    return 0;
}