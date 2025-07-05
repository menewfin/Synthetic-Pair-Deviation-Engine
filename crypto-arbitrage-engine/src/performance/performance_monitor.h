#pragma once

#include "core/types.h"
#include <memory>
#include <thread>
#include <atomic>

namespace arbitrage {

// Forward declarations
class MetricsCollector;
class MarketDataManager;
class ArbitrageDetector;
class RiskManager;

class PerformanceMonitor {
public:
    PerformanceMonitor(MetricsCollector* metrics,
                      MarketDataManager* market_data,
                      ArbitrageDetector* arbitrage_detector,
                      RiskManager* risk_manager);
    ~PerformanceMonitor();
    
    // Start/stop monitoring
    void start();
    void stop();
    
    // Real-time dashboard output
    void print_dashboard() const;
    
    // Performance alerts
    struct PerformanceAlert {
        enum AlertType {
            HIGH_LATENCY,
            LOW_THROUGHPUT,
            HIGH_CPU,
            HIGH_MEMORY,
            LOW_HIT_RATE,
            RISK_LIMIT_BREACH
        };
        
        AlertType type;
        std::string message;
        double severity;
        Timestamp timestamp;
    };
    
    std::vector<PerformanceAlert> get_active_alerts() const;
    
    // Performance reports
    void generate_hourly_report() const;
    void generate_daily_report() const;
    
    // Export performance data
    void export_to_csv(const std::string& filename) const;
    void export_to_json(const std::string& filename) const;
    
private:
    MetricsCollector* metrics_;
    MarketDataManager* market_data_;
    ArbitrageDetector* arbitrage_detector_;
    RiskManager* risk_manager_;
    
    // Monitoring thread
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> running_{false};
    
    // Alert tracking
    std::vector<PerformanceAlert> active_alerts_;
    mutable std::mutex alerts_mutex_;
    
    // Monitoring loop
    void monitor_loop();
    
    // Check various performance aspects
    void check_latency();
    void check_throughput();
    void check_system_resources();
    void check_business_metrics();
    void check_risk_metrics();
    
    // Generate alert
    void generate_alert(PerformanceAlert::AlertType type, 
                       const std::string& message,
                       double severity);
    
    // Clear old alerts
    void cleanup_alerts();
};

// Console dashboard for real-time monitoring
class ConsoleDashboard {
public:
    static void clear_screen();
    static void move_cursor(int row, int col);
    static void set_color(int color);
    static void reset_color();
    
    static void draw_box(int row, int col, int width, int height, const std::string& title);
    static void draw_progress_bar(int row, int col, int width, double percentage);
    static void draw_metric(int row, int col, const std::string& label, const std::string& value);
};

} // namespace arbitrage