# Quick Start Guide

This guide will get you up and running with the Crypto Arbitrage Detection Engine in minutes.

## Prerequisites

- Ubuntu 20.04+ or similar Linux distribution
- Internet connection for downloading dependencies
- At least 4GB RAM

## Option 1: Automated Setup (Recommended)

```bash
# Navigate to project directory
cd crypto-arbitrage-engine

# Run the automated setup script
bash scripts/setup.sh

# Once setup is complete, run the quick start
bash scripts/quick_start.sh
```

## Option 2: Manual Setup

### 1. Install Dependencies

```bash
# Update system
sudo apt-get update

# Install build tools
sudo apt-get install -y build-essential cmake git

# Install libraries
sudo apt-get install -y \
    libssl-dev \
    libboost-all-dev \
    libtbb-dev \
    libspdlog-dev \
    rapidjson-dev
```

### 2. Build the Project

```bash
# Create and enter build directory
mkdir -p build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Return to project root
cd ..
```

### 3. Run the Engine

```bash
# Create logs directory
mkdir -p logs

# Run with default configs
./build/crypto-arbitrage-engine
```

## Testing the Installation

1. **Check the logs**:
   ```bash
   tail -f logs/arbitrage.log
   ```

2. **Monitor output**: You should see:
   - "Engine Starting" message
   - Exchange connection messages
   - Market data updates
   - Performance statistics every 30 seconds

3. **Verify connections**: Look for:
   - "WebSocket connected" for each exchange
   - "Subscribed to..." messages for symbols

## Basic Configuration

### Minimal Testing Config

For testing with just Bitcoin on one exchange:

**config/exchanges.json**:
```json
{
  "exchanges": [{
    "name": "OKX",
    "enabled": true,
    "ws_endpoints": {
      "public": "wss://ws.okx.com:8443/ws/v5/public"
    },
    "symbols": {
      "spot": ["BTC-USDT"],
      "perpetual": ["BTC-USDT-SWAP"]
    }
  }]
}
```

### Performance Testing Config

For testing with multiple symbols and exchanges:

**config/exchanges.json**:
```json
{
  "exchanges": [
    {
      "name": "OKX",
      "enabled": true,
      "symbols": {
        "spot": ["BTC-USDT", "ETH-USDT", "SOL-USDT"],
        "perpetual": ["BTC-USDT-SWAP", "ETH-USDT-SWAP"]
      }
    },
    {
      "name": "BINANCE",
      "enabled": true,
      "symbols": {
        "spot": ["BTCUSDT", "ETHUSDT"],
        "perpetual": ["BTCUSDT", "ETHUSDT"]
      }
    }
  ]
}
```

## Common Issues

### Build Errors

```bash
# If GCC version is too old
sudo apt-get install gcc-9 g++-9
sudo update-alternatives --config gcc

# If websocketpp is missing
git clone https://github.com/zaphoyd/websocketpp.git
cd websocketpp && mkdir build && cd build
cmake .. && sudo make install
```

### Connection Issues

```bash
# Test WebSocket connectivity
curl -I https://ws.okx.com:8443/ws/v5/public

# Check firewall
sudo ufw status
```

### Performance Issues

```bash
# Check CPU features
grep avx2 /proc/cpuinfo

# Monitor resource usage
htop

# Reduce thread pool size in config.json
"thread_pool_size": 2
```

## Next Steps

1. **Customize symbols**: Edit `config/exchanges.json` to add your preferred trading pairs
2. **Adjust parameters**: Modify `config/config.json` for profit thresholds and risk limits
3. **Monitor metrics**: Check `metrics_final.json` after shutdown for detailed statistics
4. **Set up as service**: Use the systemd service for production deployment

## Quick Commands Reference

```bash
# Start engine
./scripts/quick_start.sh

# View logs
tail -f logs/arbitrage.log

# Stop engine
Ctrl+C

# Clean build
rm -rf build && mkdir build

# Full rebuild
cd build && cmake .. && make -j$(nproc)
```

## Support

- Check logs in `logs/` directory for detailed error messages
- Ensure all dependencies are installed with `pkg-config --list-all | grep -E "spdlog|rapidjson|tbb"`
- Verify exchange connectivity with `ping ws.okx.com`