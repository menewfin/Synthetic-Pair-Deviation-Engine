#!/bin/bash

# Quick Start Script for Crypto Arbitrage Engine
# For development and testing purposes

set -e

echo "=== Crypto Arbitrage Engine Quick Start ==="
echo

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Build directory not found. Running setup script first..."
    if [ -f "scripts/setup.sh" ]; then
        bash scripts/setup.sh
    else
        echo "Error: setup.sh not found!"
        exit 1
    fi
fi

# Check if executable exists
if [ ! -f "build/crypto-arbitrage-engine" ]; then
    echo "Executable not found. Building project..."
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    cd ..
fi

# Create logs directory if it doesn't exist
mkdir -p logs

# Check for config files
if [ ! -f "config/config.json" ] || [ ! -f "config/exchanges.json" ]; then
    echo "Config files not found. Creating defaults..."
    mkdir -p config
    
    # Create minimal config for testing
    cat > config/config.json << 'EOF'
{
  "system": {
    "thread_pool_size": 4,
    "order_book_depth": 10,
    "log_level": "debug",
    "log_file": "logs/arbitrage.log"
  },
  "arbitrage": {
    "min_profit_threshold": 5.0,
    "max_position_size": 50000.0,
    "max_portfolio_exposure": 500000.0
  }
}
EOF

    cat > config/exchanges.json << 'EOF'
{
  "exchanges": [
    {
      "name": "OKX",
      "enabled": true,
      "ws_endpoints": {
        "public": "wss://ws.okx.com:8443/ws/v5/public"
      },
      "symbols": {
        "spot": ["BTC-USDT"],
        "perpetual": ["BTC-USDT-SWAP"]
      },
      "reconnect_interval_ms": 5000,
      "heartbeat_interval_ms": 30000
    }
  ]
}
EOF
fi

# Display startup message
echo
echo "Starting Crypto Arbitrage Engine..."
echo "Config: config/config.json"
echo "Exchanges: config/exchanges.json"
echo "Logs: logs/arbitrage.log"
echo
echo "Press Ctrl+C to stop"
echo

# Run the engine
cd build
./crypto-arbitrage-engine ../config/config.json ../config/exchanges.json