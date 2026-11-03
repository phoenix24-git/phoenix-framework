// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "core_runtime.h"

#include "os_phoenix.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/core_globals.h"
#include "core/debugger/engine_debugger.h"
#include "core/io/file_access_pack.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/object/worker_thread_pool.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"
#include "core/os/thread.h"
#include "core/register_core_types.h"
#include "core/string/translation_server.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

namespace phoenix::foundation {

namespace {

struct State {
	OS_Phoenix *os = nullptr;
	Engine *engine = nullptr;
	PackedData *packed_data = nullptr;
	ProjectSettings *globals = nullptr;
	TranslationServer *translation_server = nullptr;
	MessageQueue *message_queue = nullptr;
};

State *g_state = nullptr;

} // namespace

bool CoreRuntime::initialize(const CoreOptions &p_options) {
	if (g_state) {
		return false;
	}
	g_state = new State();
	State &s = *g_state;

	s.os = new OS_Phoenix(p_options.app_name);
	String resource_dir = p_options.resource_dir;
	if (resource_dir.is_empty()) {
		char *cwd = SDL_GetCurrentDirectory();
		resource_dir = cwd ? String::utf8(cwd) : String(".");
		SDL_free(cwd);
	}
	resource_dir = resource_dir.replace_char('\\', '/');
	if (resource_dir.length() > 1 && resource_dir.ends_with("/")) {
		resource_dir = resource_dir.substr(0, resource_dir.length() - 1);
	}
	s.os->set_resource_dir(resource_dir);
	String exec_path = p_options.executable_path;
	if (exec_path.is_empty()) {
		const char *base = SDL_GetBasePath();
		exec_path = String::utf8(base ? base : "").path_join(p_options.app_name);
	}
	s.os->set_cmdline(exec_path.utf8().get_data(), p_options.args, List<String>());

	Thread::make_main_thread();
	s.os->initialize();
	CoreGlobals::print_ready = true;

	s.engine = memnew(Engine);
	register_core_types();
	// Phoenix compat seam classes (Godot registers these from scene/).
	GDREGISTER_CLASS(Node);
	GDREGISTER_CLASS(SceneState);
	GDREGISTER_CLASS(PackedScene);
	s.packed_data = memnew(PackedData);
	s.globals = memnew(ProjectSettings);
	register_core_settings();
	// Loads res://project.godot when present (Godot projects open as-is); ignores absence.
	s.globals->setup(resource_dir, String(), false, true);
	s.translation_server = memnew(TranslationServer);
	register_early_core_singletons();
	register_core_extensions();
	register_core_singletons();
	s.message_queue = memnew(MessageQueue);
	// Same default as Godot's Main::setup(): low-priority tasks may use 75% of the threads.
	WorkerThreadPool::get_singleton()->init(p_options.worker_threads, 0.75);
	return true;
}

void CoreRuntime::finalize() {
	if (!g_state) {
		return;
	}
	State &s = *g_state;
	Thread::make_main_thread();
	CoreGlobals::print_ready = false;

	if (s.message_queue) {
		s.message_queue->flush();
		memdelete(s.message_queue);
	}
	WorkerThreadPool::get_singleton()->exit_languages_threads();
	ResourceLoader::remove_custom_loaders();
	ResourceSaver::remove_custom_savers();
	EngineDebugger::deinitialize();
	s.os->finalize();

	if (s.packed_data) {
		memdelete(s.packed_data);
	}
	if (s.translation_server) {
		memdelete(s.translation_server);
	}
	if (s.globals) {
		memdelete(s.globals);
	}
	unregister_core_extensions();
	if (s.engine) {
		memdelete(s.engine);
	}
	unregister_core_types();
	s.os->finalize_core();
	delete s.os;
	Thread::release_main_thread();

	delete g_state;
	g_state = nullptr;
}

bool CoreRuntime::is_initialized() {
	return g_state != nullptr;
}

} // namespace phoenix::foundation
