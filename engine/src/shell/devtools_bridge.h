// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Bridge between the running engine and Phoenix DevTools (served by
// `phoenix run` at /__phoenix/devtools). On the web, messages are posted on
// the `phoenix-devtools` BroadcastChannel, which works from the main thread
// and from pthread workers alike. Natively these are no-ops for now.
#pragma once

#include "phoenix/console_profile.h"

namespace phoenix {
struct EngineStats;
}

namespace phoenix::devtools {

void install_log_forwarding();
void announce(const char* app_name, const char* renderer, const ConsoleProfile& profile);
void publish(const EngineStats& stats, const char* renderer, const char* storage_backend);

}  // namespace phoenix::devtools
