# Third-party dependencies, pinned to exact commit SHAs.
#
# Sources are fetched with a shallow `git fetch <sha>` into a shared cache
# (PHOENIX_DEPS_CACHE, default: <repo>/bin/cache/deps) so every app and every
# target (web, linux, ...) reuses a single download. Bumping a dependency is a
# one-line change of its SHA here, reviewed like any other code change.
include_guard(GLOBAL)

set(PHOENIX_DEPS_CACHE "${PHOENIX_REPO_ROOT}/bin/cache/deps" CACHE PATH "Shared dependency source cache")

function(phoenix_fetch_git name repo sha out_var)
    string(SUBSTRING "${sha}" 0 12 _short)
    set(_dir "${PHOENIX_DEPS_CACHE}/${name}-${_short}")
    if(NOT EXISTS "${_dir}/.phoenix-complete")
        message(STATUS "Phoenix: fetching ${name} @ ${_short}")
        file(REMOVE_RECURSE "${_dir}")
        file(MAKE_DIRECTORY "${_dir}")
        find_package(Git REQUIRED)
        foreach(_cmd IN ITEMS "init;-q" "fetch;-q;--depth;1;${repo};${sha}" "checkout;-q;FETCH_HEAD")
            execute_process(COMMAND "${GIT_EXECUTABLE}" ${_cmd}
                            WORKING_DIRECTORY "${_dir}" RESULT_VARIABLE _rc ERROR_VARIABLE _err)
            if(NOT _rc EQUAL 0)
                file(REMOVE_RECURSE "${_dir}")
                message(FATAL_ERROR "Phoenix: failed to fetch ${name}: ${_err}")
            endif()
        endforeach()
        file(TOUCH "${_dir}/.phoenix-complete")
    endif()
    set(${out_var} "${_dir}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# SDL3 (TheeStickmahn/SDL_wgpu fork: SDL_GPU with a WebGPU backend)
# ---------------------------------------------------------------------------
phoenix_fetch_git(sdl3 "https://github.com/TheeStickmahn/SDL_wgpu.git"
                  "0328a2465a87cbe130deba8428a3003cf84c0f08" PHOENIX_SDL3_DIR)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
    # SDL defaults to a thread-less web build; Phoenix needs real pthreads
    # (Web Workers sharing one wasm64 memory) for its 6-core console profile.
    set(SDL_PTHREADS ON CACHE BOOL "" FORCE)
endif()
add_subdirectory("${PHOENIX_SDL3_DIR}" "${CMAKE_BINARY_DIR}/_deps/sdl3" EXCLUDE_FROM_ALL SYSTEM)

# ---------------------------------------------------------------------------
# Jolt Physics v5.4.0
# ---------------------------------------------------------------------------
if(PHOENIX_WITH_PHYSICS)
    phoenix_fetch_git(jolt "https://github.com/jrouwe/JoltPhysics.git"
                      "036ea7b1d717b3e713ac9d8cbd47118fb9cd5d60" PHOENIX_JOLT_DIR)
    set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
    set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
    set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
    set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
    set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
    set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
    set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
    set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
    set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)  # we subclass Jolt types
    set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "" FORCE)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
    if(EMSCRIPTEN)
        set(USE_WASM_SIMD ON CACHE BOOL "" FORCE)
        set(JPH_USE_WASM64 ${PHOENIX_TARGET_WASM64} CACHE BOOL "" FORCE)
    else()
        # Portable baseline for CI runners and end-user machines.
        foreach(_isa USE_AVX USE_AVX2 USE_AVX512 USE_LZCNT USE_TZCNT USE_F16C USE_FMADD)
            set(${_isa} OFF CACHE BOOL "" FORCE)
        endforeach()
    endif()
    add_subdirectory("${PHOENIX_JOLT_DIR}/Build" "${CMAKE_BINARY_DIR}/_deps/jolt" EXCLUDE_FROM_ALL SYSTEM)
endif()

# ---------------------------------------------------------------------------
# doctest 2.4.12 (tests only)
# ---------------------------------------------------------------------------
if(PHOENIX_BUILD_TESTS)
    phoenix_fetch_git(doctest "https://github.com/doctest/doctest.git"
                      "1da23a3e8119ec5cce4f9388e91b065e20bf06f5" PHOENIX_DOCTEST_DIR)
    add_library(phoenix_doctest INTERFACE)
    target_include_directories(phoenix_doctest SYSTEM INTERFACE "${PHOENIX_DOCTEST_DIR}")
    add_library(phoenix::doctest ALIAS phoenix_doctest)
endif()
