# Tests use GoogleTest, fetched on demand so contributors don't need
# to install it system-wide.
include(FetchContent)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(pixelforge_core_tests
    test_image.cpp
    test_colorspace.cpp
    test_kernel.cpp
    test_version.cpp
)

target_link_libraries(pixelforge_core_tests
    PRIVATE
        pixelforge::core
        pixelforge::warnings
        GTest::gtest_main
)

# Tests for the shared runtime DLL (pipeline + I/O round-trip).
add_executable(pixelforge_runtime_tests
    test_pipeline.cpp
    test_io.cpp
    test_plugin_loader.cpp
)
target_link_libraries(pixelforge_runtime_tests
    PRIVATE
        pixelforge::pixelforge
        pixelforge::warnings
        GTest::gtest_main
)
target_compile_definitions(pixelforge_runtime_tests PRIVATE
    PIXELFORGE_TEST_TMP_DIR="${CMAKE_BINARY_DIR}/test_tmp"
    PIXELFORGE_TEST_PLUGIN_DIR="${CMAKE_BINARY_DIR}/plugins"
)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test_tmp")

# Make sure all built-in plugins are built before the runtime test runs,
# so the loader has something to discover.
add_dependencies(pixelforge_runtime_tests
    pixelforge::plugin_grayscale
    pixelforge::plugin_invert
    pixelforge::plugin_blur
    pixelforge::plugin_sharpen
    pixelforge::plugin_edge
)

# On Windows the runtime DLL must sit next to the test .exe at run time.
# Copy it after build so CTest can launch the executable directly.
if(WIN32)
    add_custom_command(TARGET pixelforge_runtime_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:pixelforge>
            $<TARGET_FILE_DIR:pixelforge_runtime_tests>
        VERBATIM
    )
endif()

include(GoogleTest)
gtest_discover_tests(pixelforge_core_tests    DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(pixelforge_runtime_tests DISCOVERY_MODE PRE_TEST)

# End-to-end CLI test: invoke the built pixelforge.exe on a tiny PNG and
# verify it produced an output file of the expected dimensions.
add_test(
    NAME    Cli.HelpAndVersion
    COMMAND $<TARGET_FILE:pixelforge_cli> --version
)
set_tests_properties(Cli.HelpAndVersion PROPERTIES
    PASS_REGULAR_EXPRESSION "^[0-9]+\\.[0-9]+\\.[0-9]+"
)

add_test(
    NAME    Cli.ListFilters
    COMMAND $<TARGET_FILE:pixelforge_cli> list-filters
            --plugin-dir "${CMAKE_BINARY_DIR}/plugins"
)
set_tests_properties(Cli.ListFilters PROPERTIES
    PASS_REGULAR_EXPRESSION "grayscale"
)

add_test(
    NAME    Cli.PipelineEndToEnd
    COMMAND ${CMAKE_COMMAND}
            -DPIXELFORGE_CLI=$<TARGET_FILE:pixelforge_cli>
            -DPLUGIN_DIR=${CMAKE_BINARY_DIR}/plugins
            -DTMP_DIR=${CMAKE_BINARY_DIR}/test_tmp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_smoke.cmake
)

# Python binding tests (pytest). The harness adds the build's
# python_modules dir to sys.path and to the DLL search path.
if(PIXELFORGE_BUILD_PYTHON AND TARGET _pixelforge)
    find_package(Python 3.10 REQUIRED COMPONENTS Interpreter)
    add_test(
        NAME    Python.Pybind11
        COMMAND ${Python_EXECUTABLE} -m pytest -v
                ${CMAKE_SOURCE_DIR}/python/tests
    )
    set_tests_properties(Python.Pybind11 PROPERTIES
        ENVIRONMENT "PIXELFORGE_PY_PATH=${CMAKE_BINARY_DIR}/python_modules;PIXELFORGE_PKG_ROOT=${CMAKE_SOURCE_DIR}/python;PIXELFORGE_PLUGIN_DIR=${CMAKE_BINARY_DIR}/plugins;PIXELFORGE_TMP_DIR=${CMAKE_BINARY_DIR}/test_tmp"
    )
endif()
