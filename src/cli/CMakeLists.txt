# pixelforge CLI -- user-facing executable.
#
# Built into <build>/bin alongside libpixelforge.dll so it can launch
# directly from the build tree without any PATH tweaking.
add_executable(pixelforge_cli main.cpp)

set_target_properties(pixelforge_cli PROPERTIES
    OUTPUT_NAME pixelforge        # produces pixelforge.exe / pixelforge
    DEBUG_POSTFIX d
)

target_link_libraries(pixelforge_cli
    PRIVATE
        pixelforge::pixelforge
        pixelforge::warnings
)

# On Linux, when installed to <prefix>/bin/pixelforge with libs at
# <prefix>/lib/libpixelforge.so, this RPATH lets the binary find them
# without LD_LIBRARY_PATH. macOS uses @loader_path/../lib equivalent.
if(UNIX AND NOT APPLE)
    set_target_properties(pixelforge_cli PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif(APPLE)
    set_target_properties(pixelforge_cli PROPERTIES
        INSTALL_RPATH "@loader_path/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

# Ship the CLI as part of the Runtime component so end-users get a
# usable command-line tool by default.
if(NOT DEFINED SKBUILD_PROJECT_NAME)
    install(TARGETS pixelforge_cli
        RUNTIME DESTINATION bin   COMPONENT Runtime
    )
endif()
