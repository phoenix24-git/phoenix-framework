# Builds a Phoenix app project (the directory containing phoenix.json).
# Invoked by the `phoenix` CLI, which translates phoenix.json into these cache
# variables, much like flutter_tools drives the platform builds.
include_guard(GLOBAL)

set(PHOENIX_APP_NAME "phoenix_app" CACHE STRING "App identifier (phoenix.json: name)")
set(PHOENIX_APP_TITLE "Phoenix App" CACHE STRING "Window/page title")
set(PHOENIX_VRAM_MB 3584 CACHE STRING "Console VRAM budget (MiB)")
set(PHOENIX_VRAM_PREALLOCATE ON CACHE BOOL "Reserve all VRAM pages at boot")
set(PHOENIX_WORKER_THREADS 6 CACHE STRING "Console worker threads")
set(PHOENIX_CPU_ARENA_MB 256 CACHE STRING "Console CPU arena (MiB)")
set(PHOENIX_WEB_INITIAL_MEMORY_MB 1024 CACHE STRING "Initial wasm64 linear memory (MiB)")
set(PHOENIX_WEB_MAXIMUM_MEMORY_MB 16384 CACHE STRING "Maximum wasm64 linear memory (MiB, >4 GiB needs wasm64)")
option(PHOENIX_APP_TESTS "Build the app's test/ suite (phoenix test)" OFF)

function(phoenix_add_app app_dir)
    get_filename_component(app_dir "${app_dir}" ABSOLUTE)
    if(NOT EXISTS "${app_dir}/phoenix.json")
        message(FATAL_ERROR "Not a Phoenix project (no phoenix.json): ${app_dir}")
    endif()
    if(NOT PHOENIX_APP_OUTPUT_DIR)
        set(PHOENIX_APP_OUTPUT_DIR "${CMAKE_BINARY_DIR}/out")
    endif()
    if(PHOENIX_VRAM_PREALLOCATE)
        set(PHOENIX_VRAM_PREALLOCATE_INT 1)
    else()
        set(PHOENIX_VRAM_PREALLOCATE_INT 0)
    endif()
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/phoenix_app_config.h.in"
                   "${CMAKE_BINARY_DIR}/generated/phoenix_app_config.h" @ONLY)

    file(GLOB_RECURSE app_sources CONFIGURE_DEPENDS "${app_dir}/lib/*.cpp" "${app_dir}/lib/*.h")
    add_library(phoenix_app_lib STATIC ${app_sources})
    target_include_directories(phoenix_app_lib PUBLIC "${app_dir}/lib")
    target_link_libraries(phoenix_app_lib PUBLIC phoenix::engine)
    phoenix_target_defaults(phoenix_app_lib)

    add_executable(phoenix_app "${PHOENIX_APP_MAIN_SOURCE}")
    target_include_directories(phoenix_app PRIVATE "${CMAKE_BINARY_DIR}/generated")
    # Whole-archive so phoenix_main() from lib/ is always linked in.
    target_link_libraries(phoenix_app PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,phoenix_app_lib>")
    phoenix_target_defaults(phoenix_app)
    set_target_properties(phoenix_app PROPERTIES
        OUTPUT_NAME "${PHOENIX_APP_NAME}"
        RUNTIME_OUTPUT_DIRECTORY "${PHOENIX_APP_OUTPUT_DIR}")

    if(EMSCRIPTEN)
        math(EXPR pool "${PHOENIX_WORKER_THREADS} + 4")  # workers + IO + OPFS + spare
        target_link_options(phoenix_app PRIVATE
            -sWASMFS
            -sFORCE_FILESYSTEM=1             # SDL's JS glue uses the FS API (file drop)
            -sPTHREAD_POOL_SIZE=${pool}
            -sASYNCIFY=1                     # SDL_GPU WebGPU adapter/device requests
            -sASYNCIFY_STACK_SIZE=65536
            -sALLOW_MEMORY_GROWTH=1
            -sINITIAL_MEMORY=${PHOENIX_WEB_INITIAL_MEMORY_MB}MB
            -sMAXIMUM_MEMORY=${PHOENIX_WEB_MAXIMUM_MEMORY_MB}MB
            -sSTACK_SIZE=1MB
            -sDEFAULT_PTHREAD_STACK_SIZE=1MB
            -sENVIRONMENT=web,worker
            -sEXIT_RUNTIME=0
            -sEXPORTED_RUNTIME_METHODS=ccall
            -Wno-pthreads-mem-growth)
        if(PHOENIX_BUILD_MODE STREQUAL "debug")
            # Source maps let Chrome DevTools step through C++; `phoenix run`
            # serves /__phoenix/app and /__phoenix/sdk from disk (app first,
            # since examples live inside the SDK tree).
            target_link_options(phoenix_app PRIVATE -O0 -gsource-map -sASSERTIONS=1
                "-sSOURCE_MAP_PREFIXES=['${app_dir}=/__phoenix/app','${PHOENIX_REPO_ROOT}=/__phoenix/sdk']")
        elseif(PHOENIX_BUILD_MODE STREQUAL "profile")
            target_link_options(phoenix_app PRIVATE -O2 --profiling-funcs)
        else()
            target_link_options(phoenix_app PRIVATE -O3)
        endif()
    endif()

    if(PHOENIX_APP_TESTS)
        file(GLOB_RECURSE app_tests CONFIGURE_DEPENDS "${app_dir}/test/*.cpp")
        if(app_tests)
            enable_testing()
            if(NOT TARGET phoenix::doctest)
                message(FATAL_ERROR "PHOENIX_APP_TESTS requires PHOENIX_BUILD_TESTS=ON")
            endif()
            add_executable(phoenix_app_tests "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/app_test_main.cpp" ${app_tests})
            # Tests #include "main.cpp" (like `import 'package:app/main.dart'`),
            # so they link the engine directly rather than the app library.
            target_include_directories(phoenix_app_tests PRIVATE "${app_dir}/lib")
            target_link_libraries(phoenix_app_tests PRIVATE phoenix::engine phoenix::doctest)
            phoenix_target_defaults(phoenix_app_tests)
            add_test(NAME app_tests COMMAND phoenix_app_tests)
        endif()
    endif()
endfunction()
