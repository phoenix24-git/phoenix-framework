// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "core/job_system.h"

#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_thread.h>

#include "core/log.h"

namespace phoenix {

JobSystem::~JobSystem() {
    stop();
}

bool JobSystem::start(std::uint32_t worker_count) {
    if (running()) {
        return true;
    }
    mutex_ = SDL_CreateMutex();
    has_work_ = SDL_CreateCondition();
    job_done_ = SDL_CreateCondition();
    if (!mutex_ || !has_work_ || !job_done_) {
        PHX_LOG_ERROR("JobSystem: failed to create sync primitives: %s", SDL_GetError());
        stop();
        return false;
    }
    running_.store(true, std::memory_order_release);
    for (std::uint32_t i = 0; i < worker_count; ++i) {
        char name[32];
        SDL_snprintf(name, sizeof(name), "phoenix.worker.%u", i);
        SDL_Thread* t = SDL_CreateThread(&JobSystem::worker_main, name, this);
        if (!t) {
            PHX_LOG_ERROR("JobSystem: SDL_CreateThread failed: %s", SDL_GetError());
            stop();
            return false;
        }
        threads_.push_back(t);
    }
    return true;
}

void JobSystem::stop() {
    if (mutex_) {
        SDL_LockMutex(mutex_);
        running_.store(false, std::memory_order_release);
        SDL_BroadcastCondition(has_work_);
        SDL_UnlockMutex(mutex_);
    }
    for (SDL_Thread* t : threads_) {
        SDL_WaitThread(t, nullptr);
    }
    threads_.clear();
    // Drain anything left so counters never stay pending forever.
    Job job;
    while (mutex_ && try_pop(job)) {
        execute(job);
    }
    if (job_done_)
        SDL_DestroyCondition(job_done_);
    if (has_work_)
        SDL_DestroyCondition(has_work_);
    if (mutex_)
        SDL_DestroyMutex(mutex_);
    job_done_ = has_work_ = nullptr;
    mutex_ = nullptr;
}

void JobSystem::submit(JobFn fn, void* context, JobCounter* counter) {
    if (counter) {
        counter->pending.fetch_add(1, std::memory_order_acq_rel);
    }
    if (!running()) {
        // Degenerate mode (no workers): run inline so callers stay correct.
        execute({fn, context, counter});
        return;
    }
    SDL_LockMutex(mutex_);
    queue_.push_back({fn, context, counter});
    SDL_SignalCondition(has_work_);
    SDL_UnlockMutex(mutex_);
}

bool JobSystem::try_pop(Job& out) {
    SDL_LockMutex(mutex_);
    const bool ok = !queue_.empty();
    if (ok) {
        out = queue_.front();
        queue_.pop_front();
    }
    SDL_UnlockMutex(mutex_);
    return ok;
}

void JobSystem::execute(const Job& job) {
    job.fn(job.context);
    executed_.fetch_add(1, std::memory_order_relaxed);
    if (job.counter && job.counter->pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        if (mutex_) {
            SDL_LockMutex(mutex_);
            SDL_BroadcastCondition(job_done_);
            SDL_UnlockMutex(mutex_);
        }
    }
}

void JobSystem::wait(JobCounter& counter) {
    while (!counter.done()) {
        Job job;
        if (mutex_ && try_pop(job)) {
            execute(job);
            continue;
        }
        if (!mutex_) {
            break;
        }
        SDL_LockMutex(mutex_);
        if (!counter.done() && queue_.empty()) {
            // Timed wait guards against a missed wake-up between checks.
            SDL_WaitConditionTimeout(job_done_, mutex_, 1);
        }
        SDL_UnlockMutex(mutex_);
    }
}

std::uint32_t JobSystem::queued() const {
    if (!mutex_) {
        return 0;
    }
    SDL_LockMutex(mutex_);
    const auto n = static_cast<std::uint32_t>(queue_.size());
    SDL_UnlockMutex(mutex_);
    return n;
}

int JobSystem::worker_main(void* self_ptr) {
    auto* self = static_cast<JobSystem*>(self_ptr);
    for (;;) {
        SDL_LockMutex(self->mutex_);
        while (self->running() && self->queue_.empty()) {
            SDL_WaitCondition(self->has_work_, self->mutex_);
        }
        if (!self->running() && self->queue_.empty()) {
            SDL_UnlockMutex(self->mutex_);
            return 0;
        }
        Job job = self->queue_.front();
        self->queue_.pop_front();
        SDL_UnlockMutex(self->mutex_);
        self->execute(job);
    }
}

}  // namespace phoenix
