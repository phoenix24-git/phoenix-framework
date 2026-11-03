// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <functional>
#include <utility>
#include <vector>

#include "core/os/mutex.h"
#include "phoenix/ecs/world.h"

namespace phoenix::ecs {

/// Records structural changes (spawn, destroy, add, remove) while systems iterate, and applies
/// them later in recording order. Thread-safe: parallel jobs may record into one buffer.
class CommandBuffer {
  public:
    /// Spawns an entity on playback and calls `init(world, entity)` to populate it.
    void spawn(std::function<void(World&, Entity)> init) {
        push([init = std::move(init)](World& w) { init(w, w.create()); });
    }
    void destroy(Entity e) {
        push([e](World& w) { w.destroy(e); });
    }
    template <typename T>
    void add(Entity e, T value) {
        push([e, value = std::move(value)](World& w) mutable {
            if (w.alive(e))
                w.add<T>(e, std::move(value));
        });
    }
    template <typename T>
    void remove(Entity e) {
        push([e](World& w) {
            if (w.alive(e))
                w.remove<T>(e);
        });
    }
    /// Arbitrary deferred mutation.
    void defer(std::function<void(World&)> fn) { push(std::move(fn)); }

    std::size_t size() const {
        MutexLock lock(mutex_);
        return commands_.size();
    }
    void playback(World& world) {
        std::vector<std::function<void(World&)>> commands;
        {
            MutexLock lock(mutex_);
            commands.swap(commands_);
        }
        for (auto& c : commands)
            c(world);
    }

  private:
    void push(std::function<void(World&)> fn) {
        MutexLock lock(mutex_);
        commands_.push_back(std::move(fn));
    }
    mutable BinaryMutex mutex_;
    // std::vector, not LocalVector: std::function is not trivially relocatable.
    std::vector<std::function<void(World&)>> commands_;
};

}  // namespace phoenix::ecs
