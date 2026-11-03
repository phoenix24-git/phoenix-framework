// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix compat seam for Godot core/ (see ../README.md). Declares only what the vendored
// core/ references; the API shape mirrors Godot (MIT) so upstream core/ compiles unmodified.
// Phoenix: the window/event layer is SDL3. core/ only pumps events while the remote debugger
// blocks the main thread.
#pragma once

class DisplayServer {
public:
	static DisplayServer *get_singleton();
	static void set_singleton(DisplayServer *p_server);
	virtual ~DisplayServer() = default;
	virtual void force_process_and_drop_events() {}
	virtual void process_events() {}
};
