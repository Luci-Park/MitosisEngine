# Resolves the vcpkg toolchain file from the VCPKG_ROOT environment variable.
#
# vcpkg is NOT vendored in this repository; each developer installs it once and
# shares it across projects. See docs/SETUP.md.
#
# Must be included BEFORE project(), because CMake only reads
# CMAKE_TOOLCHAIN_FILE while the first project() call runs.

if(DEFINED CMAKE_TOOLCHAIN_FILE)
    # Explicitly overridden on the command line or by a parent project.
    return()
endif()

set(_mts_vcpkg_root "$ENV{VCPKG_ROOT}")

if(_mts_vcpkg_root STREQUAL "")
    message(FATAL_ERROR
        "VCPKG_ROOT is not set.\n"
        "This project uses vcpkg in manifest mode but does not vendor it.\n"
        "Install vcpkg once, then point VCPKG_ROOT at it:\n"
        "    git clone https://github.com/microsoft/vcpkg C:/dev/vcpkg\n"
        "    C:/dev/vcpkg/bootstrap-vcpkg.bat\n"
        "    setx VCPKG_ROOT C:/dev/vcpkg\n"
        "Then restart your terminal (and VS Code entirely) so the variable is "
        "visible, and configure again.\n"
        "Full instructions: docs/SETUP.md")
endif()

file(TO_CMAKE_PATH "${_mts_vcpkg_root}" _mts_vcpkg_root)
set(_mts_toolchain "${_mts_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")

if(NOT EXISTS "${_mts_toolchain}")
    message(FATAL_ERROR
        "VCPKG_ROOT points at '${_mts_vcpkg_root}', but no vcpkg toolchain was "
        "found there.\n"
        "Expected: ${_mts_toolchain}\n"
        "Either the path is wrong, or the clone was never bootstrapped:\n"
        "    ${_mts_vcpkg_root}/bootstrap-vcpkg.bat\n"
        "Full instructions: docs/SETUP.md")
endif()

set(CMAKE_TOOLCHAIN_FILE "${_mts_toolchain}"
    CACHE STRING "vcpkg toolchain, resolved from VCPKG_ROOT")

message(STATUS "vcpkg root: ${_mts_vcpkg_root}")

unset(_mts_vcpkg_root)
unset(_mts_toolchain)
