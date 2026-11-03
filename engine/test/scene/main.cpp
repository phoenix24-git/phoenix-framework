// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
// ECS + scene tests run inside a live foundation runtime (WorkerThreadPool, StringName, Variant).
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "platform/core_runtime.h"

int main(int argc, char** argv) {
    phoenix::foundation::CoreOptions options;
    options.app_name = "phoenix_scene_tests";
    options.worker_threads = 6;  // the console profile
    phoenix::foundation::CoreRuntime::initialize(options);
    doctest::Context context(argc, argv);
    const int result = context.run();
    phoenix::foundation::CoreRuntime::finalize();
    return result;
}
