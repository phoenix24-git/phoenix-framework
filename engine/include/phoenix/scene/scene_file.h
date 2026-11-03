// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix scene files (.pscn): Godot's .tscn text syntax, holding Unity-style GameObjects.
//
//   [phoenix_scene format=1]
//
//   [node name="World"]                        ; the first node is the root
//
//   [node name="Main Camera" parent="."]       ; parent: "." = root, or a path "A/B"
//   tag = "MainCamera"
//
//   [component type="Transform"]               ; components belong to the node above
//   position = Vector3(6, 8, 6)
//   euler = Vector3(-35, 45, 0)                ; degrees, Unity's Z-X-Y order
//
//   [component type="Camera"]
//   projection = 1                              ; ProjectionMode.Orthographic
//   orthographicSize = 6.0
//
//   [node name="Game" parent="."]
//   [script class="Skyline.StackGame"]          ; a C# MetaBehaviour attached to "Game"
//   speed = 2.5                                 ; handed to the script as properties
//
// Values use Godot's Variant text syntax (Vector3(), Color(), "strings", arrays, ...), parsed
// by the foundation's VariantParser. Node keys: tag, layer, active. Component keys are the
// component's fields (docs/components.md); "Transform" also accepts `euler` in degrees.
#pragma once

#include "core/variant/dictionary.h"
#include "phoenix/scene/scene.h"

namespace phoenix::scene {

/// A [script] section: which class to attach to which GameObject. Scripts are attached by the
/// engine through the script module (C#) once the scene is built.
struct SceneScript {
    GameObject gameObject;
    String class_name;
    Dictionary properties;
};

struct SceneLoadResult {
    Error error = OK;
    String message;  ///< "<file>:<line>: what went wrong" when error != OK
    GameObject root;
    Vector<SceneScript> scripts;

    explicit operator bool() const { return error == OK; }
};

/// Builds the scene file's GameObjects into `scene`. On error, the objects created so far are
/// destroyed and the result carries the file and line of the problem.
SceneLoadResult load_scene(Scene& scene, const String& path);
/// Same, from text (tests, generated content). `source_name` is used in error messages.
SceneLoadResult load_scene_from_string(Scene& scene, const String& text,
                                       const String& source_name = "<string>");

}  // namespace phoenix::scene
