// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
// The in-game editor, driven like a user would: through WidgetTester taps, drags and typing.
#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "phoenix/editor/scene_editor.h"
#include "phoenix/testing.h"

using namespace phoenix;
using namespace phoenix::scene;

namespace {

struct EditorFixture {
    Scene scene;
    std::shared_ptr<editor::SceneEditor> editor = std::make_shared<editor::SceneEditor>();
    WidgetTester tester{{1400, 900}};
    GameObject world, camera, cube;

    EditorFixture() {
        scene.SetParallel(false);
        world = scene.CreateGameObject("World");
        camera = scene.CreateGameObject("Main Camera");
        camera.transform().SetParent(world.transform(), false);
        camera.AddComponent<Camera>();
        cube = scene.CreateGameObject("Crate");
        cube.transform().SetParent(world.transform(), false);
        cube.AddComponent<MeshFilter>();
        cube.AddComponent<MeshRenderer>();
        editor->setScene(&scene);
        WidgetsBinding::instance().setOverlayRoot(editor);
        tester.pump();
    }

    /// Taps the first field to the right of an inspector row label (PropertyRow layout).
    void tapField(const std::string& row_label, float into = 20) {
        const Rect* r = tester.find(row_label);
        REQUIRE_MESSAGE(r, row_label);
        tester.tapAt({r->x + 120 + into, r->y + r->height * 0.5f});
        tester.pump();
    }

    void typeAndEnter(int erase, const std::string& text) {
        for (int i = 0; i < erase; ++i)
            tester.sendKey(Key::backspace);
        tester.enterText(text);
        tester.sendKey(Key::enter);
        tester.pump();
    }
};

}  // namespace

TEST_SUITE("Editor") {
    TEST_CASE("the Scene dock shows the hierarchy and selects into the inspector") {
        EditorFixture f;
        CHECK(f.tester.findText("World") == 1);
        CHECK(f.tester.findText("Main Camera") == 1);
        CHECK(f.tester.findText("Select a GameObject in the Scene dock.") == 1);

        REQUIRE(f.tester.tap("Crate"));
        f.tester.pump();
        CHECK(f.editor->selection() == f.cube);
        CHECK(f.tester.findText("Transform") == 1);
        CHECK(f.tester.findText("MeshFilter") == 1);
        CHECK(f.tester.findText("MeshRenderer") == 1);
        CHECK(f.tester.findText("Camera") == 0);
    }

    TEST_CASE("renaming, toggling active, and editing the transform by typing and dragging") {
        EditorFixture f;
        f.editor->select(f.cube);
        f.tester.pump();

        // Name: the header text field (its hint is "Name").
        REQUIRE(f.tester.tap("Name"));
        f.typeAndEnter(5, "Barrel");
        CHECK(f.cube.name() == String("Barrel"));
        CHECK(f.tester.findText("Barrel") >= 1);

        // Active: the checkbox left of the name.
        const Rect* name = f.tester.find("Name");
        REQUIRE(name);
        f.tester.tapAt({name->x - 12, name->y + name->height * 0.5f});
        f.tester.pump();
        CHECK_FALSE(f.cube.activeSelf());

        // Position x: click to type.
        f.tapField("Position");
        f.typeAndEnter(3, "2.5");
        CHECK(f.cube.transform().localPosition().x == doctest::Approx(2.5));

        // Position y: drag to scrub (0.02 per pixel).
        const Rect* pos = f.tester.find("Position");
        REQUIRE(pos);
        const float row_w = 360 - 8 - 120 - 18;  // inspector width minus paddings and label
        const Offset y_field{pos->x + 120 + row_w / 3 + 20, pos->y + pos->height * 0.5f};
        f.tester.drag(y_field, {y_field.x + 50, y_field.y});
        CHECK(f.cube.transform().localPosition().y == doctest::Approx(1.0).epsilon(0.05));
    }

    TEST_CASE("reflected fields: floats, colors and enums write through to the component") {
        EditorFixture f;
        f.editor->select(f.camera);
        f.tester.pump();
        f.tapField("orthographicSize");
        f.typeAndEnter(3, "12");
        CHECK(f.camera.GetComponent<Camera>()->orthographicSize == doctest::Approx(12));

        // Enum: projection Perspective -> Orthographic through the dropdown.
        REQUIRE(f.tester.tap("Perspective"));
        f.tester.pump();
        REQUIRE(f.tester.tap("Orthographic"));
        f.tester.pump();
        CHECK(f.camera.GetComponent<Camera>()->projection == ProjectionMode::Orthographic);

        f.editor->select(f.cube);
        f.tester.pump();
        // Color r (after the 22 px swatch and 4 px gap).
        f.tapField("color", 26 + 10);
        f.typeAndEnter(3, "0.25");
        CHECK(f.cube.GetComponent<MeshRenderer>()->color.r == doctest::Approx(0.25));
    }

    TEST_CASE("Add Component searches the registry; x removes a component") {
        EditorFixture f;
        f.editor->select(f.cube);
        f.tester.pump();
        REQUIRE(f.tester.tap("Add Component (type to search)"));
        f.tester.enterText("rigidbody");
        f.tester.pump();
        REQUIRE(f.tester.tap("Rigidbody  (Physics)"));
        f.tester.pump();
        CHECK(f.cube.GetComponent<Rigidbody>() != nullptr);
        CHECK(f.tester.findText("mass") == 1);

        REQUIRE(f.tester.tap("Remove Rigidbody"));
        f.tester.pump();
        CHECK(f.cube.GetComponent<Rigidbody>() == nullptr);
    }

    TEST_CASE("menus and shortcuts add, duplicate and delete GameObjects") {
        EditorFixture f;
        f.editor->select(f.world);
        f.tester.pump();
        f.tester.tap("Scene");
        f.tester.pump();
        f.tester.tap("Add Cube");
        f.tester.pump();
        GameObject added = f.editor->selection();
        REQUIRE(added.valid());
        CHECK(added.name() == String("Cube"));
        CHECK(added.transform().parent() == f.world.transform());
        CHECK(added.GetComponent<MeshRenderer>() != nullptr);

        f.editor->select(f.cube);
        f.tester.pump();
        const std::size_t before = f.scene.gameObjectCount();
        f.tester.sendKey({.key = Key::character, .ch = 'd', .ctrl = true});
        f.tester.pump();
        CHECK(f.scene.gameObjectCount() == before + 1);
        GameObject copy = f.editor->selection();
        CHECK(copy != f.cube);
        CHECK(copy.name() == String("Crate"));
        CHECK(copy.transform().parent() == f.world.transform());

        f.tester.sendKey(Key::del);
        f.tester.pump();
        CHECK(f.scene.gameObjectCount() == before);
        CHECK(f.editor->selection() == f.world);  // the parent is selected after a delete
    }

    TEST_CASE("pause and step gate the game loop") {
        EditorFixture f;
        CHECK(f.editor->shouldUpdateScene());
        REQUIRE(f.tester.tap("Pause"));
        f.tester.pump();
        CHECK_FALSE(f.editor->playing());
        CHECK(f.tester.findText("PAUSED") == 1);
        CHECK_FALSE(f.editor->shouldUpdateScene());
        f.tester.tap("Step");
        CHECK(f.editor->shouldUpdateScene());
        CHECK_FALSE(f.editor->shouldUpdateScene());
        f.tester.sendKey(Key::f5);
        CHECK(f.editor->playing());
    }

    TEST_CASE("typing in a field does not trigger editor shortcuts") {
        EditorFixture f;
        f.editor->select(f.cube);
        f.tester.pump();
        REQUIRE(f.tester.tap("Name"));
        const std::size_t before = f.scene.gameObjectCount();
        f.tester.sendKey(Key::del);  // deletes a character, not the GameObject
        f.tester.pump();
        CHECK(f.scene.gameObjectCount() == before);
        CHECK(f.cube.valid());
    }

    TEST_CASE("the Output dock shows captured log lines") {
        EditorFixture f;
        editor::LogBuffer::instance().clear();
        editor::LogBuffer::instance().add(editor::LogBuffer::Level::warning, "low on fuel");
        f.tester.pump();
        CHECK(f.tester.findText("low on fuel") == 1);
    }

    TEST_CASE("a destroyed selection is dropped, not dereferenced") {
        EditorFixture f;
        f.editor->select(f.cube);
        f.tester.pump();
        f.scene.DestroyImmediate(f.cube);
        f.tester.pump();
        CHECK_FALSE(f.editor->selection().valid());
        CHECK(f.tester.findText("Select a GameObject in the Scene dock.") == 1);
    }
}
