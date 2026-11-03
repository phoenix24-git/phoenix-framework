// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix compat seam for Godot core/ (see ../README.md). Declares only what the vendored
// core/ references; the API shape mirrors Godot (MIT) so upstream core/ compiles unmodified.
// Phoenix: default values come from ClassDB (no scene-inheritance chain yet).
#pragma once

#include "core/object/class_db.h"

class Node;

class PropertyUtils {
public:
	static Variant get_property_default_value(const Object *p_object, const StringName &p_property, bool *r_is_valid = nullptr) {
		return ClassDB::class_get_default_property_value(p_object->get_class_name(), p_property, r_is_valid);
	}
};
