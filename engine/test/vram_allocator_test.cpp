// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "core/vram_allocator.h"

#include <doctest/doctest.h>

#include <vector>

#include "phoenix/console_profile.h"

using phoenix::VramAllocator;

TEST_SUITE("VramAllocator") {
    TEST_CASE("default console profile reserves 3.5 GiB in 256 MiB pages") {
        phoenix::ConsoleProfile profile;
        CHECK(profile.vram_budget_bytes == 3584ull * 1024 * 1024);
        CHECK(profile.worker_threads == 6);
        VramAllocator vram;
        vram.init(static_cast<std::uint32_t>(profile.vram_budget_bytes / profile.vram_page_bytes),
                  profile.vram_page_bytes);
        CHECK(vram.page_count() == 14);
        CHECK(vram.capacity() == profile.vram_budget_bytes);
    }

    TEST_CASE("allocations respect alignment and never span pages") {
        VramAllocator vram;
        vram.init(2, 1024);
        auto a = vram.allocate(100, 256);
        REQUIRE(a);
        CHECK(a->offset % 256 == 0);
        auto big = vram.allocate(1024);
        REQUIRE(big);
        CHECK(big->page != a->page);
        CHECK_FALSE(vram.allocate(2048));  // larger than a page
        CHECK_FALSE(vram.allocate(0));
    }

    TEST_CASE("free coalesces neighbours back into one block") {
        VramAllocator vram;
        vram.init(1, 4096);
        std::vector<phoenix::VramAllocation> allocs;
        for (int i = 0; i < 4; ++i) {
            auto a = vram.allocate(1024, 1);
            REQUIRE(a);
            allocs.push_back(*a);
        }
        CHECK(vram.used() == 4096);
        CHECK_FALSE(vram.allocate(1, 1));
        vram.free(allocs[1]);
        vram.free(allocs[3]);
        vram.free(allocs[2]);
        vram.free(allocs[0]);
        CHECK(vram.used() == 0);
        CHECK(vram.largest_free_block() == 4096);
    }

    TEST_CASE("best fit prefers the tightest hole") {
        VramAllocator vram;
        vram.init(1, 4096);
        auto a = vram.allocate(512, 1);
        auto b = vram.allocate(256, 1);
        auto c = vram.allocate(1024, 1);
        REQUIRE((a && b && c));
        vram.free(*b);  // 256-byte hole at 512
        auto d = vram.allocate(200, 1);
        REQUIRE(d);
        CHECK(d->offset == 512);
    }

    TEST_CASE("add_page grows capacity for on-demand reservation") {
        VramAllocator vram;
        vram.init(1, 256);
        REQUIRE(vram.allocate(256, 1));
        CHECK_FALSE(vram.allocate(16, 1));
        vram.add_page();
        auto a = vram.allocate(16, 1);
        REQUIRE(a);
        CHECK(a->page == 1);
    }
}
