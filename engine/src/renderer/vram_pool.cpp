// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "renderer/vram_pool.h"

#include <SDL3/SDL_gpu.h>

#include "core/log.h"

namespace phoenix {

VramPool::~VramPool() {
    shutdown();
}

bool VramPool::init(SDL_GPUDevice* device, const ConsoleProfile& profile, unsigned usage_flags) {
    device_ = device;
    profile_ = profile;
    usage_ = usage_flags;
    const auto max_pages = static_cast<std::uint32_t>(
        (profile.vram_budget_bytes + profile.vram_page_bytes - 1) / profile.vram_page_bytes);
    const std::uint32_t boot_pages = profile.vram_preallocate ? max_pages : 1;
    for (std::uint32_t i = 0; i < boot_pages; ++i) {
        SDL_GPUBufferCreateInfo info{};
        info.usage = usage_;
        info.size = static_cast<Uint32>(profile.vram_page_bytes);
        SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device_, &info);
        if (!buffer) {
            PHX_LOG_WARN("VRAM: page %u/%u refused by GPU (%s); continuing with %u MiB", i + 1,
                         boot_pages, SDL_GetError(),
                         static_cast<unsigned>(i * (profile.vram_page_bytes / kMiB)));
            break;
        }
        pages_.push_back(buffer);
    }
    allocator_.init(static_cast<std::uint32_t>(pages_.size()), profile.vram_page_bytes);
    PHX_LOG_INFO("VRAM: reserved %u x %u MiB pages = %u MiB (budget %u MiB, %s)",
                 static_cast<unsigned>(pages_.size()),
                 static_cast<unsigned>(profile.vram_page_bytes / kMiB),
                 static_cast<unsigned>(reserved_bytes() / kMiB),
                 static_cast<unsigned>(profile.vram_budget_bytes / kMiB),
                 profile.vram_preallocate ? "preallocated" : "on demand");
    return !pages_.empty();
}

bool VramPool::grow() {
    if (reserved_bytes() + profile_.vram_page_bytes > profile_.vram_budget_bytes) {
        return false;  // budget exhausted: the console has no more VRAM
    }
    SDL_GPUBufferCreateInfo info{};
    info.usage = usage_;
    info.size = static_cast<Uint32>(profile_.vram_page_bytes);
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device_, &info);
    if (!buffer) {
        return false;
    }
    pages_.push_back(buffer);
    allocator_.add_page();
    return true;
}

std::optional<VramAllocation> VramPool::allocate(std::uint64_t size, std::uint64_t alignment) {
    if (auto a = allocator_.allocate(size, alignment)) {
        return a;
    }
    if (grow()) {
        if (auto a = allocator_.allocate(size, alignment)) {
            return a;
        }
    }
    PHX_LOG_WARN("VRAM: allocation of %llu bytes failed (used %llu / %llu)",
                 static_cast<unsigned long long>(size),
                 static_cast<unsigned long long>(used_bytes()),
                 static_cast<unsigned long long>(reserved_bytes()));
    return std::nullopt;
}

void VramPool::shutdown() {
    for (SDL_GPUBuffer* b : pages_) {
        SDL_ReleaseGPUBuffer(device_, b);
    }
    pages_.clear();
    allocator_.init(0, profile_.vram_page_bytes);
}

}  // namespace phoenix
