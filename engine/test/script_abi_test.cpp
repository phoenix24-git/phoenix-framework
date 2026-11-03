// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Pins the Script ABI layout. The C# mirror (packages/phoenix) asserts the
// same numbers, so any drift fails CI on both sides.
#include "phoenix/script_abi.h"

#include <doctest/doctest.h>

#include <cstddef>

TEST_SUITE("Script ABI") {
    TEST_CASE("struct sizes and offsets are frozen for ABI v1") {
        CHECK(PHX_SCRIPT_ABI_VERSION == 2u);
        CHECK(sizeof(PhxBody2D) == 32);
        CHECK(offsetof(PhxBody2D, id) == 20);
        CHECK(offsetof(PhxBody2D, rgba) == 24);
        CHECK(sizeof(PhxDrawRect) == 32);
        CHECK(offsetof(PhxDrawRect, rgba) == 16);
        CHECK(offsetof(PhxDrawRect, rotation) == 20);
        CHECK(sizeof(PhxFrameInfo) == 48);
        CHECK(offsetof(PhxFrameInfo, viewport_width) == 16);
        CHECK(offsetof(PhxFrameInfo, pointer_buttons) == 32);
        CHECK(offsetof(PhxEngineApi, engine) == 8);
        CHECK(sizeof(PhxEngineApi) == 8 + 13 * sizeof(void*));
    }
}
