# Engine libraries. Layering (each may only depend on the ones above it):
#
#   phoenix::core       memory arena, VRAM allocator, SDL3 job system
#   phoenix::physics    Jolt world on the shared job system
#   phoenix::framework  widgets, layout, display list (no GPU)
#   phoenix::renderer   SDL_GPU (WebGPU) + SDL_Renderer fallback, VRAM pool
#   phoenix::engine     shell: window, frame loop, storage/IO thread, scripting
set(PHOENIX_PUBLIC_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../include")

function(phoenix_engine_library name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPS" ${ARGN})
    add_library(phoenix_${name} STATIC ${ARG_SOURCES})
    add_library(phoenix::${name} ALIAS phoenix_${name})
    target_include_directories(phoenix_${name} PUBLIC "${PHOENIX_PUBLIC_INCLUDE}" "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(phoenix_${name} PUBLIC ${ARG_DEPS})
    target_compile_definitions(phoenix_${name} PUBLIC PHOENIX_ENGINE_VERSION="${PROJECT_VERSION}")
    phoenix_target_defaults(phoenix_${name})
endfunction()

phoenix_engine_library(core
    SOURCES core/arena.cpp core/vram_allocator.cpp core/job_system.cpp
    DEPS SDL3::SDL3-static)

if(PHOENIX_WITH_PHYSICS)
    phoenix_engine_library(physics
        SOURCES physics/physics_world.cpp physics/jolt_job_system.cpp
        DEPS phoenix::core Jolt)
endif()

phoenix_engine_library(framework
    SOURCES framework/widgets.cpp framework/engine_api.cpp
    DEPS phoenix::core)

phoenix_engine_library(renderer
    SOURCES renderer/gpu_renderer.cpp renderer/sdl_renderer.cpp renderer/vram_pool.cpp
    DEPS phoenix::core)

phoenix_engine_library(engine
    SOURCES shell/engine.cpp shell/devtools_bridge.cpp scripting/script_host.cpp
            storage/storage_service.cpp
    DEPS phoenix::core phoenix::physics phoenix::framework phoenix::renderer)

# Entry point compiled into every app executable (see cmake/PhoenixApp.cmake).
set(PHOENIX_APP_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shell/app_main.cpp" CACHE INTERNAL "")
