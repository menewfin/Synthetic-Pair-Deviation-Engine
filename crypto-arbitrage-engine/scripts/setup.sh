#!/bin/bash

# Crypto Arbitrage Engine Setup Script
# This script automates the installation of dependencies and building the project

set -e  # Exit on error

echo "=== Crypto Arbitrage Engine Setup ==="
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

# Check if running on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    print_error "This script is designed for Linux systems only"
    exit 1
fi

# Check for root/sudo
if [[ $EUID -eq 0 ]]; then
   print_warning "Running as root is not recommended. Continue anyway? (y/n)"
   read -r response
   if [[ "$response" != "y" ]]; then
       exit 1
   fi
fi

# Update package list
print_status "Updating package list..."
sudo apt-get update

# Install build tools
print_status "Installing build tools..."
sudo apt-get install -y build-essential cmake git pkg-config

# Check GCC version
GCC_VERSION=$(gcc -dumpversion | cut -d. -f1)
if [[ $GCC_VERSION -lt 9 ]]; then
    print_error "GCC version 9+ required. Current version: $GCC_VERSION"
    print_warning "Installing GCC-9..."
    sudo apt-get install -y gcc-9 g++-9
    sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 90
    sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 90
fi

# Install required libraries
print_status "Installing required libraries..."
sudo apt-get install -y \
    libssl-dev \
    libboost-all-dev \
    libtbb-dev \
    libspdlog-dev \
    rapidjson-dev \
    libbenchmark-dev

# Check for AVX2 support
if grep -q avx2 /proc/cpuinfo; then
    print_status "AVX2 support detected"
else
    print_warning "AVX2 support not detected. SIMD optimizations will be disabled."
fi

# Install websocketpp if not already installed
if ! pkg-config --exists websocketpp; then
    print_status "Installing websocketpp..."
    cd /tmp
    git clone https://github.com/zaphoyd/websocketpp.git
    cd websocketpp
    mkdir -p build && cd build
    cmake ..
    sudo make install
    cd /
    rm -rf /tmp/websocketpp
else
    print_status "websocketpp already installed"
fi

# Create project directories
print_status "Creating project directories..."
mkdir -p logs
mkdir -p data
mkdir -p build

# Build the project
print_status "Building the project..."
cd build

# Configure with CMake
if [[ -f CMakeCache.txt ]]; then
    print_warning "CMake cache found. Cleaning build directory..."
    rm -rf *
fi

cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-march=native"

# Build with all available cores
CORES=$(nproc)
print_status "Building with $CORES cores..."
make -j$CORES

if [[ $? -eq 0 ]]; then
    print_status "Build completed successfully!"
else
    print_error "Build failed!"
    exit 1
fi

# Create default config files if they don't exist
cd ..
if [[ ! -f config/config.json ]]; then
    print_status "Creating default config.json..."
    cat > config/config.json << 'EOF'
{
  "system": {
    "thread_pool_size": 8,
    "order_book_depth": 20,
    "log_level": "info",
    "log_file": "logs/arbitrage.log",
    "metrics_export_interval_seconds": 300
  },
  "arbitrage": {
    "min_profit_threshold": 10.0,
    "max_position_size": 100000.0,
    "max_portfolio_exposure": 1000000.0,
    "min_liquidity_usd": 10000.0,
    "max_execution_time_ms": 100
  },
  "risk": {
    "max_var_95": 50000.0,
    "max_concentration": 0.3,
    "min_sharpe_ratio": 1.5,
    "stress_test_scenarios": 100
  }
}
EOF
fi

if [[ ! -f config/exchanges.json ]]; then
    print_status "Creating default exchanges.json..."
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
        "spot": ["BTC-USDT", "ETH-USDT", "SOL-USDT"],
        "perpetual": ["BTC-USDT-SWAP", "ETH-USDT-SWAP", "SOL-USDT-SWAP"],
        "futures": ["BTC-USD-231229", "ETH-USD-231229"]
      },
      "reconnect_interval_ms": 5000,
      "heartbeat_interval_ms": 30000,
      "rate_limit": {
        "requests_per_second": 20,
        "burst_size": 50
      }
    },
    {
      "name": "BINANCE",
      "enabled": true,
      "ws_endpoints": {
        "public": "wss://stream.binance.com:9443/ws"
      },
      "symbols": {
        "spot": ["BTCUSDT", "ETHUSDT", "SOLUSDT"],
        "perpetual": ["BTCUSDT", "ETHUSDT", "SOLUSDT"]
      },
      "reconnect_interval_ms": 5000,
      "heartbeat_interval_ms": 180000,
      "rate_limit": {
        "requests_per_second": 10,
        "burst_size": 20
      }
    },
    {
      "name": "BYBIT",
      "enabled": true,
      "ws_endpoints": {
        "public": "wss://stream.bybit.com/v5/public/spot"
      },
      "symbols": {
        "spot": ["BTCUSDT", "ETHUSDT", "SOLUSDT"],
        "perpetual": ["BTCUSDT", "ETHUSDT", "SOLUSDT"]
      },
      "reconnect_interval_ms": 5000,
      "heartbeat_interval_ms": 20000,
      "rate_limit": {
        "requests_per_second": 10,
        "burst_size": 30
      }
    }
  ]
}
EOF
fi

# Set up systemd service (optional)
print_warning "Do you want to set up a systemd service? (y/n)"
read -r response
if [[ "$response" == "y" ]]; then
    print_status "Creating systemd service..."
    
    CURRENT_DIR=$(pwd)
    SERVICE_FILE="/tmp/crypto-arbitrage.service"
    
    cat > $SERVICE_FILE << EOF
[Unit]
Description=Crypto Arbitrage Detection Engine
After=network.target

[Service]
Type=simple
User=$USER
WorkingDirectory=$CURRENT_DIR
ExecStart=$CURRENT_DIR/build/crypto-arbitrage-engine
Restart=on-failure
RestartSec=10
StandardOutput=append:$CURRENT_DIR/logs/service.log
StandardError=append:$CURRENT_DIR/logs/service_error.log

# Resource limits
LimitNOFILE=65536
LimitMEMLOCK=infinity

# CPU affinity (adjust based on your system)
CPUAffinity=0-7

[Install]
WantedBy=multi-user.target
EOF

    sudo mv $SERVICE_FILE /etc/systemd/system/crypto-arbitrage.service
    sudo systemctl daemon-reload
    print_status "Systemd service created. Use 'sudo systemctl start crypto-arbitrage' to start"
fi

# Performance tuning suggestions
echo
print_status "Setup complete!"
echo
echo "Performance tuning suggestions:"
echo "1. Add to /etc/security/limits.conf:"
echo "   * soft memlock unlimited"
echo "   * hard memlock unlimited"
echo "   * soft nofile 65536"
echo "   * hard nofile 65536"
echo
echo "2. Add to /etc/sysctl.conf:"
echo "   net.core.rmem_max = 134217728"
echo "   net.core.wmem_max = 134217728"
echo
echo "To run the engine:"
echo "   cd build"
echo "   ./crypto-arbitrage-engine"
echo
print_status "Happy arbitraging!"