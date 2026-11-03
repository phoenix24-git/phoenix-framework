// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "framework/engine_api.h"

#include <string>

#include "phoenix/engine.h"

namespace phoenix {

namespace {

const PhxBody2D* no_bodies(void*, std::uint32_t* count) {
    if (count)
        *count = 0;
    return nullptr;
}
std::uint32_t no_spawn(void*, float, float, float, float, float, std::uint32_t, std::uint32_t) {
    return 0;
}
void no_impulse(void*, std::uint32_t, float, float) {}
void no_log(void*, std::int32_t, const char*) {}
PhxDrawRect* no_rects(void*, std::uint32_t) {
    return nullptr;
}

// Safe defaults so widgets work before boot and in unit tests.
const PhxEngineApi kNullApi{PHX_SCRIPT_ABI_VERSION,
                            sizeof(PhxEngineApi),
                            nullptr,
                            no_log,
                            no_spawn,
                            no_impulse,
                            no_bodies,
                            no_rects};
const PhxEngineApi* g_api = &kNullApi;
PhxFrameInfo g_frame{};

}  // namespace

void set_engine_api(const PhxEngineApi* api) {
    g_api = api ? api : &kNullApi;
}
PhxFrameInfo& mutable_frame_info() {
    return g_frame;
}

const PhxEngineApi& engine_api() {
    return *g_api;
}
const PhxFrameInfo& frame_info() {
    return g_frame;
}

void log(std::string_view message) {
    const std::string text(message);
    g_api->log(g_api->engine, PHX_LOG_LEVEL_INFO, text.c_str());
}

}  // namespace phoenix
