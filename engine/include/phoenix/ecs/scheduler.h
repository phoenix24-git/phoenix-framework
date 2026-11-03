// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// System scheduler. Systems declare what they read and write; within a stage the scheduler packs
// systems into waves where no two systems conflict (write/write or read/write on the same
// component) and runs each wave in parallel on the foundation WorkerThreadPool. Registration order
// is preserved for conflicting systems, so results are deterministic. Structural changes recorded
// into the per-system CommandBuffers are played back at the end of the stage, in registration
// order.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <vector>

#include "core/string/ustring.h"
#include "core/templates/local_vector.h"
#include "phoenix/ecs/command_buffer.h"
#include "phoenix/ecs/world.h"

namespace phoenix::ecs {

/// Unity-like player loop phases.
enum class Stage : std::uint8_t {
    Initialization,  ///< spawning, Awake/Start
    FixedUpdate,     ///< physics step(s)
    Update,          ///< gameplay (MetaBehaviour.Update, systems)
    LateUpdate,      ///< cameras, follow logic
    PostUpdate,      ///< transform hierarchy -> LocalToWorld
    PreRender,       ///< render extract
    Count,
};

template <typename T>
struct Read {};
template <typename T>
struct Write {};
/// Runs alone (touches arbitrary state, e.g. MetaBehaviour scripts, editor tools).
struct Exclusive {};

struct SystemContext {
    World& world;
    CommandBuffer& commands;
    float delta_time;
    double time;
    std::uint64_t frame;
    /// World tick when this system last ran: use with Query::changed<T>(ctx.last_run_tick).
    std::uint32_t last_run_tick;
};

using SystemFn = std::function<void(SystemContext&)>;

struct SystemDesc {
    String name;
    Stage stage = Stage::Update;
    LocalVector<ComponentId> reads;
    LocalVector<ComponentId> writes;
    bool exclusive = false;
    SystemFn run;
};

class Scheduler {
  public:
    /// `scheduler.add<Read<Velocity>, Write<LocalTransform>>("Move", Stage::Update, fn)`.
    template <typename... Access>
    void add(const String& name, Stage stage, SystemFn fn) {
        SystemDesc d;
        d.name = name;
        d.stage = stage;
        d.run = std::move(fn);
        (collect<Access>(d), ...);
        add(std::move(d));
    }
    void add(SystemDesc desc);

    /// Runs every system of `stage`. With `parallel` false (or no WorkerThreadPool) every
    /// system runs on the calling thread, in the same wave order.
    void run_stage(World& world, Stage stage, float delta_time, double time, std::uint64_t frame,
                   bool parallel = true);

    /// Wave index per system of a stage (exposed for tests and the profiler).
    LocalVector<int> waves(Stage stage) const;
    std::uint32_t system_count() const { return std::uint32_t(systems_.size()); }
    const SystemDesc& system(std::uint32_t i) const { return systems_[i]->desc; }

  private:
    template <typename A>
    static void collect(SystemDesc& d) {
        if constexpr (std::is_same_v<A, Exclusive>) {
            d.exclusive = true;
        } else {
            collect_access(d, static_cast<A*>(nullptr));
        }
    }
    template <typename T>
    static void collect_access(SystemDesc& d, Read<T>*) {
        d.reads.push_back(component_id<T>());
    }
    template <typename T>
    static void collect_access(SystemDesc& d, Write<T>*) {
        d.writes.push_back(component_id<T>());
    }

    static bool conflicts(const SystemDesc& a, const SystemDesc& b);

    struct Entry {
        SystemDesc desc;
        CommandBuffer commands;
        std::uint32_t last_run_tick = 0;
    };
    // std::vector of unique_ptr: entries hold std::function/CommandBuffer (not relocatable).
    std::vector<std::unique_ptr<Entry>> systems_;
};

}  // namespace phoenix::ecs
