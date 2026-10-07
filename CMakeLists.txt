cmake_minimum_required(VERSION 3.16)
project(binance_capture LANGUAGES CXX)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)  # newer CMake: use BoostConfig.cmake
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BINANCE_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

add_library(project_options INTERFACE)
target_compile_options(project_options INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
if(BINANCE_SANITIZE)
  target_compile_options(project_options INTERFACE
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
  target_link_options(project_options INTERFACE -fsanitize=address,undefined)
endif()

# Core library: parsing, CSV, book, processor, replay. No network dependency,
# so the unit tests build quickly and never touch the network.
add_library(binance_core STATIC
  src/decimal.cpp
  src/csv_writer.cpp
  src/csv_reader.cpp
  src/output_file.cpp
)
target_include_directories(binance_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(binance_core PUBLIC project_options)

enable_testing()
add_subdirectory(tests)
