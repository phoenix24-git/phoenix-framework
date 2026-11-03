// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include <doctest/doctest.h>

#include <set>
#include <string>

#include "phoenix/scene/scene.h"

using namespace phoenix;
using namespace phoenix::scene;

TEST_SUITE("Unity component catalogue") {
    TEST_CASE("every built-in component is registered with Unity metadata") {
        Scene scene;  // registers the catalogue
        auto& reg = ecs::ComponentRegistry::instance();
        int builtin = 0;
        std::set<std::string> names;
        for (std::uint32_t id = 0; id < reg.count(); ++id) {
            const ecs::ComponentInfo& info = reg.info(id);
            if (info.category.is_empty() || info.unity_name.is_empty())
                continue;
            names.insert(info.name.utf8().get_data());
            ++builtin;
        }
        CHECK(builtin >= kBuiltinComponentCount);
        for (const char* unity :
             {"Rigidbody", "BoxCollider", "Camera", "Light", "MeshRenderer", "AudioSource",
              "Animator", "NavMeshAgent", "Canvas", "Button", "ParticleSystem",
              "CharacterController", "Rigidbody2D", "Tilemap", "Terrain", "VideoPlayer",
              "PlayableDirector", "LocalTransform", "ScriptHost"}) {
            CHECK_MESSAGE(names.count(unity) == 1, unity);
        }
    }

    TEST_CASE("reflection round-trips every component through a foundation Dictionary") {
        Scene scene;
        auto& reg = ecs::ComponentRegistry::instance();
        int checked = 0;
        for (std::uint32_t id = 0; id < reg.count(); ++id) {
            const ecs::ComponentInfo& info = reg.info(id);
            if (info.size == 0 || info.fields.is_empty())
                continue;
            std::vector<std::byte> a(info.size + info.align), b(info.size + info.align);
            void* pa = reinterpret_cast<void*>(
                (reinterpret_cast<std::uintptr_t>(a.data()) + info.align - 1) &
                ~std::uintptr_t(info.align - 1));
            void* pb = reinterpret_cast<void*>(
                (reinterpret_cast<std::uintptr_t>(b.data()) + info.align - 1) &
                ~std::uintptr_t(info.align - 1));
            info.construct(pa);
            info.construct(pb);
            const Dictionary d = info.to_dictionary(pa);
            CHECK_MESSAGE(d.size() == int(info.fields.size()), info.name.utf8().get_data());
            CHECK_MESSAGE(info.from_dictionary(pb, d) == int(info.fields.size()),
                          info.name.utf8().get_data());
            CHECK_MESSAGE(info.to_dictionary(pb) == d, info.name.utf8().get_data());
            info.destroy(pa);
            info.destroy(pb);
            ++checked;
        }
        CHECK(checked > 120);
    }

    TEST_CASE("fields read and write through Variant with Unity defaults") {
        Scene scene;
        GameObject go = scene.CreateGameObject("Cam");
        Camera& cam = go.AddComponent<Camera>();
        CHECK(cam.fieldOfView == doctest::Approx(60));
        const ecs::ComponentInfo* info = ecs::ComponentRegistry::instance().find("Camera");
        REQUIRE(info);
        const ecs::FieldInfo* fov = info->field("fieldOfView");
        REQUIRE(fov);
        CHECK(info->set(&cam, *fov, 75.0));
        CHECK(cam.fieldOfView == doctest::Approx(75));
        CHECK(double(info->get(&cam, *fov)) == doctest::Approx(75));
        const ecs::FieldInfo* proj = info->field("projection");
        REQUIRE(proj);
        REQUIRE(proj->enum_names);
        CHECK(String(proj->enum_names[1]) == "Orthographic");
        CHECK_FALSE(info->set(&cam, *fov, Vector3()));  // type mismatch is rejected
    }
}
