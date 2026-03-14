# Enforce C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

add_compile_options(
    -Wall
    -Wextra
    -Wshadow
    -Wconversion
)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_PROFILING "Enable Tracy profiler integration" OFF)

add_library(fc_sanitizer_options INTERFACE)
add_library(fc_profiling_options INTERFACE)

if(ENABLE_ASAN)
    target_compile_options(fc_sanitizer_options INTERFACE
        -fsanitize=address
        -fno-omit-frame-pointer
    )
    target_link_options(fc_sanitizer_options INTERFACE
        -fsanitize=address
    )
endif()

if(ENABLE_UBSAN)
    target_compile_options(fc_sanitizer_options INTERFACE
        -fsanitize=undefined
    )
    target_link_options(fc_sanitizer_options INTERFACE
        -fsanitize=undefined
    )
endif()

if(ENABLE_PROFILING)
    find_package(Tracy CONFIG REQUIRED)
    target_compile_definitions(fc_profiling_options INTERFACE TRACY_ENABLE)
endif()

function(goggles_enable_sanitizers target_name)
    target_link_libraries(${target_name} PRIVATE fc_sanitizer_options)
endfunction()

function(goggles_enable_profiling target_name)
    target_link_libraries(${target_name} PRIVATE fc_profiling_options)
    if(ENABLE_PROFILING)
        target_link_libraries(${target_name} PRIVATE Tracy::TracyClient)
    endif()
endfunction()

function(goggles_enable_pic_if_shared target_name)
    if(FILTER_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
        set_target_properties(${target_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endfunction()
