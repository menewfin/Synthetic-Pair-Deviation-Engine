# CMake generated Testfile for 
# Source directory: /workspace/crypto-arbitrage-engine/build/_deps/rapidjson-src/test/unittest
# Build directory: /workspace/crypto-arbitrage-engine/build/_deps/rapidjson-build/test/unittest
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unittest "/workspace/crypto-arbitrage-engine/build/bin/unittest")
set_tests_properties(unittest PROPERTIES  WORKING_DIRECTORY "/workspace/crypto-arbitrage-engine/bin" _BACKTRACE_TRIPLES "/workspace/crypto-arbitrage-engine/build/_deps/rapidjson-src/test/unittest/CMakeLists.txt;76;add_test;/workspace/crypto-arbitrage-engine/build/_deps/rapidjson-src/test/unittest/CMakeLists.txt;0;")
add_test(valgrind_unittest "valgrind" "--leak-check=full" "--error-exitcode=1" "/workspace/crypto-arbitrage-engine/build/bin/unittest" "--gtest_filter=-SIMD.*")
set_tests_properties(valgrind_unittest PROPERTIES  WORKING_DIRECTORY "/workspace/crypto-arbitrage-engine/bin" _BACKTRACE_TRIPLES "/workspace/crypto-arbitrage-engine/build/_deps/rapidjson-src/test/unittest/CMakeLists.txt;82;add_test;/workspace/crypto-arbitrage-engine/build/_deps/rapidjson-src/test/unittest/CMakeLists.txt;0;")
