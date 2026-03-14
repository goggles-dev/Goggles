cmake_minimum_required(VERSION 3.20)
project(FCStaticConsumer LANGUAGES CXX)

find_package(GogglesFilterChain CONFIG REQUIRED)

add_executable(static_consumer main.cpp)
target_link_libraries(static_consumer PRIVATE GogglesFilterChain::goggles-filter-chain)
target_compile_features(static_consumer PRIVATE cxx_std_20)
