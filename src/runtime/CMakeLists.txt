# PixelForge runtime
#
# Shared library: pixelforge.dll / libpixelforge.so
# Wraps the static core, adds image I/O (via vendored stb), and exposes
# the Pipeline engine + plugin loader. This is the artifact the CLI and
# Python bindings consume at runtime.

add_library(pixelforge SHARED
    pipeline.cpp
    io_stb.cpp
    plugin_loader.cpp
)
add_library(pixelforge::pixelforge ALIAS pixelforge)

target_include_directories(pixelforge
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/stb
)

target_link_libraries(pixelforge
    PUBLIC  pixelforge::core
    PRIVATE pixelforge::warnings
)

# POSIX systems need libdl for dlopen/dlsym. CMAKE_DL_LIBS expands to
# "dl" on Linux, "" on Windows/MinGW (where the loader lives in kernel32).
if(UNIX AND NOT APPLE)
    target_link_libraries(pixelforge PRIVATE ${CMAKE_DL_LIBS})
endif()

# stb_image is a C90 single-header lib that produces lots of conversion
# warnings under our strict flags. Silence them only for the file that
# actually instantiates the implementations.
if(MSVC)
    set_source_files_properties(io_stb.cpp PROPERTIES COMPILE_OPTIONS "/wd4244;/wd4267;/wd4456")
else()
    set_source_files_properties(io_stb.cpp PROPERTIES COMPILE_OPTIONS
        "-Wno-conversion;-Wno-sign-conversion;-Wno-old-style-cast;-Wno-shadow;-Wno-cast-align;-Wno-double-promotion;-Wno-missing-field-initializers")
endif()

set_target_properties(pixelforge PROPERTIES
    OUTPUT_NAME    pixelforge
    DEBUG_POSTFIX  d
    VERSION        ${PROJECT_VERSION}
    SOVERSION      ${PROJECT_VERSION_MAJOR}
    # Generates pixelforge.dll + pixelforge.lib (import lib) on Windows.
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

# Install the shared runtime. Components:
#   Runtime  -- end-user .dll / .so + symlinks (everyone needs this)
#   SDK      -- import library (.lib / .dll.a) for developers
if(NOT DEFINED SKBUILD_PROJECT_NAME)
    install(TARGETS pixelforge
        EXPORT  PixelForgeTargets
        RUNTIME DESTINATION bin       COMPONENT Runtime  # .dll on Windows
        LIBRARY DESTINATION lib       COMPONENT Runtime  # .so on Linux/macOS
        ARCHIVE DESTINATION lib       COMPONENT SDK      # .lib / .dll.a
    )
endif()
