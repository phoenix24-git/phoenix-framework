// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <memory>
#include <span>

#include "phoenix/console_profile.h"
#include "phoenix/script_abi.h"

struct SDL_Window;

namespace phoenix {

struct RendererStats {
    std::uint64_t vram_reserved = 0;  // bytes of GPU memory held by the VRAM pool
    std::uint64_t vram_used = 0;      // bytes sub-allocated inside the pool
    std::uint32_t vram_pages = 0;
    std::uint32_t rects_last_frame = 0;
};

/// Rasterizes a frame's display list (Flutter's "raster" stage).
class Renderer {
  public:
    virtual ~Renderer() = default;
    /// "sdl_gpu/webgpu", "sdl_gpu/vulkan", "sdl_renderer/opengles2", ...
    virtual const char* name() const = 0;
    virtual void render(std::span<const PhxDrawRect> rects, float logical_w, float logical_h,
                        std::uint32_t clear_rgba) = 0;
    virtual RendererStats stats() const = 0;
};

/// SDL_GPU backend (WebGPU on the web via SDL_wgpu). Returns nullptr if no
/// suitable GPU device exists; the caller then falls back.
std::unique_ptr<Renderer> create_gpu_renderer(SDL_Window* window, const ConsoleProfile& profile);

/// SDL_Renderer backend (WebGL2 / GL / D3D / Metal). Universal fallback.
std::unique_ptr<Renderer> create_sdl_renderer(SDL_Window* window);

}  // namespace phoenix
