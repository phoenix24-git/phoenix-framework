# Phoenix toolchain: Emscripten targeting wasm64 (Memory64) with shared-memory pthreads.
#
# Every translation unit (engine, SDL3, Jolt, app) must agree on pointer width and on
# shared memory, so the flags are injected here rather than per target.
#
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=engine/cmake/toolchains/emscripten-wasm64.cmake ...
include_guard(GLOBAL)

if(DEFINED ENV{EMSDK})
    set(_phx_emscripten_root "$ENV{EMSDK}/upstream/emscripten")
elseif(DEFINED ENV{EMSCRIPTEN})
    set(_phx_emscripten_root "$ENV{EMSCRIPTEN}")
else()
    find_program(_phx_emcc emcc REQUIRED)
    get_filename_component(_phx_emscripten_root "${_phx_emcc}" DIRECTORY)
    get_filename_component(_phx_emscripten_root "${_phx_emscripten_root}" REALPATH)
endif()

# -m64 selects wasm64; -pthread enables SharedArrayBuffer-backed memory + atomics.
set(CMAKE_C_FLAGS "-m64 -pthread" CACHE STRING "Phoenix wasm64 C flags")
set(CMAKE_CXX_FLAGS "-m64 -pthread" CACHE STRING "Phoenix wasm64 C++ flags")
set(PHOENIX_TARGET_WASM64 ON CACHE BOOL "Building for wasm64" FORCE)

include("${_phx_emscripten_root}/cmake/Modules/Platform/Emscripten.cmake")

# Wasm64 + pthreads need Node >= 24 for tests; prefer emsdk's bundled Node.
if(DEFINED ENV{EMSDK_NODE} AND EXISTS "$ENV{EMSDK_NODE}")
    set(CMAKE_CROSSCOMPILING_EMULATOR "$ENV{EMSDK_NODE}" CACHE FILEPATH "Node.js for wasm tests" FORCE)
endif()
