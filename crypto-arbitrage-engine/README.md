# Crypto Arbitrage Detection Engine

A high-performance C++ arbitrage detection engine for cryptocurrency markets that identifies mispricings across synthetic and real instrument pairs.

## Features

- Real-time WebSocket connections to multiple exchanges (OKX, Binance, Bybit)
- Sub-10ms arbitrage detection latency
- SIMD-optimized price calculations
- Lock-free data structures for ultra-low latency
- Comprehensive risk management with VaR/CVaR
- Statistical arbitrage with cointegration analysis
- Prometheus-compatible metrics export

## Prerequisites

### System Requirements
- Linux (Ubuntu 20.04+ or similar)
- C++17/20 compatible compiler (GCC 9+ or Clang 10+)
- CMake 3.16+
- 8GB+ RAM recommended
- AVX2 support for SIMD optimizations

### Dependencies
The project uses the following external libraries:
- spdlog (logging)
- rapidjson (JSON parsing)
- websocketpp (WebSocket client)
- Intel TBB (parallel algorithms)
- OpenSSL (for secure WebSocket connections)
- Boost (for websocketpp)

## Installation

### 1. Install System Dependencies

```bash
# Update package list
sudo apt-get update

# Install build tools
sudo apt-get install -y build-essential cmake git

# Install required libraries
sudo apt-get install -y \
    libssl-dev \
    libboost-all-dev \
    libtbb-dev \
    libspdlog-dev \
    rapidjson-dev

# Install websocketpp (if not available via apt)
git clone https://github.com/zaphoyd/websocketpp.git
cd websocketpp
mkdir build && cd build
cmake ..
sudo make install
cd ../..
```

### 2. Clone and Build the Project

```bash
# Clone the repository (or navigate to your project directory)
cd crypto-arbitrage-engine

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
make -j$(nproc)
```

## Configuration

### 1. System Configuration
Edit `config/config.json` to configure system parameters:

```json
{
  "system": {
    "thread_pool_size": 8,
    "order_book_depth": 20,
    "log_level": "info",
    "log_file": "logs/arbitrage.log"
  },
  "arbitrage": {
    "min_profit_threshold": 10.0,
    "max_position_size": 100000.0,
    "max_portfolio_exposure": 1000000.0
  }
}
```

### 2. Exchange Configuration
Edit `config/exchanges.json` to configure exchange connections:

```json
{
  "exchanges": [
    {
      "name": "OKX",
      "enabled": true,
      "ws_endpoints": {
        "public": "wss://ws.okx.com:8443/ws/v5/public"
      },
      "symbols": {
        "spot": ["BTC-USDT", "ETH-USDT"],
        "perpetual": ["BTC-USDT-SWAP", "ETH-USDT-SWAP"]
      }
    }
  ]
}
```

### 3. Create Required Directories

```bash
# From the project root
mkdir -p logs
mkdir -p data
```

## Running the Engine

### Basic Usage

```bash
# From the build directory
./crypto-arbitrage-engine

# Or with custom config files
./crypto-arbitrage-engine ../config/config.json ../config/exchanges.json
```

### Running with Monitoring

```bash
# Run with performance monitoring
./crypto-arbitrage-engine 2>&1 | tee logs/engine_$(date +%Y%m%d_%H%M%S).log
```

### Running as a Service

Create a systemd service file `/etc/systemd/system/crypto-arbitrage.service`:

```ini
[Unit]
Description=Crypto Arbitrage Detection Engine
After=network.target

[Service]
Type=simple
User=arbitrage
WorkingDirectory=/path/to/crypto-arbitrage-engine
ExecStart=/path/to/crypto-arbitrage-engine/build/crypto-arbitrage-engine
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Then enable and start:

```bash
sudo systemctl enable crypto-arbitrage
sudo systemctl start crypto-arbitrage
```

## Monitoring

### Log Files
- Main log: `logs/arbitrage.log`
- Metrics export: `metrics_final.json` (generated on shutdown)

### Real-time Monitoring
The engine prints performance statistics every 30 seconds:
- Messages processed
- Opportunities detected/executed
- Average profit
- Portfolio VaR
- System resource usage

### Prometheus Integration
Metrics are exported in Prometheus format. To scrape:

```yaml
scrape_configs:
  - job_name: 'crypto-arbitrage'
    static_configs:
      - targets: ['localhost:9090']
```

## Troubleshooting

### Connection Issues
1. Check firewall settings for WebSocket connections
2. Verify exchange API endpoints are accessible
3. Check logs for specific error messages

### Performance Issues
1. Ensure CPU has AVX2 support: `grep avx2 /proc/cpuinfo`
2. Adjust thread pool size based on CPU cores
3. Monitor system resources with `htop`

### Build Errors
1. Ensure all dependencies are installed
2. Check compiler version: `g++ --version` (need 9+)
3. Clear build directory and rebuild

## Testing

### Unit Tests
```bash
# From build directory
make test
./tests/run_tests
```

### Integration Tests
```bash
# Test exchange connections
./tests/test_exchanges

# Test arbitrage detection
./tests/test_arbitrage
```

## Performance Tuning

### CPU Affinity
Set CPU affinity for better performance:

```bash
taskset -c 0-7 ./crypto-arbitrage-engine
```

### Memory Settings
Increase system limits:

```bash
# Add to /etc/security/limits.conf
* soft memlock unlimited
* hard memlock unlimited
```

### Network Tuning
Optimize network settings:

```bash
# Add to /etc/sysctl.conf
net.core.rmem_max = 134217728
net.core.wmem_max = 134217728
net.ipv4.tcp_rmem = 4096 87380 134217728
net.ipv4.tcp_wmem = 4096 65536 134217728
```

## Development

### Adding New Exchanges
1. Create new class inheriting from `BaseExchange`
2. Implement WebSocket message parsing
3. Add to `MarketDataManager` in main.cpp

### Adding New Strategies
1. Create new synthetic pricer in `src/synthetic/`
2. Register with `ArbitrageDetector`
3. Add configuration parameters

## License

[Your License Here]

## Support

For issues and questions:
- Check logs in `logs/` directory
- Review configuration files
- Ensure all dependencies are properly installed