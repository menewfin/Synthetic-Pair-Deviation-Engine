#!/bin/bash

# Crypto Arbitrage Engine Build Script
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building Crypto Arbitrage Engine${NC}"

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}Error: CMakeLists.txt not found. Run this script from the project root.${NC}"
    exit 1
fi

# Create build directory
BUILD_TYPE=${1:-Release}
BUILD_DIR="build"

echo -e "${YELLOW}Build type: ${BUILD_TYPE}${NC}"

# Clean previous build if requested
if [ "$2" = "clean" ]; then
    echo -e "${YELLOW}Cleaning previous build...${NC}"
    rm -rf ${BUILD_DIR}
fi

# Create build directory
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# Configure cmake
echo -e "${YELLOW}Configuring CMake...${NC}"
cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DCMAKE_CXX_COMPILER=g++ \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      ..

# Build
echo -e "${YELLOW}Building...${NC}"
make -j$(nproc)

# Check if build was successful
if [ $? -eq 0 ]; then
    echo -e "${GREEN}Build completed successfully!${NC}"
    echo -e "${GREEN}Executable: ${BUILD_DIR}/arbitrage_engine${NC}"
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

# Copy config files to build directory
echo -e "${YELLOW}Copying configuration files...${NC}"
cp -r ../config .

echo -e "${GREEN}Build script completed!${NC}"
echo -e "${YELLOW}To run the engine:${NC}"
echo -e "  cd ${BUILD_DIR}"
echo -e "  ./arbitrage_engine"