# Built-in PixelForge filter plugins.
#
# Each plugin is a separate MODULE library (not SHARED) because it is
# loaded only at runtime via dlopen/LoadLibrary -- nothing links to it
# at build time. MODULE is the CMake idiom for that.
#
# To keep the demo small, plugins do not link against libpixelforge.
# They only depend on the header <pixelforge/plugin_api.h>, which is
# pure C and ABI-stable.

function(pixelforge_add_plugin name)
    set(_src ${ARGN})
    add_library(pf_${name} MODULE ${_src})
    add_library(pixelforge::plugin_${name} ALIAS pf_${name})

    set_target_properties(pf_${name} PROPERTIES
        # No "lib" prefix on POSIX -> filename is e.g. "pf_blur.so",
        # which matches the .dll name on Windows.
        PREFIX ""
        # Drop any debug suffix so the loader can find them by simple name.
        DEBUG_POSTFIX ""
    )
    target_include_directories(pf_${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/plugins/common
    )
    target_compile_features(pf_${name} PRIVATE cxx_std_20)
    target_link_libraries(pf_${name} PRIVATE pixelforge::warnings)

    # Drop plugins into a dedicated <build>/plugins/ folder so the loader
    # can scan one directory.
    set_target_properties(pf_${name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    )

    # Install each built-in plugin under <prefix>/lib/pixelforge/plugins/.
    # The CLI's auto-discovery walks <exe_dir>/../lib/pixelforge/plugins/
    # in addition to <exe_dir>/../plugins, so this Just Works.
    if(NOT DEFINED SKBUILD_PROJECT_NAME)
        install(TARGETS pf_${name}
            LIBRARY DESTINATION lib/pixelforge/plugins   COMPONENT Plugins
            RUNTIME DESTINATION lib/pixelforge/plugins   COMPONENT Plugins
        )
    endif()
endfunction()

add_subdirectory(grayscale)
add_subdirectory(invert)
add_subdirectory(blur)
add_subdirectory(sharpen)
add_subdirectory(edge)
