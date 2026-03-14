cmake_minimum_required(VERSION 3.20)
project(goggles VERSION 0.1.0 LANGUAGES CXX)

# Project-wide versioning and naming (single source of truth: project(VERSION ...) above)
add_compile_definitions(GOGGLES_VERSION="${PROJECT_VERSION}")
add_compile_definitions(GOGGLES_VERSION_MAJOR=${PROJECT_VERSION_MAJOR})
add_compile_definitions(GOGGLES_VERSION_MINOR=${PROJECT_VERSION_MINOR})
add_compile_definitions(GOGGLES_VERSION_PATCH=${PROJECT_VERSION_PATCH})
add_compile_definitions(GOGGLES_PROJECT_NAME="Goggles")

# Load modular configuration
include(cmake/Dependencies.cmake)
include(cmake/CompilerConfig.cmake)
include(cmake/CodeQuality.cmake)

option(GOGGLES_USE_BUNDLED_FILTER_CHAIN
    "Build filter-chain from the in-repo subdirectory instead of using find_package()"
    OFF)

if(GOGGLES_USE_BUNDLED_FILTER_CHAIN)
    add_subdirectory(filter-chain ${CMAKE_BINARY_DIR}/filter-chain)
else()
    find_package(GogglesFilterChain CONFIG REQUIRED)
endif()

# Project structure
add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
