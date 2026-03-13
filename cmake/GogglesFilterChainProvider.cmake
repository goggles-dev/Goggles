include_guard(GLOBAL)

set(GOGGLES_FILTER_CHAIN_PROVIDER "in-tree" CACHE STRING
    "Provider mode for goggles-filter-chain (in-tree, subdir, or package)")
set_property(CACHE GOGGLES_FILTER_CHAIN_PROVIDER PROPERTY STRINGS in-tree subdir package)

set(GOGGLES_FILTER_CHAIN_SOURCE_DIR "" CACHE PATH
    "External source tree when GOGGLES_FILTER_CHAIN_PROVIDER=subdir")
set(GOGGLES_FILTER_CHAIN_PACKAGE "goggles-filter-chain" CACHE STRING
    "Package name when GOGGLES_FILTER_CHAIN_PROVIDER=package")

function(_goggles_filter_chain_validate_provider out_mode)
    string(TOLOWER "${GOGGLES_FILTER_CHAIN_PROVIDER}" provider_mode)
    if(NOT provider_mode STREQUAL "in-tree" AND
       NOT provider_mode STREQUAL "subdir" AND
       NOT provider_mode STREQUAL "package")
        message(FATAL_ERROR
            "Invalid GOGGLES_FILTER_CHAIN_PROVIDER='${GOGGLES_FILTER_CHAIN_PROVIDER}'. "
            "Use in-tree, subdir, or package.")
    endif()
    set(${out_mode} "${provider_mode}" PARENT_SCOPE)
endfunction()

function(_goggles_filter_chain_candidate_targets out_targets)
    set(candidates
        goggles-filter-chain
        goggles_filter_chain
        GogglesFilterChain::goggles-filter-chain
        GogglesFilterChain::goggles_filter_chain
        "${GOGGLES_FILTER_CHAIN_PACKAGE}::goggles-filter-chain"
        "${GOGGLES_FILTER_CHAIN_PACKAGE}::goggles_filter_chain"
        "${GOGGLES_FILTER_CHAIN_PACKAGE}::filter-chain"
        "${GOGGLES_FILTER_CHAIN_PACKAGE}::filter_chain")

    set(found_targets)
    foreach(candidate IN LISTS candidates)
        if(TARGET "${candidate}")
            list(APPEND found_targets "${candidate}")
        endif()
    endforeach()

    get_property(global_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(candidate IN LISTS global_targets)
        if(candidate MATCHES "(^|::)goggles[-_]filter[-_]chain$")
            list(APPEND found_targets "${candidate}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES found_targets)
    set(${out_targets} "${found_targets}" PARENT_SCOPE)
endfunction()

function(_goggles_filter_chain_normalize_target resolved_target)
    if("${resolved_target}" STREQUAL "goggles-filter-chain")
        if(NOT TARGET GogglesFilterChain::goggles-filter-chain)
            add_library(GogglesFilterChain::goggles-filter-chain ALIAS goggles-filter-chain)
        endif()
        return()
    endif()

    if(TARGET goggles-filter-chain)
        return()
    endif()

    add_library(goggles-filter-chain INTERFACE)
    target_include_directories(goggles-filter-chain INTERFACE
        $<TARGET_PROPERTY:${resolved_target},INTERFACE_INCLUDE_DIRECTORIES>
    )
    target_compile_definitions(goggles-filter-chain INTERFACE
        $<TARGET_PROPERTY:${resolved_target},INTERFACE_COMPILE_DEFINITIONS>
    )
    target_link_libraries(goggles-filter-chain INTERFACE "${resolved_target}")
endfunction()

function(_goggles_filter_chain_enable_in_tree)
    if(TARGET goggles-filter-chain)
        return()
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
            goggles_diagnostics
            PROPERTIES POSITION_INDEPENDENT_CODE ON)

        target_compile_definitions(
            goggles_render_chain_obj
            goggles_render_shader_obj
            goggles_render_texture_obj
            PRIVATE GOGGLES_CHAIN_BUILD_SHARED)
    endif()

    target_include_directories(goggles-filter-chain
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/render/chain/include>
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/render/chain/api/c>
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/render/chain/api/cpp>
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
            $<INSTALL_INTERFACE:include>
        PRIVATE
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
endfunction()

function(goggles_resolve_filter_chain_provider)
    _goggles_filter_chain_validate_provider(provider_mode)

    if(provider_mode STREQUAL "in-tree")
        _goggles_filter_chain_enable_in_tree()
        set(GOGGLES_FILTER_CHAIN_PROVIDER_OWNS_TARGET TRUE PARENT_SCOPE)
        return()
    endif()

    if(provider_mode STREQUAL "subdir")
        if(GOGGLES_FILTER_CHAIN_SOURCE_DIR STREQUAL "")
            message(FATAL_ERROR
                "GOGGLES_FILTER_CHAIN_SOURCE_DIR must be set when "
                "GOGGLES_FILTER_CHAIN_PROVIDER=subdir")
        endif()

        if(NOT IS_DIRECTORY "${GOGGLES_FILTER_CHAIN_SOURCE_DIR}")
            message(FATAL_ERROR
                "GOGGLES_FILTER_CHAIN_SOURCE_DIR does not exist: "
                "${GOGGLES_FILTER_CHAIN_SOURCE_DIR}")
        endif()

        add_subdirectory(
            "${GOGGLES_FILTER_CHAIN_SOURCE_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}/providers/goggles-filter-chain")
    else()
        find_package(${GOGGLES_FILTER_CHAIN_PACKAGE} CONFIG REQUIRED)
    endif()

    _goggles_filter_chain_candidate_targets(candidate_targets)
    if(candidate_targets STREQUAL "")
        message(FATAL_ERROR
            "Filter-chain provider '${provider_mode}' did not expose a recognizable target. "
            "Expected goggles-filter-chain or a namespaced equivalent.")
    endif()

    list(GET candidate_targets 0 resolved_target)
    _goggles_filter_chain_normalize_target("${resolved_target}")

    if(NOT TARGET goggles-filter-chain)
        message(FATAL_ERROR "Filter-chain provider did not supply target goggles-filter-chain")
    endif()

    set(GOGGLES_FILTER_CHAIN_PROVIDER_OWNS_TARGET FALSE PARENT_SCOPE)
endfunction()
