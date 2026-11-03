// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cstring>

#include "core/log.h"
#include "renderer/renderer.h"
#include "renderer/shaders.h"
#include "renderer/vram_pool.h"

namespace phoenix {

namespace {

constexpr std::uint32_t kFramesInFlight = 3;
constexpr std::uint32_t kMaxRectsPerFrame = 65536;  // 2 MiB of instances per frame

class GpuRenderer final : public Renderer {
  public:
    explicit GpuRenderer(SDL_Window* window) : window_(window) {}
    ~GpuRenderer() override {
        if (!device_)
            return;
        SDL_WaitForGPUIdle(device_);
        if (transfer_)
            SDL_ReleaseGPUTransferBuffer(device_, transfer_);
        if (pipeline_)
            SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
        vram_.shutdown();
        SDL_ReleaseWindowFromGPUDevice(device_, window_);
        SDL_DestroyGPUDevice(device_);
    }

    bool init(const ConsoleProfile& profile) {
        device_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_WGSL, /*debug=*/false, nullptr);
        if (!device_) {
            PHX_LOG_WARN("Renderer: no WGSL-capable SDL_GPU device: %s", SDL_GetError());
            return false;
        }
        SDL_snprintf(name_, sizeof(name_), "sdl_gpu/%s", SDL_GetGPUDeviceDriver(device_));
        if (!SDL_ClaimWindowForGPUDevice(device_, window_)) {
            PHX_LOG_WARN("Renderer: claim window failed: %s", SDL_GetError());
            return false;
        }
        if (!create_pipeline()) {
            return false;
        }
        if (!vram_.init(device_, profile, SDL_GPU_BUFFERUSAGE_VERTEX)) {
            PHX_LOG_WARN("Renderer: VRAM pool unavailable");
            return false;
        }
        for (auto& slot : frames_) {
            auto a = vram_.allocate(kMaxRectsPerFrame * sizeof(PhxDrawRect));
            if (!a)
                return false;
            slot = *a;
        }
        SDL_GPUTransferBufferCreateInfo tinfo{};
        tinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tinfo.size = kMaxRectsPerFrame * sizeof(PhxDrawRect);
        transfer_ = SDL_CreateGPUTransferBuffer(device_, &tinfo);
        return transfer_ != nullptr;
    }

    const char* name() const override { return name_; }

    RendererStats stats() const override {
        return {vram_.reserved_bytes(), vram_.used_bytes(), vram_.page_count(), last_count_};
    }

    void render(std::span<const PhxDrawRect> rects, float logical_w, float logical_h,
                std::uint32_t clear_rgba) override {
        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device_);
        if (!cmd)
            return;
        const auto count =
            static_cast<std::uint32_t>(std::min<std::size_t>(rects.size(), kMaxRectsPerFrame));
        const VramAllocation& slot = frames_[frame_ % kFramesInFlight];
        ++frame_;
        last_count_ = count;

        if (count > 0) {
            // Blit the display list into the transfer buffer, then copy it into
            // this frame's slot of the preallocated VRAM page.
            void* mapped = SDL_MapGPUTransferBuffer(device_, transfer_, /*cycle=*/true);
            std::memcpy(mapped, rects.data(), count * sizeof(PhxDrawRect));
            SDL_UnmapGPUTransferBuffer(device_, transfer_);
            SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
            SDL_GPUTransferBufferLocation src{transfer_, 0};
            SDL_GPUBufferRegion dst{vram_.page_buffer(slot.page), static_cast<Uint32>(slot.offset),
                                    static_cast<Uint32>(count * sizeof(PhxDrawRect))};
            SDL_UploadToGPUBuffer(copy, &src, &dst, /*cycle=*/false);
            SDL_EndGPUCopyPass(copy);
        }

        SDL_GPUTexture* swapchain = nullptr;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window_, &swapchain, nullptr, nullptr) ||
            !swapchain) {
            SDL_SubmitGPUCommandBuffer(cmd);
            return;
        }
        SDL_GPUColorTargetInfo target{};
        target.texture = swapchain;
        target.clear_color = {((clear_rgba >> 24) & 0xFF) / 255.0f,
                              ((clear_rgba >> 16) & 0xFF) / 255.0f,
                              ((clear_rgba >> 8) & 0xFF) / 255.0f, (clear_rgba & 0xFF) / 255.0f};
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
        if (count > 0) {
            SDL_BindGPUGraphicsPipeline(pass, pipeline_);
            const float viewport[4] = {logical_w, logical_h, 0.0f, 0.0f};
            SDL_PushGPUVertexUniformData(cmd, 0, viewport, sizeof(viewport));
            SDL_GPUBufferBinding binding{vram_.page_buffer(slot.page),
                                         static_cast<Uint32>(slot.offset)};
            SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
            SDL_DrawGPUPrimitives(pass, 6, count, 0, 0);
        }
        SDL_EndGPURenderPass(pass);
        SDL_SubmitGPUCommandBuffer(cmd);
    }

  private:
    bool create_pipeline() {
        SDL_GPUShaderCreateInfo vs{};
        vs.code = reinterpret_cast<const Uint8*>(shaders::kRectWgsl);
        vs.code_size = sizeof(shaders::kRectWgsl) - 1;
        vs.entrypoint = "vs_main";
        vs.format = SDL_GPU_SHADERFORMAT_WGSL;
        vs.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vs.num_uniform_buffers = 1;
        SDL_GPUShaderCreateInfo fs = vs;
        fs.entrypoint = "fs_main";
        fs.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs.num_uniform_buffers = 0;
        SDL_GPUShader* vshader = SDL_CreateGPUShader(device_, &vs);
        SDL_GPUShader* fshader = SDL_CreateGPUShader(device_, &fs);
        if (!vshader || !fshader) {
            PHX_LOG_WARN("Renderer: shader creation failed: %s", SDL_GetError());
            return false;
        }

        SDL_GPUColorTargetDescription color{};
        color.format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
        color.blend_state.enable_blend = true;
        color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUVertexBufferDescription vb{};
        vb.slot = 0;
        vb.pitch = sizeof(PhxDrawRect);
        vb.input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
        SDL_GPUVertexAttribute attrs[6] = {
            {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(PhxDrawRect, x)},
            {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(PhxDrawRect, w)},
            {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(PhxDrawRect, rgba)},
            {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, offsetof(PhxDrawRect, rotation)},
            {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, offsetof(PhxDrawRect, corner_radius)},
            {5, 0, SDL_GPU_VERTEXELEMENTFORMAT_UINT, offsetof(PhxDrawRect, flags)},
        };

        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = vshader;
        info.fragment_shader = fshader;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.vertex_input_state.vertex_buffer_descriptions = &vb;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = attrs;
        info.vertex_input_state.num_vertex_attributes = 6;
        info.target_info.color_target_descriptions = &color;
        info.target_info.num_color_targets = 1;
        pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
        SDL_ReleaseGPUShader(device_, vshader);
        SDL_ReleaseGPUShader(device_, fshader);
        if (!pipeline_) {
            PHX_LOG_WARN("Renderer: pipeline creation failed: %s", SDL_GetError());
        }
        return pipeline_ != nullptr;
    }

    SDL_Window* window_;
    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
    SDL_GPUTransferBuffer* transfer_ = nullptr;
    VramPool vram_;
    VramAllocation frames_[kFramesInFlight]{};
    std::uint64_t frame_ = 0;
    std::uint32_t last_count_ = 0;
    char name_[64] = "sdl_gpu";
};

}  // namespace

std::unique_ptr<Renderer> create_gpu_renderer(SDL_Window* window, const ConsoleProfile& profile) {
    auto renderer = std::make_unique<GpuRenderer>(window);
    if (!renderer->init(profile)) {
        return nullptr;
    }
    return renderer;
}

}  // namespace phoenix
