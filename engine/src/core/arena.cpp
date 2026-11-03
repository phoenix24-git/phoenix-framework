// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "core/arena.h"

#include <SDL3/SDL_stdinc.h>

namespace phoenix {

namespace {
constexpr std::size_t kBlockAlignment = 64;  // cache line
}

LinearArena::~LinearArena() {
    release();
}

bool LinearArena::reserve(std::size_t capacity) {
    release();
    base_ = static_cast<std::byte*>(SDL_aligned_alloc(kBlockAlignment, capacity));
    if (!base_) {
        return false;
    }
    capacity_ = capacity;
    offset_ = 0;
    high_water_ = 0;
    return true;
}

void LinearArena::release() {
    if (base_) {
        SDL_aligned_free(base_);
    }
    base_ = nullptr;
    capacity_ = offset_ = high_water_ = 0;
}

void* LinearArena::allocate(std::size_t size, std::size_t alignment) {
    if (!base_ || alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return nullptr;
    }
    const auto addr = reinterpret_cast<std::uintptr_t>(base_) + offset_;
    const std::size_t padding = (alignment - (addr & (alignment - 1))) & (alignment - 1);
    if (padding > capacity_ - offset_ || size > capacity_ - offset_ - padding) {
        return nullptr;
    }
    void* result = base_ + offset_ + padding;
    offset_ += padding + size;
    if (offset_ > high_water_) {
        high_water_ = offset_;
    }
    return result;
}

}  // namespace phoenix
