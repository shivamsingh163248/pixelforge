# PixelForge top-level build
#
# Industry conventions:
#   - C++20, no compiler extensions
#   - Out-of-source builds only (enforced)
#   - Position-independent code (needed for plugins / shared libs later)
#   - Single project version, exposed to C++ via generated header
cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Version)

project(PixelForge
    VERSION     ${PIXELFORGE_VERSION}
    DESCRIPTION "Cross-platform image processing SDK"
    LANGUAGES   CXX
)

# Block in-source builds.
if(CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use a build/ folder.")
endif()

# Defaults that make sense for a real SDK.
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_CXX_VISIBILITY_PRESET     hidden)

# Group artifacts neatly so multi-config generators (VS) and single-config
# generators (Ninja) both produce predictable layouts.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(PIXELFORGE_BUILD_TESTS  "Build unit tests"          ON)
option(PIXELFORGE_BUILD_PYTHON "Build Python bindings"     ON)
option(PIXELFORGE_BUILD_CLI    "Build pixelforge CLI exe"  ON)

# Statically link the MinGW C/C++ runtime so we don't depend on
# libstdc++-6.dll / libgcc_s_seh-1.dll / libwinpthread-1.dll being on PATH.
# This matches how a redistributable Windows binary should look.
if(MINGW)
    set(_pf_mingw_static "-static-libgcc -static-libstdc++ -static")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} ${_pf_mingw_static}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${_pf_mingw_static}")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${_pf_mingw_static}")
endif()

include(CompilerWarnings)

# Generated version header for C++ consumers.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/pixelforge/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/pixelforge/version.h"
    @ONLY
)

add_subdirectory(src/core)
add_subdirectory(src/runtime)
if(PIXELFORGE_BUILD_CLI)
    add_subdirectory(src/cli)
endif()
add_subdirectory(plugins)

if(PIXELFORGE_BUILD_PYTHON)
    add_subdirectory(python)
endif()

if(PIXELFORGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install + package (skipped during scikit-build-core wheel builds, which
# install only the Python bindings into the wheel root).
# ---------------------------------------------------------------------------
if(NOT DEFINED SKBUILD_PROJECT_NAME)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # SDK headers (public API + generated version header).
    install(DIRECTORY include/pixelforge
        DESTINATION include
        COMPONENT   SDK
        FILES_MATCHING PATTERN "*.h"
    )
    install(FILES "${CMAKE_BINARY_DIR}/generated/pixelforge/version.h"
        DESTINATION include/pixelforge
        COMPONENT   SDK
    )

    # CMake package config so consumers can `find_package(PixelForge)`.
    install(EXPORT PixelForgeTargets
        FILE        PixelForgeTargets.cmake
        NAMESPACE   pixelforge::
        DESTINATION lib/cmake/PixelForge
        COMPONENT   SDK
    )
    file(WRITE "${CMAKE_BINARY_DIR}/PixelForgeConfig.cmake.in"
"@PACKAGE_INIT@\ninclude(\"\${CMAKE_CURRENT_LIST_DIR}/PixelForgeTargets.cmake\")\ncheck_required_components(PixelForge)\n")
    configure_package_config_file(
        "${CMAKE_BINARY_DIR}/PixelForgeConfig.cmake.in"
        "${CMAKE_BINARY_DIR}/PixelForgeConfig.cmake"
        INSTALL_DESTINATION lib/cmake/PixelForge
    )
    write_basic_package_version_file(
        "${CMAKE_BINARY_DIR}/PixelForgeConfigVersion.cmake"
        VERSION       ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        "${CMAKE_BINARY_DIR}/PixelForgeConfig.cmake"
        "${CMAKE_BINARY_DIR}/PixelForgeConfigVersion.cmake"
        DESTINATION lib/cmake/PixelForge
        COMPONENT   SDK
    )

    install(FILES LICENSE README.md
        DESTINATION share/pixelforge
        COMPONENT   Runtime
    )

    # On Windows, install a tiny marker file under each pseudo-component so
    # CPack/NSIS actually emits a Section + $<Name>_selected variable for
    # the user-toggleable PATH and Desktop-icon checkboxes (see CPack
    # configuration below). Without an install() rule the component is
    # silently dropped from the wizard.
    if(WIN32)
        file(WRITE "${CMAKE_BINARY_DIR}/marker.txt"
             "PixelForge installer marker — safe to delete.\n")
        install(FILES "${CMAKE_BINARY_DIR}/marker.txt"
                DESTINATION share/pixelforge/.markers
                RENAME path-user.txt
                COMPONENT PathUser)
        install(FILES "${CMAKE_BINARY_DIR}/marker.txt"
                DESTINATION share/pixelforge/.markers
                RENAME path-all.txt
                COMPONENT PathAll)
        install(FILES "${CMAKE_BINARY_DIR}/marker.txt"
                DESTINATION share/pixelforge/.markers
                RENAME desktop-icon.txt
                COMPONENT DesktopIcon)
    endif()

    # -----------------------------------------------------------------------
    # CPack — produces native installers (NSIS/WIX on Windows, DEB/RPM on
    # Linux, productbuild on macOS) and ZIP/TGZ archives on every platform.
    # -----------------------------------------------------------------------
    set(CPACK_PACKAGE_NAME           "PixelForge")
    set(CPACK_PACKAGE_VENDOR         "PixelForge Authors")
    set(CPACK_PACKAGE_VERSION         ${PROJECT_VERSION})
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_PACKAGE_HOMEPAGE_URL   "https://github.com/example/pixelforge")
    set(CPACK_RESOURCE_FILE_LICENSE  "${CMAKE_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_README   "${CMAKE_SOURCE_DIR}/README.md")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "PixelForge ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
    set(CPACK_PACKAGE_FILE_NAME
        "pixelforge-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_PACKAGE_CONTACT        "pixelforge@example.com")

    # Components & grouping.
    # Real components (install files) + 3 pseudo-components on Windows that
    # install no files but show up as user-toggleable checkboxes on the
    # Components page (PATH-current-user, PATH-all-users, Desktop icon).
    if(WIN32)
        set(CPACK_COMPONENTS_ALL Runtime SDK Plugins PathUser PathAll DesktopIcon)
    else()
        set(CPACK_COMPONENTS_ALL Runtime SDK Plugins)
    endif()
    set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "PixelForge Runtime")
    set(CPACK_COMPONENT_SDK_DISPLAY_NAME     "Development SDK (headers + import libs + CMake config)")
    set(CPACK_COMPONENT_PLUGINS_DISPLAY_NAME "Built-in image filter plugins")
    set(CPACK_COMPONENT_RUNTIME_REQUIRED     TRUE)
    set(CPACK_COMPONENT_SDK_DEPENDS          Runtime)
    set(CPACK_COMPONENT_PLUGINS_DEPENDS      Runtime)
    set(CPACK_COMPONENT_RUNTIME_DESCRIPTION
        "The pixelforge.exe CLI plus the libpixelforge runtime library required to run it.")
    set(CPACK_COMPONENT_SDK_DESCRIPTION
        "C++ headers, import library and CMake package config for building applications against PixelForge.")
    set(CPACK_COMPONENT_PLUGINS_DESCRIPTION
        "Five reference filter plugins: grayscale, invert, blur, sharpen, edge.")
    if(WIN32)
        # Pseudo-components: their checkbox state is read at install time
        # via $<Name>_selected to gate the PATH/shortcut actions below.
        set(CPACK_COMPONENT_PATHUSER_DISPLAY_NAME    "Add PixelForge to PATH (current user)")
        set(CPACK_COMPONENT_PATHUSER_DESCRIPTION
            "Append the install bin/ folder to your user PATH so 'pixelforge' is on the command line in any new shell. No admin rights needed. Recommended.")
        set(CPACK_COMPONENT_PATHALL_DISPLAY_NAME     "Add PixelForge to PATH (all users)")
        set(CPACK_COMPONENT_PATHALL_DESCRIPTION
            "Append the install bin/ folder to the SYSTEM PATH so 'pixelforge' is on the command line for every user on this machine. Requires admin rights.")
        set(CPACK_COMPONENT_DESKTOPICON_DISPLAY_NAME "Create Desktop icon")
        set(CPACK_COMPONENT_DESKTOPICON_DESCRIPTION
            "Place a 'PixelForge CLI' shortcut on your Desktop that opens a console with --help.")
        # Default state: PathUser ON, PathAll OFF, DesktopIcon ON.
        set(CPACK_COMPONENT_PATHALL_DISABLED         TRUE)
    endif()

    # Per-platform generators.
    if(WIN32)
        set(CPACK_GENERATOR                  "ZIP;NSIS")
        set(CPACK_NSIS_DISPLAY_NAME          "PixelForge ${PROJECT_VERSION}")
        set(CPACK_NSIS_PACKAGE_NAME          "PixelForge")
        set(CPACK_NSIS_HELP_LINK             "${CPACK_PACKAGE_HOMEPAGE_URL}")
        set(CPACK_NSIS_URL_INFO_ABOUT        "${CPACK_PACKAGE_HOMEPAGE_URL}")
        set(CPACK_NSIS_CONTACT               "${CPACK_PACKAGE_CONTACT}")
        # Do NOT use CPACK_NSIS_MODIFY_PATH: its EnvVarUpdate macro silently
        # fails when the existing PATH is > 1024 chars. We do the PATH and
        # Desktop-shortcut work ourselves below, gated by checkbox state.
        set(CPACK_NSIS_MODIFY_PATH           OFF)
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
        set(CPACK_NSIS_MUI_FINISHPAGE_RUN    "")    # no auto-launch on finish
        # Show CLI in Start Menu so users can find it.
        set(CPACK_PACKAGE_EXECUTABLES        "pixelforge" "PixelForge CLI")
        # Proper Add/Remove Programs entry.
        set(CPACK_NSIS_INSTALLED_ICON_NAME   "bin\\\\pixelforge.exe")
        # Run after every component section. We check $<Name>_selected
        # (defined by CPack for each component) and only act if checked.
        # NOTE: We use native NSIS IntCmp instead of LogicLib ${If} because
        # CPack's generated script does not include LogicLib.nsh.
        # The _selected variables are set by CPack's InitSection macro on .onInit.
        # We use NSIS native registry operations for PATH modification.
        # HWND_BROADCAST=0xFFFF=65535, WM_SETTINGCHANGE=0x001A=26
        set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
            SetShellVarContext current
            IntCmp \$PathUser_selected 1 do_pathuser skip_pathuser skip_pathuser
            do_pathuser:
                DetailPrint 'Adding PixelForge bin to USER PATH...'
                ReadRegStr \$1 HKCU 'Environment' 'PATH'
                StrCpy \$2 '\$INSTDIR\\\\bin'
                WriteRegExpandStr HKCU 'Environment' 'PATH' '\$1;\$2'
                SendMessage 65535 26 0 'STR:Environment' /TIMEOUT=5000
            skip_pathuser:
            IntCmp \$PathAll_selected 1 do_pathall skip_pathall skip_pathall
            do_pathall:
                DetailPrint 'Adding PixelForge bin to MACHINE PATH (admin required)...'
                ReadRegStr \$1 HKLM 'SYSTEM\\\\CurrentControlSet\\\\Control\\\\Session Manager\\\\Environment' 'PATH'
                StrCpy \$2 '\$INSTDIR\\\\bin'
                WriteRegExpandStr HKLM 'SYSTEM\\\\CurrentControlSet\\\\Control\\\\Session Manager\\\\Environment' 'PATH' '\$1;\$2'
                SendMessage 65535 26 0 'STR:Environment' /TIMEOUT=5000
            skip_pathall:
            IntCmp \$DesktopIcon_selected 1 do_desktop skip_desktop skip_desktop
            do_desktop:
                DetailPrint 'Creating Desktop shortcut...'
                CreateShortCut '\$DESKTOP\\\\PixelForge CLI.lnk' '\$INSTDIR\\\\bin\\\\pixelforge.exe' '' '\$INSTDIR\\\\bin\\\\pixelforge.exe' 0
            skip_desktop:
        ")
        # Uninstaller: remove desktop shortcut. PATH cleanup is done manually
        # or at next install (installer only adds if not present).
        set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
            SetShellVarContext current
            DetailPrint 'Removing Desktop shortcut (if present)...'
            Delete '\$DESKTOP\\\\PixelForge CLI.lnk'
        ")
    elseif(APPLE)
        set(CPACK_GENERATOR                  "TGZ;productbuild")
    else()
        set(CPACK_GENERATOR                  "TGZ;DEB;RPM")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER  "${CPACK_PACKAGE_CONTACT}")
        set(CPACK_DEBIAN_PACKAGE_SECTION     "graphics")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS   ON)
        set(CPACK_DEBIAN_PACKAGE_HOMEPAGE    "${CPACK_PACKAGE_HOMEPAGE_URL}")
        # One .deb per component: pixelforge-runtime, pixelforge-sdk, pixelforge-plugins
        set(CPACK_DEB_COMPONENT_INSTALL      ON)
        set(CPACK_RPM_COMPONENT_INSTALL      ON)
        set(CPACK_RPM_PACKAGE_LICENSE        "MIT")
        set(CPACK_RPM_PACKAGE_GROUP          "Development/Libraries")
    endif()

    include(CPack)
endif()

message(STATUS "PixelForge ${PROJECT_VERSION} configured")
message(STATUS "  Compiler  : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests     : ${PIXELFORGE_BUILD_TESTS}")
