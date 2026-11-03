add_executable(phoenix_engine_tests
    main.cpp
    arena_test.cpp
    vram_allocator_test.cpp
    job_system_test.cpp
    physics_test.cpp
    widgets_test.cpp
    tool_widgets_test.cpp
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

# ECS + Unity-style scene layer (needs the foundation runtime).
if(PHOENIX_WITH_FOUNDATION)
    add_executable(phoenix_scene_tests scene/main.cpp scene/ecs_test.cpp scene/components_test.cpp scene/scene_test.cpp
                                       scene/scene_file_test.cpp scene/editor_test.cpp)
    target_link_libraries(phoenix_scene_tests PRIVATE phoenix::scene phoenix::editor phoenix::doctest)
    target_include_directories(phoenix_scene_tests PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/phoenix_core_glue")
    phoenix_target_defaults(phoenix_scene_tests)
    if(EMSCRIPTEN)
        target_compile_definitions(phoenix_scene_tests PRIVATE DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS)
        target_link_options(phoenix_scene_tests PRIVATE
            -sPROXY_TO_PTHREAD -sPTHREAD_POOL_SIZE=16 -sEXIT_RUNTIME=1 -sNODERAWFS=1
            -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=8GB -sSTACK_SIZE=4MB -sDEFAULT_PTHREAD_STACK_SIZE=2MB
            -sENVIRONMENT=node,worker -Wno-pthreads-mem-growth
            "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/node_pre.js")
    endif()
    add_test(NAME phoenix_scene_tests COMMAND phoenix_scene_tests)
    set_tests_properties(phoenix_scene_tests PROPERTIES TIMEOUT 300)

    # Virtual file system: res://, user://, packs.
    add_executable(phoenix_vfs_tests vfs/main.cpp vfs/vfs_test.cpp)
    target_link_libraries(phoenix_vfs_tests PRIVATE phoenix::vfs phoenix::doctest)
    phoenix_target_defaults(phoenix_vfs_tests)
    if(EMSCRIPTEN)
        target_compile_definitions(phoenix_vfs_tests PRIVATE DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS)
        target_link_options(phoenix_vfs_tests PRIVATE
            -sPROXY_TO_PTHREAD -sPTHREAD_POOL_SIZE=8 -sEXIT_RUNTIME=1 -sNODERAWFS=1
            -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=4GB -sSTACK_SIZE=4MB -sDEFAULT_PTHREAD_STACK_SIZE=2MB
            -sENVIRONMENT=node,worker -Wno-pthreads-mem-growth
            "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/node_pre.js")
    endif()
    add_test(NAME phoenix_vfs_tests COMMAND phoenix_vfs_tests)
    set_tests_properties(phoenix_vfs_tests PROPERTIES TIMEOUT 300)
    if(TARGET phoenix_pack)
        # The pack tool end to end: pack a folder, then mount the result.
        add_test(NAME phoenix_pack_smoke
                 COMMAND "${CMAKE_COMMAND}" -DPACK=$<TARGET_FILE:phoenix_pack>
                         -DWORK=${CMAKE_CURRENT_BINARY_DIR}/pack_smoke
                         -P "${CMAKE_CURRENT_SOURCE_DIR}/vfs/pack_smoke.cmake")
    endif()
endif()
