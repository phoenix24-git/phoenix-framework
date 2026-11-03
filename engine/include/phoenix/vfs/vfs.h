// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix virtual file system: Godot-style virtual paths on top of the foundation's
// FileAccess/DirAccess/PackedData.
//
//   res://   the project's resources. In development it is the project folder; packs
//            mounted with mount_pack() are layered on top (later packs win). An exported game
//            runs from a main pack (<game>.pck next to the executable, or
//            CoreOptions::main_pack), and then res:// is read-only.
//   user://  per-user writable data: saves, settings, caches. Desktop:
//            <OS data dir>/phoenix/app_userdata/<project name>; web: the OPFS mount
//            (/persistent) when the engine created it, otherwise in-memory.
//   uid://   stable resource IDs (Godot's ResourceUID), resolved to res:// paths.
//
// Any other absolute path is the host file system (tools, editor). All functions use foundation
// types (String, PackedByteArray, Error) and are thread-safe for file I/O; mount packs from the
// main thread before loading starts.
//
//   using namespace phoenix;
//   vfs::write_text("user://settings.cfg", "volume=0.8");
//   String level = vfs::read_text("res://levels/01.json");
//   vfs::mount_pack("user://dlc/forest.pck");  // its files now appear under res://
#pragma once

#include "core/error/error_list.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

namespace phoenix::vfs {

enum class Root : uint8_t {
    Resources,   ///< res://
    User,        ///< user://
    Uid,         ///< uid://
    Filesystem,  ///< an absolute host path
    Invalid,     ///< relative, unknown scheme, or escapes its root
};

Root root_of(const String& path);

/// Canonical form of a virtual or absolute path: '\' becomes '/', "." and duplicate '/' are
/// dropped, ".." is resolved. Returns an empty String when the path is relative, uses an
/// unknown scheme, or climbs above its root ("res://../secrets"), so callers can't be tricked
/// into leaving the sandbox.
String normalize(const String& path);

/// The host path for a virtual path (res://a.png -> /work/game/a.png). When res:// is the
/// exported main pack there is no host folder, and res:// paths come back unchanged.
String globalize(const String& path);
/// The virtual path for a host path inside the project or user folder, else unchanged.
String localize(const String& host_path);

/// Mounts a .pck or .zip so its files appear under res://. With replace_files, files the pack
/// shares with earlier sources (or the project folder) come from this pack. `offset` locates a
/// pack embedded in a bigger file. Accepts virtual paths (user://dlc.pck) and host paths.
bool mount_pack(const String& pack_path, bool replace_files = true, uint64_t offset = 0);
/// True when res:// is backed by any pack (a main pack or mounted packs).
bool using_packs();
/// True when res:// is the exported main pack: writes to res:// are refused.
bool resources_read_only();

bool file_exists(const String& path);
bool dir_exists(const String& path);
/// Size in bytes, or -1 when the file does not exist.
int64_t file_size(const String& path);
/// Last modification time (Unix seconds), 0 when unknown (e.g. inside packs).
uint64_t modified_time(const String& path);

PackedByteArray read_bytes(const String& path, Error* r_error = nullptr);
String read_text(const String& path, Error* r_error = nullptr);

/// Writes the whole file, creating missing parent directories. The data goes to a temporary
/// file that replaces the target only once it is complete, so a crash never leaves a
/// half-written save.
Error write_bytes(const String& path, const PackedByteArray& data);
Error write_text(const String& path, const String& text);

/// Creates the directory and any missing parents (OK if it already exists).
Error make_dir(const String& path);
/// Removes a file or an empty directory.
Error remove(const String& path);
/// Removes a directory and everything inside it (not allowed on res:// when read-only).
Error remove_recursive(const String& path);
Error rename(const String& from, const String& to);
Error copy(const String& from, const String& to);

/// Names of the files (not directories) directly inside `dir`, sorted. Hidden files
/// (starting with '.') are skipped unless include_hidden.
PackedStringArray list_files(const String& dir, bool include_hidden = false);
/// Names of the subdirectories directly inside `dir`, sorted.
PackedStringArray list_dirs(const String& dir, bool include_hidden = false);

/// Host folders behind res:// and user:// (user:// is created on first use).
String resource_dir();
String user_dir();

}  // namespace phoenix::vfs
