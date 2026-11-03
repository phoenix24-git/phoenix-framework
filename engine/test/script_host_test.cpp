// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Loads a real NativeAOT-compiled C# script module and drives it through the
// Script ABI. Enabled when PHOENIX_TEST_SCRIPT_MODULE points at the published
// examples/csharp_script shared library (CI builds it first).
#include "scripting/script_host.h"

#include <SDL3/SDL_stdinc.h>
#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "phoenix/script_abi.h"

namespace {

struct FakeEngine {
    std::vector<PhxBody2D> bodies;
    std::vector<PhxDrawRect> rects;
    std::vector<std::string> logs;
    std::uint32_t spawned = 0;
};

void fake_log(void* e, std::int32_t, const char* text) {
    static_cast<FakeEngine*>(e)->logs.emplace_back(text);
}
std::uint32_t fake_spawn(void* e, float x, float y, float hw, float hh, float a, std::uint32_t dyn,
                         std::uint32_t rgba) {
    auto* fe = static_cast<FakeEngine*>(e);
    const auto id = fe->spawned++;
    fe->bodies.push_back(PhxBody2D{x, y, a, hw, hh, id, rgba, dyn});
    return id;
}
void fake_impulse(void*, std::uint32_t, float, float) {}
const PhxBody2D* fake_bodies(void* e, std::uint32_t* count) {
    auto* fe = static_cast<FakeEngine*>(e);
    *count = static_cast<std::uint32_t>(fe->bodies.size());
    return fe->bodies.data();
}
PhxDrawRect* fake_rects(void* e, std::uint32_t count) {
    auto* fe = static_cast<FakeEngine*>(e);
    fe->rects.assign(count, PhxDrawRect{});
    return fe->rects.data();
}

}  // namespace

TEST_SUITE("ScriptHost (C# NativeAOT)") {
    TEST_CASE("missing module fails cleanly") {
        phoenix::ScriptHost host;
        PhxEngineApi api{};
        CHECK_FALSE(host.load("/nonexistent/phoenix_script.so", &api));
        CHECK_FALSE(host.loaded());
    }

    TEST_CASE("C# script spawns bodies and writes draw rects through the ABI" *
              doctest::skip(SDL_getenv("PHOENIX_TEST_SCRIPT_MODULE") == nullptr)) {
        FakeEngine fake;
        PhxEngineApi api{
            PHX_SCRIPT_ABI_VERSION, sizeof(PhxEngineApi), &fake,     fake_log, fake_spawn,
            fake_impulse,           fake_bodies,          fake_rects};
        phoenix::ScriptHost host;
        REQUIRE(host.load(SDL_getenv("PHOENIX_TEST_SCRIPT_MODULE"), &api));
        REQUIRE_FALSE(fake.logs.empty());
        CHECK(fake.logs.front().find("RainScript") != std::string::npos);

        PhxFrameInfo frame{};
        frame.viewport_width = 800;
        frame.viewport_height = 600;
        for (int i = 0; i < 4; ++i) {
            frame.time_seconds = i * 0.5;
            frame.frame_index = static_cast<std::uint32_t>(i);
            host.update(frame);
        }
        CHECK(fake.spawned == 4);
        // The script read the bodies span and wrote one HUD rect per body.
        REQUIRE(fake.rects.size() == fake.bodies.size());
        CHECK(fake.rects[0].rgba == fake.bodies[0].rgba);
        CHECK(fake.rects[0].y == doctest::Approx(588.0f));
        host.unload();
        CHECK_FALSE(host.loaded());
    }

    TEST_CASE("ABI version mismatch is rejected by the C# side" *
              doctest::skip(SDL_getenv("PHOENIX_TEST_SCRIPT_MODULE") == nullptr)) {
        FakeEngine fake;
        PhxEngineApi api{PHX_SCRIPT_ABI_VERSION + 1,
                         sizeof(PhxEngineApi),
                         &fake,
                         fake_log,
                         fake_spawn,
                         fake_impulse,
                         fake_bodies,
                         fake_rects};
        phoenix::ScriptHost host;
        CHECK_FALSE(host.load(SDL_getenv("PHOENIX_TEST_SCRIPT_MODULE"), &api));
    }
}
