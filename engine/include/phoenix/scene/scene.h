// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix scene API: the Unity workflow on a data-oriented ECS.
//
//   using namespace phoenix::scene;
//
//   class Spinner : public MetaBehaviour {          // Phoenix's MonoBehaviour
//   public:
//       float speed = 90.0f;
//       void Update() override { transform().Rotate(Vector3(0, speed * Time::deltaTime, 0)); }
//   };
//
//   Scene scene;
//   GameObject cube = scene.CreateGameObject("Cube");
//   cube.AddComponent<MeshRenderer>();
//   cube.AddComponent<Spinner>().speed = 180;
//   scene.Update(1.0f / 60.0f);                      // runs the player loop once
//
// GameObject and Transform are lightweight handles (scene + entity). Component data lives in
// archetype chunks; GetComponent<T>() is a direct chunk lookup. Math and strings are the
// foundation types (Vector3, Quaternion, Transform3D, String) in a right-handed, Y-up,
// -Z-forward space; Euler angles use degrees and Unity's Z-X-Y application order.
#pragma once

#include <memory>
#include <type_traits>
#include <vector>

#include "phoenix/ecs/command_buffer.h"
#include "phoenix/ecs/scheduler.h"
#include "phoenix/ecs/world.h"
#include "phoenix/scene/components.gen.h"

namespace phoenix::scene {

class Scene;
class GameObject;
class MetaBehaviour;

/// Holds an entity's MetaBehaviour instances (owned). Added by AddComponent<T: MetaBehaviour>.
struct ScriptHost {
    std::vector<std::unique_ptr<MetaBehaviour>> behaviours;

    ScriptHost() = default;
    ScriptHost(ScriptHost&&) = default;
    ScriptHost& operator=(ScriptHost&&) = default;
    // vector<unique_ptr> claims to be copyable; make it explicit that this component is not.
    ScriptHost(const ScriptHost&) = delete;
    ScriptHost& operator=(const ScriptHost&) = delete;
};

/// Unity's Time, for the active Scene's current frame.
struct Time {
    static inline float deltaTime = 0.0f;
    static inline float unscaledDeltaTime = 0.0f;
    static inline float fixedDeltaTime = 0.02f;
    static inline float timeScale = 1.0f;
    static inline double time = 0.0;
    static inline std::uint64_t frameCount = 0;
};

/// Handle to an entity's transform (LocalTransform + Parent/Children).
class Transform {
  public:
    Transform() = default;
    Transform(Scene* scene, Entity e) : scene_(scene), entity_(e) {}

    bool valid() const;
    Entity entity() const { return entity_; }
    GameObject gameObject() const;

    // Local space
    Vector3 localPosition() const;
    void SetLocalPosition(const Vector3& p);
    Quaternion localRotation() const;
    void SetLocalRotation(const Quaternion& q);
    Vector3 localScale() const;
    void SetLocalScale(const Vector3& s);
    Vector3 localEulerAngles() const;
    void SetLocalEulerAngles(const Vector3& degrees);

    // World space (computed on demand, always current like Unity)
    Transform3D localToWorldMatrix() const;
    Vector3 position() const;
    void SetPosition(const Vector3& p);
    Quaternion rotation() const;
    void SetRotation(const Quaternion& q);
    Vector3 eulerAngles() const;
    void SetEulerAngles(const Vector3& degrees);
    Vector3 lossyScale() const;
    Vector3 forward() const;  ///< -Z in Phoenix's right-handed space
    Vector3 right() const;
    Vector3 up() const;

    void Translate(const Vector3& delta, bool world_space = false);
    void Rotate(const Vector3& euler_degrees, bool world_space = false);
    void RotateAround(const Vector3& point, const Vector3& axis, float degrees);
    void LookAt(const Vector3& target, const Vector3& world_up = Vector3(0, 1, 0));
    Vector3 TransformPoint(const Vector3& p) const;
    Vector3 InverseTransformPoint(const Vector3& p) const;
    Vector3 TransformDirection(const Vector3& d) const;

    // Hierarchy
    Transform parent() const;
    void SetParent(Transform parent, bool world_position_stays = true);
    int childCount() const;
    Transform GetChild(int index) const;
    Transform Find(const String& name) const;
    Transform root() const;
    bool IsChildOf(Transform parent) const;
    int GetSiblingIndex() const;

    bool operator==(const Transform& o) const { return scene_ == o.scene_ && entity_ == o.entity_; }
    explicit operator bool() const { return valid(); }

  private:
    LocalTransform* local() const;
    Scene* scene_ = nullptr;
    Entity entity_;
};

/// Handle to an entity, with Unity's GameObject API.
class GameObject {
  public:
    GameObject() = default;
    GameObject(Scene* scene, Entity e) : scene_(scene), entity_(e) {}

    bool valid() const;
    explicit operator bool() const { return valid(); }
    Entity entity() const { return entity_; }
    Scene* scene() const { return scene_; }

    String name() const;
    void SetName(const String& name);
    String tag() const;
    void SetTag(const String& tag);
    bool CompareTag(const String& tag) const { return this->tag() == tag; }
    int layer() const;
    void SetLayer(int layer);

    bool activeSelf() const;
    bool activeInHierarchy() const;
    void SetActive(bool active);

    Transform transform() const { return Transform(scene_, entity_); }

    /// Adds a data component (any ECS component) or a MetaBehaviour subclass.
    template <typename T, typename... Args>
    T& AddComponent(Args&&... args);
    /// Pointer to the component (or the first MetaBehaviour of type T), or nullptr.
    template <typename T>
    T* GetComponent() const;
    template <typename T>
    bool TryGetComponent(T*& out) const {
        out = GetComponent<T>();
        return out != nullptr;
    }
    template <typename T>
    T* GetComponentInChildren() const;
    template <typename T>
    T* GetComponentInParent() const;
    /// Removes a data component, or destroys the first MetaBehaviour of type T.
    template <typename T>
    void RemoveComponent();

    bool operator==(const GameObject& o) const {
        return scene_ == o.scene_ && entity_ == o.entity_;
    }

  private:
    Scene* scene_ = nullptr;
    Entity entity_;
};

/// Phoenix's MonoBehaviour: subclass it, override the lifecycle methods you need.
class MetaBehaviour {
  public:
    virtual ~MetaBehaviour() = default;

    bool enabled() const { return enabled_; }
    void SetEnabled(bool enabled);
    GameObject gameObject() const { return game_object_; }
    Transform transform() const { return game_object_.transform(); }
    template <typename T>
    T* GetComponent() const {
        return game_object_.GetComponent<T>();
    }
    bool isActiveAndEnabled() const { return enabled_ && game_object_.activeInHierarchy(); }

  protected:
    // Unity lifecycle, in call order.
    virtual void Awake() {}
    virtual void OnEnable() {}
    virtual void Start() {}
    virtual void FixedUpdate() {}
    virtual void Update() {}
    virtual void LateUpdate() {}
    virtual void OnDisable() {}
    virtual void OnDestroy() {}

  private:
    friend class Scene;
    GameObject game_object_;
    bool enabled_ = true;
    bool awoken_ = false;
    bool started_ = false;
    bool enable_called_ = false;
    bool destroyed_ = false;
};

/// A world + player loop. Owns the ECS World, the system Scheduler and MetaBehaviour lifecycle.
class Scene {
  public:
    Scene();
    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    ecs::World& world() { return world_; }
    ecs::Scheduler& scheduler() { return scheduler_; }

    GameObject CreateGameObject(const String& name = "GameObject");
    /// Unity's Object.Destroy: deferred to the end of the current frame (children included).
    void Destroy(GameObject go);
    /// Unity's Object.DestroyImmediate.
    void DestroyImmediate(GameObject go);
    /// Unity's Object.Instantiate: deep copy of the hierarchy (data components; behaviours are
    /// re-created only if they are copy-constructible).
    GameObject Instantiate(GameObject original);
    GameObject Find(const String& name);
    std::vector<GameObject> FindGameObjectsWithTag(const String& tag);
    std::size_t gameObjectCount() const { return world_.entity_count(); }

    /// Runs one frame of the player loop:
    /// Initialization (Awake/Start) -> FixedUpdate x N -> Update -> LateUpdate ->
    /// PostUpdate (transform hierarchy) -> PreRender -> end-of-frame destruction.
    void Update(float delta_time);
    /// When false, systems run on the calling thread (deterministic tests, tools).
    void SetParallel(bool parallel) { parallel_ = parallel; }

    // Used by GameObject/MetaBehaviour.
    void AttachBehaviour(GameObject go, std::unique_ptr<MetaBehaviour> behaviour);
    void DestroyBehaviour(MetaBehaviour* behaviour);
    void NotifyActiveChanged(GameObject go, bool active);

  private:
    void RunBehaviours(void (*call)(MetaBehaviour&));
    void StartPending();
    void FlushDestroyed();
    void DestroyRecursive(Entity e);
    void CollectHierarchy(Entity e, std::vector<Entity>& out);

    ecs::World world_;
    ecs::Scheduler scheduler_;
    std::vector<MetaBehaviour*> behaviours_;  // all live behaviours, in creation order
    std::vector<Entity> pending_destroy_;
    double fixed_accumulator_ = 0.0;
    bool parallel_ = true;
    bool in_update_ = false;
};

// --- GameObject templates ------------------------------------------------------------------

template <typename T, typename... Args>
T& GameObject::AddComponent(Args&&... args) {
    if constexpr (std::is_base_of_v<MetaBehaviour, T>) {
        auto behaviour = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *behaviour;
        scene_->AttachBehaviour(*this, std::move(behaviour));
        return ref;
    } else {
        return scene_->world().add<T>(entity_, std::forward<Args>(args)...);
    }
}

template <typename T>
T* GameObject::GetComponent() const {
    if (!valid())
        return nullptr;
    if constexpr (std::is_base_of_v<MetaBehaviour, T>) {
        ScriptHost* host = scene_->world().get<ScriptHost>(entity_);
        if (!host)
            return nullptr;
        for (auto& b : host->behaviours) {
            if (auto* t = dynamic_cast<T*>(b.get()))
                return t;
        }
        return nullptr;
    } else {
        return scene_->world().get<T>(entity_);
    }
}

template <typename T>
T* GameObject::GetComponentInChildren() const {
    if (T* self = GetComponent<T>())
        return self;
    const Transform t = transform();
    for (int i = 0; i < t.childCount(); ++i) {
        if (T* c = t.GetChild(i).gameObject().GetComponentInChildren<T>())
            return c;
    }
    return nullptr;
}

template <typename T>
T* GameObject::GetComponentInParent() const {
    for (Transform t = transform(); t.valid(); t = t.parent()) {
        if (T* c = t.gameObject().GetComponent<T>())
            return c;
    }
    return nullptr;
}

template <typename T>
void GameObject::RemoveComponent() {
    if constexpr (std::is_base_of_v<MetaBehaviour, T>) {
        if (T* b = GetComponent<T>())
            scene_->DestroyBehaviour(b);
    } else {
        scene_->world().remove<T>(entity_);
    }
}

}  // namespace phoenix::scene
