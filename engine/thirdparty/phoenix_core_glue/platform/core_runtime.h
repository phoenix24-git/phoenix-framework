// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Brings Godot's core/ up and down inside a Phoenix process: OS (SDL3), Engine, ClassDB
// type registration, ProjectSettings, TranslationServer, MessageQueue and the core
// singletons. The order follows Godot's Main::test_setup()/test_cleanup() (MIT).
#pragma once

#include "core/string/ustring.h"
#include "core/templates/list.h"

namespace phoenix::foundation {

struct CoreOptions {
	String app_name = "phoenix";
	/// Directory that res:// maps to (empty: current working directory).
	String resource_dir;
	/// Reported by OS::get_executable_path() (defaults to the real executable).
	String executable_path;
	List<String> args;
	/// WorkerThreadPool size (-1: one per logical core, like Godot's default).
	int worker_threads = -1;
};

class CoreRuntime {
public:
	/// Initializes Godot core once per process. Returns false if already initialized.
	static bool initialize(const CoreOptions &p_options = CoreOptions());
	static void finalize();
	static bool is_initialized();
};

} // namespace phoenix::foundation
