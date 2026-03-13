# Utility subsystem
# Provides error handling, logging, and configuration infrastructure

add_library(goggles_util_logging_obj OBJECT
    logging.cpp
)

target_include_directories(goggles_util_logging_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/src/render/chain/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(goggles_util_logging_obj PRIVATE
    spdlog::spdlog
)

goggles_enable_clang_tidy(goggles_util_logging_obj)
goggles_enable_sanitizers(goggles_util_logging_obj)
goggles_enable_profiling(goggles_util_logging_obj)

add_library(goggles_util STATIC
    $<TARGET_OBJECTS:goggles_util_logging_obj>
    config.cpp
    paths.cpp
    job_system.cpp
)

if(DEFINED GOGGLES_CHAIN_LIBRARY_TYPE)
    string(TOUPPER "${GOGGLES_CHAIN_LIBRARY_TYPE}" GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED)
    if(GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
        set_target_properties(goggles_util PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endif()

target_include_directories(goggles_util PUBLIC
    ${CMAKE_SOURCE_DIR}/src/render/chain/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependencies (provided by Pixi)
target_link_libraries(goggles_util PUBLIC
    nonstd::expected-lite
    spdlog::spdlog
    toml11::toml11
    BS_thread_pool
    Threads::Threads
)

goggles_enable_clang_tidy(goggles_util)
goggles_enable_sanitizers(goggles_util)
goggles_enable_profiling(goggles_util)

add_subdirectory(diagnostics)
