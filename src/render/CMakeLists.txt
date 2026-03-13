# Render subsystem

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

add_library(goggles-filter-chain ${GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED}
    $<TARGET_OBJECTS:goggles_render_chain_obj>
    $<TARGET_OBJECTS:goggles_render_shader_obj>
    $<TARGET_OBJECTS:goggles_render_texture_obj>
    $<TARGET_OBJECTS:goggles_diagnostics>
    $<TARGET_OBJECTS:goggles_util_logging_obj>
)

if(GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
    set_target_properties(
        goggles_render_chain_obj
        goggles_render_shader_obj
        goggles_render_texture_obj
        PROPERTIES POSITION_INDEPENDENT_CODE ON)

    target_compile_definitions(goggles_render_chain_obj PRIVATE GOGGLES_CHAIN_BUILD_SHARED)
    target_compile_definitions(goggles_render_shader_obj PRIVATE GOGGLES_CHAIN_BUILD_SHARED)
    target_compile_definitions(goggles_render_texture_obj PRIVATE GOGGLES_CHAIN_BUILD_SHARED)
endif()

target_include_directories(goggles-filter-chain
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/render/chain/include
        ${CMAKE_SOURCE_DIR}/src/render/chain/api/c
        ${CMAKE_SOURCE_DIR}/src/render/chain/api/cpp
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/render
)

target_compile_definitions(goggles-filter-chain PRIVATE
    $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:GOGGLES_CHAIN_BUILD_SHARED>
)

if(GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
    target_compile_definitions(goggles-filter-chain INTERFACE GOGGLES_CHAIN_USE_SHARED)
endif()

target_compile_definitions(goggles-filter-chain PUBLIC
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

target_link_libraries(goggles-filter-chain
    PUBLIC
        Vulkan::Vulkan
        nonstd::expected-lite
    PRIVATE
        spdlog::spdlog
        slang::slang
        stb_image
)

goggles_enable_clang_tidy(goggles-filter-chain)
goggles_enable_sanitizers(goggles-filter-chain)
goggles_enable_profiling(goggles-filter-chain)

if(NOT TARGET GogglesFilterChain::goggles-filter-chain)
    add_library(GogglesFilterChain::goggles-filter-chain ALIAS goggles-filter-chain)
endif()

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
