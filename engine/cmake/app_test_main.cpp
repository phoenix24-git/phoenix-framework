// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
// doctest runner for `phoenix test`.
#define DOCTEST_CONFIG_IMPLEMENT
#include <SDL3/SDL_main.h>
#include <doctest/doctest.h>

int main(int argc, char** argv) {
    doctest::Context context(argc, argv);
    return context.run();
}
