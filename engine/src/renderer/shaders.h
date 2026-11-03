// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// WGSL shaders for the SDL_GPU WebGPU backend. Binding layout follows SDL_GPU:
// vertex uniforms live in @group(1), fragment resources in @group(2/3).
#pragma once

namespace phoenix::shaders {

// One instance per PhxDrawRect (32-byte stride, see script_abi.h).
inline constexpr char kRectWgsl[] = R"WGSL(
struct Viewport { size: vec4<f32> };
@group(1) @binding(0) var<uniform> viewport: Viewport;

struct VsIn {
    @builtin(vertex_index) vi: u32,
    @location(0) pos: vec2<f32>,
    @location(1) size: vec2<f32>,
    @location(2) abgr: vec4<f32>,
    @location(3) rotation: f32,
    @location(4) corner_radius: f32,
    @location(5) flags: u32,
};
struct VsOut {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec4<f32>,
};

@vertex
fn vs_main(in: VsIn) -> VsOut {
    var corners = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 0.0), vec2<f32>(0.0, 1.0),
        vec2<f32>(0.0, 1.0), vec2<f32>(1.0, 0.0), vec2<f32>(1.0, 1.0));
    let k = corners[in.vi];
    var p: vec2<f32>;
    if ((in.flags & 1u) != 0u) {
        // PHX_DRAW_PARALLELOGRAM: corner pos, edges a = size and b = (rotation, corner_radius).
        p = in.pos + k.x * in.size + k.y * vec2<f32>(in.rotation, in.corner_radius);
    } else {
        let local = (k - vec2<f32>(0.5, 0.5)) * in.size;
        let c = cos(in.rotation);
        let s = sin(in.rotation);
        p = in.pos + in.size * 0.5 + vec2<f32>(local.x * c - local.y * s, local.x * s + local.y * c);
    }
    var out: VsOut;
    out.position = vec4<f32>(p.x / viewport.size.x * 2.0 - 1.0, 1.0 - p.y / viewport.size.y * 2.0, 0.0, 1.0);
    // PhxDrawRect.rgba is 0xRRGGBBAA; read as little-endian bytes it arrives as ABGR.
    out.color = in.abgr.wzyx;
    return out;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    return in.color;
}
)WGSL";

}  // namespace phoenix::shaders
