# pybind11-based Python extension: _pixelforge
pybind11_add_module(_pixelforge MODULE bindings.cpp)

target_link_libraries(_pixelforge PRIVATE pixelforge::pixelforge)
target_compile_features(_pixelforge PRIVATE cxx_std_20)

set_target_properties(_pixelforge PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python_modules"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python_modules"
)

# On Windows the runtime DLL must sit next to the .pyd at import time.
if(WIN32)
    add_custom_command(TARGET _pixelforge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:pixelforge>
            $<TARGET_FILE_DIR:_pixelforge>
        VERBATIM
    )
endif()

# --- Wheel install layout --------------------------------------------------
# When driven by scikit-build-core (PEP 517 wheel build), every artifact
# the runtime needs is installed into the `pixelforge` package directory:
#   site-packages/pixelforge/__init__.py            (from python/pixelforge/)
#   site-packages/pixelforge/_pixelforge*.pyd
#   site-packages/pixelforge/libpixelforge.{dll,so}
#   site-packages/pixelforge/plugins/pf_*.{dll,so}
# scikit-build-core sets SKBUILD_PROJECT_NAME so we key off that.
if(DEFINED SKBUILD_PROJECT_NAME)
    install(TARGETS _pixelforge
        LIBRARY DESTINATION pixelforge
        RUNTIME DESTINATION pixelforge
    )
    # Bundle the shared runtime alongside the .pyd.
    install(TARGETS pixelforge
        LIBRARY DESTINATION pixelforge   # .so on Linux/macOS
        RUNTIME DESTINATION pixelforge   # .dll on Windows
    )
    # Bundle every built plugin under pixelforge/plugins/.
    foreach(_plugin IN ITEMS grayscale invert blur sharpen edge)
        if(TARGET pf_${_plugin})
            install(TARGETS pf_${_plugin}
                LIBRARY DESTINATION pixelforge/plugins
                RUNTIME DESTINATION pixelforge/plugins
            )
        endif()
    endforeach()
endif()
