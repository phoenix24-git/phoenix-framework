// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix compat seam for Godot core/ (see ../README.md). Declares only what the vendored
// core/ references; the API shape mirrors Godot (MIT) so upstream core/ compiles unmodified.
// Phoenix: scenes are ECS worlds; Node is the hierarchy-view base class that Godot-compatible
// APIs (Resource::get_local_scene, local-to-scene resources) refer to.
#pragma once

#include "core/object/object.h"

class Node : public Object {
	GDCLASS(Node, Object);
};
