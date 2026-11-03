// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <optional>
#include <vector>

#include "core/vram_allocator.h"
#include "phoenix/console_profile.h"

struct SDL_GPUDevice;
struct SDL_GPUBuffer;

namespace phoenix {

/// Console-style VRAM pool: reserves the profile's GPU memory budget at boot
/// as fixed pages (one SDL_GPUBuffer each) and sub-allocates from them.
class VramPool {
  public:
    ~VramPool();
    /// Reserves pages up to the budget. Stops early (and logs) if the GPU
    /// refuses; returns false only if not even one page could be created.
    bool init(SDL_GPUDevice* device, const ConsoleProfile& profile, unsigned usage_flags);
    void shutdown();

    std::optional<VramAllocation> allocate(std::uint64_t size, std::uint64_t alignment = 256);
    void free(const VramAllocation& a) { allocator_.free(a); }
    SDL_GPUBuffer* page_buffer(std::uint32_t page) const { return pages_[page]; }

    std::uint64_t reserved_bytes() const { return allocator_.capacity(); }
    std::uint64_t used_bytes() const { return allocator_.used(); }
    std::uint32_t page_count() const { return allocator_.page_count(); }

  private:
    bool grow();
    SDL_GPUDevice* device_ = nullptr;
    std::vector<SDL_GPUBuffer*> pages_;
    VramAllocator allocator_;
    ConsoleProfile profile_{};
    unsigned usage_ = 0;
};

}  // namespace phoenix
