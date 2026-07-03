# nanobind-based Python extension: _pixelforge_nb (parallel implementation
# for learning comparison vs pybind11).
nanobind_add_module(_pixelforge_nb NB_STATIC bindings.cpp)

target_link_libraries(_pixelforge_nb PRIVATE pixelforge::pixelforge)
target_compile_features(_pixelforge_nb PRIVATE cxx_std_20)

set_target_properties(_pixelforge_nb PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python_modules"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python_modules"
)

if(WIN32)
    add_custom_command(TARGET _pixelforge_nb POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:pixelforge>
            $<TARGET_FILE_DIR:_pixelforge_nb>
        VERBATIM
    )
endif()
