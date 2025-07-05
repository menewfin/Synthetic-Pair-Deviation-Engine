# Synthetic Pair Deviation Engine

A high-performance cryptocurrency arbitrage trading system written in C++ that detects and exploits price deviations between synthetic pairs and real pairs across multiple exchanges.

## 🚀 Features

### Core Capabilities
- **Multi-Exchange Support**: Real-time WebSocket connections to OKX, Binance, and Bybit
- **Synthetic Pricing**: Advanced pricing models for futures, perpetuals, and complex derivatives
- **Arbitrage Detection**: Multiple strategies including spot, synthetic, funding, and calendar spreads
- **Risk Management**: Comprehensive position limits, VaR calculations, and exposure controls
- **High Performance**: SIMD optimizations, lock-free data structures, and microsecond latency

### Arbitrage Strategies
1. **Spot Arbitrage**: Price differences across exchanges for the same asset
2. **Synthetic Arbitrage**: Mispricing between real and synthetic instruments
3. **Funding Arbitrage**: Exploiting funding rate differences on perpetual swaps
4. **Calendar Spreads**: Futures contract spread mispricings
5. **Statistical Arbitrage**: Mean reversion on synthetic spreads

### Technical Architecture
- **Multi-threaded Design**: Parallel processing for market data and detection
- **Memory Optimization**: Custom allocators and object pooling
- **SIMD Acceleration**: Vectorized calculations for order book operations
- **Real-time Processing**: Sub-10ms opportunity detection latency

## 📋 Requirements

### System Requirements
- **OS**: Linux (Ubuntu 20.04+ recommended)
- **CPU**: x86_64 with AVX2 support
- **Memory**: 4GB+ RAM
- **Network**: Low-latency internet connection

### Dependencies
- **C++20** compatible compiler (GCC 10+ or Clang 11+)
- **CMake 3.16+**
- **Boost 1.70+** (system, thread)
- **OpenSSL** (for WebSocket TLS)

The following dependencies are automatically fetched by CMake:
- **spdlog**: Fast logging library
- **RapidJSON**: JSON parsing
- **WebSocket++**: WebSocket client
- **Intel TBB**: Threading Building Blocks

## 🛠️ Building

### Quick Start
```bash
# Clone the repository
git clone <repository-url>
cd crypto-arbitrage-engine

# Make scripts executable
chmod +x scripts/*.sh

# Build the project
./scripts/build.sh

# Run the engine
cd build
./scripts/run.sh
```

### Manual Build
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Build Options
- **Release**: Optimized build (default)
- **Debug**: Debug symbols and assertions
- **RelWithDebInfo**: Optimized with debug info

```bash
./scripts/build.sh Debug     # Debug build
./scripts/build.sh Release   # Release build
./scripts/build.sh Release clean  # Clean rebuild
```

## ⚙️ Configuration

### Exchange Configuration (`config/exchanges.json`)
```json
{
    "exchanges": [
        {
            "name": "BINANCE",
            "enabled": true,
            "ws_endpoints": {
                "spot": "wss://stream.binance.com:9443/ws",
                "futures": "wss://fstream.binance.com/ws"
            },
            "symbols": {
                "spot": ["BTCUSDT", "ETHUSDT", "SOLUSDT"],
                "perpetual": ["BTCUSDT", "ETHUSDT", "SOLUSDT"]
            }
        }
    ]
}
```

### System Configuration (`config/config.json`)
```json
{
    "arbitrage": {
        "min_profit_threshold": 0.001,
        "max_position_size": 100000.0,
        "opportunity_ttl_ms": 500
    },
    "risk": {
        "max_portfolio_exposure": 1000000.0,
        "max_funding_rate_exposure": 0.01,
        "min_liquidity_score": 0.7
    }
}
```

## 🚦 Usage

### Starting the Engine
```bash
cd build
./scripts/run.sh
```

### Command Line Options
```bash
./arbitrage_engine [options]
```

### Environment Variables
- `TBB_NUM_THREADS`: Number of TBB worker threads (default: CPU cores)
- `OMP_NUM_THREADS`: OpenMP thread count (default: CPU cores)

## 📊 Monitoring

### Real-time Metrics
The engine provides comprehensive real-time monitoring:

- **Opportunities**: Detection rate, execution rate, profit distribution
- **System**: CPU usage, memory consumption, network latency
- **Risk**: Portfolio exposure, position limits, VaR calculations
- **Performance**: Processing latency, message throughput

### Logging
Logs are written to `logs/arbitrage_engine.log` with configurable levels:
- `trace`: Detailed debugging information
- `debug`: Development debugging
- `info`: General information (default)
- `warn`: Warning messages
- `error`: Error conditions

## 🏗️ Architecture

### Core Components

1. **Market Data Manager**: Aggregates real-time data from exchanges
2. **Synthetic Pricers**: Calculate fair values for complex instruments
3. **Arbitrage Detector**: Identifies profitable opportunities
4. **Risk Manager**: Enforces position limits and risk controls
5. **Exchange Adapters**: Handle exchange-specific protocols

### Data Flow
```
Exchange WebSockets → Market Data Manager → Synthetic Pricers
                                         ↓
Risk Manager ← Arbitrage Detector ← Opportunity Detection
```

### Threading Model
- **Main Thread**: Coordination and monitoring
- **Exchange Threads**: One per exchange WebSocket
- **Processing Threads**: Market data aggregation
- **Detection Threads**: Arbitrage opportunity scanning
- **Risk Thread**: Portfolio monitoring

## 🛡️ Risk Management

### Position Limits
- Per-symbol position limits
- Exchange exposure limits
- Total portfolio exposure cap

### Risk Metrics
- **Value at Risk (VaR)**: Portfolio risk estimation
- **Correlation Risk**: Cross-asset correlation monitoring
- **Funding Risk**: Perpetual swap funding exposure
- **Liquidity Risk**: Market impact assessment

### Safety Features
- Automatic position sizing based on Kelly criterion
- Real-time P&L monitoring
- Emergency stop mechanisms
- Risk alerts and notifications

## 🔧 Development

### Code Structure
```
src/
├── arbitrage/          # Opportunity detection logic
├── core/              # Core types and utilities
├── exchange/          # Exchange adapters
├── market_data/       # Data processing and order books
├── performance/       # Metrics and monitoring
├── risk/             # Risk management
├── synthetic/        # Pricing models
└── utils/            # Utilities (logging, threading, memory)
```

### Key Classes
- `ArbitrageDetector`: Main detection engine
- `MarketDataManager`: Real-time data aggregation
- `SyntheticPricer`: Base class for pricing models
- `RiskManager`: Portfolio risk controls
- `ExchangeBase`: Exchange adapter interface

### Adding New Exchanges
1. Inherit from `ExchangeBase`
2. Implement WebSocket message parsing
3. Add to exchange factory in `main.cpp`
4. Update configuration files

### Adding New Strategies
1. Create new pricer class inheriting from `SyntheticPricer`
2. Implement pricing and opportunity detection logic
3. Register with `ArbitrageDetector`
4. Add configuration parameters

## 📈 Performance Tuning

### System Optimization
- Set CPU governor to `performance`
- Disable CPU frequency scaling
- Increase network buffer sizes
- Use dedicated CPU cores for critical threads

### Application Tuning
- Adjust thread pool sizes based on workload
- Optimize memory pool sizes
- Tune detection frequency vs. latency
- Configure exchange rate limits

### Monitoring Performance
```bash
# CPU usage
htop

# Memory usage
free -h

# Network statistics
iftop

# Application metrics
tail -f logs/arbitrage_engine.log | grep "METRICS"
```

## 🐛 Troubleshooting

### Common Issues

1. **WebSocket Connection Failures**
   - Check internet connectivity
   - Verify exchange endpoints
   - Check firewall settings

2. **High Memory Usage**
   - Adjust market data buffer sizes
   - Reduce order book depth
   - Enable memory pooling

3. **Slow Performance**
   - Check CPU governor settings
   - Verify SIMD support
   - Optimize thread configuration

### Debug Mode
```bash
./scripts/build.sh Debug
cd build
gdb ./arbitrage_engine
```

## 📄 License

This project is licensed under the MIT License. See the LICENSE file for details.

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

## ⚠️ Disclaimer

This software is for educational and research purposes only. Cryptocurrency trading involves substantial risk and is not suitable for all investors. Always conduct your own research and consider your risk tolerance before trading.

## 📞 Support

For questions, issues, or contributions:
- Create an issue on GitHub
- Review the documentation
- Check the troubleshooting section

---

**Built with ❤️ for the cryptocurrency arbitrage community**