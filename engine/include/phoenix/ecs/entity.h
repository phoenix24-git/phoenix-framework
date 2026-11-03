// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <functional>

namespace phoenix::ecs {

/// 64-bit entity handle: slot index + generation. A destroyed entity's slot is reused with a
/// new generation, so stale handles are detected instead of aliasing a different entity.
struct Entity {
    std::uint32_t index = ~0u;
    std::uint32_t generation = 0;

    constexpr bool valid() const { return index != ~0u; }
    constexpr std::uint64_t bits() const { return (std::uint64_t(generation) << 32) | index; }
    friend constexpr bool operator==(Entity a, Entity b) {
        return a.index == b.index && a.generation == b.generation;
    }
    static constexpr Entity null() { return {}; }
};

}  // namespace phoenix::ecs

template <>
struct std::hash<phoenix::ecs::Entity> {
    std::size_t operator()(phoenix::ecs::Entity e) const noexcept {
        return std::hash<std::uint64_t>{}(e.bits());
    }
};
