cmake_minimum_required(VERSION 3.20)
project(affix VERSION 0.2.0 LANGUAGES CXX)

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
add_library(affix
    src/topology/topology.cpp
    src/placement/placement.cpp
    src/this_thread/this_thread.cpp
    src/observability/observability.cpp
    src/numa/numa.cpp
    src/warmup/warmup.cpp
    src/advisor/advisor.cpp
    ${PLATFORM_SOURCES}
)

add_library(affix::affix ALIAS affix)
add_library(corepilot::corepilot ALIAS affix) # Legacy alias

target_include_directories(affix
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)
target_link_libraries(affix PUBLIC Threads::Threads)

# CLI Tool
add_executable(affix-cli tools/affix_cli.cpp)
target_link_libraries(affix-cli PRIVATE affix::affix)

# Unit Tests
enable_testing()
add_executable(affix-tests tests/main.cpp)
target_link_libraries(affix-tests PRIVATE affix::affix)
add_test(NAME AffixTests COMMAND affix-tests)

# Benchmark Suite
add_executable(affix-bench benchmarks/bench_contention.cpp)
target_link_libraries(affix-bench PRIVATE affix::affix)

# Install targets for standard distribution
include(GNUInstallDirs)
install(TARGETS affix affix-cli
    EXPORT affixTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/affix DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
