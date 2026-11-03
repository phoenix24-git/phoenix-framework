// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "physics/jolt_job_system.h"

#include <SDL3/SDL_timer.h>

#include "core/job_system.h"

namespace phoenix {

JoltJobSystem::JoltJobSystem(phoenix::JobSystem& jobs, JPH::uint max_jobs, JPH::uint max_barriers)
    : JPH::JobSystemWithBarrier(max_barriers), jobs_(jobs) {
    pool_.Init(max_jobs, max_jobs);
}

int JoltJobSystem::GetMaxConcurrency() const {
    return static_cast<int>(jobs_.worker_count()) + 1;
}

JPH::JobHandle JoltJobSystem::CreateJob(const char* name, JPH::ColorArg color,
                                        const JobFunction& fn, JPH::uint32 num_dependencies) {
    JPH::uint32 index;
    for (;;) {
        index = pool_.ConstructObject(name, color, this, fn, num_dependencies);
        if (index != JPH::FixedSizeFreeList<Job>::cInvalidObjectIndex) {
            break;
        }
        SDL_DelayNS(100'000);  // pool exhausted: let workers drain
    }
    Job* job = &pool_.Get(index);
    JobHandle handle(job);  // keeps a reference while the job may already run
    if (num_dependencies == 0) {
        QueueJob(job);
    }
    return handle;
}

void JoltJobSystem::QueueJob(Job* job) {
    job->AddRef();
    jobs_.submit(
        [](void* ctx) {
            auto* j = static_cast<Job*>(ctx);
            j->Execute();
            j->Release();
        },
        job);
}

void JoltJobSystem::QueueJobs(Job** jobs, JPH::uint count) {
    for (JPH::uint i = 0; i < count; ++i) {
        QueueJob(jobs[i]);
    }
}

void JoltJobSystem::FreeJob(Job* job) {
    pool_.DestructObject(job);
}

}  // namespace phoenix
