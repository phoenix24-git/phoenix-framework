add_executable(phoenix_engine_tests
    main.cpp
    arena_test.cpp
    vram_allocator_test.cpp
    job_system_test.cpp
    physics_test.cpp
    widgets_test.cpp
    storage_test.cpp
    script_abi_test.cpp
    script_host_test.cpp)
target_link_libraries(phoenix_engine_tests PRIVATE phoenix::engine phoenix::doctest)
phoenix_target_defaults(phoenix_engine_tests)

if(EMSCRIPTEN)
    # Wasm builds compile without exception support; keep REQUIRE semantics without throwing.
    target_compile_definitions(phoenix_engine_tests PRIVATE DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS)
    # Run under Node: main() on a pthread so tests may block; 8 workers in the pool.
    target_link_options(phoenix_engine_tests PRIVATE
        -sPROXY_TO_PTHREAD -sPTHREAD_POOL_SIZE=10 -sEXIT_RUNTIME=1 -sWASMFS
        -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=8GB -sSTACK_SIZE=1MB
        -sENVIRONMENT=node,worker -Wno-pthreads-mem-growth
        "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/node_pre.js")
endif()

add_test(NAME phoenix_engine_tests COMMAND phoenix_engine_tests --reporters=console)
set_tests_properties(phoenix_engine_tests PROPERTIES TIMEOUT 300)
