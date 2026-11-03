// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "os_phoenix.h"

#include "file_access_sdl.h"

#include "core/io/logger.h"
#include "core/os/main_loop.h"

#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_locale.h>
#include <SDL3/SDL_platform.h>
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_time.h>
#include <SDL3/SDL_timer.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <random>
#include <string>
#include <vector>

OS_Phoenix::OS_Phoenix(String p_app_name) :
		app_name(std::move(p_app_name)) {
	// Same logger setup as Godot's OS_Unix: stdout/stderr through StdLogger.
	Vector<Logger *> loggers;
	loggers.push_back(memnew(StdLogger));
	_set_logger(memnew(CompositeLogger(loggers)));
}

void OS_Phoenix::initialize() {
	FileAccess::make_default<FileAccessSDL>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessSDL>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessSDL>(FileAccess::ACCESS_FILESYSTEM);
	DirAccess::make_default<DirAccessSDL>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessSDL>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessSDL>(DirAccess::ACCESS_FILESYSTEM);
}

void OS_Phoenix::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
	}
	main_loop = nullptr;
}

void OS_Phoenix::finalize() {
	delete_main_loop();
	for (KeyValue<ProcessID, SDL_Process *> &E : processes) {
		SDL_DestroyProcess(E.value);
	}
	processes.clear();
}

bool OS_Phoenix::_check_internal_feature_support(const String &p_feature) {
#ifdef __EMSCRIPTEN__
	return p_feature == "web" || p_feature == "wasm64" || p_feature == "threads";
#else
	return p_feature == "pc" || p_feature == "threads";
#endif
}

String OS_Phoenix::get_stdin_string(int64_t p_buffer_size) {
	std::vector<char> buffer(size_t(MAX(p_buffer_size, int64_t(1))) + 1, '\0');
	if (!std::fgets(buffer.data(), int(buffer.size()), stdin)) {
		return String();
	}
	return String::utf8(buffer.data());
}

PackedByteArray OS_Phoenix::get_stdin_buffer(int64_t p_buffer_size) {
	PackedByteArray out;
	out.resize(p_buffer_size);
	const size_t n = std::fread(out.ptrw(), 1, size_t(p_buffer_size), stdin);
	out.resize(int64_t(n));
	return out;
}

Error OS_Phoenix::get_entropy(uint8_t *r_buffer, int p_bytes) {
	// std::random_device is a CSPRNG on every Phoenix target (getrandom, BCryptGenRandom,
	// arc4random, crypto.getRandomValues under Emscripten).
	std::random_device rd;
	for (int i = 0; i < p_bytes; i += 4) {
		const uint32_t v = rd();
		for (int j = 0; j < 4 && i + j < p_bytes; j++) {
			r_buffer[i + j] = uint8_t(v >> (j * 8));
		}
	}
	return OK;
}

namespace {

struct ProcessArgs {
	std::vector<std::string> storage;
	std::vector<const char *> argv;
	ProcessArgs(const String &p_path, const List<String> &p_arguments) {
		storage.emplace_back(p_path.utf8().get_data());
		for (const String &arg : p_arguments) {
			storage.emplace_back(arg.utf8().get_data());
		}
		for (const std::string &s : storage) {
			argv.push_back(s.c_str());
		}
		argv.push_back(nullptr);
	}
};

} // namespace

Error OS_Phoenix::execute(const String &p_path, const List<String> &p_arguments, String *r_pipe, int *r_exitcode, bool read_stderr, Mutex *p_pipe_mutex, bool p_open_console) {
	ProcessArgs args(p_path, p_arguments);
	const SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args.argv.data());
	SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, r_pipe ? SDL_PROCESS_STDIO_APP : SDL_PROCESS_STDIO_NULL);
	SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, r_pipe && read_stderr);
	SDL_Process *process = SDL_CreateProcessWithProperties(props);
	SDL_DestroyProperties(props);
	if (!process) {
		return ERR_CANT_FORK;
	}
	int exit_code = 0;
	if (r_pipe) {
		size_t size = 0;
		void *data = SDL_ReadProcess(process, &size, &exit_code);
		if (data) {
			if (p_pipe_mutex) {
				p_pipe_mutex->lock();
			}
			r_pipe->append_utf8(static_cast<const char *>(data), int(size));
			if (p_pipe_mutex) {
				p_pipe_mutex->unlock();
			}
			SDL_free(data);
		}
	} else {
		SDL_WaitProcess(process, true, &exit_code);
	}
	SDL_DestroyProcess(process);
	if (r_exitcode) {
		*r_exitcode = exit_code;
	}
	return OK;
}

Error OS_Phoenix::create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id, bool p_open_console) {
	ProcessArgs args(p_path, p_arguments);
	SDL_Process *process = SDL_CreateProcess(args.argv.data(), false);
	if (!process) {
		return ERR_CANT_FORK;
	}
	const ProcessID pid = ProcessID(SDL_GetNumberProperty(SDL_GetProcessProperties(process), SDL_PROP_PROCESS_PID_NUMBER, 0));
	processes[pid] = process;
	if (r_child_id) {
		*r_child_id = pid;
	}
	return OK;
}

Error OS_Phoenix::kill(const ProcessID &p_pid) {
	SDL_Process **process = processes.getptr(p_pid);
	if (!process) {
		return ERR_INVALID_PARAMETER;
	}
	return SDL_KillProcess(*process, true) ? OK : FAILED;
}

bool OS_Phoenix::is_process_running(const ProcessID &p_pid) const {
	SDL_Process *const *process = processes.getptr(p_pid);
	return process && !SDL_WaitProcess(*process, false, nullptr);
}

int OS_Phoenix::get_process_exit_code(const ProcessID &p_pid) const {
	SDL_Process *const *process = processes.getptr(p_pid);
	int code = -1;
	if (process && SDL_WaitProcess(*process, false, &code)) {
		return code;
	}
	return -1;
}

bool OS_Phoenix::has_environment(const String &p_var) const {
	return SDL_getenv(p_var.utf8().get_data()) != nullptr;
}

String OS_Phoenix::get_environment(const String &p_var) const {
	const char *v = SDL_getenv(p_var.utf8().get_data());
	return v ? String::utf8(v) : String();
}

void OS_Phoenix::set_environment(const String &p_var, const String &p_value) const {
	SDL_setenv_unsafe(p_var.utf8().get_data(), p_value.utf8().get_data(), 1);
}

void OS_Phoenix::unset_environment(const String &p_var) const {
	SDL_unsetenv_unsafe(p_var.utf8().get_data());
}

String OS_Phoenix::get_name() const {
	const String platform = String::utf8(SDL_GetPlatform());
	if (platform == "Emscripten") {
		return "Web";
	}
	if (platform == "Mac OS X") {
		return "macOS";
	}
	return platform;  // "Linux", "Windows", "iOS", "Android", ...
}

OS::DateTime OS_Phoenix::get_datetime(bool p_utc) const {
	SDL_Time now = 0;
	SDL_GetCurrentTime(&now);
	SDL_DateTime dt{};
	SDL_TimeToDateTime(now, &dt, !p_utc);
	DateTime ret;
	ret.year = dt.year;
	ret.month = Month(dt.month);           // 1..12, same as Godot's MONTH_JANUARY = 1
	ret.day = uint8_t(dt.day);
	ret.weekday = Weekday(dt.day_of_week);  // 0 = Sunday in both
	ret.hour = uint8_t(dt.hour);
	ret.minute = uint8_t(dt.minute);
	ret.second = uint8_t(dt.second);
	ret.dst = false;
	return ret;
}

OS::TimeZoneInfo OS_Phoenix::get_time_zone_info() const {
	SDL_Time now = 0;
	SDL_GetCurrentTime(&now);
	SDL_DateTime dt{};
	SDL_TimeToDateTime(now, &dt, true);
	TimeZoneInfo info;
	info.bias = dt.utc_offset / 60;
	info.name = String();
	return info;
}

double OS_Phoenix::get_unix_time() const {
	SDL_Time now = 0;
	SDL_GetCurrentTime(&now);
	return double(now) / double(SDL_NS_PER_SECOND);
}

void OS_Phoenix::delay_usec(uint32_t p_usec) const {
	SDL_DelayNS(Uint64(p_usec) * 1000);
}

uint64_t OS_Phoenix::get_ticks_usec() const {
	return SDL_GetTicksNS() / 1000;
}

String OS_Phoenix::get_resource_dir() const {
	return resource_dir.is_empty() ? OS::get_resource_dir() : resource_dir;
}

String OS_Phoenix::get_data_path() const {
#ifdef __EMSCRIPTEN__
	// Fixed, absolute root in the virtual file system (Godot's web platform uses "/userfs"
	// too). Persistence is the storage service's job (OPFS under /persistent). Under Node with
	// NODERAWFS (tests) "/" is the host's root, which may not be writable: use the temp dir.
	// access() is not reliable here (Emscripten reports uid 0), so probe with a real write.
	if (SDL_CreateDirectory("/userfs/.probe")) {
		SDL_RemovePath("/userfs/.probe");
		return "/userfs";
	}
	return get_cache_path().path_join("phoenix-userdata");
#else
	// SDL's per-user data root (XDG_DATA_HOME on Linux, %APPDATA% on Windows,
	// ~/Library/Application Support on macOS): SDL_GetPrefPath returns <root>/<org>/<app>/.
	char *pref = SDL_GetPrefPath("phoenix", "_probe");
	String root = pref ? String::utf8(pref).replace_char('\\', '/') : String();
	if (pref) {
		SDL_RemovePath(pref);  // the probe directory SDL just created
		SDL_free(pref);
	}
	if (root.ends_with("/")) {
		root = root.substr(0, root.length() - 1);
	}
	root = root.get_base_dir().get_base_dir();
	if (!root.is_absolute_path()) {
		// SDL could not resolve a per-user root (no HOME, sandbox): fall back to a temp dir.
		root = get_cache_path().path_join("phoenix-userdata");
	}
	return root;
#endif
}

String OS_Phoenix::get_config_path() const {
#if defined(__linux__) || defined(__FreeBSD__)
	const String xdg = get_environment("XDG_CONFIG_HOME");
	if (!xdg.is_empty() && xdg.is_absolute_path()) {
		return xdg;
	}
	if (has_environment("HOME")) {
		return get_environment("HOME").path_join(".config");
	}
#endif
	return get_data_path();
}

String OS_Phoenix::get_user_data_dir(const String &p_user_dir) const {
	// Godot passes "<godot dir name>/app_userdata/<project>" (or a custom dir name).
	const String dir = get_data_path().path_join(p_user_dir.is_empty() ? app_name : p_user_dir);
	SDL_CreateDirectory(dir.utf8().get_data());  // creates missing parents too
	return dir;
}

int OS_Phoenix::get_process_id() const {
#ifdef _WIN32
	return _getpid();
#elif defined(__EMSCRIPTEN__)
	return 42;  // Godot's web platform reports a fixed id as well
#else
	return int(getpid());
#endif
}

String OS_Phoenix::get_locale() const {
	int count = 0;
	SDL_Locale **locales = SDL_GetPreferredLocales(&count);
	String locale = "en";
	if (locales && count > 0 && locales[0]->language) {
		locale = String::utf8(locales[0]->language);
		if (locales[0]->country) {
			locale += "_" + String::utf8(locales[0]->country);
		}
	}
	SDL_free(locales);
	return locale;
}

int OS_Phoenix::get_processor_count() const {
	return SDL_GetNumLogicalCPUCores();
}

String OS_Phoenix::get_cache_path() const {
	for (const char *var : { "TMPDIR", "TEMP", "TMP" }) {
		if (const char *v = SDL_getenv(var)) {
			return String::utf8(v).replace_char('\\', '/');
		}
	}
	return "/tmp";
}
