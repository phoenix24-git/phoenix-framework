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

# Entry point compiled into every app executable (see cmake/PhoenixApp.cmake).
set(PHOENIX_APP_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shell/app_main.cpp" CACHE INTERNAL "")

# --- ECS + Unity-style scene layer, built on the foundation (phoenix_core) ------------------
if(PHOENIX_WITH_FOUNDATION)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(PHOENIX_COMPONENTS_GEN "${CMAKE_CURRENT_BINARY_DIR}/generated")
    add_custom_command(
        OUTPUT "${PHOENIX_COMPONENTS_GEN}/phoenix/scene/components.gen.h"
               "${PHOENIX_COMPONENTS_GEN}/phoenix/scene/components.gen.cpp"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../components/gen_components.py"
                --out "${PHOENIX_COMPONENTS_GEN}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../components/catalogue.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/../components/gen_components.py"
        COMMENT "Generating built-in components from components/catalogue.py"
        VERBATIM)

    phoenix_engine_library(ecs
        SOURCES ecs/component.cpp ecs/world.cpp ecs/scheduler.cpp
        DEPS phoenix::foundation)
    if(NOT MSVC)
        # PHX_FIELD uses offsetof on components holding foundation String/Vector members
        # (not "standard layout"); every supported compiler handles non-virtual types.
        target_compile_options(phoenix_ecs PUBLIC -Wno-invalid-offsetof)
    endif()

    # Godot-style virtual file system (res://, user://, uid://, packs).
    phoenix_engine_library(vfs
        SOURCES vfs/vfs.cpp
        DEPS phoenix::foundation)

    if(NOT EMSCRIPTEN)
        # Packs a project folder into a .pck (res://) for export and DLC.
        add_executable(phoenix_pack "${CMAKE_CURRENT_SOURCE_DIR}/../tools/phoenix_pack.cpp")
        target_link_libraries(phoenix_pack PRIVATE phoenix::foundation)
        phoenix_target_defaults(phoenix_pack)
    endif()

    phoenix_engine_library(scene
        SOURCES scene/scene.cpp scene/scene_file.cpp scene/scene_renderer.cpp
                "${PHOENIX_COMPONENTS_GEN}/phoenix/scene/components.gen.cpp"
        DEPS phoenix::ecs)
    target_include_directories(phoenix_scene PUBLIC "${PHOENIX_COMPONENTS_GEN}")
endif()

# --- The embedder (window, frame loop, scripts, scene) -----------------------------------------
phoenix_engine_library(engine
    SOURCES shell/engine.cpp shell/devtools_bridge.cpp scripting/script_host.cpp
            storage/storage_service.cpp
    DEPS phoenix::core phoenix::physics phoenix::framework phoenix::renderer)
if(PHOENIX_WITH_FOUNDATION)
    target_link_libraries(phoenix_engine PUBLIC phoenix::scene phoenix::vfs)
    target_compile_definitions(phoenix_engine PRIVATE PHOENIX_HAS_SCENE=1)
endif()
