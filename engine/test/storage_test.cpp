// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include <doctest/doctest.h>

#include <optional>
#include <string>

#include "storage/storage_service.h"

using phoenix::StorageService;

TEST_SUITE("StorageService") {
    TEST_CASE("keys are restricted to safe file names") {
        CHECK(StorageService::valid_key("counter"));
        CHECK(StorageService::valid_key("save-1.json"));
        CHECK_FALSE(StorageService::valid_key(""));
        CHECK_FALSE(StorageService::valid_key("../etc/passwd"));
        CHECK_FALSE(StorageService::valid_key(".hidden"));
        CHECK_FALSE(StorageService::valid_key("a/b"));
    }

#ifndef __EMSCRIPTEN__  // OPFS needs a browser; covered by the web e2e suite.
    TEST_CASE("write then read round-trips on the IO thread") {
        const char* tmp = SDL_GetUserFolder(SDL_FOLDER_HOME);
        std::string root = std::string(tmp ? tmp : ".") + "/.phoenix-storage-test";
        StorageService storage;
        REQUIRE(storage.start(root));
        storage.write("counter", "42");
        std::optional<std::string> result;
        bool called = false;
        storage.read("counter", [&](std::optional<std::string> v) {
            called = true;
            result = std::move(v);
        });
        storage.flush();
        storage.pump();
        CHECK(called);
        REQUIRE(result.has_value());
        CHECK(*result == "42");
        CHECK(std::string(storage.backend()) == "native");

        bool missing_called = false;
        storage.read("does-not-exist", [&](std::optional<std::string> v) {
            missing_called = true;
            CHECK_FALSE(v.has_value());
        });
        storage.flush();
        storage.pump();
        CHECK(missing_called);
        storage.stop();
        SDL_RemovePath((root + "/counter").c_str());
        SDL_RemovePath(root.c_str());
    }
#endif
}
