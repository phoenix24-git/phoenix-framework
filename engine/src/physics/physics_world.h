// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "phoenix/script_abi.h"

struct SDL_Mutex;

namespace phoenix {

class JobSystem;

/// Jolt-backed rigid body world, constrained to the XY plane (2.5D) for the
/// MVP. Simulation runs one frame ahead on the job system: the main thread
/// calls `begin_step`, keeps rendering the previous snapshot, and swaps in the
/// new one when `poll_step` reports completion. The main thread never blocks.
class PhysicsWorld {
  public:
    PhysicsWorld();
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    bool init(JobSystem& jobs, std::uint32_t max_bodies = 4096);
    void shutdown();

    /// Thread-safe. The body is created at the start of the next step; the
    /// returned id is valid immediately.
    std::uint32_t add_box(float x, float y, float half_w, float half_h, float angle, bool dynamic,
                          std::uint32_t rgba);
    void add_impulse(std::uint32_t id, float ix, float iy);

    /// Kicks an asynchronous step. Returns false if a step is still running.
    bool begin_step(float dt);
    /// Returns true (and publishes the new snapshot) once the step finished.
    bool poll_step();
    /// Synchronous step: begin + wait. Used by tests and headless tools.
    void step(float dt);

    /// Latest published snapshot. Layout is the C ABI `PhxBody2D`, so scripts
    /// (C++ or C#) can read it as a zero-copy span.
    std::span<const PhxBody2D> bodies() const { return front_; }
    std::uint32_t body_count() const { return static_cast<std::uint32_t>(front_.size()); }
    std::uint64_t steps_completed() const { return steps_; }

  private:
    struct Impl;
    static void step_job(void* self);
    void run_step();

    std::unique_ptr<Impl> impl_;
    JobSystem* jobs_ = nullptr;
    SDL_Mutex* command_mutex_ = nullptr;
    std::vector<PhxBody2D> front_;
    std::vector<PhxBody2D> back_;
    float pending_dt_ = 0.0f;
    std::atomic<bool> stepping_{false};
    std::atomic<bool> step_ready_{false};
    std::uint64_t steps_ = 0;
};

}  // namespace phoenix
