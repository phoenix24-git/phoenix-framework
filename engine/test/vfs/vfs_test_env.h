// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

namespace phoenix::vfs_test {
extern std::string g_project_dir;  ///< host folder behind res://
extern std::string g_user_root;    ///< CoreOptions::user_data_root
extern std::string g_executable;   ///< this test binary (for the main-pack child process)
}  // namespace phoenix::vfs_test
