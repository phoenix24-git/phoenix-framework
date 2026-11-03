// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "file_access_sdl.h"

#include "core/os/os.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>

namespace {

bool path_info(const String &p_path, SDL_PathInfo *r_info) {
	return SDL_GetPathInfo(p_path.utf8().get_data(), r_info);
}

uint64_t to_unix_seconds(SDL_Time p_time) {
	return p_time > 0 ? uint64_t(p_time / SDL_NS_PER_SECOND) : 0;
}

} // namespace

// --- FileAccessSDL ----------------------------------------------------------------------------

FileAccessSDL::~FileAccessSDL() {
	_close();
}

Error FileAccessSDL::open_internal(const String &p_path, int p_mode_flags) {
	_close();
	path_src = p_path;
	path = fix_path(p_path);
	flags = p_mode_flags;
	last_eof = false;

	const char *mode = nullptr;
	switch (p_mode_flags) {
		case READ: mode = "rb"; break;
		case WRITE: mode = "wb"; break;
		case READ_WRITE: mode = "r+b"; break;
		case WRITE_READ: mode = "w+b"; break;
		default: return ERR_INVALID_PARAMETER;
	}

	SDL_PathInfo info;
	if (path_info(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
		return ERR_FILE_CANT_OPEN;
	}

	// Like Godot's Unix driver: with backup-save enabled (the editor turns it on), plain WRITE
	// goes to a temp file renamed on close, so a crash never leaves a half-written file.
	String open_path = path;
	if (is_backup_save_enabled() && p_mode_flags == WRITE) {
		save_path = path;
		open_path = path + ".tmp";
	}

	stream = SDL_IOFromFile(open_path.utf8().get_data(), mode);
	if (!stream) {
		save_path = String();
		last_error = (p_mode_flags & READ) && !path_info(path, &info) ? ERR_FILE_NOT_FOUND : ERR_FILE_CANT_OPEN;
		return last_error;
	}
	last_error = OK;
	return OK;
}

void FileAccessSDL::_close() {
	if (!stream) {
		return;
	}
	SDL_CloseIO(stream);
	stream = nullptr;
	if (!save_path.is_empty()) {
		const CharString tmp = (save_path + ".tmp").utf8();
		if (!SDL_RenamePath(tmp.get_data(), save_path.utf8().get_data())) {
			// Some platforms refuse to rename over an existing file.
			SDL_RemovePath(save_path.utf8().get_data());
			if (!SDL_RenamePath(tmp.get_data(), save_path.utf8().get_data())) {
				last_error = ERR_FILE_CANT_WRITE;
			}
		}
		save_path = String();
	}
}

void FileAccessSDL::close() {
	_close();
}

bool FileAccessSDL::is_open() const {
	return stream != nullptr;
}

String FileAccessSDL::get_path() const {
	return path_src;
}

String FileAccessSDL::get_path_absolute() const {
	return path;
}

void FileAccessSDL::seek(uint64_t p_position) {
	ERR_FAIL_NULL_MSG(stream, "File must be opened before use.");
	last_eof = false;
	if (SDL_SeekIO(stream, Sint64(p_position), SDL_IO_SEEK_SET) < 0) {
		last_error = FAILED;
	}
}

void FileAccessSDL::seek_end(int64_t p_position) {
	ERR_FAIL_NULL_MSG(stream, "File must be opened before use.");
	last_eof = false;
	if (SDL_SeekIO(stream, Sint64(p_position), SDL_IO_SEEK_END) < 0) {
		last_error = FAILED;
	}
}

uint64_t FileAccessSDL::get_position() const {
	ERR_FAIL_NULL_V_MSG(stream, 0, "File must be opened before use.");
	const Sint64 pos = SDL_TellIO(stream);
	return pos < 0 ? 0 : uint64_t(pos);
}

uint64_t FileAccessSDL::get_length() const {
	ERR_FAIL_NULL_V_MSG(stream, 0, "File must be opened before use.");
	const Sint64 size = SDL_GetIOSize(stream);
	return size < 0 ? 0 : uint64_t(size);
}

bool FileAccessSDL::eof_reached() const {
	return last_eof;
}

uint64_t FileAccessSDL::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_NULL_V_MSG(stream, 0, "File must be opened before use.");
	ERR_FAIL_COND_V(!p_dst && p_length > 0, 0);
	const size_t read = SDL_ReadIO(stream, p_dst, size_t(p_length));
	if (read < p_length) {
		last_eof = true;
	}
	return read;
}

Error FileAccessSDL::get_error() const {
	return last_eof ? ERR_FILE_EOF : last_error;
}

Error FileAccessSDL::resize(int64_t p_length) {
	ERR_FAIL_NULL_V_MSG(stream, ERR_FILE_CANT_OPEN, "File must be opened before use.");
	const uint64_t length = get_length();
	if (uint64_t(p_length) == length) {
		return OK;
	}
	if (uint64_t(p_length) > length) {
		// Grow with zeros (SDL_IOStream has no truncate; shrinking needs the native driver).
		const uint64_t pos = get_position();
		seek_end(0);
		static const uint8_t zeros[4096] = {};
		uint64_t remaining = uint64_t(p_length) - length;
		while (remaining > 0) {
			const uint64_t chunk = MIN(remaining, uint64_t(sizeof(zeros)));
			if (SDL_WriteIO(stream, zeros, size_t(chunk)) != chunk) {
				return ERR_FILE_CANT_WRITE;
			}
			remaining -= chunk;
		}
		seek(pos);
		return OK;
	}
	return ERR_UNAVAILABLE;
}

void FileAccessSDL::flush() {
	ERR_FAIL_NULL_MSG(stream, "File must be opened before use.");
	SDL_FlushIO(stream);
}

bool FileAccessSDL::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_NULL_V_MSG(stream, false, "File must be opened before use.");
	ERR_FAIL_COND_V(!p_src && p_length > 0, false);
	return SDL_WriteIO(stream, p_src, size_t(p_length)) == p_length;
}

bool FileAccessSDL::file_exists(const String &p_path) {
	SDL_PathInfo info;
	return path_info(fix_path(p_path), &info) && info.type == SDL_PATHTYPE_FILE;
}

uint64_t FileAccessSDL::_get_modified_time(const String &p_file) {
	SDL_PathInfo info;
	return path_info(fix_path(p_file), &info) ? to_unix_seconds(info.modify_time) : 0;
}

uint64_t FileAccessSDL::_get_access_time(const String &p_file) {
	SDL_PathInfo info;
	return path_info(fix_path(p_file), &info) ? to_unix_seconds(info.access_time) : 0;
}

int64_t FileAccessSDL::_get_size(const String &p_file) {
	SDL_PathInfo info;
	return path_info(fix_path(p_file), &info) ? int64_t(info.size) : -1;
}

BitField<FileAccess::UnixPermissionFlags> FileAccessSDL::_get_unix_permissions(const String &p_file) {
	return 0;
}

Error FileAccessSDL::_set_unix_permissions(const String &p_file, BitField<FileAccess::UnixPermissionFlags> p_permissions) {
	return ERR_UNAVAILABLE;
}

bool FileAccessSDL::_get_hidden_attribute(const String &p_file) {
	return p_file.get_file().begins_with(".");
}

Error FileAccessSDL::_set_hidden_attribute(const String &p_file, bool p_hidden) {
	return ERR_UNAVAILABLE;
}

bool FileAccessSDL::_get_read_only_attribute(const String &p_file) {
	return false;
}

Error FileAccessSDL::_set_read_only_attribute(const String &p_file, bool p_ro) {
	return ERR_UNAVAILABLE;
}

// --- DirAccessSDL -----------------------------------------------------------------------------

DirAccessSDL::DirAccessSDL() {
	char *cwd = SDL_GetCurrentDirectory();
	current_dir = cwd ? String::utf8(cwd) : String("/");
	SDL_free(cwd);
	current_dir = current_dir.replace_char('\\', '/');
	if (current_dir.length() > 1 && current_dir.ends_with("/")) {
		current_dir = current_dir.substr(0, current_dir.length() - 1);
	}
}

DirAccessSDL::~DirAccessSDL() {
	list_dir_end();
}

String DirAccessSDL::_abs(const String &p_path) const {
	String p = fix_path(p_path);
	if (p.is_relative_path()) {
		p = current_dir.path_join(p);
	}
	return p.simplify_path();
}

Error DirAccessSDL::list_dir_begin() {
	list_dir_end();
	int count = 0;
	char **entries = SDL_GlobDirectory(current_dir.utf8().get_data(), nullptr, 0, &count);
	if (!entries) {
		return ERR_CANT_OPEN;
	}
	for (int i = 0; i < count; i++) {
		listing.push_back(String::utf8(entries[i]));
	}
	SDL_free(entries);
	listing.sort();
	list_index = 0;
	listing_active = true;
	return OK;
}

String DirAccessSDL::get_next() {
	if (!listing_active || list_index >= listing.size()) {
		return String();
	}
	const String name = listing[list_index++];
	SDL_PathInfo info;
	cur_is_dir = path_info(current_dir.path_join(name), &info) && info.type == SDL_PATHTYPE_DIRECTORY;
	cur_is_hidden = name.begins_with(".");
	return name;
}

bool DirAccessSDL::current_is_dir() const {
	return cur_is_dir;
}

bool DirAccessSDL::current_is_hidden() const {
	return cur_is_hidden;
}

void DirAccessSDL::list_dir_end() {
	listing.clear();
	list_index = -1;
	listing_active = false;
}

Error DirAccessSDL::change_dir(String p_dir) {
	const String target = _abs(p_dir);
	SDL_PathInfo info;
	if (!path_info(target, &info) || info.type != SDL_PATHTYPE_DIRECTORY) {
		return ERR_INVALID_PARAMETER;
	}
	current_dir = target;
	return OK;
}

String DirAccessSDL::get_current_dir(bool p_include_drive) const {
	const String base = _get_root_path();
	if (!base.is_empty()) {
		const String bd = current_dir.replace_first(base, "");
		return _get_root_string() + (bd.begins_with("/") ? bd.substr(1) : bd);
	}
	return current_dir;
}

Error DirAccessSDL::make_dir(String p_dir) {
	const String target = _abs(p_dir);
	SDL_PathInfo info;
	if (path_info(target, &info)) {
		return ERR_ALREADY_EXISTS;
	}
	return SDL_CreateDirectory(target.utf8().get_data()) ? OK : ERR_CANT_CREATE;
}

bool DirAccessSDL::file_exists(String p_file) {
	SDL_PathInfo info;
	return path_info(_abs(p_file), &info) && info.type == SDL_PATHTYPE_FILE;
}

bool DirAccessSDL::dir_exists(String p_dir) {
	SDL_PathInfo info;
	return path_info(_abs(p_dir), &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

Error DirAccessSDL::rename(String p_path, String p_new_path) {
	return SDL_RenamePath(_abs(p_path).utf8().get_data(), _abs(p_new_path).utf8().get_data()) ? OK : FAILED;
}

Error DirAccessSDL::remove(String p_path) {
	return SDL_RemovePath(_abs(p_path).utf8().get_data()) ? OK : FAILED;
}
