#pragma once

#include "core/types.h"
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace arbitrage {

class MetricsCollector {
public:
    MetricsCollector();
    ~MetricsCollector();
    
    // Latency tracking
    void record_processing_latency(const std::string& operation, uint64_t microseconds);
    void record_detection_latency(uint64_t microseconds);
    void record_execution_latency(uint64_t microseconds);
    
    // Throughput tracking
    void increment_messages_processed();
    void increment_opportunities_detected();
    void increment_opportunities_executed();
    
    // Business metrics
    void record_trade(const ArbitrageOpportunity& opportunity, double actual_profit);
    void record_missed_opportunity(const ArbitrageOpportunity& opportunity, const std::string& reason);
    
    // System metrics
    void update_memory_usage();
    void update_cpu_usage();
    
    // Get current metrics
    PerformanceMetrics get_current_metrics() const;
    
    // Get detailed statistics
    struct DetailedStatistics {
        // Latency percentiles (in microseconds)
        struct LatencyStats {
            uint64_t p50;
            uint64_t p90;
            uint64_t p95;
            uint64_t p99;
            uint64_t max;
            uint64_t count;
        };
        
        std::unordered_map<std::string, LatencyStats> operation_latencies;
        
        // Throughput over time
        struct ThroughputStats {
            uint64_t messages_per_second;
            uint64_t opportunities_per_minute;
            uint64_t trades_per_hour;
        };
        
        ThroughputStats throughput;
        
        // Business metrics
        struct BusinessStats {
            double total_profit;
            double win_rate;
            double avg_profit_per_trade;
            double sharpe_ratio;
            uint64_t total_trades;
            uint64_t winning_trades;
            uint64_t losing_trades;
        };
        
        BusinessStats business;
        
        // System health
        struct SystemStats {
            double avg_cpu_usage;
            double peak_cpu_usage;
            uint64_t avg_memory_mb;
            uint64_t peak_memory_mb;
            double uptime_hours;
        };
        
        SystemStats system;
    };
    
    DetailedStatistics get_detailed_statistics() const;
    
    // Reset metrics
    void reset();
    
    // Export metrics for monitoring systems
    std::string export_prometheus_format() const;
    std::string export_json() const;
    
private:
    // Latency tracking with circular buffer
    template<size_t N>
    class LatencyTracker {
    public:
        void record(uint64_t value) {
            buffer_[index_ % N] = value;
            index_++;
            count_ = std::min(count_ + 1, N);
        }
        
        uint64_t percentile(double p) const {
            if (count_ == 0) return 0;
            
            std::vector<uint64_t> sorted;
            sorted.reserve(count_);
            
            for (size_t i = 0; i < count_; ++i) {
                sorted.push_back(buffer_[i]);
            }
            
            std::sort(sorted.begin(), sorted.end());
            
            size_t idx = static_cast<size_t>(p * (count_ - 1));
            return sorted[idx];
        }
        
        uint64_t max() const {
            if (count_ == 0) return 0;
            return *std::max_element(buffer_.begin(), buffer_.begin() + count_);
        }
        
        size_t count() const { return count_; }
        
    private:
        std::array<uint64_t, N> buffer_{};
        size_t index_ = 0;
        size_t count_ = 0;
    };
    
    // Latency trackers
    std::unordered_map<std::string, std::unique_ptr<LatencyTracker<1000>>> operation_latencies_;
    LatencyTracker<1000> detection_latencies_;
    LatencyTracker<1000> execution_latencies_;
    mutable std::mutex latency_mutex_;
    
    // Throughput counters
    std::atomic<uint64_t> messages_processed_{0};
    std::atomic<uint64_t> opportunities_detected_{0};
    std::atomic<uint64_t> opportunities_executed_{0};
    
    // Business metrics
    struct TradeRecord {
        Timestamp timestamp;
        std::string opportunity_id;
        double expected_profit;
        double actual_profit;
        bool successful;
    };
    
    std::vector<TradeRecord> trade_history_;
    mutable std::mutex trade_mutex_;
    
    // System metrics
    std::atomic<uint64_t> current_memory_mb_{0};
    std::atomic<double> current_cpu_percent_{0.0};
    uint64_t peak_memory_mb_ = 0;
    double peak_cpu_percent_ = 0.0;
    
    // Start time
    std::chrono::steady_clock::time_point start_time_;
    
    // Background thread for system metrics
    std::unique_ptr<std::thread> metrics_thread_;
    std::atomic<bool> running_{false};
    
    void metrics_update_loop();
    uint64_t get_process_memory_mb() const;
    double get_process_cpu_percent() const;
};

// Global metrics instance
class GlobalMetrics {
public:
    static MetricsCollector& instance() {
        static MetricsCollector collector;
        return collector;
    }
};

// RAII metric timer
class MetricTimer {
public:
    MetricTimer(const std::string& operation)
        : operation_(operation)
        , start_(std::chrono::high_resolution_clock::now()) {}
    
    ~MetricTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
        GlobalMetrics::instance().record_processing_latency(operation_, duration);
    }
    
private:
    std::string operation_;
    std::chrono::high_resolution_clock::time_point start_;
};

} // namespace arbitrage