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
	/// Optional .pck or .zip mounted as the main pack: res:// then reads from it only (an
	/// exported game). Empty: Godot's lookup (<executable>.pck next to the binary), else the
	/// resource_dir with any later packs layered on top.
	String main_pack;
	/// Root that user:// lives under (empty: the platform default, see
	/// OS_Phoenix::detect_data_root()). user:// = <root>/phoenix/app_userdata/<project name>,
	/// or <root>/<custom_user_dir_name> when the project sets one.
	String user_data_root;
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
	/// True when res:// is served by a main pack (an exported game): it is read-only then.
	static bool is_running_from_main_pack();
};

} // namespace phoenix::foundation
