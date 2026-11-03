// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Persistent key/value storage. On the web it is backed by the browser's
// Origin Private File System (OPFS) through Emscripten WasmFS; natively it is
// the per-user app data directory. All I/O runs on the engine IO thread;
// callbacks are delivered on the main (UI) thread.
#pragma once

#include <functional>
#include <optional>
#include <string>

namespace phoenix::storage {

void write(std::string key, std::string value);
void read(std::string key, std::function<void(std::optional<std::string>)> on_done);

/// "opfs", "native" or "memory" (fallback when persistence is unavailable).
const char* backend_name();

}  // namespace phoenix::storage
