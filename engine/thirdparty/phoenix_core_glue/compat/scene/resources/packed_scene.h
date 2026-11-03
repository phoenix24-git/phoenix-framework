// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix compat seam for Godot core/ (see ../README.md). Declares only what the vendored
// core/ references; the API shape mirrors Godot (MIT) so upstream core/ compiles unmodified.
// Phoenix: placeholder for Godot's PackedScene/SceneState until Phoenix's .tscn/.scn
// importer provides the real implementation (engine roadmap: Godot formats). It reports an
// empty scene, so binary-resource class scanning simply finds no nodes.
#pragma once

#include "core/io/resource.h"

class SceneState : public RefCounted {
	GDCLASS(SceneState, RefCounted);

public:
	int get_node_count() const { return 0; }
	StringName get_node_type(int p_idx) const { return StringName(); }
	int get_node_property_count(int p_idx) const { return 0; }
	Variant get_node_property_value(int p_idx, int p_prop) const { return Variant(); }
};

class PackedScene : public Resource {
	GDCLASS(PackedScene, Resource);
	Ref<SceneState> state;

public:
	PackedScene() { state.instantiate(); }
	Ref<SceneState> get_state() const { return state; }
};
