---
title: ECS and Unity components
parent: Engine roadmap
nav_order: 5
---

# Data-driven ECS with a Unity-style API
{: .no_toc }

Unity developers should feel at home: `GameObject`, `Transform`, `GetComponent<T>()`,
`MonoBehaviour.Update()`, prefabs, and the Inspector. Underneath, everything is an **archetype
ECS**, so the same scene scales to hundreds of thousands of entities, much like Unity DOTS but
without a separate workflow.

1. TOC
{:toc}

## ECS core design

| Concept | Design |
|---|---|
| Entity | 64-bit `{index, generation}`; stale handles are detected |
| Component | plain C++ struct (or C# `struct` through the Script ABI), registered with `ClassDB` for reflection, serialization and the inspector |
| Storage | **archetypes**: all entities with the same component set share 16 KiB chunks in SoA layout. Iteration is linear memory, which is cache- and SIMD-friendly |
| Tags & shared components | zero-size tags; shared components (e.g. `RenderMesh`) are stored once per chunk |
| Queries | `world.query<const LocalToWorld, Velocity&>().without<Static>()`, cached per archetype |
| Systems | functions with declared read/write sets. The scheduler builds a dependency graph and runs systems in parallel on the job system (6 workers on the console profile) |
| Change detection | per-chunk, per-component version numbers (`changed<T>()` filters), so render extract only uploads dirty transforms |
| Structural changes | `CommandBuffer` (deferred create/destroy/add/remove, played back at sync points) |
| Relationships | `Parent`/`Children` components and a hierarchy system that computes `LocalToWorld` breadth-first in parallel |
| Determinism | fixed-step simulation group, stable iteration order, and an optional deterministic math mode for lockstep netcode |
| Serialization | reflection-driven, text (`.phscn`) and binary (`.phbin`), versioned with migration hooks |

Reference designs we study: Unity DOTS (chunks, change versions), Bevy (scheduler, query ergonomics,
plugins), flecs (relationships, queries). Godot's scene tree is mirrored as a hierarchy *view* over
the ECS, so Godot imports and a node-style API work too.

## Three API levels (pick per task)

```csharp
// 1. Unity-style, for fast prototyping. Each MonoBehaviour is a managed component on an entity.
public sealed class Spinner : MonoBehaviour
{
    public float speed = 90f;                          // shows in the Inspector
    void Update() => transform.Rotate(0, speed * Time.deltaTime, 0);
}

// 2. Data-oriented, for scale: IComponentData + ISystem, Burst-like through NativeAOT.
public struct Spin : IComponentData { public float Speed; }

public partial struct SpinSystem : ISystem
{
    public void OnUpdate(ref SystemState state)
    {
        foreach (var (rot, spin) in Query<RefRW<LocalTransform>, RefRO<Spin>>())
            rot.ValueRW = rot.ValueRO.RotateY(spin.ValueRO.Speed * state.DeltaTime);
    }
}
```

```cpp
// 3. Native C++ systems: engine features and hot paths.
world.system("Spin").each([](LocalTransform& t, const Spin& s, const Time& time) {
    t.rotation = t.rotation * quat_from_axis_angle(Vec3::up(), s.speed * time.delta);
});
```

All three operate on the same entities. `GameObject` is a handle to an entity, `transform` is a view
of its `LocalTransform`, and `GetComponent<T>()` is a chunk lookup. A managed `MonoBehaviour`
costs the same as it does in Unity, so use it where convenience matters and `ISystem` where scale
matters. "Bake" tooling converts authoring components into runtime data, like Unity's Bakers.

## Nested prefabs

| Feature | Behaviour |
|---|---|
| Prefab asset | an entity hierarchy saved as a resource (`.phprefab`, text) |
| Nesting | prefabs contain prefab instances, to any depth |
| Overrides | stored as `(entity path, component, property path) → value`, plus added/removed components and children. Paths use stable per-entity GUIDs, so renames don't break them |
| Variants | a prefab that inherits another and stores only overrides (Unity's Prefab Variant) |
| Apply / revert | per property, per component or per entity, to any level of the nesting chain |
| Runtime instantiate | `Instantiate(prefab)` clones a pre-baked chunk image (memcpy-fast) |
| Godot interop | `.tscn` scenes import as prefabs; Godot's "editable children" maps to overrides ([formats](godot-formats.md)) |

## Unity component → Phoenix mapping

**Phase** is when the component reaches parity (see the [phases](index.md#phases)). **Godot**
lists the nearest node, used by the importer.

### Core & scene

| Unity | Phoenix component(s) | Implemented by | Godot | Phase |
|---|---|---|---|---|
| GameObject | `Entity` + `Name`, `Active`, `Layer`, `Tags` | ECS | `Node` | 1 |
| Transform | `LocalTransform`, `Parent`, `Children`, `LocalToWorld` | hierarchy system | `Node3D` | 1 |
| RectTransform | `RectTransform` (anchors, pivot, offsets) | UI layout system | `Control` | 2 |
| MonoBehaviour | `ManagedBehaviour` (C# instance + lifecycle) | script host | script on `Node` | 2 |
| Prefab instance (not a component in Unity) | `PrefabInstance` (source + overrides) | scene system | instanced `PackedScene` | 2 |

### Rendering

| Unity | Phoenix | Implemented by | Godot | Phase |
|---|---|---|---|---|
| MeshFilter + MeshRenderer | `MeshRenderer {mesh, materials[], shadows, layers}` | RenderingServer instance | `MeshInstance3D` | 2 |
| SkinnedMeshRenderer | `SkinnedMeshRenderer {mesh, skeleton, bones}` + `BlendShapeWeights` | GPU skinning | `MeshInstance3D` + `Skeleton3D` | 3 |
| Camera | `Camera {projection, fov, near/far, viewport, target, pipeline override}` | RenderingServer viewport | `Camera3D` | 2 |
| Light (directional/point/spot/area) | `Light {type, color, intensity (physical units), range, angles, shadows, IES, cookie}` | RenderingServer | `DirectionalLight3D` / `OmniLight3D` / `SpotLight3D` | 2 (area: 3) |
| Light Probe Group | `LightProbeVolume` (baked SH) / `GIProbeVolume` (DDGI) | GI system | `LightmapProbe`, `VoxelGI` | 2–3 |
| Light Probe Proxy Volume | `ProbeProxyVolume` | GI system | — | 3 |
| Reflection Probe | `ReflectionProbe {mode, box projection, blend distance}` | RenderingServer | `ReflectionProbe` | 2 |
| Planar Reflection Probe (HDRP) | `PlanarReflection` | Ultra pipeline | — | 4 |
| LOD Group | `LODGroup {levels[], fade mode}`; automatic with Virtual Geometry | culling system | `visibility_range_*` | 2 |
| Occlusion Area / Portal | `OcclusionVolume`, `OcclusionPortal` (needed only for Mobile; GPU HZB elsewhere) | culling | `OccluderInstance3D` | 3 |
| Volume (URP/HDRP) | `PostProcessVolume {profile, priority, blend, global/local}` | pipeline settings blend | `WorldEnvironment` | 2 |
| Skybox | `Environment.sky` (in `Volume` / scene settings) | RenderingServer | `WorldEnvironment` + `Sky` | 2 |
| Decal Projector | `Decal {material, size, fade}` | clustered decals | `Decal` | 2 |
| Local Volumetric Fog (HDRP) | `FogVolume` | volumetrics | `FogVolume` | 3 |
| Lens Flare (SRP) | `LensFlare` | post | — | 3 |
| Line Renderer | `LineRenderer {points, width curve, gradient}` | dynamic mesh | `MeshInstance3D` (ImmediateMesh) | 2 |
| Trail Renderer | `TrailRenderer` | dynamic mesh | `GPUParticles3D` trails | 3 |
| Billboard Renderer | `Billboard` | instancing | `Sprite3D` billboard | 3 |
| Particle System | `ParticleEmitter` (modules: shape, velocity, color/size over life, noise, collision, sub-emitters) | GPU particles | `GPUParticles3D` / `CPUParticles3D` | 3 |
| Particle System Force Field | `ParticleForceField` | GPU particles | `GPUParticlesAttractor3D` | 3 |
| Visual Effect (VFX Graph) | `VisualEffect {graph asset}` | GPU particles (graph → Slang compute) | — | 4 |
| Sprite Renderer | `SpriteRenderer {sprite, color, flip, sorting}` | 2D batcher | `Sprite2D` | 2 |
| Sprite Mask, Sorting Group | `SpriteMask`, `SortingGroup` | 2D batcher | `CanvasGroup`, `z_index` | 2 |
| TextMeshPro / TextMesh | `Text3D {font (SDF), text, style}` | TextServer | `Label3D` | 2 |
| Projector (built-in) | covered by `Decal` | — | `Decal` | 2 |
| Water Surface (HDRP) | `WaterSurface` | Ultra pipeline | — | 4 |
| Custom Pass Volume (HDRP) | `CustomPassVolume` (inject frame-graph passes) | pipeline | — | 3 |

### Physics 3D (Jolt)

| Unity | Phoenix | Godot | Phase |
|---|---|---|---|
| Rigidbody | `RigidBody {mass, drag, interpolation, constraints, kinematic}` | `RigidBody3D` | 2 |
| Box / Sphere / Capsule Collider | `BoxCollider`, `SphereCollider`, `CapsuleCollider` | `CollisionShape3D` | 2 |
| Mesh Collider (convex / triangle) | `MeshCollider {convex}` | `CollisionShape3D` (Concave/ConvexPolygon) | 2 |
| Terrain Collider | `HeightFieldCollider` | `HeightMapShape3D` | 3 |
| Wheel Collider | `VehicleWheel` (Jolt vehicle constraint) | `VehicleWheel3D` | 3 |
| Character Controller | `CharacterController` (Jolt `CharacterVirtual`) | `CharacterBody3D` | 2 |
| Fixed / Hinge / Spring / Character / Configurable Joint | `FixedJoint`, `HingeJoint`, `SpringJoint`, `ConeJoint`, `SixDOFJoint` | `*Joint3D` | 3 |
| Articulation Body | `Articulation` (reduced-coordinate chains) | — | 4 |
| Cloth | `ClothSimulation` (Jolt soft bodies) | `SoftBody3D` | 4 |
| Constant Force | `ConstantForce` | — (script) | 2 |
| Trigger colliders, layers, Physics Material | `Collider.isTrigger`, `PhysicsLayer` matrix, `PhysicsMaterial` | `Area3D`, layers/masks | 2 |

### Physics 2D

| Unity | Phoenix | Godot | Phase |
|---|---|---|---|
| Rigidbody2D | `RigidBody2D` | `RigidBody2D` | 3 |
| Box / Circle / Capsule / Polygon / Edge Collider 2D | `*Collider2D` | `CollisionShape2D` / `CollisionPolygon2D` | 3 |
| Composite / Tilemap Collider 2D | `CompositeCollider2D`, `TilemapCollider2D` | `TileMapLayer` physics | 3 |
| 2D joints (Distance, Fixed, Friction, Hinge, Relative, Slider, Spring, Target, Wheel) | `*Joint2D` | `PinJoint2D`, `GrooveJoint2D`, `DampedSpringJoint2D` | 3 |
| Effectors (Area, Buoyancy, Point, Platform, Surface) | `*Effector2D` | `Area2D` gravity/damping overrides | 4 |

The 2D backend is Jolt constrained to a plane (as in the MVP) at first. A dedicated 2D solver
(Box2D v3, MIT) is evaluated in Phase 3.

### Animation

| Unity | Phoenix | Godot | Phase |
|---|---|---|---|
| Animator (+ Animator Controller) | `Animator {graph asset, parameters}`: state machines, blend trees, layers, avatar masks, root motion | `AnimationTree` | 2 (basic) → 3 |
| Animation (legacy) | `AnimationPlayer` | `AnimationPlayer` | 2 |
| Playable Director (Timeline) | `Timeline {asset}`: tracks for animation, audio, activation, events, camera | `AnimationPlayer` tracks | 3 |
| Aim / LookAt / Parent / Position / Rotation / Scale Constraint | `*Constraint` | `LookAtModifier3D`, `BoneAttachment3D` | 3 |
| Animation Rigging (Rig Builder, IK) | `IKChain` (two-bone, FABRIK, CCD), `Rig` | `SkeletonIK3D` / `SkeletonModifier3D` | 3 |

### Audio

| Unity | Phoenix | Godot | Phase |
|---|---|---|---|
| Audio Source | `AudioSource {clip, volume, pitch, loop, spatial blend, rolloff}` | `AudioStreamPlayer3D` | 2 |
| Audio Listener | `AudioListener` | `AudioListener3D` | 2 |
| Audio Reverb Zone | `ReverbZone` | `Area3D` reverb bus | 3 |
| Audio filters (Low/High Pass, Echo, Distortion, Chorus, Reverb) | effect chain on `AudioBus` / per-source effects | `AudioEffect*` on buses | 3 |
| Audio Mixer (asset) | `AudioBusLayout` | `AudioBusLayout` | 2 |

### Navigation (Recast/Detour)

| Unity | Phoenix | Godot | Phase |
|---|---|---|---|
| NavMesh Surface | `NavMeshSurface {agent type, bake settings}` (runtime and editor bake) | `NavigationRegion3D` | 3 |
| NavMesh Agent | `NavAgent {speed, radius, avoidance}` | `NavigationAgent3D` | 3 |
| NavMesh Obstacle | `NavObstacle {carve}` | `NavigationObstacle3D` | 3 |
| Off Mesh Link / NavMesh Link | `NavLink` | `NavigationLink3D` | 3 |

### UI (retained, ECS-backed; uGUI concepts + Flutter-style widgets)

| Unity | Phoenix | Godot | Phase |
|---|---|---|---|
| Canvas, Canvas Scaler, Canvas Group | `Canvas {render mode, sort}`, `CanvasScaler`, `CanvasGroup` | `CanvasLayer`, content scale, `CanvasGroup` | 2 |
| Graphic Raycaster, EventSystem, Input Module | `UIRaycaster`, `UIEventSystem` (over InputServer) | built into `Control` | 2 |
| Image, Raw Image | `UIImage {sprite, nine-slice, fill}`, `UIRawImage` | `TextureRect`, `NinePatchRect` | 2 |
| Text / TMP UGUI | `UIText` (SDF, rich text, shaping via HarfBuzz) | `Label`, `RichTextLabel` | 2 |
| Button, Toggle, Toggle Group, Slider, Scrollbar, Dropdown, Input Field | `UIButton`, `UIToggle`, `UIToggleGroup`, `UISlider`, `UIScrollbar`, `UIDropdown`, `UITextField` | `Button`, `CheckBox`, `HSlider`, `ScrollBar`, `OptionButton`, `LineEdit` | 2 |
| Scroll Rect, Mask, Rect Mask 2D | `UIScrollView`, `UIMask`, `UIRectMask` | `ScrollContainer`, `clip_contents` | 2 |
| Horizontal / Vertical / Grid Layout Group, Content Size Fitter, Aspect Ratio Fitter, Layout Element | `UIHorizontalLayout`, `UIVerticalLayout`, `UIGridLayout`, `UIContentSizeFitter`, `UIAspectRatioFitter`, `UILayoutElement` | `HBoxContainer`, `VBoxContainer`, `GridContainer`, `AspectRatioContainer` | 2 |
| UI Toolkit (UIDocument) | the existing **Flutter-style widget layer** (`Scaffold`, `Column`, …), hosted by `UIDocument` | `Control` scenes | 2 |

### 2D, terrain, world, misc

| Unity | Phoenix | Godot | Phase |
|---|---|---|---|
| Grid, Tilemap, Tilemap Renderer | `Grid`, `Tilemap`, `TilemapRenderer` | `TileMapLayer` | 3 |
| Sprite Shape, Splines (Spline Container) | `Spline`, `SplineExtrude`, `SpriteShape` | `Path3D`, `Curve3D` | 3 |
| Terrain, Wind Zone, Tree | `Terrain` (clipmap + virtual texturing), `WindZone`, `FoliageLayer` | (plugins) | 3 |
| Video Player | `VideoPlayer` (WebCodecs on the web, platform decoders natively) | `VideoStreamPlayer` | 3 |
| Player Input (Input System) | `PlayerInput {action map asset}` | `InputMap` actions | 2 |
| Cinemachine (Brain, Virtual Camera) | `CameraBrain`, `VirtualCamera {follow, aim, noise}` | `Camera3D` + plugins | 3 |
| Tracked Pose Driver (XR) | `TrackedPose` (OpenXR / WebXR) | `XRNode3D` | 4 |
| Netcode (NetworkObject, NetworkTransform) | `NetworkIdentity`, `NetworkTransform`, replication system | `MultiplayerSynchronizer`, `MultiplayerSpawner` | 4 |

## Performance targets (CI benchmarks)

- 1M entities × `LocalTransform` update in < 2 ms on the 6-worker profile (native), and < 6 ms on
  wasm64 in Chrome.
- `Instantiate` of a 100-entity prefab: < 20 µs.
- Render extract of 100k dirty transforms: < 1 ms (bulk `instances_set_transforms`).
