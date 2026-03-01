find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(GPU_PYTEST_SUITE "${CMAKE_SOURCE_DIR}/tests/gpu")

add_test(NAME goggles_gpu_pytest
         COMMAND "${Python3_EXECUTABLE}" -m pytest "${GPU_PYTEST_SUITE}" -v)

set_tests_properties(goggles_gpu_pytest PROPERTIES
    LABELS "gpu"
    TIMEOUT 120
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
