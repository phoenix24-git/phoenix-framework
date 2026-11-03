// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix compat seam for Godot core/ (see ../README.md). Declares only what the vendored
// core/ references; the API shape mirrors Godot (MIT) so upstream core/ compiles unmodified.
// Phoenix: core/ only calls sync() while waiting on threaded resource loads. Phoenix's
// RenderingServer installs itself with set_singleton(); a no-op server is used until then.
#pragma once

class RenderingServer {
public:
	static RenderingServer *get_singleton();
	static void set_singleton(RenderingServer *p_server);
	virtual ~RenderingServer() = default;
	virtual void sync() {}
};
