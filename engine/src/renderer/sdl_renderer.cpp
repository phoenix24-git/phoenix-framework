// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include <cmath>
#include <string>
#include <vector>

#include "core/log.h"
#include "renderer/renderer.h"

namespace phoenix {

namespace {

/// Fallback rasterizer on SDL_Renderer (WebGL2 in browsers without WebGPU).
class SdlRenderer final : public Renderer {
  public:
    explicit SdlRenderer(SDL_Window* window) : window_(window) {}
    ~SdlRenderer() override {
        if (renderer_)
            SDL_DestroyRenderer(renderer_);
    }

    bool init() {
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) {
            PHX_LOG_ERROR("Renderer: SDL_CreateRenderer failed: %s", SDL_GetError());
            return false;
        }
        SDL_SetRenderVSync(renderer_, 1);
        SDL_snprintf(name_, sizeof(name_), "sdl_renderer/%s", SDL_GetRendererName(renderer_));
        return true;
    }

    const char* name() const override { return name_; }
    RendererStats stats() const override { return {0, 0, 0, last_count_}; }

    bool capture_next_frame(const char* bmp_path) override {
        capture_path_ = bmp_path ? bmp_path : "";
        return true;
    }

    void render(std::span<const PhxDrawRect> rects, float logical_w, float logical_h,
                std::uint32_t clear) override {
        int pw = 0, ph = 0;
        SDL_GetCurrentRenderOutputSize(renderer_, &pw, &ph);
        const float sx = logical_w > 0 ? pw / logical_w : 1.0f;
        const float sy = logical_h > 0 ? ph / logical_h : 1.0f;
        SDL_SetRenderDrawColor(renderer_, (clear >> 24) & 0xFF, (clear >> 16) & 0xFF,
                               (clear >> 8) & 0xFF, clear & 0xFF);
        SDL_RenderClear(renderer_);

        vertices_.clear();
        indices_.clear();
        for (const PhxDrawRect& r : rects) {
            const SDL_FColor color{((r.rgba >> 24) & 0xFF) / 255.0f,
                                   ((r.rgba >> 16) & 0xFF) / 255.0f,
                                   ((r.rgba >> 8) & 0xFF) / 255.0f, (r.rgba & 0xFF) / 255.0f};
            const int base = static_cast<int>(vertices_.size());
            if (r.flags & PHX_DRAW_PARALLELOGRAM) {
                // Corners o, o+a, o+a+b, o+b with a = (w, h), b = (rotation, corner_radius).
                const float ox[4] = {r.x, r.x + r.w, r.x + r.w + r.rotation, r.x + r.rotation};
                const float oy[4] = {r.y, r.y + r.h, r.y + r.h + r.corner_radius,
                                     r.y + r.corner_radius};
                for (int i = 0; i < 4; ++i)
                    vertices_.push_back({{ox[i] * sx, oy[i] * sy}, color, {0, 0}});
            } else {
                const float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
                const float c = std::cos(r.rotation), s = std::sin(r.rotation);
                const float hx[4] = {-0.5f, 0.5f, 0.5f, -0.5f};
                const float hy[4] = {-0.5f, -0.5f, 0.5f, 0.5f};
                for (int i = 0; i < 4; ++i) {
                    const float lx = hx[i] * r.w, ly = hy[i] * r.h;
                    vertices_.push_back({{(cx + lx * c - ly * s) * sx, (cy + lx * s + ly * c) * sy},
                                         color,
                                         {0, 0}});
                }
            }
            for (int i : {0, 1, 2, 0, 2, 3})
                indices_.push_back(base + i);
        }
        if (!vertices_.empty()) {
            SDL_RenderGeometry(renderer_, nullptr, vertices_.data(),
                               static_cast<int>(vertices_.size()), indices_.data(),
                               static_cast<int>(indices_.size()));
        }
        last_count_ = static_cast<std::uint32_t>(rects.size());
        if (!capture_path_.empty()) {
            if (SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr)) {
                if (SDL_SaveBMP(shot, capture_path_.c_str()))
                    PHX_LOG_INFO("Renderer: saved %s", capture_path_.c_str());
                else
                    PHX_LOG_ERROR("Renderer: cannot save %s: %s", capture_path_.c_str(),
                                  SDL_GetError());
                SDL_DestroySurface(shot);
            }
            capture_path_.clear();
        }
        SDL_RenderPresent(renderer_);
    }

  private:
    SDL_Window* window_;
    SDL_Renderer* renderer_ = nullptr;
    std::vector<SDL_Vertex> vertices_;
    std::vector<int> indices_;
    std::uint32_t last_count_ = 0;
    std::string capture_path_;
    char name_[64] = "sdl_renderer";
};

}  // namespace

std::unique_ptr<Renderer> create_sdl_renderer(SDL_Window* window) {
    auto r = std::make_unique<SdlRenderer>(window);
    if (!r->init())
        return nullptr;
    return r;
}

}  // namespace phoenix
