# pixelforge_core
#
# A pure C++ static library with zero external dependencies. It owns the
# in-memory pixel representation and the math primitives that every other
# PixelForge component (runtime DLL, plugins, Python bindings) consumes.
add_library(pixelforge_core STATIC
    image.cpp
    colorspace.cpp
    kernel.cpp
)
add_library(pixelforge::core ALIAS pixelforge_core)

target_include_directories(pixelforge_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(pixelforge_core PUBLIC cxx_std_20)
target_link_libraries(pixelforge_core PRIVATE pixelforge::warnings)

set_target_properties(pixelforge_core PROPERTIES
    OUTPUT_NAME pixelforge_core
    DEBUG_POSTFIX d
)

# Install the static lib only as part of the SDK component (developers
# linking against PixelForge). End-users of the runtime DLL/CLI don't need it.
if(NOT DEFINED SKBUILD_PROJECT_NAME)
    install(TARGETS pixelforge_core
        EXPORT  PixelForgeTargets
        ARCHIVE DESTINATION lib       COMPONENT SDK
    )
endif()
