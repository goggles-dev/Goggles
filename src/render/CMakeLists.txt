# Render subsystem

include(CMakePackageConfigHelpers)
include(${CMAKE_SOURCE_DIR}/cmake/GogglesFilterChainProvider.cmake)

add_subdirectory(backend)
add_subdirectory(chain)
add_subdirectory(shader)
add_subdirectory(texture)

set(GOGGLES_CHAIN_LIBRARY_TYPE "STATIC" CACHE STRING
    "Library type for goggles-filter-chain (STATIC or SHARED)")
set_property(CACHE GOGGLES_CHAIN_LIBRARY_TYPE PROPERTY STRINGS STATIC SHARED)

string(TOUPPER "${GOGGLES_CHAIN_LIBRARY_TYPE}" GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED)
if(NOT GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "STATIC" AND
   NOT GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
    message(FATAL_ERROR
        "Invalid GOGGLES_CHAIN_LIBRARY_TYPE='${GOGGLES_CHAIN_LIBRARY_TYPE}'. Use STATIC or SHARED.")
endif()

goggles_resolve_filter_chain_provider()

add_library(goggles_render STATIC
    $<TARGET_OBJECTS:goggles_render_backend_obj>
)

target_include_directories(goggles_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Link deps here so downstream users only need to link goggles_render.
target_link_libraries(goggles_render PUBLIC
    goggles_util
    Vulkan::Vulkan
    SDL3::SDL3
    goggles-filter-chain
)

target_compile_definitions(goggles_render PUBLIC
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

goggles_enable_clang_tidy(goggles_render)
goggles_enable_sanitizers(goggles_render)
goggles_enable_profiling(goggles_render)

install(FILES
    ${CMAKE_SOURCE_DIR}/src/render/chain/api/c/goggles_filter_chain.h
    ${CMAKE_SOURCE_DIR}/src/render/chain/api/cpp/goggles_filter_chain.hpp
    DESTINATION include
)

if(GOGGLES_FILTER_CHAIN_PROVIDER_OWNS_TARGET)
    install(FILES
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/error.hpp
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/filter_controls.hpp
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/result.hpp
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/scale_mode.hpp
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/vulkan_context.hpp
        DESTINATION include/goggles/filter_chain
    )

    install(FILES
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/diagnostics/compile_report.hpp
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/diagnostics/diagnostic_event.hpp
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/diagnostics/diagnostic_policy.hpp
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/diagnostics/diagnostic_session.hpp
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/diagnostics/diagnostic_sink.hpp
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/diagnostics/gpu_timestamp_pool.hpp
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/diagnostics/log_sink.hpp
        ${CMAKE_SOURCE_DIR}/src/render/chain/include/goggles/filter_chain/diagnostics/source_provenance.hpp
        DESTINATION include/goggles/filter_chain/diagnostics
    )

    install(FILES ${CMAKE_SOURCE_DIR}/src/util/error.hpp
        DESTINATION include/util
    )

    install(TARGETS goggles-filter-chain
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )

    set(GOGGLES_FILTER_CHAIN_INSTALL_CMAKEDIR "lib/cmake/${GOGGLES_FILTER_CHAIN_PACKAGE}")
    if(GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
        set(GOGGLES_FILTER_CHAIN_SHARED_DEFINE ";GOGGLES_CHAIN_USE_SHARED")
    else()
        set(GOGGLES_FILTER_CHAIN_SHARED_DEFINE "")
    endif()

    configure_package_config_file(
        ${CMAKE_SOURCE_DIR}/cmake/GogglesFilterChainConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${GOGGLES_FILTER_CHAIN_PACKAGE}Config.cmake
        INSTALL_DESTINATION ${GOGGLES_FILTER_CHAIN_INSTALL_CMAKEDIR}
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${GOGGLES_FILTER_CHAIN_PACKAGE}ConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${GOGGLES_FILTER_CHAIN_PACKAGE}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${GOGGLES_FILTER_CHAIN_PACKAGE}ConfigVersion.cmake
        DESTINATION ${GOGGLES_FILTER_CHAIN_INSTALL_CMAKEDIR}
    )
endif()
