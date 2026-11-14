cmake_minimum_required(VERSION 3.20)
project(corepilot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings and options
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

# Platform specific source selection
set(PLATFORM_SOURCES)
if(APPLE)
    list(APPEND PLATFORM_SOURCES src/platform/platform_darwin.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES src/platform/platform_linux.cpp)
endif()

# Core library
add_library(corepilot
    src/topology/topology.cpp
    src/placement/placement.cpp
    src/this_thread/this_thread.cpp
    src/observability/observability.cpp
    ${PLATFORM_SOURCES}
)

add_library(corepilot::corepilot ALIAS corepilot)

target_include_directories(corepilot
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)
target_link_libraries(corepilot PUBLIC Threads::Threads)

# CLI Tool
add_executable(corepilot-cli tools/corepilot_cli.cpp)
target_link_libraries(corepilot-cli PRIVATE corepilot::corepilot)

# Unit Tests
enable_testing()
add_executable(corepilot-tests tests/main.cpp)
target_link_libraries(corepilot-tests PRIVATE corepilot::corepilot)
add_test(NAME CorePilotTests COMMAND corepilot-tests)

# Benchmark Suite
add_executable(corepilot-bench benchmarks/bench_contention.cpp)
target_link_libraries(corepilot-bench PRIVATE corepilot::corepilot)

# Install targets for standard distribution
include(GNUInstallDirs)
install(TARGETS corepilot corepilot-cli
    EXPORT corepilotTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/corepilot DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
