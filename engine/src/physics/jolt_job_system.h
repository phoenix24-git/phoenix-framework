// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/JobSystemWithBarrier.h>

namespace phoenix {

class JobSystem;

/// Adapts Jolt's job interface onto the engine's SDL3 JobSystem so physics
/// shares the same 6 console worker threads instead of spawning its own.
class JoltJobSystem final : public JPH::JobSystemWithBarrier {
  public:
    JoltJobSystem(phoenix::JobSystem& jobs, JPH::uint max_jobs, JPH::uint max_barriers);

    int GetMaxConcurrency() const override;
    JobHandle CreateJob(const char* name, JPH::ColorArg color, const JobFunction& fn,
                        JPH::uint32 num_dependencies = 0) override;

  protected:
    void QueueJob(Job* job) override;
    void QueueJobs(Job** jobs, JPH::uint count) override;
    void FreeJob(Job* job) override;

  private:
    phoenix::JobSystem& jobs_;
    JPH::FixedSizeFreeList<Job> pool_;
};

}  // namespace phoenix
