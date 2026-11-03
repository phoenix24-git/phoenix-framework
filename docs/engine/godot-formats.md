---
title: Godot formats
parent: Engine roadmap
nav_order: 6
---

# Godot file formats and Phoenix's own formats
{: .no_toc }

Phoenix reads Godot 4 projects. That gives users an import path from the Godot ecosystem, and gives
us a well-documented, diff-friendly text format design to learn from.

1. TOC
{:toc}

## Formats we read

| Godot file | What it is | Phoenix support | Phase |
|---|---|---|---|
| `project.godot` | `ConfigFile` (INI-like with Variant values): settings, autoloads, input map, layers | read → `phoenix.json` + `ProjectSettings` (unknown keys are kept) | 2 |
| `.tscn` | **text scene**: `[gd_scene]` header, `[ext_resource]`, `[sub_resource]`, `[node name= type= parent=]`, `[connection]`, `[editable]` | full parser → **prefab** (entities + components + overrides); signals → event bindings | 2 |
| `.tres` | **text resource** (materials, environments, curves, themes, …) | parser → Phoenix resources via a type-mapping table | 2 |
| `.scn` / `.res` | **binary** scene/resource (`RSRC` header, typed property stream) | binary reader (versioned) | 3 |
| `.import` | sidecar importer settings (`[remap]`, `[deps]`, `[params]`), plus `uid` | read and honoured: same importer options, same UIDs | 2 |
| `.uid` (Godot 4.4+) | per-file stable UID for scripts/shaders | mapped into `ResourceUID` | 2 |
| `.godot/imported/*` | Godot's import cache | **ignored**: we re-import from source for reproducibility | — |
| `.gdshader` / `.gdshaderinc` | Godot shading language | translator → Slang for `spatial`, `canvas_item`, `particles` and `sky` (common subset) | 3 |
| `.gd` (GDScript) | scripting | **not executed**. The importer keeps references and the exported variables (`@export`) as component data, and reports scripts that need porting to C#/C++ | 3 |
| `.cs` (Godot C#) | Godot .NET scripts | a migration guide plus an analyzer that maps `Node` APIs to Phoenix equivalents | 4 |
| `.translation`, `.po`/`.csv` | localization | read | 3 |
| `export_presets.cfg` | export settings | mapped to `phoenix build` targets | 3 |

### Node-type mapping (excerpt)

The importer table is generated from the [component mapping](ecs-components.md): `Node3D` →
`LocalTransform`, `MeshInstance3D` → `MeshRenderer`, `Camera3D` → `Camera`,
`DirectionalLight3D` → `Light{Directional}`, `RigidBody3D` → `RigidBody` (+ children
`CollisionShape3D` → colliders), `CharacterBody3D` → `CharacterController`, `AnimationPlayer` →
`AnimationPlayer`, `GPUParticles3D` → `ParticleEmitter`, `Control` subclasses → UI components, and
`WorldEnvironment` → global `Volume`. Unknown node types become an entity with an
`UnsupportedNode {type, properties}` component, so nothing is silently lost.

### Parser notes

- Values are Godot `Variant` literals: `Vector3(0, 1, 0)`, `Transform3D(...)`, `Color(...)`,
  `ExtResource("1_abc")`, `SubResource("Mat_x")`, `PackedFloat32Array(...)`, `NodePath("..")`, and
  typed `Array[...]`. One tokenizer covers `.tscn`, `.tres`, `project.godot` and `.import`.
- Godot 4 vs 3: `format=3` is Godot 4. Godot 3 scenes (`format=2`) are a stretch goal.
- Tests: a corpus of Godot demo-project scenes (MIT-licensed `godot-demo-projects`) imported in CI
  and compared against expected entity dumps.

## Phoenix's own formats

| Extension | Content | Notes |
|---|---|---|
| `.phscn` | scene / prefab, **text** | git-friendly: stable GUID per entity, components sorted, one property per line |
| `.phprefab` | prefab, text | same grammar as `.phscn`, plus `base` (variant) and `overrides` |
| `.phres` | resource, text | materials, render pipeline assets, input maps, … |
| `.phbin` | binary scene/resource | built from text for shipping; memory-mappable chunk images for instant instantiation |
| `*.import` | importer settings next to each source asset | Godot-compatible keys plus our own, so one file serves both tools |
| `.phpack` | packed game data | index + compressed blobs (zstd), mounted into the VFS; one per platform/quality tier |

Asset sources the importers understand (via [libraries](shaders-rhi.md#libraries)): glTF/GLB,
FBX, OBJ, DAE and anything Assimp reads; PNG/JPEG/EXR/HDR/KTX2/DDS; WAV/OGG/MP3/FLAC;
TTF/OTF/WOFF2; `.tscn`/`.tres`.
