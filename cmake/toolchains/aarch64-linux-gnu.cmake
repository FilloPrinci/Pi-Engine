# Cross-compilation toolchain: x86_64 Linux host -> aarch64 Linux target (Raspberry Pi 4/5).
# Chainloaded by vcpkg (see CMakePresets.json, presets "pi4"/"pi5") via
# VCPKG_CHAINLOAD_TOOLCHAIN_FILE, so vcpkg's own toolchain logic runs first and this file
# only fills in the actual cross compiler.
#
# Requires the aarch64-linux-gnu cross toolchain installed on the host, e.g. on
# Debian/Ubuntu: `apt install g++-aarch64-linux-gnu gcc-aarch64-linux-gnu`.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Only look for libraries/headers in the target sysroot, but let CMake find host tools
# (e.g. code generators run at build time) on the host PATH.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
