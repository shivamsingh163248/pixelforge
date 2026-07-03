# Centralised compiler-warning policy. Apply by linking the
# pixelforge::warnings INTERFACE target into any of our own targets.
add_library(pixelforge_warnings INTERFACE)
add_library(pixelforge::warnings ALIAS pixelforge_warnings)

# Internal-only target: it must appear in the same export set as anything
# that links to it, otherwise install(EXPORT ...) fails. We export it as
# an empty INTERFACE so downstream consumers don't inherit our flags.
if(NOT DEFINED SKBUILD_PROJECT_NAME)
    install(TARGETS pixelforge_warnings EXPORT PixelForgeTargets)
endif()

if(MSVC)
    target_compile_options(pixelforge_warnings INTERFACE
        /W4 /permissive- /Zc:__cplusplus /Zc:preprocessor /utf-8
    )
else()
    target_compile_options(pixelforge_warnings INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wsign-conversion
        -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Woverloaded-virtual -Wdouble-promotion
    )
endif()

# Optional sanitizers (enabled by nightly CI). Comma-separated list, e.g.:
#   -DPIXELFORGE_SANITIZERS=address,undefined
#   -DPIXELFORGE_SANITIZERS=thread
# Only honored on GCC/Clang; MSVC has its own /fsanitize=address handled separately.
set(PIXELFORGE_SANITIZERS "" CACHE STRING "Comma-separated sanitizers: address,undefined,thread,leak")
if(PIXELFORGE_SANITIZERS AND NOT MSVC)
    string(REPLACE "," ";" _pf_san_list "${PIXELFORGE_SANITIZERS}")
    foreach(_san IN LISTS _pf_san_list)
        target_compile_options(pixelforge_warnings INTERFACE -fsanitize=${_san} -fno-omit-frame-pointer -g)
        target_link_options   (pixelforge_warnings INTERFACE -fsanitize=${_san})
    endforeach()
    message(STATUS "PixelForge: sanitizers enabled = ${PIXELFORGE_SANITIZERS}")
endif()
