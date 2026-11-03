---
title: Coming from Unity
nav_order: 4
---

# Coming from Unity
{: .no_toc }

Phoenix keeps Unity's workflow (`GameObject`, `Transform`, `AddComponent`/`GetComponent`, the
script lifecycle) on top of a **data-oriented ECS**. Component data lives in archetype chunks, as
in Unity DOTS, so the same scene scales from prototype to hundreds of thousands of entities. You
don't need a separate DOTS workflow.

1. TOC
{:toc}

## The same code shape

```cpp
#include <phoenix/scene/scene.h>
using namespace phoenix::scene;

class Spinner : public MetaBehaviour {        // MonoBehaviour -> MetaBehaviour
public:
    float speed = 90.0f;

protected:
    void Start() override { transform().SetLocalScale(Vector3(1, 1, 1)); }
    void Update() override {
        transform().Rotate(Vector3(0, speed * Time::deltaTime, 0));
    }
};

Scene scene;
GameObject cube = scene.CreateGameObject("Cube");
cube.AddComponent<MeshRenderer>();
cube.AddComponent<BoxCollider>().size = Vector3(2, 2, 2);
cube.AddComponent<Spinner>().speed = 180;
GameObject copy = scene.Instantiate(cube);    // deep copy, children included
scene.Update(1.0f / 60.0f);                   // one frame of the player loop
```

## Name map

| Unity | Phoenix |
|---|---|
| `MonoBehaviour` | `MetaBehaviour` (C++ and C#) |
| `GameObject`, `Transform` | `GameObject`, `Transform` (lightweight handles to an entity) |
| `gameObject.AddComponent<T>()` / `GetComponent<T>()` / `TryGetComponent` | same names |
| `GetComponentInChildren<T>()` / `GetComponentInParent<T>()` | same |
| `Destroy(obj)` / `DestroyImmediate` / `Instantiate` | `scene.Destroy` / `DestroyImmediate` / `Instantiate` |
| `GameObject.Find`, `FindGameObjectsWithTag` | `scene.Find`, `scene.FindGameObjectsWithTag` |
| `transform.position = v` | `transform().SetPosition(v)` (C++); C# keeps properties |
| `transform.parent`, `SetParent`, `GetChild`, `Find("a/b")` | same |
| `Time.deltaTime`, `fixedDeltaTime`, `timeScale`, `frameCount` | `Time::deltaTime`, … |
| `Vector3`, `Quaternion`, `Color`, `Matrix4x4` | foundation `Vector3`, `Quaternion`, `Color`, `Transform3D` |
| Unity DOTS `IComponentData` + `ISystem` | plain struct + `scene.scheduler().add<Read<A>, Write<B>>(...)` |
| All built-in components | see [Unity components](components.md): 141 entries |

## Lifecycle

| Event | When |
|---|---|
| `Awake` | when the component is added to an active GameObject (or when its GameObject first becomes active) |
| `OnEnable` / `OnDisable` | when the behaviour or its GameObject is enabled or disabled |
| `Start` | before the behaviour's first `Update`, in the same frame |
| `FixedUpdate` | every `Time::fixedDeltaTime` of accumulated time (at most 8 steps per frame) |
| `Update`, then `LateUpdate` | once per frame |
| `OnDestroy` | `Destroy` is deferred to the end of the frame, like Unity |

The player loop: Initialization → FixedUpdate × N → Update → LateUpdate → PostUpdate (transform
hierarchy) → PreRender → end-of-frame destruction. Your ECS systems slot into the same stages.

## Data-oriented systems

```cpp
// Integrate every Rigidbody, chunk by chunk; independent systems run in parallel waves.
scene.scheduler().add<ecs::Read<Rigidbody>, ecs::Write<LocalTransform>>(
    "Integrate", ecs::Stage::FixedUpdate, [](ecs::SystemContext& ctx) {
        ctx.world.query<LocalTransform, const Rigidbody>().each(
            [&](LocalTransform& t, const Rigidbody& rb) { t.position += rb.linearVelocity * ctx.delta_time; });
    });
```

- `query<...>()` supports `with<>()`, `without<>()`, `changed<T>(tick)`, `each`, `for_each_chunk`,
  and `par_each`, which uses the WorkerThreadPool.
- Structural changes made inside systems go through `ctx.commands`, a command buffer that is
  played back at the end of the stage.
- Every component is reflected, so its fields convert to and from `Variant`/`Dictionary`. This
  drives serialization, prefabs and the inspector.

## Differences to know about

{: .important }
**Coordinates:** Phoenix uses its foundation's right-handed, Y-up space, where **forward is −Z**.
Unity is left-handed with forward +Z. `transform().forward()` returns −Z. The Unity scene importer
flips Z, so imported content looks the same.

- C++ uses `SetPosition(v)`-style setters where C# uses properties.
- Many built-in components are **data-only** today. You can add, edit, save and query them, and
  write your own systems for them. Their built-in behaviour (physics bodies, rendering, audio)
  arrives with the server that owns it. Each component's status is listed on the
  [components page](components.md).
- `Instantiate` copies data components and children. MetaBehaviour instances are not cloned yet.
