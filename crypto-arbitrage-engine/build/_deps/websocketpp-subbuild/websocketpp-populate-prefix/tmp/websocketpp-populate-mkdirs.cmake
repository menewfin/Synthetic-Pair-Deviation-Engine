# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/workspace/crypto-arbitrage-engine/build/_deps/websocketpp-src")
  file(MAKE_DIRECTORY "/workspace/crypto-arbitrage-engine/build/_deps/websocketpp-src")
endif()
file(MAKE_DIRECTORY
  "/workspace/crypto-arbitrage-engine/build/_deps/websocketpp-build"
  "/workspace/crypto-arbitrage-engine/build/_deps/websocketpp-subbuild/websocketpp-populate-prefix"
  "/workspace/crypto-arbitrage-engine/build/_deps/websocketpp-subbuild/websocketpp-populate-prefix/tmp"
  "/workspace/crypto-arbitrage-engine/build/_deps/websocketpp-subbuild/websocketpp-populate-prefix/src/websocketpp-populate-stamp"
  "/workspace/crypto-arbitrage-engine/build/_deps/websocketpp-subbuild/websocketpp-populate-prefix/src"
  "/workspace/crypto-arbitrage-engine/build/_deps/websocketpp-subbuild/websocketpp-populate-prefix/src/websocketpp-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/workspace/crypto-arbitrage-engine/build/_deps/websocketpp-subbuild/websocketpp-populate-prefix/src/websocketpp-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/workspace/crypto-arbitrage-engine/build/_deps/websocketpp-subbuild/websocketpp-populate-prefix/src/websocketpp-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
