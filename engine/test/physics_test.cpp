// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include <doctest/doctest.h>

#include <cmath>

#include "core/job_system.h"
#include "physics/physics_world.h"

using phoenix::JobSystem;
using phoenix::PhysicsWorld;

TEST_SUITE("PhysicsWorld (Jolt)") {
    TEST_CASE("a dynamic box falls and comes to rest on static ground") {
        JobSystem jobs;
        REQUIRE(jobs.start(6));
        PhysicsWorld world;
        REQUIRE(world.init(jobs, 256));
        world.add_box(0, -0.5f, 20, 0.5f, 0, /*dynamic=*/false, 0x888888FFu);
        const auto box = world.add_box(0, 5, 0.5f, 0.5f, 0, /*dynamic=*/true, 0xFF0000FFu);

        world.step(1.0f / 60.0f);
        REQUIRE(world.body_count() == 2);
        const float start_y = world.bodies()[box].y;
        for (int i = 0; i < 240; ++i)
            world.step(1.0f / 60.0f);

        const PhxBody2D& b = world.bodies()[box];
        CHECK(b.id == box);
        CHECK(b.y < start_y);                               // it fell
        CHECK(b.y == doctest::Approx(0.5f).epsilon(0.05));  // resting on ground top (y=0)
        CHECK(std::abs(b.x) < 0.1f);                        // Plane2D: no drift
        CHECK(b.flags == 1u);
        CHECK(world.steps_completed() == 241);
    }

    TEST_CASE("asynchronous stepping never blocks and publishes snapshots") {
        JobSystem jobs;
        REQUIRE(jobs.start(6));
        PhysicsWorld world;
        REQUIRE(world.init(jobs));
        for (int i = 0; i < 50; ++i) {
            world.add_box(static_cast<float>(i % 10) - 5.0f, 2.0f + i * 0.6f, 0.25f, 0.25f, 0.3f,
                          true, 0x00FF00FFu);
        }
        REQUIRE(world.begin_step(1.0f / 60.0f));
        CHECK_FALSE(world.begin_step(1.0f / 60.0f));  // one step in flight at a time
        while (!world.poll_step()) {
        }
        CHECK(world.body_count() == 50);
    }

    TEST_CASE("impulses move bodies") {
        JobSystem jobs;
        REQUIRE(jobs.start(2));
        PhysicsWorld world;
        REQUIRE(world.init(jobs));
        const auto id = world.add_box(0, 10, 0.5f, 0.5f, 0, true, 0xFFFFFFFFu);
        world.step(1.0f / 60.0f);
        world.add_impulse(id, 5000.0f, 0.0f);  // 1 m^3 box at 1000 kg/m^3 -> 5 m/s
        for (int i = 0; i < 10; ++i)
            world.step(1.0f / 60.0f);
        CHECK(world.bodies()[0].x > 0.1f);
    }
}
