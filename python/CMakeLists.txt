# Python bindings for PixelForge.
#
# Two parallel modules are built so we can compare/contrast binding libraries:
#   - pybind11_module/  -> _pixelforge.<so|pyd>
#   - nanobind_module/  -> _pixelforge_nb.<so|pyd>
#
# Both are gated on whether the matching helper library can be fetched.
# The Python interpreter is required either way.

option(PIXELFORGE_BUILD_PYBIND11 "Build pybind11 Python module" ON)
option(PIXELFORGE_BUILD_NANOBIND "Build nanobind Python module" ON)

# Use modern FindPython (creates Python::Module which nanobind needs).
# Force pybind11 to share the same Python interpreter via PYBIND11_FINDPYTHON.
#
# On Windows we may have multiple Pythons (a "headless" C:\Python313 from the
# Python.org installer with components stripped, plus the Microsoft Store
# Python that actually has pip/numpy/pytest). Probe well-known locations and
# pin to the first one whose interpreter actually has a working stdlib.
if(WIN32 AND NOT DEFINED CACHE{Python_EXECUTABLE})
    set(_pf_py_candidates
        "$ENV{LOCALAPPDATA}/Microsoft/WindowsApps/PythonSoftwareFoundation.Python.3.13_qbz5n2kfra8p0/python.exe"
        "$ENV{LOCALAPPDATA}/Microsoft/WindowsApps/python.exe"
    )
    foreach(_cand IN LISTS _pf_py_candidates)
        if(EXISTS "${_cand}")
            execute_process(COMMAND "${_cand}" -c "import sys, sysconfig; sys.exit(0 if sysconfig.get_paths().get('stdlib') else 1)"
                            RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
            if(_rc EQUAL 0)
                set(Python_EXECUTABLE "${_cand}" CACHE FILEPATH "Python interpreter for PixelForge bindings")
                message(STATUS "PixelForge: pinned Python_EXECUTABLE=${Python_EXECUTABLE}")
                break()
            endif()
        endif()
    endforeach()
endif()

find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module)
set(PYBIND11_FINDPYTHON ON CACHE BOOL "Use modern FindPython in pybind11" FORCE)

message(STATUS "PixelForge Python: ${Python_EXECUTABLE} (${Python_VERSION})")

include(FetchContent)

if(PIXELFORGE_BUILD_PYBIND11)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.13.6
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(pybind11)
    add_subdirectory(pybind11_module)
endif()

if(PIXELFORGE_BUILD_NANOBIND)
    FetchContent_Declare(
        nanobind
        GIT_REPOSITORY https://github.com/wjakob/nanobind.git
        GIT_TAG        v2.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nanobind)
    add_subdirectory(nanobind_module)
endif()
