// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace phoenix {

/// Linear (bump) allocator over one block reserved at boot.
///
/// Not thread-safe by design: each thread/frame owns its arena. Reset is O(1),
/// which makes it ideal for per-frame scratch data (display lists, commands).
class LinearArena {
  public:
    LinearArena() = default;
    ~LinearArena();
    LinearArena(const LinearArena&) = delete;
    LinearArena& operator=(const LinearArena&) = delete;

    /// Reserves `capacity` bytes. Returns false if the host cannot provide them.
    bool reserve(std::size_t capacity);
    void release();

    /// Returns nullptr when the arena is exhausted (never throws).
    void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));

    template <typename T, typename... Args>
    T* make(Args&&... args) {
        static_assert(std::is_trivially_destructible_v<T>, "arena objects are never destroyed");
        void* p = allocate(sizeof(T), alignof(T));
        return p ? new (p) T(static_cast<Args&&>(args)...) : nullptr;
    }

    struct Marker {
        std::size_t offset;
    };
    Marker mark() const { return {offset_}; }
    void rewind(Marker m) { offset_ = m.offset <= offset_ ? m.offset : offset_; }
    void reset() { offset_ = 0; }

    std::size_t capacity() const { return capacity_; }
    std::size_t used() const { return offset_; }
    std::size_t high_water() const { return high_water_; }

  private:
    std::byte* base_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t offset_ = 0;
    std::size_t high_water_ = 0;
};

}  // namespace phoenix
