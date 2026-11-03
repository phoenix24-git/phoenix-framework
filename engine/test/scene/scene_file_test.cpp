// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/scene/scene_file.h"

#include <doctest/doctest.h>

#include <cmath>
#include <string>

#include "phoenix/scene/scene_renderer.h"

using namespace phoenix;
using namespace phoenix::scene;

namespace {

const char* kScene = R"PSCN(
[phoenix_scene format=1]

[node name="World"]

[node name="Main Camera" parent="."]
tag = "MainCamera"

[component type="Transform"]
position = Vector3(6, 8, 6)
euler = Vector3(-35.264, 45, 0)

[component type="Camera"]
projection = "Orthographic"
orthographicSize = 4.0
backgroundColor = Color(0.1, 0.2, 0.3, 1)

[node name="Sun" parent="."]

[component type="Transform"]
euler = Vector3(-50, 30, 0)

[component type="Light"]
type = "Directional"
intensity = 1

[node name="Tower" parent="."]

[node name="Base" parent="Tower"]
layer = 3

[component type="Transform"]
scale = Vector3(3, 1, 3)

[component type="MeshFilter"]

[component type="MeshRenderer"]
color = Color(0.9, 0.5, 0.2, 1)

[node name="Game" parent="."]

[script class="Skyline.StackGame"]
speed = 2.5
title = "hello"
)PSCN";

}  // namespace

TEST_CASE("scene file: builds the hierarchy, components and script list") {
    Scene scene;
    const SceneLoadResult r = load_scene_from_string(scene, kScene, "test.pscn");
    REQUIRE_MESSAGE(r, std::string(r.message.utf8().get_data()));
    CHECK(r.root.name() == "World");
    CHECK(scene.gameObjectCount() == 6);

    GameObject cam = scene.Find("Main Camera");
    REQUIRE(cam.valid());
    CHECK(cam.CompareTag("MainCamera"));
    CHECK(cam.transform().parent() == r.root.transform());
    CHECK(cam.transform().localPosition().is_equal_approx(Vector3(6, 8, 6)));
    CHECK(cam.transform().localEulerAngles().is_equal_approx(Vector3(-35.264f, 45, 0)));
    const Camera* camera = cam.GetComponent<Camera>();
    REQUIRE(camera);
    CHECK(camera->projection == ProjectionMode::Orthographic);  // enum by name
    CHECK(camera->orthographicSize == doctest::Approx(4.0));
    CHECK(camera->backgroundColor.is_equal_approx(Color(0.1f, 0.2f, 0.3f, 1)));

    GameObject base = r.root.transform().Find("Tower/Base").gameObject();
    REQUIRE(base.valid());
    CHECK(base.layer() == 3);
    CHECK(base.transform().localScale().is_equal_approx(Vector3(3, 1, 3)));
    REQUIRE(base.GetComponent<MeshFilter>());
    CHECK(base.GetComponent<MeshFilter>()->primitive == PrimitiveType::Cube);  // default
    CHECK(base.GetComponent<MeshRenderer>()->color.is_equal_approx(Color(0.9f, 0.5f, 0.2f, 1)));

    REQUIRE(r.scripts.size() == 1);
    CHECK(r.scripts[0].gameObject == scene.Find("Game"));
    CHECK(r.scripts[0].class_name == "Skyline.StackGame");
    CHECK(double(r.scripts[0].properties["speed"]) == doctest::Approx(2.5));
    CHECK(String(r.scripts[0].properties["title"]) == "hello");
}

TEST_CASE("scene file: errors name the line and undo partial scenes") {
    const auto expect_error = [](const char* text, const char* fragment) {
        Scene scene;
        const SceneLoadResult r = load_scene_from_string(scene, text, "bad.pscn");
        CHECK_FALSE(r);
        INFO("message: ", std::string(r.message.utf8().get_data()));
        CHECK(r.message.begins_with("bad.pscn:"));
        CHECK(r.message.contains(fragment));
        CHECK(scene.gameObjectCount() == 0);  // nothing left half-built
    };
    expect_error("[node name=\"A\"]", "header");
    expect_error("[phoenix_scene format=2]\n[node name=\"A\"]", "format");
    expect_error("[phoenix_scene format=1]\n[node name=\"A\"]\n[node name=\"B\"]", "parent");
    expect_error("[phoenix_scene format=1]\n[node name=\"A\"]\n[node name=\"B\" parent=\"Nope\"]",
                 "not found");
    expect_error("[phoenix_scene format=1]\n[node name=\"A\"]\n[component type=\"Teleporter\"]",
                 "unknown component");
    expect_error(
        "[phoenix_scene format=1]\n[node name=\"A\"]\n[component type=\"Camera\"]\nzoom = 2",
        "no field");
    expect_error(
        "[phoenix_scene format=1]\n[node name=\"A\"]\n[component type=\"Camera\"]\nprojection = "
        "\"Fisheye\"",
        "no value");
    expect_error(
        "[phoenix_scene format=1]\n[node name=\"A\"]\n[component type=\"Camera\"]\nfieldOfView = "
        "\"wide\"",
        "cannot assign");
    expect_error("[phoenix_scene format=1]\n[node name=\"A\"]\nspeed = 3", "node keys");
    expect_error("[phoenix_scene format=1]\n[node name=\"A\"]\n[widget]", "unknown section");
    expect_error("[phoenix_scene format=1]\n", "no [node]");

    Scene scene;
    const SceneLoadResult r = load_scene_from_string(
        scene,
        "[phoenix_scene format=1]\n\n[node name=\"A\"]\n\n[component type=\"Camera\"]\nzoom = 2\n",
        "f");
    CHECK(r.message.begins_with("f:6:"));  // the line of the bad key
}

TEST_CASE("scene renderer: an isometric camera sees three lit faces per cube") {
    Scene scene;
    const SceneLoadResult r = load_scene_from_string(scene, kScene, "test.pscn");
    REQUIRE(r);
    scene.SetParallel(false);
    scene.Update(1.0f / 60.0f);  // the transform system computes LocalToWorld

    std::vector<PhxDrawRect> out;
    SceneRenderStats stats;
    const std::uint32_t clear = render_scene(scene, 800, 600, out, &stats);
    CHECK(stats.has_camera);
    CHECK(stats.meshes == 1);
    CHECK(stats.faces == 3);  // top + two sides
    REQUIRE(out.size() == 3);
    CHECK(clear == 0x1A334DFFu);  // Color(0.1, 0.2, 0.3) -> RRGGBBAA
    for (const PhxDrawRect& f : out) {
        CHECK(f.flags == PHX_DRAW_PARALLELOGRAM);
        // The block sits at the origin, which the camera looks at: faces surround the center.
        CHECK(std::abs(f.x - 400) < 300);
        CHECK(std::abs(f.y - 300) < 300);
    }
    // Lighting: the three faces get different shades of the same base color.
    CHECK(out[0].rgba != out[1].rgba);
    CHECK(out[1].rgba != out[2].rgba);

    // Hidden objects and transparent colors are skipped.
    scene.Find("Base").SetActive(false);
    out.clear();
    render_scene(scene, 800, 600, out, &stats);
    CHECK(out.empty());
}

TEST_CASE("scene renderer: no camera draws nothing") {
    Scene scene;
    GameObject cube = scene.CreateGameObject("Cube");
    cube.AddComponent<MeshFilter>();
    cube.AddComponent<MeshRenderer>();
    std::vector<PhxDrawRect> out;
    SceneRenderStats stats;
    CHECK(render_scene(scene, 800, 600, out, &stats) == 0x000000FFu);
    CHECK_FALSE(stats.has_camera);
    CHECK(out.empty());
}

TEST_CASE("C# TransformData mirrors LocalTransform (40 bytes)") {
    // packages/phoenix/src/GameObject.cs reads and writes this struct in place.
    CHECK(sizeof(LocalTransform) == 40);
    CHECK(offsetof(LocalTransform, position) == 0);
    CHECK(offsetof(LocalTransform, rotation) == 12);
    CHECK(offsetof(LocalTransform, scale) == 28);
    CHECK(sizeof(Color) == 16);
}

TEST_CASE("scene renderer: a higher box offset away from the camera still draws on top") {
    // The tower top (A) and the sliding slab one level up (B), pushed back toward -X: B's
    // center is farther from the camera than A's, so depth sorting alone would draw A over it.
    Scene scene;
    REQUIRE(load_scene_from_string(scene, R"PSCN(
[phoenix_scene format=1]
[node name="World"]
[node name="Cam" parent="."]
[component type="Transform"]
position = Vector3(6, 6, 6)
euler = Vector3(-35.264, 45, 0)
[component type="Camera"]
projection = "Orthographic"
[node name="A" parent="."]
[component type="Transform"]
scale = Vector3(3, 0.4, 3)
[component type="MeshFilter"]
[component type="MeshRenderer"]
color = Color(1, 0, 0, 1)
[node name="B" parent="."]
[component type="Transform"]
position = Vector3(-2, 0.4, 0)
scale = Vector3(3, 0.4, 3)
[component type="MeshFilter"]
[component type="MeshRenderer"]
color = Color(0, 0, 1, 1)
)PSCN"));
    scene.SetParallel(false);
    scene.Update(1.0f / 60.0f);
    std::vector<PhxDrawRect> out;
    render_scene(scene, 800, 600, out);
    REQUIRE(out.size() == 6);
    // Red (A) faces first, blue (B) faces last.
    for (int i = 0; i < 3; ++i) {
        CHECK(((out[i].rgba >> 24) & 0xFF) > 0);     // red channel: A
        CHECK(((out[i + 3].rgba >> 8) & 0xFF) > 0);  // blue channel: B
    }
}
