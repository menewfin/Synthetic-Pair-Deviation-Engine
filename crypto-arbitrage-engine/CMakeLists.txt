cmake_minimum_required(VERSION 3.16)
project(CryptoArbitrageEngine VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -O3 -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

# Enable SIMD instructions
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma")

# Find packages
find_package(Threads REQUIRED)
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)

# External libraries (using FetchContent)
include(FetchContent)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# rapidjson
FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG v1.1.0
)
FetchContent_MakeAvailable(rapidjson)

# websocketpp
FetchContent_Declare(
    websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG 0.8.2
)
FetchContent_MakeAvailable(websocketpp)

# Intel TBB
FetchContent_Declare(
    tbb
    GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
    GIT_TAG v2021.10.0
)
FetchContent_MakeAvailable(tbb)

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${rapidjson_SOURCE_DIR}/include
    ${websocketpp_SOURCE_DIR}
)

# Source files
set(CORE_SOURCES
    src/main.cpp
    src/utils/logger.cpp
    src/exchange/exchange_base.cpp
    src/exchange/okx/okx_websocket.cpp
    src/exchange/binance/binance_websocket.cpp
    src/exchange/bybit/bybit_websocket.cpp
    src/market_data/order_book.cpp
    src/market_data/market_data_manager.cpp
    src/synthetic/synthetic_pricer.cpp
    src/synthetic/futures_pricer.cpp
    src/synthetic/perpetual_pricer.cpp
    src/arbitrage/arbitrage_detector.cpp
    src/risk/risk_manager.cpp
    src/performance/metrics_collector.cpp
)

# Main executable
add_executable(arbitrage_engine ${CORE_SOURCES})

# Link libraries
target_link_libraries(arbitrage_engine
    PRIVATE
    Threads::Threads
    Boost::system
    Boost::thread
    spdlog::spdlog
    TBB::tbb
)

# Enable Link Time Optimization
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
    set_property(TARGET arbitrage_engine PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Tests
enable_testing()
add_subdirectory(tests)

# Installation
install(TARGETS arbitrage_engine DESTINATION bin)
install(DIRECTORY config/ DESTINATION etc/arbitrage_engine)