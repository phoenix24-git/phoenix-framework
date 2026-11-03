// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "shell/devtools_bridge.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>

#include "shell/engine.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/em_js.h>

// clang-format off
EM_JS(void, phx_js_post, (const char* kind, const char* json), {
    // wasm64: pointers arrive as BigInt; UTF8ToString wants a Number.
    const g = globalThis;
    const type = UTF8ToString(Number(kind));
    let data;
    try { data = JSON.parse(UTF8ToString(Number(json))); } catch (e) { return; }
    if (!g.__phoenixChannel && typeof BroadcastChannel !== 'undefined') {
        g.__phoenixChannel = new BroadcastChannel('phoenix-devtools');
    }
    if (g.__phoenixChannel) g.__phoenixChannel.postMessage({ type, data, ts: Date.now() });
    if (typeof window !== 'undefined') {
        g.phoenix = g.phoenix || {};
        if (type === 'stats') g.phoenix.stats = data;
        if (type === 'hello') g.phoenix.info = data;
        if (g.phoenix.onMessage) g.phoenix.onMessage(type, data);
    }
});
// clang-format on
#endif

namespace phoenix::devtools {

namespace {

SDL_LogOutputFunction g_default_output = nullptr;
void* g_default_userdata = nullptr;

[[maybe_unused]] void escape_json(const char* in, char* out, size_t cap) {
    size_t o = 0;
    for (const char* p = in; *p && o + 7 < cap; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c == '"' || c == '\\') {
            out[o++] = '\\';
            out[o++] = static_cast<char>(c);
        } else if (c < 0x20) {
            o += SDL_snprintf(out + o, cap - o, "\\u%04x", c);
        } else {
            out[o++] = static_cast<char>(c);
        }
    }
    out[o] = '\0';
}

void SDLCALL forward_log([[maybe_unused]] void* userdata, int category, SDL_LogPriority priority,
                         const char* message) {
    if (g_default_output) {
        g_default_output(g_default_userdata, category, priority, message);
    }
#ifdef __EMSCRIPTEN__
    char escaped[2048];
    escape_json(message, escaped, sizeof(escaped));
    char json[2200];
    SDL_snprintf(json, sizeof(json), "{\"level\":%d,\"message\":\"%s\"}",
                 static_cast<int>(priority), escaped);
    phx_js_post("log", json);
#endif
}

}  // namespace

void install_log_forwarding() {
    if (!g_default_output) {
        g_default_output = SDL_GetDefaultLogOutputFunction();
        SDL_SetLogOutputFunction(&forward_log, nullptr);
    }
}

void announce(const char* app_name, const char* renderer, const ConsoleProfile& profile) {
#ifdef __EMSCRIPTEN__
    char json[512];
    SDL_snprintf(json, sizeof(json),
                 "{\"app\":\"%s\",\"renderer\":\"%s\",\"engine\":\"%s\",\"vramBudgetMiB\":%llu,"
                 "\"workers\":%u,\"wasm64\":%s}",
                 app_name, renderer, PHOENIX_ENGINE_VERSION,
                 static_cast<unsigned long long>(profile.vram_budget_bytes / kMiB),
                 profile.worker_threads, sizeof(void*) == 8 ? "true" : "false");
    phx_js_post("hello", json);
#else
    (void)app_name;
    (void)renderer;
    (void)profile;
#endif
}

void publish(const EngineStats& s, const char* renderer, const char* storage_backend) {
#ifdef __EMSCRIPTEN__
    char json[1024];
    SDL_snprintf(
        json, sizeof(json),
        "{\"fps\":%.1f,\"frameMs\":%.2f,\"frame\":%llu,\"bodies\":%u,\"physicsSteps\":%llu,"
        "\"workers\":%u,\"jobs\":%llu,\"arenaHighWater\":%llu,\"arenaCapacity\":%llu,"
        "\"vramReserved\":%llu,\"vramUsed\":%llu,\"vramPages\":%u,\"rects\":%u,"
        "\"renderer\":\"%s\",\"storage\":\"%s\"}",
        s.fps, s.frame_ms, static_cast<unsigned long long>(s.frame), s.bodies,
        static_cast<unsigned long long>(s.physics_steps), s.workers,
        static_cast<unsigned long long>(s.jobs_executed),
        static_cast<unsigned long long>(s.arena_high_water),
        static_cast<unsigned long long>(s.arena_capacity),
        static_cast<unsigned long long>(s.renderer.vram_reserved),
        static_cast<unsigned long long>(s.renderer.vram_used), s.renderer.vram_pages,
        s.renderer.rects_last_frame, renderer, storage_backend);
    phx_js_post("stats", json);
#else
    (void)s;
    (void)renderer;
    (void)storage_backend;
#endif
}

}  // namespace phoenix::devtools
