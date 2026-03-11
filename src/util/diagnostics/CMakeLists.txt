# Diagnostics subsystem
# Provides diagnostic event model, session, sinks, and ledgers

option(GOGGLES_DIAGNOSTICS_FORENSIC "Enable forensic diagnostics instrumentation" OFF)

add_library(goggles_diagnostics OBJECT
    log_sink.cpp
    test_harness_sink.cpp
    diagnostic_session.cpp
    gpu_timestamp_pool.cpp
)

if(GOGGLES_DIAGNOSTICS_FORENSIC)
    target_compile_definitions(goggles_diagnostics PUBLIC
        GOGGLES_DIAGNOSTICS_FORENSIC
    )
endif()

if(DEFINED GOGGLES_CHAIN_LIBRARY_TYPE)
    string(TOUPPER "${GOGGLES_CHAIN_LIBRARY_TYPE}" GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED)
    if(GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
        set_target_properties(goggles_diagnostics PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endif()

target_include_directories(goggles_diagnostics PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(goggles_diagnostics PUBLIC
    goggles_util
    Vulkan::Vulkan
)

goggles_enable_clang_tidy(goggles_diagnostics)
goggles_enable_sanitizers(goggles_diagnostics)
goggles_enable_profiling(goggles_diagnostics)
