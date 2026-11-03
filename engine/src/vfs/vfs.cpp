// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/vfs/vfs.h"

#include <cstring>

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"
#include "core/os/os.h"
#include "core_runtime.h"

namespace phoenix::vfs {

namespace {

constexpr const char* kRes = "res://";
constexpr const char* kUser = "user://";
constexpr const char* kUid = "uid://";

bool has_scheme(const String& p) {
    const int sep = p.find("://");
    if (sep <= 0) {
        return false;
    }
    for (int i = 0; i < sep; ++i) {
        if (!is_ascii_alphanumeric_char(p[i])) {
            return false;
        }
    }
    return true;
}

// Splits `p` into a root prefix ("res://", "/", "C:/", "//server/") and the rest.
bool split_root(const String& p, String& r_prefix, String& r_rest) {
    if (p.begins_with(kRes)) {
        r_prefix = kRes;
    } else if (p.begins_with(kUser)) {
        r_prefix = kUser;
    } else if (has_scheme(p)) {
        return false;  // unknown scheme (uid:// is handled by the caller)
    } else if (p.begins_with("//")) {
        // UNC share: keep //server/share as the root.
        const int server_end = p.find("/", 2);
        if (server_end < 0) {
            return false;
        }
        const int share_end = p.find("/", server_end + 1);
        r_prefix = share_end < 0 ? p + "/" : p.substr(0, share_end + 1);
        r_rest = share_end < 0 ? String() : p.substr(share_end + 1);
        return true;
    } else if (p.begins_with("/")) {
        r_prefix = "/";
    } else if (p.length() >= 2 && is_ascii_alphabet_char(p[0]) && p[1] == ':' &&
               (p.length() == 2 || p[2] == '/')) {
        r_prefix = p.substr(0, 2) + "/";
    } else {
        return false;  // relative
    }
    r_rest = p.substr(r_prefix.length());
    return true;
}

// An exported game sees only its main pack under res://, never files that happen to sit in
// the working directory.
bool sealed_miss(const String& normalized) {
    return normalized.begins_with(kRes) && resources_read_only() &&
           !PackedData::get_singleton()->has_path(normalized);
}

bool exists_normalized(const String& p) {
    return !p.is_empty() && !sealed_miss(p) && FileAccess::exists(p);
}

Error check_writable(const String& normalized) {
    if (normalized.is_empty()) {
        return ERR_INVALID_PARAMETER;
    }
    const Root root = root_of(normalized);
    if (root == Root::Uid || (root == Root::Resources && resources_read_only())) {
        return ERR_FILE_NO_PERMISSION;
    }
    return OK;
}

// Host path for a writable target. Mutations always go to the host file system, never
// through pack directory access (packs are read-only).
String host_path(const String& normalized) {
    return ProjectSettings::get_singleton()->globalize_path(normalized);
}

PackedStringArray list(const String& dir, bool want_dirs, bool include_hidden) {
    PackedStringArray out;
    const String path = normalize(dir);
    if (path.is_empty()) {
        return out;
    }
    Ref<DirAccess> da = DirAccess::open(path);
    if (da.is_null() || da->list_dir_begin() != OK) {
        return out;
    }
    for (String name = da->get_next(); !name.is_empty(); name = da->get_next()) {
        if (name == "." || name == ".." || da->current_is_dir() != want_dirs) {
            continue;
        }
        if (!include_hidden && name.begins_with(".")) {
            continue;
        }
        out.push_back(name);
    }
    da->list_dir_end();
    out.sort();
    return out;
}

}  // namespace

Root root_of(const String& path) {
    const String p = normalize(path);
    if (p.is_empty()) {
        return Root::Invalid;
    }
    if (p.begins_with(kRes)) {
        return Root::Resources;
    }
    if (p.begins_with(kUser)) {
        return Root::User;
    }
    if (p.begins_with(kUid)) {
        return Root::Uid;
    }
    return Root::Filesystem;
}

String normalize(const String& path) {
    const String p = path.replace_char('\\', '/');
    if (p.begins_with(kUid)) {
        // Opaque identifier (uid://<base-34 digits>): no path segments to resolve.
        const String id = p.substr(6);
        if (id.is_empty()) {
            return String();
        }
        for (int i = 0; i < id.length(); ++i) {
            if (!is_ascii_alphanumeric_char(id[i])) {
                return String();
            }
        }
        return p;
    }
    String prefix;
    String rest;
    if (!split_root(p, prefix, rest)) {
        return String();
    }
    Vector<String> parts;
    for (const String& seg : rest.split("/", false)) {
        if (seg == ".") {
            continue;
        }
        if (seg == "..") {
            if (parts.is_empty()) {
                return String();  // would climb above the root
            }
            parts.resize(parts.size() - 1);
            continue;
        }
        parts.push_back(seg);
    }
    return prefix + String("/").join(parts);
}

String globalize(const String& path) {
    const String p = normalize(path);
    if (p.is_empty()) {
        return String();
    }
    if (p.begins_with(kRes) && resources_read_only()) {
        return p;  // the exported main pack has no host folder
    }
    return ProjectSettings::get_singleton()->globalize_path(p);
}

String localize(const String& host_path_in) {
    const String p = normalize(host_path_in);
    if (p.is_empty() || p.begins_with(kRes) || p.begins_with(kUser) || p.begins_with(kUid)) {
        return p;
    }
    const String user = OS::get_singleton()->get_user_data_dir().replace_char('\\', '/');
    if (!user.is_empty() && (p == user || p.begins_with(user.path_join("")))) {
        return String(kUser) + p.substr(user.length()).trim_prefix("/");
    }
    return ProjectSettings::get_singleton()->localize_path(p);
}

bool mount_pack(const String& pack_path, bool replace_files, uint64_t offset) {
    const String p = normalize(pack_path);
    if (p.is_empty() || p.begins_with(kUid)) {
        return false;
    }
    // Packs are opened through FileAccess; user:// and host paths resolve as usual.
    const String source = p.begins_with(kUser) ? host_path(p) : p;
    // ProjectSettings exposes pack loading through its bound (scripting) API only.
    return bool(ProjectSettings::get_singleton()->call("load_resource_pack", source, replace_files,
                                                       int64_t(offset)));
}

bool using_packs() {
    return ProjectSettings::get_singleton()->is_using_datapack();
}

bool resources_read_only() {
    return foundation::CoreRuntime::is_running_from_main_pack();
}

bool file_exists(const String& path) {
    return exists_normalized(normalize(path));
}

bool dir_exists(const String& path) {
    const String p = normalize(path);
    if (p.is_empty() || p.begins_with(kUid)) {
        return false;
    }
    if (p.begins_with(kRes) && resources_read_only()) {
        return p == kRes || PackedData::get_singleton()->has_directory(p);
    }
    Ref<DirAccess> da = DirAccess::create_for_path(p);
    return da.is_valid() && da->dir_exists(p);
}

int64_t file_size(const String& path) {
    const String p = normalize(path);
    if (!exists_normalized(p)) {
        return -1;
    }
    Ref<FileAccess> f = FileAccess::open(p, FileAccess::READ);
    return f.is_valid() ? int64_t(f->get_length()) : -1;
}

uint64_t modified_time(const String& path) {
    const String p = normalize(path);
    return exists_normalized(p) ? FileAccess::get_modified_time(p) : 0;
}

PackedByteArray read_bytes(const String& path, Error* r_error) {
    const String p = normalize(path);
    if (p.is_empty()) {
        if (r_error) {
            *r_error = ERR_INVALID_PARAMETER;
        }
        return PackedByteArray();
    }
    if (!exists_normalized(p)) {
        if (r_error) {
            *r_error = ERR_FILE_NOT_FOUND;
        }
        return PackedByteArray();
    }
    Error err = OK;
    PackedByteArray data = FileAccess::get_file_as_bytes(p, &err);
    if (r_error) {
        *r_error = err;
    }
    return data;
}

String read_text(const String& path, Error* r_error) {
    Error err = OK;
    const PackedByteArray data = read_bytes(path, &err);
    if (r_error) {
        *r_error = err;
    }
    if (err != OK) {
        return String();
    }
    return String::utf8(reinterpret_cast<const char*>(data.ptr()), int(data.size()));
}

Error write_bytes(const String& path, const PackedByteArray& data) {
    const String p = normalize(path);
    if (const Error err = check_writable(p); err != OK) {
        return err;
    }
    const String target = host_path(p);
    if (const Error err = DirAccess::make_dir_recursive_absolute(target.get_base_dir());
        err != OK && err != ERR_ALREADY_EXISTS) {
        return err;
    }
    // Write next to the target, then swap it in: readers never see a partial file.
    const String tmp = target + ".phx-tmp";
    {
        Error err = OK;
        Ref<FileAccess> f = FileAccess::open(tmp, FileAccess::WRITE, &err);
        if (f.is_null()) {
            return err != OK ? err : ERR_FILE_CANT_WRITE;
        }
        if (!data.is_empty() && !f->store_buffer(data.ptr(), data.size())) {
            f.unref();
            DirAccess::remove_absolute(tmp);
            return ERR_FILE_CANT_WRITE;
        }
    }
    if (DirAccess::rename_absolute(tmp, target) != OK) {
        // Some platforms refuse to rename over an existing file.
        DirAccess::remove_absolute(target);
        if (DirAccess::rename_absolute(tmp, target) != OK) {
            DirAccess::remove_absolute(tmp);
            return ERR_FILE_CANT_WRITE;
        }
    }
    return OK;
}

Error write_text(const String& path, const String& text) {
    const CharString utf8 = text.utf8();
    PackedByteArray data;
    data.resize(utf8.length());
    if (utf8.length() > 0) {
        memcpy(data.ptrw(), utf8.get_data(), utf8.length());
    }
    return write_bytes(path, data);
}

Error make_dir(const String& path) {
    const String p = normalize(path);
    if (const Error err = check_writable(p); err != OK) {
        return err;
    }
    const Error err = DirAccess::make_dir_recursive_absolute(host_path(p));
    return err == ERR_ALREADY_EXISTS ? OK : err;
}

Error remove(const String& path) {
    const String p = normalize(path);
    if (const Error err = check_writable(p); err != OK) {
        return err;
    }
    const String target = host_path(p);
    Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    if (!da->file_exists(target) && !da->dir_exists(target)) {
        return ERR_FILE_NOT_FOUND;
    }
    return da->remove(target);
}

Error remove_recursive(const String& path) {
    const String p = normalize(path);
    if (const Error err = check_writable(p); err != OK) {
        return err;
    }
    const String target = host_path(p);
    if (p == kRes || p == kUser || target == host_path(kRes) || target == host_path(kUser)) {
        return ERR_FILE_NO_PERMISSION;  // never wipe a whole root
    }
    Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    if (da->file_exists(target)) {
        return da->remove(target);
    }
    if (da->change_dir(target) != OK) {
        return ERR_FILE_NOT_FOUND;
    }
    if (const Error err = da->erase_contents_recursive(); err != OK) {
        return err;
    }
    return DirAccess::remove_absolute(target);
}

Error rename(const String& from, const String& to) {
    const String src = normalize(from);
    const String dst = normalize(to);
    if (const Error err = check_writable(src); err != OK) {
        return err;
    }
    if (const Error err = check_writable(dst); err != OK) {
        return err;
    }
    const String dst_host = host_path(dst);
    if (const Error err = DirAccess::make_dir_recursive_absolute(dst_host.get_base_dir());
        err != OK && err != ERR_ALREADY_EXISTS) {
        return err;
    }
    return DirAccess::rename_absolute(host_path(src), dst_host);
}

Error copy(const String& from, const String& to) {
    Error err = OK;
    const PackedByteArray data = read_bytes(from, &err);  // the source may live in a pack
    if (err != OK) {
        return err;
    }
    return write_bytes(to, data);
}

PackedStringArray list_files(const String& dir, bool include_hidden) {
    return list(dir, false, include_hidden);
}

PackedStringArray list_dirs(const String& dir, bool include_hidden) {
    return list(dir, true, include_hidden);
}

String resource_dir() {
    return ProjectSettings::get_singleton()->get_resource_path();
}

String user_dir() {
    return OS::get_singleton()->get_user_data_dir();
}

}  // namespace phoenix::vfs
