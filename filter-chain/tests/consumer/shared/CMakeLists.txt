cmake_minimum_required(VERSION 3.20)
project(FCSharedConsumer LANGUAGES CXX)

find_package(GogglesFilterChain CONFIG REQUIRED)

add_executable(shared_consumer main.cpp)
target_link_libraries(shared_consumer PRIVATE GogglesFilterChain::goggles-filter-chain)
target_compile_features(shared_consumer PRIVATE cxx_std_20)
