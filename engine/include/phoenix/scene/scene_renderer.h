// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Draws a Scene through its main camera into a display list (PhxDrawRect parallelograms).
//
// Today: orthographic cameras (a perspective Camera is drawn orthographically), MeshFilter
// cubes (PrimitiveType::Cube), MeshRenderer.color, and one Directional Light. Faces are
// back-face culled, lit, and painter-sorted. Under an orthographic projection every cube face
// is exactly a parallelogram, so this is real 3D, not a 2.5D approximation.
#pragma once

#include <cstdint>
#include <vector>

#include "phoenix/scene/scene.h"
#include "phoenix/script_abi.h"

namespace phoenix::scene {

struct SceneRenderStats {
    std::uint32_t meshes = 0;  ///< MeshRenderers considered
    std::uint32_t faces = 0;   ///< faces emitted after culling
    bool has_camera = false;
};

/// The camera used for rendering: the active Camera tagged "MainCamera", else the first active one.
GameObject main_camera(Scene& scene);

/// Appends the scene's visible faces to `out` (logical pixels, y down) for a `width` x `height`
/// viewport. Returns the clear color (0xRRGGBBAA) from the camera's backgroundColor.
std::uint32_t render_scene(Scene& scene, float width, float height, std::vector<PhxDrawRect>& out,
                           SceneRenderStats* stats = nullptr);

}  // namespace phoenix::scene
