// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix compat seam for Godot core/ (see ../README.md). Declares only what the vendored
// core/ references; the API shape mirrors Godot (MIT) so upstream core/ compiles unmodified.
// Phoenix: RenderingDevice is owned by Phoenix's RenderingServer implementation.
#pragma once

// The real header pulls these in; core/config/engine.cpp relies on that.
#include "core/core_globals.h"
#include "core/object/object.h"

class RenderingDevice {
public:
	static RenderingDevice *get_singleton();
	static void set_singleton(RenderingDevice *p_device);
	virtual ~RenderingDevice() = default;
	virtual void _set_max_fps(int p_max_fps) {}
};
