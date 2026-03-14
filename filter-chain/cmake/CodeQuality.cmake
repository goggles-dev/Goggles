option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
    set(GOGGLES_FILTER_CHAIN_CLANG_TIDY_CONFIG
        ${CLANG_TIDY_EXE}
        "--header-filter=${CMAKE_CURRENT_SOURCE_DIR}/(include|src)/.*"
        "--exclude-header-filter=${CMAKE_CURRENT_SOURCE_DIR}/include/goggles_filter_chain\\.h"
    )

    function(goggles_enable_clang_tidy target_name)
        set_target_properties(${target_name} PROPERTIES
            CXX_CLANG_TIDY "${GOGGLES_FILTER_CHAIN_CLANG_TIDY_CONFIG}")
    endfunction()
else()
    function(goggles_enable_clang_tidy target_name)
    endfunction()
endif()
