// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// phoenix_pack: packs a project folder into a .pck that mounts as res://.
//
//   phoenix_pack <project_dir> <output.pck> [--exclude <glob>]... [--verbose]
//
// Every file under <project_dir> becomes res://<relative path>. Hidden files and folders
// (".git", ".phoenix", ...) and build/ are skipped; --exclude adds globs matched against the
// res:// path (e.g. --exclude "res://raw/*" --exclude "*.psd").
#include <cstdio>
#include <cstring>

#include "core/io/dir_access.h"
#include "core/io/pck_packer.h"
#include "core/os/os.h"
#include "core_runtime.h"

namespace {

String g_output;  // never pack the pack being written

int usage() {
    std::fprintf(
        stderr,
        "usage: phoenix_pack <project_dir> <output.pck> [--exclude <glob>]... [--verbose]\n");
    return 2;
}

bool excluded(const String& res_path, const Vector<String>& globs) {
    for (const String& g : globs) {
        if (res_path.matchn(g)) {
            return true;
        }
    }
    return false;
}

Error add_dir(PCKPacker& pck, const String& host_dir, const String& res_dir,
              const Vector<String>& globs, int& r_count) {
    Ref<DirAccess> da = DirAccess::open(host_dir);
    if (da.is_null()) {
        return ERR_CANT_OPEN;
    }
    PackedStringArray files;
    PackedStringArray dirs;
    da->list_dir_begin();
    for (String name = da->get_next(); !name.is_empty(); name = da->get_next()) {
        if (name.begins_with(".")) {
            continue;  // hidden: VCS, editor caches
        }
        (da->current_is_dir() ? dirs : files).push_back(name);
    }
    da->list_dir_end();
    files.sort();  // deterministic packs: same input, same bytes
    dirs.sort();
    for (const String& f : files) {
        const String res_path = res_dir.path_join(f);
        if (excluded(res_path, globs) || host_dir.path_join(f).simplify_path() == g_output) {
            continue;
        }
        if (const Error err = pck.add_file(res_path, host_dir.path_join(f)); err != OK) {
            std::fprintf(stderr, "phoenix_pack: cannot add %s\n", res_path.utf8().get_data());
            return err;
        }
        ++r_count;
    }
    for (const String& d : dirs) {
        const String res_path = res_dir.path_join(d);
        if ((res_dir == "res://" && d == "build") || excluded(res_path + "/", globs)) {
            continue;
        }
        if (const Error err = add_dir(pck, host_dir.path_join(d), res_path, globs, r_count);
            err != OK) {
            return err;
        }
    }
    return OK;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        return usage();
    }
    Vector<String> globs;
    bool verbose = false;
    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "--exclude") == 0 && i + 1 < argc) {
            globs.push_back(String::utf8(argv[++i]));
        } else if (std::strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else {
            return usage();
        }
    }

    phoenix::foundation::CoreOptions options;
    options.app_name = "phoenix_pack";
    options.worker_threads = 1;
    phoenix::foundation::CoreRuntime::initialize(options);

    int result = 0;
    {
        const String project = String::utf8(argv[1]).replace_char('\\', '/');
        const String output = String::utf8(argv[2]).replace_char('\\', '/');
        g_output = output.simplify_path();
        PCKPacker pck;
        int count = 0;
        Error err = pck.pck_start(output);
        if (err == OK) {
            err = add_dir(pck, project, "res://", globs, count);
        }
        if (err == OK) {
            err = pck.flush(verbose);
        }
        if (err != OK) {
            std::fprintf(stderr, "phoenix_pack: failed (%s)\n", error_names[err]);
            result = 1;
        } else {
            std::printf("phoenix_pack: %d files -> %s\n", count, output.utf8().get_data());
        }
    }
    g_output = String();  // free before the core shuts down
    phoenix::foundation::CoreRuntime::finalize();
    return result;
}
