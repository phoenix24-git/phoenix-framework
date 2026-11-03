// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Godot's OS singleton for Phoenix, implemented on SDL3 (time, env, processes, paths)
// so Godot core/ behaves the same on desktop and on the web.
#pragma once

#include "core/os/os.h"
#include "core/templates/hash_map.h"

struct SDL_Process;

class OS_Phoenix : public OS {
	MainLoop *main_loop = nullptr;
	String resource_dir;
	String app_name;
	String data_root;  // resolved once in initialize() (user:// lives below it)
	mutable HashMap<ProcessID, SDL_Process *> processes;

public:
	explicit OS_Phoenix(String p_app_name = "phoenix");

	void set_resource_dir(const String &p_dir) { resource_dir = p_dir; }
	/// Overrides the per-user data root (default: the platform's, see get_data_path()).
	void set_data_root(const String &p_dir) { data_root = p_dir; }
	/// The platform default for get_data_path() (probes the file system; not cached).
	static String detect_data_root();
	using OS::set_cmdline;  // public for CoreRuntime

	virtual void initialize() override;
	virtual void initialize_joypads() override {}
	virtual void set_main_loop(MainLoop *p_main_loop) override { main_loop = p_main_loop; }
	virtual void delete_main_loop() override;
	virtual MainLoop *get_main_loop() const override { return main_loop; }
	virtual void finalize() override;
	virtual void finalize_core() override {}
	virtual bool _check_internal_feature_support(const String &p_feature) override;

	virtual Vector<String> get_video_adapter_driver_info() const override { return Vector<String>(); }
	virtual String get_stdin_string(int64_t p_buffer_size = 1024) override;
	virtual PackedByteArray get_stdin_buffer(int64_t p_buffer_size = 1024) override;
	virtual Error get_entropy(uint8_t *r_buffer, int p_bytes) override;

	virtual Error execute(const String &p_path, const List<String> &p_arguments, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool read_stderr = false, Mutex *p_pipe_mutex = nullptr, bool p_open_console = false) override;
	virtual Error create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id = nullptr, bool p_open_console = false) override;
	virtual Error kill(const ProcessID &p_pid) override;
	virtual bool is_process_running(const ProcessID &p_pid) const override;
	virtual int get_process_exit_code(const ProcessID &p_pid) const override;

	virtual bool has_environment(const String &p_var) const override;
	virtual String get_environment(const String &p_var) const override;
	virtual void set_environment(const String &p_var, const String &p_value) const override;
	virtual void unset_environment(const String &p_var) const override;

	virtual String get_name() const override;
	virtual String get_distribution_name() const override { return String(); }
	virtual String get_version() const override { return String(); }

	virtual DateTime get_datetime(bool p_utc = false) const override;
	virtual TimeZoneInfo get_time_zone_info() const override;
	virtual double get_unix_time() const override;
	virtual void delay_usec(uint32_t p_usec) const override;
	virtual uint64_t get_ticks_usec() const override;

	virtual String get_resource_dir() const override;
	virtual String get_user_data_dir(const String &p_user_dir) const override;
	virtual String get_locale() const override;
	virtual int get_processor_count() const override;
	virtual String get_cache_path() const override;
	virtual String get_data_path() const override;
	virtual String get_config_path() const override;
	virtual String get_godot_dir_name() const override { return "phoenix"; }
	virtual int get_process_id() const override;
};
