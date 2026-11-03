// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/scene/scene.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace phoenix::scene;

namespace {

std::vector<std::string>* g_log = nullptr;

class Recorder : public MetaBehaviour {
  public:
    std::string id = "R";
    int updates = 0;
    int fixed_updates = 0;

  protected:
    void log(const char* what) {
        if (g_log)
            g_log->push_back(id + ":" + what);
    }
    void Awake() override { log("Awake"); }
    void OnEnable() override { log("OnEnable"); }
    void Start() override { log("Start"); }
    void FixedUpdate() override { ++fixed_updates; }
    void Update() override {
        ++updates;
        log("Update");
    }
    void LateUpdate() override { log("LateUpdate"); }
    void OnDisable() override { log("OnDisable"); }
    void OnDestroy() override { log("OnDestroy"); }
};

class Spinner : public MetaBehaviour {
  public:
    float speed = 90.0f;

  protected:
    void Update() override { transform().Rotate(Vector3(0, speed * Time::deltaTime, 0)); }
};

bool near(const Vector3& a, const Vector3& b, float eps = 1e-4f) {
    return (a - b).length() < eps;
}

}  // namespace

TEST_SUITE("Scene / GameObject / Transform") {
    TEST_CASE("CreateGameObject gives a named entity with a Transform") {
        Scene scene;
        GameObject go = scene.CreateGameObject("Player");
        CHECK(go.valid());
        CHECK(go.name() == "Player");
        CHECK(go.tag() == "Untagged");
        CHECK(go.GetComponent<LocalTransform>() != nullptr);
        CHECK(scene.Find("Player") == go);
        go.SetTag("Hero");
        CHECK(go.CompareTag("Hero"));
        CHECK(scene.FindGameObjectsWithTag("Hero").size() == 1);
    }

    TEST_CASE("world transforms compose through the hierarchy and stay current") {
        Scene scene;
        GameObject parent = scene.CreateGameObject("Parent");
        GameObject child = scene.CreateGameObject("Child");
        parent.transform().SetPosition(Vector3(10, 0, 0));
        parent.transform().SetLocalScale(Vector3(2, 2, 2));
        child.transform().SetParent(parent.transform(), /*world_position_stays=*/false);
        child.transform().SetLocalPosition(Vector3(1, 0, 0));
        CHECK(near(child.transform().position(), Vector3(12, 0, 0)));
        CHECK(parent.transform().childCount() == 1);
        CHECK(parent.transform().Find("Child") == child.transform());
        CHECK(child.transform().root() == parent.transform());

        // Re-parent keeping the world position (Unity's default).
        GameObject other = scene.CreateGameObject("Other");
        other.transform().SetPosition(Vector3(0, 5, 0));
        child.transform().SetParent(other.transform());
        CHECK(near(child.transform().position(), Vector3(12, 0, 0)));
        CHECK(parent.transform().childCount() == 0);

        // The PostUpdate TransformSystem writes the same matrices into LocalToWorld.
        scene.Update(0.016f);
        CHECK(near(child.GetComponent<LocalToWorld>()->value.origin, child.transform().position()));
    }

    TEST_CASE("rotation helpers follow Phoenix's -Z forward convention") {
        Scene scene;
        Transform t = scene.CreateGameObject().transform();
        CHECK(near(t.forward(), Vector3(0, 0, -1)));
        t.SetEulerAngles(Vector3(0, 90, 0));
        CHECK(near(t.forward(), Vector3(-1, 0, 0)));
        CHECK(near(t.eulerAngles(), Vector3(0, 90, 0), 1e-3f));
        t.SetPosition(Vector3());
        t.LookAt(Vector3(0, 0, 10));
        CHECK(near(t.forward(), Vector3(0, 0, 1)));
        t.Translate(Vector3(0, 0, -2));  // local forward
        CHECK(near(t.position(), Vector3(0, 0, 2)));
    }

    TEST_CASE("MetaBehaviour follows Unity's lifecycle order") {
        std::vector<std::string> log;
        g_log = &log;
        {
            Scene scene;
            GameObject go = scene.CreateGameObject("Hero");
            Recorder& r = go.AddComponent<Recorder>();
            CHECK(log == std::vector<std::string>{"R:Awake", "R:OnEnable"});
            CHECK(go.GetComponent<Recorder>() == &r);

            scene.Update(0.016f);
            CHECK(log == std::vector<std::string>{"R:Awake", "R:OnEnable", "R:Start", "R:Update",
                                                  "R:LateUpdate"});

            Time::fixedDeltaTime = 0.02f;
            scene.Update(0.05f);  // 0.016 + 0.05 = 0.066 -> 3 fixed steps in total
            CHECK(r.fixed_updates == 3);
            CHECK(r.updates == 2);

            log.clear();
            go.SetActive(false);
            CHECK(log == std::vector<std::string>{"R:OnDisable"});
            scene.Update(0.016f);
            CHECK(r.updates == 2);  // inactive objects don't update
            go.SetActive(true);
            CHECK(log.back() == "R:OnEnable");

            log.clear();
            scene.Destroy(
                go);  // deferred like Unity's Object.Destroy outside Update -> immediate flush
            CHECK(log == std::vector<std::string>{"R:OnDisable", "R:OnDestroy"});
            CHECK_FALSE(go.valid());
        }
        g_log = nullptr;
        Time::time = 0;
        Time::frameCount = 0;
    }

    TEST_CASE("MetaBehaviour drives the transform; Destroy inside Update is deferred") {
        Scene scene;
        GameObject go = scene.CreateGameObject("Spinner");
        go.AddComponent<Spinner>().speed = 90.0f;
        for (int i = 0; i < 10; ++i)
            scene.Update(0.1f);  // Unity: Start + Update in the first frame
        CHECK(go.transform().eulerAngles().y == doctest::Approx(90.0).epsilon(0.01));

        class SelfDestruct : public MetaBehaviour {
          protected:
            void Update() override {
                gameObject().scene()->Destroy(gameObject());
                destroyed_during_update = gameObject().valid();  // still alive until frame end
            }

          public:
            bool destroyed_during_update = false;
        };
        GameObject bomb = scene.CreateGameObject("Bomb");
        SelfDestruct& sd = bomb.AddComponent<SelfDestruct>();
        scene.Update(0.016f);  // Start
        scene.Update(0.016f);  // Update -> Destroy (deferred)
        (void)sd;
        CHECK_FALSE(bomb.valid());
    }

    TEST_CASE("Instantiate deep-copies data components and children") {
        Scene scene;
        GameObject prefab = scene.CreateGameObject("Crate");
        prefab.AddComponent<Rigidbody>().mass = 25.0f;
        prefab.AddComponent<BoxCollider>().size = Vector3(2, 2, 2);
        GameObject lid = scene.CreateGameObject("Lid");
        lid.transform().SetParent(prefab.transform(), false);
        lid.transform().SetLocalPosition(Vector3(0, 1, 0));

        GameObject copy = scene.Instantiate(prefab);
        REQUIRE(copy.GetComponent<Rigidbody>());
        CHECK(copy.GetComponent<Rigidbody>()->mass == doctest::Approx(25));
        CHECK(copy.GetComponent<BoxCollider>()->size == Vector3(2, 2, 2));
        REQUIRE(copy.transform().childCount() == 1);
        CHECK(copy.transform().GetChild(0).gameObject().name() == "Lid");
        CHECK(copy.transform().GetChild(0) != lid.transform());
        copy.GetComponent<Rigidbody>()->mass = 1;
        CHECK(prefab.GetComponent<Rigidbody>()->mass == doctest::Approx(25));  // independent

        scene.Destroy(prefab);  // destroys the child too
        CHECK_FALSE(lid.valid());
        CHECK(copy.valid());
    }

    TEST_CASE("data-oriented systems and MetaBehaviours share one world") {
        Scene scene;
        for (int i = 0; i < 1000; ++i) {
            GameObject go = scene.CreateGameObject("Body");
            go.AddComponent<Rigidbody>().linearVelocity = Vector3(0, -1, 0);
        }
        // A DOTS-style system: integrate velocity for every Rigidbody, in parallel waves.
        scene.scheduler().add<phoenix::ecs::Read<Rigidbody>, phoenix::ecs::Write<LocalTransform>>(
            "Integrate", phoenix::ecs::Stage::FixedUpdate, [](phoenix::ecs::SystemContext& ctx) {
                ctx.world.query<LocalTransform, const Rigidbody>().each(
                    [&](LocalTransform& t, const Rigidbody& rb) {
                        t.position += rb.linearVelocity * ctx.delta_time;
                    });
            });
        Time::fixedDeltaTime = 0.02f;
        scene.Update(0.1f);  // 5 fixed steps
        GameObject any = scene.Find("Body");
        CHECK(any.transform().position().y == doctest::Approx(-0.1).epsilon(0.001));
    }
}
