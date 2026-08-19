cmake_minimum_required(VERSION 3.24)

project(PiEngine
    VERSION 0.1.0
    DESCRIPTION "Open source 3D engine optimized for Raspberry Pi 4/5"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

# Hardware profile selected via CMake preset (pi4 | pi5 | desktop). Consumed later by the
# Hardware Profile System (docs/01, section 4/7.3); for now it is only propagated as a
# compile definition so engine code can start branching on it if needed.
set(PI_ENGINE_HARDWARE_PROFILE "desktop" CACHE STRING "Target hardware profile: pi4 | pi5 | desktop")

option(PI_ENGINE_BUILD_TESTS "Build unit tests (doctest)" ON)
option(PI_ENGINE_BUILD_SAMPLES "Build milestone samples" ON)
option(PI_ENGINE_BUILD_COOKER "Build the offline asset Cooker CLI tool (docs/01 section 12.4)" ON)
option(PI_ENGINE_BUILD_EDITOR "Build the Editor application (docs/01 section 6, docs/06-editor-roadmap.md)" ON)

add_subdirectory(engine)

if(PI_ENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# The Cooker is build-time tooling: it must run on the machine doing the build, not the
# cross-compilation target, so it's skipped entirely while actually cross-compiling (see
# cmake/CookAssets.cmake's own comment for what that means for samples in that case).
if(PI_ENGINE_BUILD_COOKER AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(tools/cooker)
endif()
include(CookAssets)

if(PI_ENGINE_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

if(PI_ENGINE_BUILD_EDITOR)
    add_subdirectory(editor)
endif()
