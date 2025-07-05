# How to Run the Crypto Arbitrage Engine

## Prerequisites

### System Requirements
- **OS**: Linux (Ubuntu 20.04+ recommended)
- **Compiler**: GCC 9+ or Clang 10+ with C++20 support
- **CMake**: Version 3.16 or higher
- **Memory**: At least 4GB RAM (recommended 8GB+)
- **CPU**: Multi-core processor with AVX2 support

### Dependencies
The project uses FetchContent to automatically download most dependencies, but you need some system packages:

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    libboost-system-dev \
    libboost-thread-dev \
    libssl-dev \
    pkg-config

# For CentOS/RHEL/Fedora
sudo dnf install -y \
    gcc-c++ \
    cmake \
    git \
    boost-devel \
    openssl-devel \
    pkgconfig
```

## Build Instructions

### 1. Navigate to Project Directory
```bash
cd crypto-arbitrage-engine
```

### 2. Create Build Directory
```bash
mkdir build
cd build
```

### 3. Configure with CMake
```bash
# Release build (optimized for performance)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Debug build (for development)
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

### 4. Build the Project
```bash
# Build with all available CPU cores
make -j$(nproc)

# Alternative: use CMake to build
cmake --build . --parallel
```

## Configuration

### 1. Create Logs Directory
```bash
mkdir -p ../logs
```

### 2. Configure the Engine
The engine uses two main configuration files:

#### Main Configuration (`config/config.json`)
- **System settings**: Thread pool size, logging, memory settings
- **Arbitrage parameters**: Profit thresholds, position limits
- **Risk management**: Portfolio exposure, position limits
- **Performance monitoring**: Metrics and thresholds

Key settings to review:
- `min_profit_threshold`: Minimum profit threshold (default: 0.001 = 0.1%)
- `max_position_size`: Maximum position size in USD (default: $100,000)
- `log_level`: "debug", "info", "warn", "error" (default: "info")

#### Exchange Configuration (`config/exchanges.json`)
- **Exchange endpoints**: WebSocket URLs for market data
- **Trading symbols**: Cryptocurrencies to monitor
- **Rate limits**: Connection and message limits
- **Exchange-specific settings**

## Running the Engine

### 1. Basic Execution
```bash
# Run from the build directory
./arbitrage_engine

# Or specify custom config files
./arbitrage_engine ../config/config.json ../config/exchanges.json
```

### 2. Run with Custom Log Level
Edit `config/config.json` and change the log level:
```json
{
    "system": {
        "log_level": "debug"  // Change to "debug" for verbose output
    }
}
```

### 3. Background Execution
```bash
# Run in background and redirect output
nohup ./arbitrage_engine > ../logs/engine.out 2>&1 &

# Check process
ps aux | grep arbitrage_engine
```

## Understanding the Output

### Startup Messages
```
=== Crypto Arbitrage Engine Starting ===
Config: config/config.json
Thread pool size: 16
Min profit threshold: 0.10 bps
```

### Exchange Connections
```
Adding exchange: OKX
Adding exchange: BINANCE  
Adding exchange: BYBIT
Starting market data collection...
Subscribing to BTC-USDT across all exchanges
```

### Arbitrage Opportunities
```
Arbitrage opportunity detected: opp_20231201_143052_001
  Type: Simple arbitrage
  Expected profit: $12.45 (0.15%)
  Required capital: $8,300.00
  Execution risk: 0.02
  Risk check: PASSED - Ready for execution
```

### Performance Statistics (every 30 seconds)
```
=== Performance Update ===
Messages processed: 45,231
Opportunities detected: 12
Opportunities executed: 8
Average profit: 8.5 bps
Portfolio VaR: $1,250.00
Memory usage: 187 MB
CPU usage: 23.4%
```

## Monitoring and Control

### 1. Graceful Shutdown
```bash
# Send SIGTERM to gracefully shut down
kill -TERM <pid>

# Or use Ctrl+C if running in foreground
```

### 2. Log Files
- **Engine logs**: `logs/arbitrage_engine.log`
- **Final metrics**: `metrics_final.json` (created on shutdown)

### 3. Real-time Monitoring
```bash
# Monitor log file
tail -f logs/arbitrage_engine.log

# Monitor system resources
top -p $(pgrep arbitrage_engine)
```

## Troubleshooting

### Common Issues

#### 1. Build Failures
```bash
# Clear build cache and retry
rm -rf build/*
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make clean
make -j$(nproc)
```

#### 2. Missing Dependencies
```bash
# Check if Boost is properly installed
find /usr -name "libboost_system*" 2>/dev/null

# Reinstall if needed
sudo apt install --reinstall libboost-all-dev
```

#### 3. WebSocket Connection Issues
- Check internet connectivity
- Verify exchange endpoints in `config/exchanges.json`
- Check firewall settings for outbound HTTPS/WSS connections

#### 4. High CPU/Memory Usage
- Reduce `thread_pool_size` in config
- Lower `order_book_depth` setting
- Increase `opportunity_ttl_ms` to reduce processing frequency

#### 5. No Arbitrage Opportunities
- Lower `min_profit_threshold` for testing
- Check if exchanges are connected (look for "Starting market data collection" message)
- Verify trading symbols are correctly configured

### Debug Mode
```bash
# Build in debug mode for more detailed information
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run with debug logging
# Edit config.json: "log_level": "debug"
./arbitrage_engine
```

## Performance Optimization

### System Tuning
```bash
# Increase file descriptor limits
ulimit -n 65536

# Set CPU governor to performance mode
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

### Configuration Tuning
- **High-frequency trading**: Increase `thread_pool_size`, decrease `opportunity_ttl_ms`
- **Lower resource usage**: Decrease `thread_pool_size`, increase `metrics_update_interval_ms`
- **More opportunities**: Lower `min_profit_threshold`, disable `enable_maker_only`

## Safety Notes

⚠️ **Important**: This is a demonstration/research engine. Before using with real funds:

1. **Test thoroughly** with paper trading
2. **Review risk parameters** carefully
3. **Monitor system performance** under load
4. **Implement proper error handling** for production use
5. **Consider market impact** and slippage in real trading

The engine currently operates in **detection-only mode** and does not execute actual trades.