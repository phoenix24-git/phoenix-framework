// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Godot FileAccess / DirAccess implemented on SDL3, so Godot core/ runs on every Phoenix
// platform (Linux, Windows, macOS, web/wasm64) with one implementation. Behaviour follows
// Godot's drivers/unix (MIT), including backup-save (write to "<file>.tmp", rename on close).
#pragma once

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

struct SDL_IOStream;

class FileAccessSDL : public FileAccess {
	GDSOFTCLASS(FileAccessSDL, FileAccess);

	SDL_IOStream *stream = nullptr;
	int flags = 0;
	String path;
	String path_src;
	String save_path;
	mutable bool last_eof = false;
	mutable Error last_error = OK;

	void _close();

protected:
	virtual Error open_internal(const String &p_path, int p_mode_flags) override;
	virtual uint64_t _get_modified_time(const String &p_file) override;
	virtual uint64_t _get_access_time(const String &p_file) override;
	virtual int64_t _get_size(const String &p_file) override;
	virtual BitField<FileAccess::UnixPermissionFlags> _get_unix_permissions(const String &p_file) override;
	virtual Error _set_unix_permissions(const String &p_file, BitField<FileAccess::UnixPermissionFlags> p_permissions) override;
	virtual bool _get_hidden_attribute(const String &p_file) override;
	virtual Error _set_hidden_attribute(const String &p_file, bool p_hidden) override;
	virtual bool _get_read_only_attribute(const String &p_file) override;
	virtual Error _set_read_only_attribute(const String &p_file, bool p_ro) override;

public:
	virtual bool is_open() const override;
	virtual String get_path() const override;
	virtual String get_path_absolute() const override;
	virtual void seek(uint64_t p_position) override;
	virtual void seek_end(int64_t p_position = 0) override;
	virtual uint64_t get_position() const override;
	virtual uint64_t get_length() const override;
	virtual bool eof_reached() const override;
	virtual uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override;
	virtual Error get_error() const override;
	virtual Error resize(int64_t p_length) override;
	virtual void flush() override;
	virtual bool store_buffer(const uint8_t *p_src, uint64_t p_length) override;
	virtual bool file_exists(const String &p_path) override;
	virtual void close() override;

	FileAccessSDL() = default;
	virtual ~FileAccessSDL() override;
};

class DirAccessSDL : public DirAccess {
	GDSOFTCLASS(DirAccessSDL, DirAccess);

	String current_dir;
	Vector<String> listing;
	int list_index = -1;
	bool listing_active = false;
	bool cur_is_dir = false;
	bool cur_is_hidden = false;

	String _abs(const String &p_path) const;

public:
	virtual Error list_dir_begin() override;
	virtual String get_next() override;
	virtual bool current_is_dir() const override;
	virtual bool current_is_hidden() const override;
	virtual void list_dir_end() override;

	virtual int get_drive_count() override { return 0; }
	virtual String get_drive(int p_drive) override { return String(); }

	virtual Error change_dir(String p_dir) override;
	virtual String get_current_dir(bool p_include_drive = true) const override;
	virtual Error make_dir(String p_dir) override;
	virtual bool file_exists(String p_file) override;
	virtual bool dir_exists(String p_dir) override;
	virtual uint64_t get_space_left() override { return 0; }
	virtual Error rename(String p_path, String p_new_path) override;
	virtual Error remove(String p_path) override;
	virtual bool is_link(String p_file) override { return false; }
	virtual String read_link(String p_file) override { return p_file; }
	virtual Error create_link(String p_source, String p_target) override { return ERR_UNAVAILABLE; }
	virtual String get_filesystem_type() const override { return "SDL"; }

	DirAccessSDL();
	virtual ~DirAccessSDL() override;
};
