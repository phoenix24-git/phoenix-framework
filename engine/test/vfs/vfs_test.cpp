// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/vfs/vfs.h"

#include <doctest/doctest.h>

#include <cstring>
#include <string>

#include "core/io/pck_packer.h"
#include "core/io/zip_io.h"
#include "core/os/os.h"
#include "vfs_test_env.h"

using namespace phoenix;

namespace {

String project_dir() {
    return String::utf8(vfs_test::g_project_dir.c_str());
}

PackedByteArray bytes(const char* text) {
    PackedByteArray out;
    const int n = int(std::strlen(text));
    out.resize(n);
    memcpy(out.ptrw(), text, n);
    return out;
}

// Builds a .pck in user:// with the given res:// files.
String make_pck(const String& name, const Vector<Pair<String, String>>& files) {
    const String path = vfs::globalize("user://packs").path_join(name);
    REQUIRE(vfs::make_dir("user://packs") == OK);
    PCKPacker pck;
    REQUIRE(pck.pck_start(path) == OK);
    for (const Pair<String, String>& f : files) {
        const CharString text = f.second.utf8();
        PackedByteArray data;
        data.resize(text.length());
        memcpy(data.ptrw(), text.get_data(), text.length());
        REQUIRE(pck.add_file_from_buffer(f.first, data) == OK);
    }
    REQUIRE(pck.flush() == OK);
    return path;
}

}  // namespace

TEST_CASE("vfs: normalize resolves segments and refuses to leave a root") {
    CHECK(vfs::normalize("res://a/./b//c.txt") == "res://a/b/c.txt");
    CHECK(vfs::normalize("res://a/b/../c.txt") == "res://a/c.txt");
    CHECK(vfs::normalize("res://") == "res://");
    CHECK(vfs::normalize("user://saves\\slot1.sav") == "user://saves/slot1.sav");
    CHECK(vfs::normalize("/var/tmp/../log") == "/var/log");
    CHECK(vfs::normalize("C:\\Games\\x") == "C:/Games/x");
    CHECK(vfs::normalize("uid://b3kd8") == "uid://b3kd8");

    // Escapes and malformed paths become empty: callers can't be walked out of the sandbox.
    CHECK(vfs::normalize("res://../secrets.txt").is_empty());
    CHECK(vfs::normalize("res://a/../../etc/passwd").is_empty());
    CHECK(vfs::normalize("user://..").is_empty());
    CHECK(vfs::normalize("relative/path.txt").is_empty());
    CHECK(vfs::normalize("http://example.com/x").is_empty());
    CHECK(vfs::normalize("uid://not/an/id").is_empty());
    CHECK(vfs::normalize("/..").is_empty());
}

TEST_CASE("vfs: root_of classifies paths") {
    CHECK(vfs::root_of("res://icon.svg") == vfs::Root::Resources);
    CHECK(vfs::root_of("user://save.dat") == vfs::Root::User);
    CHECK(vfs::root_of("uid://abc123") == vfs::Root::Uid);
    CHECK(vfs::root_of("/tmp/x") == vfs::Root::Filesystem);
    CHECK(vfs::root_of("res://../x") == vfs::Root::Invalid);
    CHECK(vfs::root_of("x.txt") == vfs::Root::Invalid);
}

TEST_CASE("vfs: res:// reads the project folder") {
    CHECK(vfs::resource_dir() == project_dir());
    CHECK_FALSE(vfs::resources_read_only());
    CHECK(vfs::read_text("res://project_only.txt") == "from the project folder");
    CHECK(vfs::read_text("res://levels/01.json") == "{\"name\": \"intro\"}");
    CHECK(vfs::file_exists("res://levels/02.json"));
    CHECK(vfs::dir_exists("res://levels"));
    CHECK_FALSE(vfs::file_exists("res://levels"));
    CHECK(vfs::file_size("res://levels/02.json") == 2);
    CHECK(vfs::file_size("res://missing.txt") == -1);
    CHECK(vfs::modified_time("res://levels/01.json") > 0);

    Error err = OK;
    CHECK(vfs::read_bytes("res://missing.txt", &err).is_empty());
    CHECK(err == ERR_FILE_NOT_FOUND);
    CHECK(vfs::read_text("res://../outside.txt", &err).is_empty());
    CHECK(err == ERR_INVALID_PARAMETER);
}

TEST_CASE("vfs: globalize and localize round-trip") {
    CHECK(vfs::globalize("res://levels/01.json") == project_dir().path_join("levels/01.json"));
    CHECK(vfs::localize(project_dir().path_join("levels/01.json")) == "res://levels/01.json");

    const String user = vfs::user_dir();
    CHECK(user.begins_with(String::utf8(vfs_test::g_user_root.c_str())));
    CHECK(user.ends_with("phoenix_vfs_tests"));  // named after the app (no project name set)
    CHECK(vfs::globalize("user://saves/a.sav") == user.path_join("saves/a.sav"));
    CHECK(vfs::localize(user.path_join("saves/a.sav")) == "user://saves/a.sav");
    CHECK(vfs::localize(user) == "user://");
    CHECK(vfs::globalize("res://../x").is_empty());
}

TEST_CASE("vfs: user:// read/write, directories and listing") {
    CHECK(vfs::write_text("user://saves/slot1.json", "{\"hp\": 7}") == OK);  // creates saves/
    CHECK(vfs::read_text("user://saves/slot1.json") == "{\"hp\": 7}");
    CHECK(vfs::write_text("user://saves/slot1.json", "{\"hp\": 9}") == OK);  // replaces
    CHECK(vfs::read_text("user://saves/slot1.json") == "{\"hp\": 9}");
    CHECK(vfs::write_bytes("user://saves/empty.bin", PackedByteArray()) == OK);
    CHECK(vfs::file_size("user://saves/empty.bin") == 0);

    const PackedByteArray blob = bytes("\x01\x02\x00\xff");
    CHECK(vfs::write_bytes("user://saves/blob.bin", blob) == OK);
    CHECK(vfs::read_bytes("user://saves/blob.bin") == blob);

    // Atomic writes leave no temporary files behind.
    CHECK(vfs::list_files("user://saves", true) ==
          PackedStringArray({"blob.bin", "empty.bin", "slot1.json"}));

    CHECK(vfs::make_dir("user://a/b/c") == OK);
    CHECK(vfs::make_dir("user://a/b/c") == OK);  // idempotent
    CHECK(vfs::dir_exists("user://a/b/c"));
    CHECK(vfs::list_dirs("user://a") == PackedStringArray({"b"}));

    CHECK(vfs::copy("user://saves/slot1.json", "user://backup/slot1.json") == OK);
    CHECK(vfs::read_text("user://backup/slot1.json") == "{\"hp\": 9}");
    CHECK(vfs::rename("user://backup/slot1.json", "user://backup/old/slot1.json") == OK);
    CHECK_FALSE(vfs::file_exists("user://backup/slot1.json"));
    CHECK(vfs::file_exists("user://backup/old/slot1.json"));

    CHECK(vfs::remove("user://saves/empty.bin") == OK);
    CHECK(vfs::remove("user://saves/empty.bin") == ERR_FILE_NOT_FOUND);
    CHECK(vfs::remove_recursive("user://backup") == OK);
    CHECK_FALSE(vfs::dir_exists("user://backup"));
    CHECK(vfs::remove_recursive("user://") == ERR_FILE_NO_PERMISSION);
    CHECK(vfs::remove_recursive("res://") == ERR_FILE_NO_PERMISSION);
    CHECK(vfs::remove_recursive("user://a") == OK);
}

TEST_CASE("vfs: listing skips hidden entries unless asked") {
    CHECK(vfs::list_files("res://") == PackedStringArray({"project_only.txt", "shared.txt"}));
    const PackedStringArray all = vfs::list_files("res://", true);
    CHECK(all.has(".hidden"));
    CHECK(vfs::list_dirs("res://") == PackedStringArray({"levels"}));
    CHECK(vfs::list_files("res://levels") == PackedStringArray({"01.json", "02.json"}));
    CHECK(vfs::list_files("res://nope").is_empty());
}

TEST_CASE("vfs: invalid and read-only targets are refused") {
    CHECK(vfs::write_text("res://../escape.txt", "x") == ERR_INVALID_PARAMETER);
    CHECK(vfs::write_text("relative.txt", "x") == ERR_INVALID_PARAMETER);
    CHECK(vfs::write_text("uid://abc", "x") == ERR_FILE_NO_PERMISSION);
    CHECK(vfs::make_dir("user://../../etc") == ERR_INVALID_PARAMETER);
    CHECK(vfs::remove("res://../../important") == ERR_INVALID_PARAMETER);
    // In development res:// is the project folder, so tools and the editor may write to it.
    CHECK(vfs::write_text("res://generated/notes.txt", "dev") == OK);
    CHECK(vfs::read_text("res://generated/notes.txt") == "dev");
}

TEST_CASE("vfs: .pck and .zip packs layer over res:// (DLC, mods)") {
    const String dlc = make_pck("dlc.pck", {{"res://dlc/forest.json", "{\"trees\": 3}"},
                                            {"res://shared.txt", "dlc version"}});
    CHECK_FALSE(vfs::using_packs());
    REQUIRE(vfs::mount_pack(dlc));
    CHECK(vfs::using_packs());
    CHECK_FALSE(vfs::resources_read_only());  // mounted packs never seal the project

    CHECK(vfs::read_text("res://dlc/forest.json") == "{\"trees\": 3}");
    CHECK(vfs::read_text("res://shared.txt") == "dlc version");  // replace_files
    CHECK(vfs::read_text("res://project_only.txt") == "from the project folder");
    CHECK(vfs::dir_exists("res://dlc"));
    CHECK(vfs::list_dirs("res://").has("dlc"));
    CHECK(vfs::list_dirs("res://").has("levels"));  // the project folder stays listable
    CHECK(vfs::list_files("res://dlc") == PackedStringArray({"forest.json"}));

    // A patch mounted without replace_files keeps the existing versions.
    const String patch = make_pck(
        "patch.pck", {{"res://shared.txt", "patch version"}, {"res://patch_only.txt", "patched"}});
    REQUIRE(vfs::mount_pack(patch, /*replace_files=*/false));
    CHECK(vfs::read_text("res://shared.txt") == "dlc version");
    CHECK(vfs::read_text("res://patch_only.txt") == "patched");

    // ZIP archives work as packs too (paths inside the zip are relative to res://).
    Ref<FileAccess> zip_io_handle;
    zlib_filefunc_def io = zipio_create_io(&zip_io_handle);  // Godot's minizip has no stdio
    zipFile zf = zipOpen2("user://packs/mod.zip", APPEND_STATUS_CREATE, nullptr, &io);
    REQUIRE(zf != nullptr);
    const char* text = "zipped content";
    REQUIRE(zipOpenNewFileInZip(zf, "mods/readme.txt", nullptr, nullptr, 0, nullptr, 0, nullptr,
                                Z_DEFLATED, Z_DEFAULT_COMPRESSION) == ZIP_OK);
    REQUIRE(zipWriteInFileInZip(zf, text, unsigned(std::strlen(text))) == ZIP_OK);
    REQUIRE(zipCloseFileInZip(zf) == ZIP_OK);
    REQUIRE(zipClose(zf, nullptr) == ZIP_OK);
    REQUIRE(vfs::mount_pack("user://packs/mod.zip"));
    CHECK(vfs::read_text("res://mods/readme.txt") == "zipped content");

    CHECK_FALSE(vfs::mount_pack("user://packs/missing.pck"));
    CHECK_FALSE(vfs::mount_pack("res://../evil.pck"));
}

#if !defined(__EMSCRIPTEN__)
TEST_CASE("vfs: an exported game runs sealed from its main pack") {
    const String pack = make_pck("game.pck", {{"res://packed/hello.txt", "from the main pack"}});
    List<String> args;
    args.push_back("--main-pack-child");
    args.push_back(pack);
    String output;
    int exit_code = -1;
    REQUIRE(OS::get_singleton()->execute(String::utf8(vfs_test::g_executable.c_str()), args,
                                         &output, &exit_code, true) == OK);
    INFO("child output: ", std::string(output.utf8().get_data()));
    CHECK(exit_code == 0);  // see run_main_pack_child() in main.cpp for the codes
}
#endif
