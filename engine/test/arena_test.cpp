// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "core/arena.h"

#include <doctest/doctest.h>

#include <cstdint>

using phoenix::LinearArena;

TEST_SUITE("LinearArena") {
    TEST_CASE("allocations are aligned and bounded") {
        LinearArena arena;
        REQUIRE(arena.reserve(1024));
        void* a = arena.allocate(3, 1);
        void* b = arena.allocate(16, 64);
        REQUIRE(a != nullptr);
        REQUIRE(b != nullptr);
        CHECK(reinterpret_cast<std::uintptr_t>(b) % 64 == 0);
        CHECK(arena.allocate(4096) == nullptr);  // exhausted, never throws
        CHECK(arena.used() <= arena.capacity());
    }

    TEST_CASE("rejects non power-of-two alignment") {
        LinearArena arena;
        REQUIRE(arena.reserve(256));
        CHECK(arena.allocate(8, 3) == nullptr);
        CHECK(arena.allocate(8, 0) == nullptr);
    }

    TEST_CASE("mark/rewind/reset reuse memory and track high water") {
        LinearArena arena;
        REQUIRE(arena.reserve(512));
        auto mark = arena.mark();
        arena.allocate(100);
        const auto used = arena.used();
        arena.rewind(mark);
        CHECK(arena.used() == 0);
        arena.allocate(10);
        arena.reset();
        CHECK(arena.used() == 0);
        CHECK(arena.high_water() >= used);
    }

    TEST_CASE("make constructs trivially destructible objects") {
        struct Pod {
            int a;
            float b;
        };
        LinearArena arena;
        REQUIRE(arena.reserve(64));
        Pod* p = arena.make<Pod>(Pod{7, 2.5f});
        REQUIRE(p != nullptr);
        CHECK(p->a == 7);
        CHECK(p->b == doctest::Approx(2.5f));
    }
}
