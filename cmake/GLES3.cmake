# Desktop GLES3 uses the GLESv2 library name on Mesa and ANGLE too.
# Prefer pkg-config's complete link information; allow an explicit library
# override and retain find_library for systems without pkg-config metadata.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND AND NOT TCPE_GLES3_LIBRARY)
    pkg_check_modules(TCPE_GLES3_PC QUIET IMPORTED_TARGET glesv2)
endif()
if(TARGET PkgConfig::TCPE_GLES3_PC)
    set(TCPE_GLES3_LINK_TARGET PkgConfig::TCPE_GLES3_PC)
else()
    find_library(TCPE_GLES3_LIBRARY NAMES GLESv2)
    if(TCPE_GLES3_LIBRARY)
        set(TCPE_GLES3_LINK_TARGET "${TCPE_GLES3_LIBRARY}")
    endif()
endif()
# GLES3Compat.h and GLES3Compat.cpp include these Khronos headers directly.
find_path(TCPE_GLES3_INCLUDE_DIR NAMES GLES3/gl3.h
    HINTS ${TCPE_GLES3_PC_INCLUDE_DIRS})
if(NOT TCPE_GLES3_LINK_TARGET OR NOT TCPE_GLES3_INCLUDE_DIR OR
   NOT EXISTS "${TCPE_GLES3_INCLUDE_DIR}/GLES3/gl31.h")
    message(FATAL_ERROR
        "TCPE's desktop GLES3 build needs the OpenGL ES development library "
        "(named GLESv2) and GLES3 headers. A graphics driver alone is not enough.\n"
        "Ubuntu/Debian: sudo apt install libgles-dev pkg-config\n"
        "Fedora: sudo dnf install mesa-libGLES-devel pkgconf-pkg-config\n"
        "Arch: sudo pacman -S libglvnd pkgconf\n"
        "For a custom SDK/ANGLE installation, set CMAKE_PREFIX_PATH or pass "
        "-DTCPE_GLES3_LIBRARY=/path/to/libGLESv2 and "
        "-DTCPE_GLES3_INCLUDE_DIR=/path/to/include.\n"
        "The headless server does not need graphics: -DTCPE_DEDICATED_SERVER=ON")
endif()
