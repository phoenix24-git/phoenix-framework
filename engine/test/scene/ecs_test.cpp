// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include <doctest/doctest.h>

#include <atomic>

#include "phoenix/ecs/command_buffer.h"
#include "phoenix/ecs/parallel.h"
#include "phoenix/ecs/scheduler.h"
#include "phoenix/ecs/world.h"

using namespace phoenix::ecs;

namespace {
struct Position {
    Vector3 value;
};
struct Velocity {
    Vector3 value;
};
struct Health {
    int hp = 100;
};
struct Label {
    String text;
    ::Vector<Entity> links;
};
struct Frozen {};  // tag
}  // namespace

TEST_SUITE("ECS World") {
    TEST_CASE("entities are created, destroyed and their slots reused with a new generation") {
        World w;
        const Entity a = w.create();
        const Entity b = w.create();
        CHECK(w.alive(a));
        CHECK(w.entity_count() == 2);
        w.destroy(a);
        CHECK_FALSE(w.alive(a));
        const Entity c = w.create();
        CHECK(c.index == a.index);
        CHECK(c.generation == a.generation + 1);
        CHECK_FALSE(w.alive(a));  // stale handle stays dead
        CHECK(w.alive(b));
    }

    TEST_CASE("add/get/remove move entities between archetypes and keep values") {
        World w;
        const Entity e = w.create();
        w.add<Position>(e, Position{Vector3(1, 2, 3)});
        w.add<Velocity>(e, Velocity{Vector3(4, 5, 6)});
        w.add<Frozen>(e);
        REQUIRE(w.get<Position>(e));
        CHECK(w.get<Position>(e)->value == Vector3(1, 2, 3));
        CHECK(w.has<Frozen>(e));
        w.remove<Velocity>(e);
        CHECK_FALSE(w.has<Velocity>(e));
        CHECK(w.get<Position>(e)->value == Vector3(1, 2, 3));
        CHECK(w.components_of(e).size() == 2);
    }

    TEST_CASE("swap-remove keeps every other entity intact across many chunks") {
        World w;
        std::vector<Entity> es;
        for (int i = 0; i < 5000; ++i) {
            Entity e = w.create();
            w.add<Health>(e, Health{i});
            es.push_back(e);
        }
        for (int i = 0; i < 5000; i += 3)
            w.destroy(es[i]);
        bool ok = true;
        for (int i = 0; i < 5000; ++i) {
            if (i % 3 == 0) {
                ok &= !w.alive(es[i]);
            } else {
                ok &= w.get<Health>(es[i]) && w.get<Health>(es[i])->hp == i;
            }
        }
        CHECK(ok);
        CHECK(w.query<const Health>().count() == 5000 - 1667);
    }

    TEST_CASE("non-trivial components (String, Vector) survive moves") {
        World w;
        const Entity a = w.create();
        const Entity b = w.create();
        w.add<Label>(a, Label{"hello", {}});
        w.get<Label>(a)->links.push_back(b);
        w.add<Position>(a);  // moves Label to another archetype
        w.add<Velocity>(b);
        REQUIRE(w.get<Label>(a));
        CHECK(w.get<Label>(a)->text == "hello");
        CHECK(w.get<Label>(a)->links.size() == 1);
        CHECK(w.get<Label>(a)->links[0] == b);
        w.destroy(a);
    }

    TEST_CASE("queries filter with/without and iterate entities with their components") {
        World w;
        for (int i = 0; i < 10; ++i) {
            Entity e = w.create();
            w.add<Position>(e, Position{Vector3(float(i), 0, 0)});
            if (i % 2 == 0)
                w.add<Velocity>(e, Velocity{Vector3(1, 0, 0)});
            if (i == 4)
                w.add<Frozen>(e);
        }
        CHECK(w.query<const Position>().count() == 10);
        CHECK(w.query<Position, const Velocity>().count() == 5);
        CHECK(w.query<Position, const Velocity>().without<Frozen>().count() == 4);
        w.query<Position, const Velocity>().without<Frozen>().each(
            [](Position& p, const Velocity& v) { p.value += v.value; });
        float sum = 0;
        w.query<const Position>().each([&](Entity, const Position& p) { sum += p.value.x; });
        CHECK(sum == doctest::Approx(45 + 4));
    }

    TEST_CASE("changed<T>() sees only chunks written after a tick") {
        World w;
        for (int i = 0; i < 3; ++i)
            w.add<Health>(w.create());
        const std::uint32_t since = w.advance_tick();
        CHECK(w.query<const Health>().changed<Health>(since).count() == 0);
        w.advance_tick();
        w.query<Health>().each([](Health& h) { h.hp -= 1; });  // write access bumps versions
        CHECK(w.query<const Health>().changed<Health>(since).count() == 3);
    }

    TEST_CASE("par_each runs chunks on the WorkerThreadPool") {
        World w;
        for (int i = 0; i < 100000; ++i) {
            Entity e = w.create();
            w.add<Position>(e);
            w.add<Velocity>(e, Velocity{Vector3(1, 2, 3)});
        }
        w.query<Position, const Velocity>().par_each(
            [](Position& p, const Velocity& v) { p.value += v.value * 2.0f; });
        double sum = 0;
        w.query<const Position>().each([&](const Position& p) { sum += p.value.y; });
        CHECK(sum == doctest::Approx(100000 * 4.0));
    }

    TEST_CASE("CommandBuffer defers structural changes made while iterating") {
        World w;
        for (int i = 0; i < 4; ++i)
            w.add<Health>(w.create(), Health{i});
        CommandBuffer cb;
        w.query<const Health>().each([&](Entity e, const Health& h) {
            if (h.hp % 2 == 0)
                cb.destroy(e);
            cb.spawn([](World& world, Entity n) { world.add<Frozen>(n); });
        });
        CHECK(cb.size() == 6);
        cb.playback(w);
        CHECK(w.query<const Health>().count() == 2);
        CHECK(w.query<const Frozen>().count() == 4);
    }
}

TEST_SUITE("ECS Scheduler") {
    TEST_CASE("systems that don't conflict share a wave; conflicting ones keep order") {
        Scheduler s;
        s.add<Write<Position>>("A", Stage::Update, [](SystemContext&) {});
        s.add<Read<Position>>("B", Stage::Update, [](SystemContext&) {});
        s.add<Write<Velocity>>("C", Stage::Update, [](SystemContext&) {});
        s.add<Exclusive>("D", Stage::Update, [](SystemContext&) {});
        s.add<Read<Health>>("E", Stage::LateUpdate, [](SystemContext&) {});
        const LocalVector<int> waves = s.waves(Stage::Update);
        CHECK(waves[0] == 0);   // A
        CHECK(waves[1] == 1);   // B reads what A writes
        CHECK(waves[2] == 0);   // C independent of A
        CHECK(waves[3] == 2);   // D exclusive: after everything before it
        CHECK(waves[4] == -1);  // other stage
    }

    TEST_CASE("parallel and sequential runs produce identical results") {
        auto run = [](bool parallel) {
            World w;
            for (int i = 0; i < 20000; ++i) {
                Entity e = w.create();
                w.add<Position>(e);
                w.add<Velocity>(e, Velocity{Vector3(1, 0, 0)});
                w.add<Health>(e);
            }
            Scheduler s;
            s.add<Read<Velocity>, Write<Position>>("Move", Stage::Update, [](SystemContext& ctx) {
                ctx.world.query<Position, const Velocity>().each(
                    [&](Position& p, const Velocity& v) { p.value += v.value * ctx.delta_time; });
            });
            s.add<Write<Health>>("Decay", Stage::Update, [](SystemContext& ctx) {
                ctx.world.query<Health>().each([](Health& h) { h.hp -= 1; });
            });
            for (int f = 0; f < 10; ++f)
                s.run_stage(w, Stage::Update, 0.5f, f * 0.5, f, parallel);
            double x = 0;
            long hp = 0;
            w.query<const Position, const Health>().each([&](const Position& p, const Health& h) {
                x += p.value.x;
                hp += h.hp;
            });
            return std::pair<double, long>(x, hp);
        };
        const auto seq = run(false);
        const auto par = run(true);
        CHECK(seq.first == doctest::Approx(20000 * 5.0));
        CHECK(seq.second == 20000L * 90);
        CHECK(par == seq);
    }

    TEST_CASE("per-system command buffers play back at the end of the stage") {
        World w;
        Scheduler s;
        s.add<Write<Health>>("Spawner", Stage::Update, [](SystemContext& ctx) {
            ctx.commands.spawn([](World& world, Entity e) { world.add<Health>(e); });
        });
        s.run_stage(w, Stage::Update, 0.016f, 0, 1, true);
        s.run_stage(w, Stage::Update, 0.016f, 0, 2, true);
        CHECK(w.query<const Health>().count() == 2);
    }
}
