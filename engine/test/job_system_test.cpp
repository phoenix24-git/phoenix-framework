// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "core/job_system.h"

#include <doctest/doctest.h>

#include <atomic>
#include <vector>

using phoenix::JobCounter;
using phoenix::JobSystem;

TEST_SUITE("JobSystem") {
    TEST_CASE("starts the console's 6 SDL worker threads") {
        JobSystem jobs;
        REQUIRE(jobs.start(6));
        CHECK(jobs.worker_count() == 6);
        CHECK(jobs.running());
        jobs.stop();
        CHECK_FALSE(jobs.running());
    }

    TEST_CASE("submit + wait runs every job exactly once") {
        JobSystem jobs;
        REQUIRE(jobs.start(4));
        std::atomic<int> sum{0};
        JobCounter counter;
        for (int i = 0; i < 1000; ++i) {
            jobs.submit([](void* ctx) { static_cast<std::atomic<int>*>(ctx)->fetch_add(1); }, &sum,
                        &counter);
        }
        jobs.wait(counter);
        CHECK(sum.load() == 1000);
        CHECK(counter.done());
    }

    TEST_CASE("parallel_for covers the whole range") {
        JobSystem jobs;
        REQUIRE(jobs.start(6));
        std::vector<int> data(10'000, 0);
        jobs.parallel_for(static_cast<std::uint32_t>(data.size()), 128,
                          [&](std::uint32_t begin, std::uint32_t end) {
                              for (auto i = begin; i < end; ++i)
                                  data[i] = static_cast<int>(i) * 2;
                          });
        bool ok = true;
        for (std::size_t i = 0; i < data.size(); ++i)
            ok &= data[i] == static_cast<int>(i) * 2;
        CHECK(ok);
    }

    TEST_CASE("nested wait inside a job cannot deadlock") {
        JobSystem jobs;
        REQUIRE(jobs.start(1));  // single worker forces the helping path
        struct Ctx {
            JobSystem* jobs;
            std::atomic<int> inner{0};
        } ctx{&jobs};
        JobCounter outer;
        jobs.submit(
            [](void* p) {
                auto* c = static_cast<Ctx*>(p);
                JobCounter inner;
                for (int i = 0; i < 8; ++i) {
                    c->jobs->submit([](void* q) { static_cast<Ctx*>(q)->inner.fetch_add(1); }, c,
                                    &inner);
                }
                c->jobs->wait(inner);
            },
            &ctx, &outer);
        jobs.wait(outer);
        CHECK(ctx.inner.load() == 8);
    }

    TEST_CASE("without workers jobs run inline") {
        JobSystem jobs;
        int value = 0;
        JobCounter counter;
        jobs.submit([](void* p) { *static_cast<int*>(p) = 42; }, &value, &counter);
        jobs.wait(counter);
        CHECK(value == 42);
    }
}
