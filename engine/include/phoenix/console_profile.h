// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// The "virtual console" hardware profile. Like a console SDK, Phoenix reserves
// fixed hardware budgets at boot instead of growing on demand, so frame times
// are predictable and out-of-memory happens at startup, not mid-game.
#pragma once

#include <cstdint>

namespace phoenix {

inline constexpr std::uint64_t kMiB = 1024ull * 1024ull;

struct ConsoleProfile {
    /// Total GPU memory budget managed by the VRAM pool (default 3.5 GiB).
    std::uint64_t vram_budget_bytes = 3584 * kMiB;
    /// VRAM is reserved in pages; WebGPU caps single buffers (maxBufferSize).
    std::uint64_t vram_page_bytes = 256 * kMiB;
    /// Reserve all VRAM pages at boot (console-style) instead of lazily.
    bool vram_preallocate = true;
    /// CPU arena reserved at boot for engine/frame allocations.
    std::uint64_t cpu_arena_bytes = 256 * kMiB;
    /// Engine worker threads (SDL3 threads); main thread is extra.
    std::uint32_t worker_threads = 6;
};

}  // namespace phoenix
