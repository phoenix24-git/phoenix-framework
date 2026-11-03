// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix compat seam for Godot core/. core/extension/godot_instance.cpp (libgodot
// embedding) drives Godot's Main; Phoenix owns its own main loop, so these report
// "unavailable" and GodotInstance::start() fails cleanly instead of running Godot.
#pragma once

#include "core/error/error_list.h"

class Main {
public:
	static Error setup2(bool p_show_boot_logo = true) { return ERR_UNAVAILABLE; }
	static int start() { return 1; }  // EXIT_FAILURE
	static bool iteration() { return true; }  // "should quit"
};
