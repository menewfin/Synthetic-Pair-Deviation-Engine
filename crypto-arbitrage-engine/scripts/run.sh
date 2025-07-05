#!/bin/bash

# Crypto Arbitrage Engine Run Script
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Starting Crypto Arbitrage Engine${NC}"

# Check if executable exists
if [ ! -f "arbitrage_engine" ]; then
    echo -e "${RED}Error: arbitrage_engine executable not found.${NC}"
    echo -e "${YELLOW}Please run the build script first:${NC}"
    echo -e "  cd .."
    echo -e "  ./scripts/build.sh"
    exit 1
fi

# Check if config files exist
if [ ! -d "config" ]; then
    echo -e "${YELLOW}Config directory not found, copying from source...${NC}"
    cp -r ../config .
fi

# Create logs directory
mkdir -p logs

# Set environment variables
export TBB_NUM_THREADS=${TBB_NUM_THREADS:-$(nproc)}
export OMP_NUM_THREADS=${OMP_NUM_THREADS:-$(nproc)}

# Check for required dependencies
echo -e "${YELLOW}Checking system requirements...${NC}"

# Check available memory
AVAILABLE_MEM=$(free -m | awk 'NR==2{printf "%.1f", $7/1024}')
echo -e "Available memory: ${AVAILABLE_MEM}GB"

if (( $(echo "${AVAILABLE_MEM} < 4.0" | bc -l) )); then
    echo -e "${YELLOW}Warning: Less than 4GB available memory. Performance may be affected.${NC}"
fi

# Check CPU features
if ! grep -q avx2 /proc/cpuinfo; then
    echo -e "${YELLOW}Warning: AVX2 not detected. SIMD optimizations may not work optimally.${NC}"
fi

# Start the engine
echo -e "${GREEN}Starting arbitrage engine...${NC}"
echo -e "${YELLOW}Press Ctrl+C to stop${NC}"
echo -e "==========================================\n"

# Run with nice priority for real-time performance
exec nice -n -10 ./arbitrage_engine "$@"