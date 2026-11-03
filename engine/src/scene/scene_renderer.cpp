// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/scene/scene_renderer.h"

#include <algorithm>
#include <cmath>

namespace phoenix::scene {

namespace {

std::uint32_t to_rgba(const Color& c) {
    const auto ch = [](float v) {
        return std::uint32_t(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return (ch(c.r) << 24) | (ch(c.g) << 16) | (ch(c.b) << 8) | ch(c.a);
}

// Unit cube corners (±0.5) indexed by bits: x = bit 0, y = bit 1, z = bit 2.
Vector3 corner(int i) {
    return Vector3((i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f);
}

struct Face {
    Vector3 normal;
    int c0, c1, c3;  // corner o, o + a, o + b (o + a + b is implied)
};

constexpr int kFaceCount = 6;
const Face kFaces[kFaceCount] = {
    {Vector3(1, 0, 0), 1, 3, 5},  {Vector3(-1, 0, 0), 0, 4, 2}, {Vector3(0, 1, 0), 2, 6, 3},
    {Vector3(0, -1, 0), 0, 1, 4}, {Vector3(0, 0, 1), 4, 5, 6},  {Vector3(0, 0, -1), 0, 2, 1},
};

// One mesh's visible faces, plus what ordering needs: its world bounds and view depth.
struct DrawMesh {
    Vector3 min, max;  // world-space AABB
    float depth;       // view-space z of the center (more negative = farther)
    PhxDrawRect faces[3];
    int face_count = 0;
};

// True when `a` must be drawn before `b`: they are separated along a world axis and `a` is on
// the far side from the camera (whose view direction is `forward`). Boxes that don't overlap on
// some axis can always be ordered this way; that's what makes stacked boxes at different
// heights correct, where sorting by depth alone fails.
bool behind(const DrawMesh& a, const DrawMesh& b, const Vector3& forward) {
    constexpr float eps = 1e-3f;
    for (int k = 0; k < 3; ++k) {
        if (std::abs(forward[k]) < 1e-4f) {
            continue;
        }
        // The camera sits on the side its view direction points away from, so `a` is farther
        // when it lies on the side the camera looks toward.
        if (a.max[k] <= b.min[k] + eps) {
            return forward[k] < 0;  // a on the low side; the camera looks toward -k
        }
        if (b.max[k] <= a.min[k] + eps) {
            return forward[k] > 0;
        }
    }
    return false;
}

}  // namespace

GameObject main_camera(Scene& scene) {
    GameObject first;
    GameObject tagged;
    scene.world().query<const Camera>().each([&](ecs::Entity e, const Camera&) {
        const GameObject go(&scene, e);
        if (!go.activeInHierarchy()) {
            return;
        }
        if (!first.valid()) {
            first = go;
        }
        if (!tagged.valid() && go.CompareTag("MainCamera")) {
            tagged = go;
        }
    });
    return tagged.valid() ? tagged : first;
}

std::uint32_t render_scene(Scene& scene, float width, float height, std::vector<PhxDrawRect>& out,
                           SceneRenderStats* stats) {
    SceneRenderStats local_stats;
    SceneRenderStats& st = stats ? *stats : local_stats;
    st = {};
    const GameObject camera = main_camera(scene);
    if (!camera.valid() || width <= 0 || height <= 0) {
        return 0x000000FFu;
    }
    st.has_camera = true;
    ecs::World& world = scene.world();
    const Camera& cam = *world.get<Camera>(camera.entity());
    Color clear = cam.backgroundColor;
    clear.a = 1.0f;

    // Directional light: the first active one, else a soft key light from the upper left.
    Vector3 to_light = Vector3(0.45f, 1.0f, 0.3f).normalized();
    Color light_color(1, 1, 1, 1);
    float intensity = 1.0f;
    bool found = false;
    world.query<const Light, const LocalToWorld>().each(
        [&](ecs::Entity e, const Light& light, const LocalToWorld& ltw) {
            if (found || light.type != LightType::Directional ||
                !GameObject(&scene, e).activeInHierarchy()) {
                return;
            }
            found = true;
            to_light = ltw.value.basis.get_column(2).normalized();  // the light shines along -Z
            light_color = light.color;
            intensity = light.intensity;
        });

    // View: the camera's world transform inverted; orthographic, orthographicSize = half height.
    const Transform3D view = world.get<LocalToWorld>(camera.entity())->value.affine_inverse();
    const float half_h = std::max(cam.orthographicSize, 0.001f);
    const float ppu = height / (2.0f * half_h);  // pixels per world unit
    const float cx = width * 0.5f, cy = height * 0.5f;
    const auto project = [&](const Vector3& v) { return Vector2(cx + v.x * ppu, cy - v.y * ppu); };

    std::vector<DrawMesh> meshes;
    world.query<const LocalToWorld, const MeshFilter, const MeshRenderer>().each(
        [&](ecs::Entity e, const LocalToWorld& ltw, const MeshFilter& filter,
            const MeshRenderer& renderer) {
            if (filter.primitive != PrimitiveType::Cube || renderer.color.a <= 0.0f ||
                !GameObject(&scene, e).activeInHierarchy()) {
                return;
            }
            ++st.meshes;
            DrawMesh mesh;
            const Transform3D to_view = view * ltw.value;
            Vector3 v[8];
            for (int i = 0; i < 8; ++i) {
                v[i] = to_view.xform(corner(i));
                const Vector3 w = ltw.value.xform(corner(i));
                mesh.min = i == 0 ? w : mesh.min.min(w);
                mesh.max = i == 0 ? w : mesh.max.max(w);
            }
            mesh.depth = to_view.origin.z;
            for (const Face& f : kFaces) {
                const Vector3 n_view = to_view.basis.xform(f.normal);
                if (n_view.z <= 1e-5f || mesh.face_count == 3) {
                    continue;  // facing away (the camera looks down -Z); a box shows at most 3
                }
                const Vector3 n_world = ltw.value.basis.xform(f.normal).normalized();
                const float diffuse = std::max(0.0f, n_world.dot(to_light)) * intensity;
                const float shade = 0.42f + 0.58f * diffuse;
                const Color& base = renderer.color;
                const Color lit(base.r * shade * light_color.r, base.g * shade * light_color.g,
                                base.b * shade * light_color.b, base.a);
                const Vector2 o = project(v[f.c0]);
                const Vector2 a = project(v[f.c1]) - o;
                const Vector2 b = project(v[f.c3]) - o;
                mesh.faces[mesh.face_count++] =
                    PhxDrawRect{o.x, o.y, a.x, a.y, to_rgba(lit), b.x, b.y, PHX_DRAW_PARALLELOGRAM};
            }
            if (mesh.face_count > 0) {
                meshes.push_back(mesh);
            }
        });

    // Order meshes back to front: a topological sort of the "behind" relation, taking the
    // farthest ready mesh first; depth order breaks cycles (intersecting or rotated boxes).
    const Vector3 forward = -world.get<LocalToWorld>(camera.entity())->value.basis.get_column(2);
    const std::size_t n = meshes.size();
    std::vector<int> blockers(n, 0);
    std::vector<std::vector<int>> after(n);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (i != j && behind(meshes[i], meshes[j], forward)) {
                after[i].push_back(int(j));
                ++blockers[j];
            }
        }
    }
    std::vector<bool> done(n, false);
    std::uint32_t face_total = 0;
    for (std::size_t emitted = 0; emitted < n; ++emitted) {
        int pick = -1;
        for (std::size_t i = 0; i < n; ++i) {
            if (!done[i] && blockers[i] == 0 &&
                (pick < 0 || meshes[i].depth < meshes[pick].depth)) {
                pick = int(i);
            }
        }
        if (pick < 0) {  // a cycle: fall back to the farthest remaining mesh
            for (std::size_t i = 0; i < n; ++i) {
                if (!done[i] && (pick < 0 || meshes[i].depth < meshes[pick].depth)) {
                    pick = int(i);
                }
            }
        }
        done[pick] = true;
        for (const int j : after[pick]) {
            --blockers[j];
        }
        const DrawMesh& m = meshes[pick];
        out.insert(out.end(), m.faces, m.faces + m.face_count);
        face_total += std::uint32_t(m.face_count);
    }
    st.faces = face_total;
    return to_rgba(clear);
}

}  // namespace phoenix::scene
