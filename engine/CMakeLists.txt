# Phoenix Engine — the native core (C++20) that every Phoenix app embeds.
#
#   Standalone (engine development / CI):
#       cmake --preset native-debug && cmake --build --preset native-debug && ctest --preset native-debug
#
#   App build (driven by the `phoenix` CLI, never by hand):
#       cmake -S engine -B <app>/build/<target>/cmake -DPHOENIX_APP_DIR=<app> ...
cmake_minimum_required(VERSION 3.25)

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../VERSION" PHOENIX_VERSION LIMIT_COUNT 1)
project(PhoenixEngine VERSION ${PHOENIX_VERSION} LANGUAGES C CXX
        DESCRIPTION "Phoenix engine: SDL3 + Jolt + SDL_GPU/WebGPU core")

get_filename_component(PHOENIX_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

option(PHOENIX_BUILD_TESTS "Build engine unit tests" ${PROJECT_IS_TOP_LEVEL})
option(PHOENIX_WITH_PHYSICS "Build the Jolt physics module" ON)
option(PHOENIX_WITH_FOUNDATION "Build the vendored Godot core/ foundation layer" ON)
option(PHOENIX_WARNINGS_AS_ERRORS "Treat engine warnings as errors" OFF)
set(PHOENIX_SANITIZE "" CACHE STRING "Sanitizers for native builds, e.g. address,undefined or thread")
set(PHOENIX_APP_DIR "" CACHE PATH "Phoenix app project to build (set by the phoenix CLI)")
set(PHOENIX_APP_OUTPUT_DIR "" CACHE PATH "Where the runnable app bundle is written")
set(PHOENIX_BUILD_MODE "debug" CACHE STRING "debug | profile | release (Flutter-style build modes)")
set_property(CACHE PHOENIX_BUILD_MODE PROPERTY STRINGS debug profile release)
if(NOT DEFINED PHOENIX_TARGET_WASM64)
    set(PHOENIX_TARGET_WASM64 OFF)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(PhoenixCompilerOptions)
include(PhoenixDependencies)

if(PHOENIX_BUILD_TESTS)
    enable_testing()  # before any add_subdirectory that registers tests
endif()
if(PHOENIX_WITH_FOUNDATION)
    add_subdirectory(thirdparty/phoenix_core_glue)
endif()
add_subdirectory(src)

if(PHOENIX_BUILD_TESTS)
    add_subdirectory(test)
endif()

if(PHOENIX_APP_DIR)
    include(PhoenixApp)
    phoenix_add_app("${PHOENIX_APP_DIR}")
endif()
