#include "metrics_collector.h"
#include "utils/logger.h"
#include <fstream>
#include <sstream>
#include <sys/resource.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace arbitrage {

MetricsCollector::MetricsCollector() 
    : start_time_(std::chrono::steady_clock::now()) {
    running_ = true;
    
    // Start background thread for system metrics
    metrics_thread_ = std::make_unique<std::thread>([this]() {
        metrics_update_loop();
    });
}

MetricsCollector::~MetricsCollector() {
    running_ = false;
    
    if (metrics_thread_ && metrics_thread_->joinable()) {
        metrics_thread_->join();
    }
}

void MetricsCollector::record_processing_latency(const std::string& operation, uint64_t microseconds) {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    
    auto& tracker = operation_latencies_[operation];
    if (!tracker) {
        tracker = std::make_unique<LatencyTracker<1000>>();
    }
    tracker->record(microseconds);
}

void MetricsCollector::record_detection_latency(uint64_t microseconds) {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    detection_latencies_.record(microseconds);
}

void MetricsCollector::record_execution_latency(uint64_t microseconds) {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    execution_latencies_.record(microseconds);
}

void MetricsCollector::increment_messages_processed() {
    messages_processed_++;
}

void MetricsCollector::increment_opportunities_detected() {
    opportunities_detected_++;
}

void MetricsCollector::increment_opportunities_executed() {
    opportunities_executed_++;
}

void MetricsCollector::record_trade(const ArbitrageOpportunity& opportunity, double actual_profit) {
    std::lock_guard<std::mutex> lock(trade_mutex_);
    
    TradeRecord record;
    record.timestamp = utils::get_current_timestamp();
    record.opportunity_id = opportunity.id;
    record.expected_profit = opportunity.expected_profit;
    record.actual_profit = actual_profit;
    record.successful = actual_profit > 0;
    
    trade_history_.push_back(record);
    
    LOG_INFO("Trade recorded: {} - Expected: ${:.2f}, Actual: ${:.2f}", 
            opportunity.id, opportunity.expected_profit, actual_profit);
}

void MetricsCollector::record_missed_opportunity(const ArbitrageOpportunity& opportunity, 
                                                const std::string& reason) {
    LOG_WARN("Missed opportunity: {} - Reason: {}", opportunity.id, reason);
}

void MetricsCollector::update_memory_usage() {
    uint64_t memory_mb = get_process_memory_mb();
    current_memory_mb_ = memory_mb;
    
    if (memory_mb > peak_memory_mb_) {
        peak_memory_mb_ = memory_mb;
    }
}

void MetricsCollector::update_cpu_usage() {
    double cpu_percent = get_process_cpu_percent();
    current_cpu_percent_ = cpu_percent;
    
    if (cpu_percent > peak_cpu_percent_) {
        peak_cpu_percent_ = cpu_percent;
    }
}

PerformanceMetrics MetricsCollector::get_current_metrics() const {
    PerformanceMetrics metrics;
    
    // Latency metrics
    {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        
        if (detection_latencies_.count() > 0) {
            metrics.avg_detection_latency = detection_latencies_.percentile(0.5);
            metrics.max_processing_latency = detection_latencies_.max();
        }
        
        // Average across all operations
        uint64_t total_latency = 0;
        uint64_t total_count = 0;
        
        for (const auto& [op, tracker] : operation_latencies_) {
            if (tracker->count() > 0) {
                total_latency += tracker->percentile(0.5) * tracker->count();
                total_count += tracker->count();
            }
        }
        
        if (total_count > 0) {
            metrics.avg_processing_latency = total_latency / total_count;
        }
    }
    
    // Throughput metrics
    metrics.messages_processed = messages_processed_.load();
    metrics.opportunities_detected = opportunities_detected_.load();
    metrics.opportunities_executed = opportunities_executed_.load();
    
    // System metrics
    metrics.memory_usage_mb = current_memory_mb_.load();
    metrics.cpu_usage_percent = current_cpu_percent_.load();
    
    // Business metrics
    {
        std::lock_guard<std::mutex> lock(trade_mutex_);
        
        metrics.total_trades = trade_history_.size();
        
        for (const auto& trade : trade_history_) {
            metrics.total_pnl += trade.actual_profit;
            if (trade.successful) {
                metrics.profitable_trades++;
            }
        }
    }
    
    return metrics;
}

MetricsCollector::DetailedStatistics MetricsCollector::get_detailed_statistics() const {
    DetailedStatistics stats{};
    
    // Latency statistics
    {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        
        for (const auto& [op, tracker] : operation_latencies_) {
            if (tracker->count() > 0) {
                DetailedStatistics::LatencyStats lat_stats;
                lat_stats.p50 = tracker->percentile(0.5);
                lat_stats.p90 = tracker->percentile(0.9);
                lat_stats.p95 = tracker->percentile(0.95);
                lat_stats.p99 = tracker->percentile(0.99);
                lat_stats.max = tracker->max();
                lat_stats.count = tracker->count();
                
                stats.operation_latencies[op] = lat_stats;
            }
        }
    }
    
    // Calculate throughput
    auto uptime = std::chrono::steady_clock::now() - start_time_;
    double uptime_seconds = std::chrono::duration<double>(uptime).count();
    
    stats.throughput.messages_per_second = messages_processed_ / uptime_seconds;
    stats.throughput.opportunities_per_minute = (opportunities_detected_ * 60) / uptime_seconds;
    stats.throughput.trades_per_hour = (opportunities_executed_ * 3600) / uptime_seconds;
    
    // Business statistics
    {
        std::lock_guard<std::mutex> lock(trade_mutex_);
        
        stats.business.total_trades = trade_history_.size();
        
        for (const auto& trade : trade_history_) {
            stats.business.total_profit += trade.actual_profit;
            
            if (trade.successful) {
                stats.business.winning_trades++;
            } else {
                stats.business.losing_trades++;
            }
        }
        
        if (stats.business.total_trades > 0) {
            stats.business.win_rate = 
                static_cast<double>(stats.business.winning_trades) / stats.business.total_trades;
            stats.business.avg_profit_per_trade = 
                stats.business.total_profit / stats.business.total_trades;
        }
    }
    
    // System statistics
    stats.system.avg_cpu_usage = current_cpu_percent_.load();
    stats.system.peak_cpu_usage = peak_cpu_percent_;
    stats.system.avg_memory_mb = current_memory_mb_.load();
    stats.system.peak_memory_mb = peak_memory_mb_;
    stats.system.uptime_hours = uptime_seconds / 3600.0;
    
    return stats;
}

void MetricsCollector::reset() {
    messages_processed_ = 0;
    opportunities_detected_ = 0;
    opportunities_executed_ = 0;
    
    {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        operation_latencies_.clear();
        detection_latencies_ = LatencyTracker<1000>();
        execution_latencies_ = LatencyTracker<1000>();
    }
    
    {
        std::lock_guard<std::mutex> lock(trade_mutex_);
        trade_history_.clear();
    }
    
    start_time_ = std::chrono::steady_clock::now();
}

std::string MetricsCollector::export_prometheus_format() const {
    auto metrics = get_current_metrics();
    auto stats = get_detailed_statistics();
    
    std::stringstream ss;
    
    // Latency metrics
    ss << "# HELP arbitrage_processing_latency_us Processing latency in microseconds\n";
    ss << "# TYPE arbitrage_processing_latency_us gauge\n";
    ss << "arbitrage_processing_latency_us " << metrics.avg_processing_latency << "\n";
    
    ss << "# HELP arbitrage_detection_latency_us Detection latency in microseconds\n";
    ss << "# TYPE arbitrage_detection_latency_us gauge\n";
    ss << "arbitrage_detection_latency_us " << metrics.avg_detection_latency << "\n";
    
    // Throughput metrics
    ss << "# HELP arbitrage_messages_processed_total Total messages processed\n";
    ss << "# TYPE arbitrage_messages_processed_total counter\n";
    ss << "arbitrage_messages_processed_total " << metrics.messages_processed << "\n";
    
    ss << "# HELP arbitrage_opportunities_detected_total Total opportunities detected\n";
    ss << "# TYPE arbitrage_opportunities_detected_total counter\n";
    ss << "arbitrage_opportunities_detected_total " << metrics.opportunities_detected << "\n";
    
    // Business metrics
    ss << "# HELP arbitrage_total_pnl_usd Total P&L in USD\n";
    ss << "# TYPE arbitrage_total_pnl_usd gauge\n";
    ss << "arbitrage_total_pnl_usd " << metrics.total_pnl << "\n";
    
    ss << "# HELP arbitrage_win_rate Win rate percentage\n";
    ss << "# TYPE arbitrage_win_rate gauge\n";
    ss << "arbitrage_win_rate " << stats.business.win_rate * 100 << "\n";
    
    // System metrics
    ss << "# HELP arbitrage_memory_usage_mb Memory usage in MB\n";
    ss << "# TYPE arbitrage_memory_usage_mb gauge\n";
    ss << "arbitrage_memory_usage_mb " << metrics.memory_usage_mb << "\n";
    
    ss << "# HELP arbitrage_cpu_usage_percent CPU usage percentage\n";
    ss << "# TYPE arbitrage_cpu_usage_percent gauge\n";
    ss << "arbitrage_cpu_usage_percent " << metrics.cpu_usage_percent << "\n";
    
    return ss.str();
}

std::string MetricsCollector::export_json() const {
    auto metrics = get_current_metrics();
    auto stats = get_detailed_statistics();
    
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    
    // Add metrics to JSON
    rapidjson::Value performance(rapidjson::kObjectType);
    performance.AddMember("avg_processing_latency_us", metrics.avg_processing_latency, allocator);
    performance.AddMember("avg_detection_latency_us", metrics.avg_detection_latency, allocator);
    performance.AddMember("messages_processed", metrics.messages_processed, allocator);
    performance.AddMember("opportunities_detected", metrics.opportunities_detected, allocator);
    
    rapidjson::Value business(rapidjson::kObjectType);
    business.AddMember("total_pnl", metrics.total_pnl, allocator);
    business.AddMember("total_trades", metrics.total_trades, allocator);
    business.AddMember("win_rate", stats.business.win_rate, allocator);
    
    rapidjson::Value system(rapidjson::kObjectType);
    system.AddMember("memory_mb", metrics.memory_usage_mb, allocator);
    system.AddMember("cpu_percent", metrics.cpu_usage_percent, allocator);
    system.AddMember("uptime_hours", stats.system.uptime_hours, allocator);
    
    doc.AddMember("performance", performance, allocator);
    doc.AddMember("business", business, allocator);
    doc.AddMember("system", system, allocator);
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    
    return buffer.GetString();
}

void MetricsCollector::metrics_update_loop() {
    while (running_) {
        update_memory_usage();
        update_cpu_usage();
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

uint64_t MetricsCollector::get_process_memory_mb() const {
    // Get RSS (Resident Set Size) on Linux
    std::ifstream statm("/proc/self/statm");
    if (statm.is_open()) {
        long page_size = sysconf(_SC_PAGE_SIZE);
        long rss_pages;
        statm >> rss_pages >> rss_pages;  // Second value is RSS
        return (rss_pages * page_size) / (1024 * 1024);
    }
    return 0;
}

double MetricsCollector::get_process_cpu_percent() const {
    // Simplified CPU usage - would need more sophisticated implementation
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        double cpu_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6 +
                         usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
        
        auto uptime = std::chrono::steady_clock::now() - start_time_;
        double uptime_seconds = std::chrono::duration<double>(uptime).count();
        
        if (uptime_seconds > 0) {
            return (cpu_time / uptime_seconds) * 100.0;
        }
    }
    return 0.0;
}

} // namespace arbitrage