// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Query::par_each: chunk-parallel iteration on the foundation WorkerThreadPool (sized to the
// console profile's worker count; Web Workers sharing one wasm64 memory in the browser).
#pragma once

#include <tuple>

#include "core/object/worker_thread_pool.h"
#include "phoenix/ecs/world.h"

namespace phoenix::ecs {

template <typename... Ts>
template <typename Fn>
void Query<Ts...>::par_each(Fn&& fn) {
    LocalVector<Chunk*> chunks = matching_chunks();
    if (chunks.is_empty())
        return;
    world_.begin_iteration();
    // Mark written columns on this thread so workers never touch chunk metadata.
    const std::uint32_t tick = world_.tick();
    for (Chunk* c : chunks) {
        const Archetype& a = *c->archetype;
        ((std::is_const_v<Ts> ? void()
                              : void(c->versions[std::uint32_t(a.column_of(
                                         component_id<std::remove_const_t<Ts>>()))] = tick)),
         ...);
    }
    struct Job {
        LocalVector<Chunk*>* chunks;
        Fn* fn;
        static void run(void* self, std::uint32_t k) {
            Job& job = *static_cast<Job*>(self);
            Chunk& c = *(*job.chunks)[k];
            const Archetype& a = *c.archetype;
            Entity* entities = c.entities();
            auto cols = std::make_tuple(static_cast<Ts*>(
                a.column_ptr(c, a.column_of(component_id<std::remove_const_t<Ts>>())))...);
            for (std::uint32_t i = 0; i < c.count; ++i) {
                std::apply(
                    [&](auto*... p) {
                        if constexpr (std::is_invocable_v<Fn&, Entity, Ts&...>) {
                            (*job.fn)(entities[i], p[i]...);
                        } else {
                            (*job.fn)(p[i]...);
                        }
                    },
                    cols);
            }
        }
    } job{&chunks, &fn};
    WorkerThreadPool* pool = WorkerThreadPool::get_singleton();
    if (pool && chunks.size() > 1) {
        const WorkerThreadPool::GroupID group = pool->add_native_group_task(
            &Job::run, &job, int(chunks.size()), -1, true, "ecs.par_each");
        pool->wait_for_group_task_completion(group);
    } else {
        for (std::uint32_t k = 0; k < chunks.size(); ++k)
            Job::run(&job, k);
    }
    world_.end_iteration();
}

}  // namespace phoenix::ecs
