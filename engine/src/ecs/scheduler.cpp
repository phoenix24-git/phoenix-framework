// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/ecs/scheduler.h"

#include <algorithm>

#include "core/object/worker_thread_pool.h"

namespace phoenix::ecs {

namespace {
bool overlap(const LocalVector<ComponentId>& a, const LocalVector<ComponentId>& b) {
    for (ComponentId x : a) {
        if (b.find(x) >= 0)
            return true;
    }
    return false;
}
}  // namespace

void Scheduler::add(SystemDesc desc) {
    auto e = std::make_unique<Entry>();
    e->desc = std::move(desc);
    systems_.push_back(std::move(e));
}

bool Scheduler::conflicts(const SystemDesc& a, const SystemDesc& b) {
    if (a.exclusive || b.exclusive)
        return true;
    return overlap(a.writes, b.writes) || overlap(a.writes, b.reads) || overlap(a.reads, b.writes);
}

LocalVector<int> Scheduler::waves(Stage stage) const {
    // A system runs in the wave after the last earlier system it conflicts with, so conflicting
    // systems keep registration order while independent ones run side by side.
    LocalVector<int> out;
    out.resize(std::uint32_t(systems_.size()));
    for (std::uint32_t i = 0; i < out.size(); ++i) {
        out[i] = -1;
        if (systems_[i]->desc.stage != stage)
            continue;
        int wave = 0;
        for (std::uint32_t j = 0; j < i; ++j) {
            if (out[j] >= 0 && conflicts(systems_[i]->desc, systems_[j]->desc)) {
                wave = std::max(wave, out[j] + 1);
            }
        }
        out[i] = wave;
    }
    return out;
}

void Scheduler::run_stage(World& world, Stage stage, float delta_time, double time,
                          std::uint64_t frame, bool parallel) {
    const LocalVector<int> wave_of = waves(stage);
    int wave_count = 0;
    for (int w : wave_of)
        wave_count = std::max(wave_count, w + 1);
    const std::uint32_t tick = world.advance_tick();

    struct Batch {
        World* world;
        float dt;
        double time;
        std::uint64_t frame;
        LocalVector<Entry*> entries;
        static void run(void* self, std::uint32_t i) {
            Batch& b = *static_cast<Batch*>(self);
            Entry& e = *b.entries[i];
            SystemContext ctx{*b.world, e.commands, b.dt, b.time, b.frame, e.last_run_tick};
            e.desc.run(ctx);
        }
    };
    WorkerThreadPool* pool = parallel ? WorkerThreadPool::get_singleton() : nullptr;
    for (int wave = 0; wave < wave_count; ++wave) {
        Batch batch{&world, delta_time, time, frame, {}};
        for (std::uint32_t i = 0; i < wave_of.size(); ++i) {
            if (wave_of[i] == wave)
                batch.entries.push_back(systems_[i].get());
        }
        if (pool && batch.entries.size() > 1) {
            const WorkerThreadPool::GroupID group = pool->add_native_group_task(
                &Batch::run, &batch, int(batch.entries.size()), -1, true, "ecs.systems");
            pool->wait_for_group_task_completion(group);
        } else {
            for (std::uint32_t i = 0; i < batch.entries.size(); ++i)
                Batch::run(&batch, i);
        }
    }
    for (std::uint32_t i = 0; i < wave_of.size(); ++i) {
        if (wave_of[i] < 0)
            continue;
        systems_[i]->last_run_tick = tick;
        systems_[i]->commands.playback(world);
    }
}

}  // namespace phoenix::ecs
