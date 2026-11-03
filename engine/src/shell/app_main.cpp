// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Process entry point for Phoenix apps, compiled into each app executable.
// Uses SDL3 main callbacks so the same code drives a native loop and the
// browser's requestAnimationFrame loop.
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "phoenix/phoenix.h"
#include "phoenix_app_config.h"
#include "shell/engine.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/em_js.h>
// Reads an integer URL query parameter (e.g. ?vram=512), or -1 if absent.
// clang-format off
EM_JS(int, phx_query_int, (const char* name), {
    try {
        const v = new URLSearchParams(globalThis.location.search).get(UTF8ToString(Number(name)));
        return v === null ? -1 : (parseInt(v, 10) | 0);
    } catch (e) {
        return -1;
    }
});
// clang-format on
#endif

namespace {

phoenix::Engine* g_engine = nullptr;

bool parse_flag(const char* arg, const char* name, const char** value) {
    const size_t n = std::strlen(name);
    if (std::strncmp(arg, name, n) == 0 && arg[n] == '=') {
        *value = arg + n + 1;
        return true;
    }
    return false;
}

phoenix::EngineConfig make_config(int argc, char** argv) {
    phoenix::EngineConfig c;
    c.app_name = PHOENIX_APP_NAME;
    c.title = PHOENIX_APP_TITLE;
    c.build_mode = PHOENIX_BUILD_MODE;
    c.profile.vram_budget_bytes = PHOENIX_VRAM_MB * phoenix::kMiB;
    c.profile.vram_preallocate = PHOENIX_VRAM_PREALLOCATE != 0;
    c.profile.cpu_arena_bytes = PHOENIX_CPU_ARENA_MB * phoenix::kMiB;
    c.profile.worker_threads = PHOENIX_WORKER_THREADS;
#ifdef __EMSCRIPTEN__
    if (int v = phx_query_int("vram"); v > 0)
        c.profile.vram_budget_bytes = v * phoenix::kMiB;
    if (int v = phx_query_int("workers"); v >= 0)
        c.profile.worker_threads = static_cast<unsigned>(v);
    if (int v = phx_query_int("frames"); v > 0)
        c.max_frames = static_cast<unsigned>(v);
#endif
    for (int i = 1; i < argc; ++i) {
        const char* v = nullptr;
        if (std::strcmp(argv[i], "--headless") == 0)
            c.headless = true;
        else if (parse_flag(argv[i], "--frames", &v))
            c.max_frames = std::strtoull(v, nullptr, 10);
        else if (parse_flag(argv[i], "--script", &v))
            c.script_module = v;
        else if (parse_flag(argv[i], "--res", &v))
            c.res_dir = v;
        else if (parse_flag(argv[i], "--screenshot", &v))
            c.screenshot = v;
        else if (parse_flag(argv[i], "--fixed-dt", &v))
            c.fixed_dt = std::strtof(v, nullptr);
        else if (parse_flag(argv[i], "--vram-mb", &v))
            c.profile.vram_budget_bytes = std::strtoull(v, nullptr, 10) * phoenix::kMiB;
        else if (parse_flag(argv[i], "--workers", &v))
            c.profile.worker_threads = static_cast<unsigned>(std::strtoul(v, nullptr, 10));
    }
    if (c.script_module.empty()) {
        if (const char* env = SDL_getenv("PHOENIX_SCRIPT_MODULE"))
            c.script_module = env;
    }
    if (c.profile.vram_page_bytes > c.profile.vram_budget_bytes) {
        c.profile.vram_page_bytes = c.profile.vram_budget_bytes;
    }
    return c;
}

}  // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    g_engine = new phoenix::Engine();
    *appstate = g_engine;
    if (!g_engine->init(make_config(argc, argv))) {
        return SDL_APP_FAILURE;
    }
    phoenix_main();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void*, SDL_Event* event) {
    return g_engine->handle_event(*event) ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

SDL_AppResult SDL_AppIterate(void*) {
    return g_engine->iterate() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

void SDL_AppQuit(void*, SDL_AppResult) {
    delete g_engine;
    g_engine = nullptr;
}
