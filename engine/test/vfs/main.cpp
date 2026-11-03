// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
// VFS tests run against a throwaway project folder (res://) and user folder (user://).
//
// `phoenix_vfs_tests --main-pack-child <pck>` is the child process used by the exported-game
// test: it starts the runtime from a main pack and reports through its exit code.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

#include "core_runtime.h"
#include "phoenix/vfs/vfs.h"
#include "vfs_test_env.h"

namespace fs = std::filesystem;

namespace phoenix::vfs_test {
std::string g_project_dir;
std::string g_user_root;
std::string g_executable;
}  // namespace phoenix::vfs_test

namespace {

void write_file(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::FILE* f = std::fopen(path.string().c_str(), "wb");
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

// Exit codes: 0 = all checks passed, 10+ = which check failed.
int run_main_pack_child(const char* pack) {
    phoenix::foundation::CoreOptions options;
    options.app_name = "phoenix_vfs_child";
    options.main_pack = String::utf8(pack);
    options.resource_dir = String::utf8(phoenix::vfs_test::g_project_dir.c_str());
    options.user_data_root = String::utf8(phoenix::vfs_test::g_user_root.c_str());
    options.worker_threads = 1;
    phoenix::foundation::CoreRuntime::initialize(options);
    int code = 0;
    using namespace phoenix;
    if (!vfs::resources_read_only()) {
        code = 10;
    } else if (vfs::read_text("res://packed/hello.txt") != "from the main pack") {
        code = 11;
    } else if (vfs::file_exists("res://project_only.txt")) {
        code = 12;  // the project folder must not leak into an exported game
    } else if (vfs::write_text("res://packed/hello.txt", "x") != ERR_FILE_NO_PERMISSION) {
        code = 13;
    } else if (vfs::globalize("res://packed/hello.txt") != "res://packed/hello.txt") {
        code = 14;
    } else if (vfs::write_text("user://still_writable.txt", "ok") != OK) {
        code = 15;
    }
    phoenix::foundation::CoreRuntime::finalize();
    return code;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace phoenix::vfs_test;
    std::error_code ec;
    const fs::path base = fs::temp_directory_path(ec) / "phoenix_vfs_tests";
    const bool child = argc >= 3 && std::strcmp(argv[1], "--main-pack-child") == 0;
    g_project_dir = (base / "project").generic_string();
    g_user_root = (base / (child ? "user_child" : "user")).generic_string();
    g_executable = fs::absolute(argv[0], ec).generic_string();
    if (child) {
        return run_main_pack_child(argv[2]);
    }

    fs::remove_all(base, ec);
    write_file(base / "project" / "project_only.txt", "from the project folder");
    write_file(base / "project" / "shared.txt", "project version");
    write_file(base / "project" / "levels" / "01.json", "{\"name\": \"intro\"}");
    write_file(base / "project" / "levels" / "02.json", "{}");
    write_file(base / "project" / ".hidden", "secret");

    phoenix::foundation::CoreOptions options;
    options.app_name = "phoenix_vfs_tests";
    options.resource_dir = String::utf8(g_project_dir.c_str());
    options.user_data_root = String::utf8(g_user_root.c_str());
    options.worker_threads = 2;
    phoenix::foundation::CoreRuntime::initialize(options);
    doctest::Context context(argc, argv);
    const int result = context.run();
    phoenix::foundation::CoreRuntime::finalize();
    fs::remove_all(base, ec);
    return result;
}
