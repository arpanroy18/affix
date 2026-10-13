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
