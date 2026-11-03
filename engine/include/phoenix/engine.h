// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// C++ scripting API. C++ scripts use exactly the same `PhxEngineApi` table as
// C# scripts, so both languages see identical, zero-copy engine services.
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "phoenix/script_abi.h"

namespace phoenix {

/// The engine service table (valid after the engine booted).
const PhxEngineApi& engine_api();
/// Input and timing for the current frame.
const PhxFrameInfo& frame_info();

inline std::uint32_t spawn_box(float x, float y, float half_w, float half_h, float angle = 0.0f,
                               bool dynamic = true, std::uint32_t rgba = 0x4F8EF7FFu) {
    const auto& api = engine_api();
    return api.spawn_box(api.engine, x, y, half_w, half_h, angle, dynamic ? 1u : 0u, rgba);
}

inline void apply_impulse(std::uint32_t body, float ix, float iy) {
    const auto& api = engine_api();
    api.apply_impulse(api.engine, body, ix, iy);
}

/// Zero-copy view of the latest physics snapshot.
inline std::span<const PhxBody2D> bodies() {
    const auto& api = engine_api();
    std::uint32_t count = 0;
    const PhxBody2D* data = api.bodies(api.engine, &count);
    return {data, count};
}

void log(std::string_view message);

/// Runs a scene file (Godot .tscn syntax with Unity-style components, see
/// phoenix/scene/scene_file.h): call it from phoenix_main() instead of (or alongside) runApp().
/// The path is virtual: "res://scenes/main.pscn" maps to the app's res/ folder.
bool runScene(std::string_view path);

}  // namespace phoenix
