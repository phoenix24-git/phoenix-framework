// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <vector>

struct SDL_Thread;
struct SDL_Mutex;
struct SDL_Condition;

namespace phoenix {

/// Counts outstanding jobs; `JobSystem::wait` blocks (while helping) until zero.
struct JobCounter {
    std::atomic<std::uint32_t> pending{0};
    bool done() const { return pending.load(std::memory_order_acquire) == 0; }
};

/// Fixed-size worker pool built purely on the SDL3 thread API, so the same code
/// runs on desktop threads and on Emscripten pthread Web Workers that share one
/// wasm64 linear memory.
///
/// Jobs are POD {function, context} pairs: submitting never allocates.
class JobSystem {
  public:
    using JobFn = void (*)(void* context);

    JobSystem() = default;
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    bool start(std::uint32_t worker_count);
    void stop();

    void submit(JobFn fn, void* context, JobCounter* counter = nullptr);

    /// Waits for `counter` to reach zero. The calling thread executes queued
    /// jobs while it waits, so waiting from inside a job can never deadlock.
    void wait(JobCounter& counter);

    /// Runs fn(begin, end) over [0, count) split into batches, on all workers
    /// plus the calling thread. Returns when every batch has finished.
    template <typename Fn>
    void parallel_for(std::uint32_t count, std::uint32_t batch, Fn&& fn) {
        if (count == 0) {
            return;
        }
        batch = batch == 0 ? 1 : batch;
        struct Ctx {
            Fn* fn;
            std::uint32_t begin, end;
        };
        const std::uint32_t batches = (count + batch - 1) / batch;
        std::vector<Ctx> contexts(batches);
        JobCounter counter;
        for (std::uint32_t i = 0; i < batches; ++i) {
            contexts[i] = {&fn, i * batch, std::min(count, (i + 1) * batch)};
            submit(
                [](void* c) {
                    auto* ctx = static_cast<Ctx*>(c);
                    (*ctx->fn)(ctx->begin, ctx->end);
                },
                &contexts[i], &counter);
        }
        wait(counter);
    }

    std::uint32_t worker_count() const { return static_cast<std::uint32_t>(threads_.size()); }
    std::uint64_t jobs_executed() const { return executed_.load(std::memory_order_relaxed); }
    std::uint32_t queued() const;
    bool running() const { return running_.load(std::memory_order_acquire); }

  private:
    struct Job {
        JobFn fn;
        void* context;
        JobCounter* counter;
    };
    static int worker_main(void* self);
    bool try_pop(Job& out);
    void execute(const Job& job);

    std::vector<SDL_Thread*> threads_;
    std::deque<Job> queue_;
    SDL_Mutex* mutex_ = nullptr;
    SDL_Condition* has_work_ = nullptr;
    SDL_Condition* job_done_ = nullptr;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> executed_{0};
};

}  // namespace phoenix
