---
title: Godot core study
parent: Engine roadmap
nav_order: 1
---

# Studying Godot's `core/` and what Phoenix adopts
{: .no_toc }

Godot's `core/` folder is the engine's standard library: it compiles without any server, scene or
editor code. That separation is the main lesson. Phoenix gets the same layer as
`engine/src/core` (namespace `phoenix`). Godot is MIT-licensed, so adapting its code is allowed
with attribution, although most pieces are re-implemented to fit our job system and wasm64 target.

1. TOC
{:toc}

## Folder by folder

| Godot `core/` | What it is | Phoenix plan | Notes and tricks worth keeping |
|---|---|---|---|
| `templates/` | `Vector` (copy-on-write), `LocalVector`, `HashMap`, `HashSet`, `RBMap`, `List`, `SelfList`, `PagedAllocator`, `RID_Owner`, `CommandQueueMT`, `SafeRefCount` | `core/containers/`: keep the std containers for internal code, and add `RIDOwner`, `PagedAllocator`, `CommandQueue`, `SmallVector` | **RID_Owner**: chunked storage with a validator per slot, giving stable handles, O(1) lookup and use-after-free detection. **CommandQueueMT**: a lock-light ring buffer of type-erased commands, which is what lets each server run on its own thread. |
| `variant/` | `Variant` (a tagged union of ~40 types), `Array`, `Dictionary`, `Callable`, `Signal`, typed arrays | `core/variant/`: a smaller `Variant` (POD math types, `String`, `StringName`, `RID`, `ObjectID`, `Array`, `Dictionary`, packed arrays) | Variant is the glue for reflection, serialization, the inspector, undo/redo and scripting. Keep it out of hot paths: ECS components are plain structs, and Variant is only used at the edges. |
| `object/` | `Object`, `ClassDB` (reflection), `MethodBind`, `RefCounted`, `ObjectDB`, `MessageQueue`, `UndoRedo`, `WorkerThreadPool`, `ScriptLanguage` | `core/object/`: `ClassDB` reflection via a registration macro + C++20 templates, `RefCounted`, `ObjectID`, a deferred-call queue, `UndoRedo`, `WorkerThreadPool` = our `JobSystem` | Reflection metadata (property hints: range, enum, file, color) drives the editor inspector automatically. The same metadata registers ECS components. |
| `io/` | `FileAccess`, `DirAccess`, `ResourceLoader/Saver`, `ResourceFormatLoader`, `ResourceUID`, `ConfigFile`, `Image`, `JSON`, PCK packing, `Marshalls` | `core/io/`: VFS (`res://`, `user://`, mounted packs, OPFS on the web), `ResourceLoader` with threaded and async loads, `ResourceUID` (stable ids), `ConfigFile`, pack files | **Threaded loading with progress** (`load_threaded_request`) maps directly onto our IO thread. **UIDs** stop moves and renames from breaking references. Pack files with an index allow O(1) lookup and memory-mapped reads. |
| `math/` | vectors, `Basis`, `Transform3D`, `AABB`, `Plane`, `Projection`, `Geometry3D`, `DynamicBVH`, `Delaunay`, A*, convex hull | `core/math/`: SIMD-friendly POD types (shared with ECS and shaders through Slang structs), a dynamic BVH, frustum and HZB helpers | Godot's `DynamicBVH` (used for culling and physics broadphase) is a good template for scene culling before GPU-driven culling lands. |
| `string/` | `String` (UTF-32), `StringName` (interned), `NodePath`, translation | `core/string/`: UTF-8 `String`, interned `StringName` (pointer compare), `NodePath` for prefab overrides | Interned names make property paths and signals cheap to compare. |
| `os/` | `OS`, `Thread`, `Mutex`, `Semaphore`, `Memory`, keyboard codes | already provided by SDL3 (`SDL_Thread`, `SDL_Mutex`, …) | We don't duplicate SDL. A thin `OS` facade exposes paths, the clipboard and processes for the editor. |
| `config/` | `Engine`, `ProjectSettings` | `core/config/`: `ProjectSettings` backed by `phoenix.json`, which also loads `project.godot` | Settings carry override suffixes per platform/feature (`.mobile`, `.web`), which map onto our pipeline tiers. |
| `extension/` | GDExtension: a C ABI for native plugins | already in place as the **Script ABI** (C, versioned, blittable). Next we extend it to expose `ClassDB` so C#/C++ plugins can register classes and components. | A C ABI with an interface-version handshake is exactly what our C# NativeAOT bridge does. |
| `input/` | `Input`, `InputEvent`, `InputMap` (actions) | `core/input/`: actions + bindings (keyboard, gamepad, touch, pen) over SDL3 events | The action map is an asset, editable in the editor. |
| `debugger/` | remote debugger protocol, profiler hooks | `core/debugger/`: our DevTools BroadcastChannel/WebSocket protocol, generalized (scene tree, profiler, logs) | The editor attaches to the running game the same way DevTools already does. |
| `crypto/` | hashing, AES, TLS helpers | `core/crypto/`: SHA-256 for asset hashing and import caching; anything else only when needed | |
| `core_bind.*`, `register_core_types.*` | script-facing singletons; type registration at boot | `core/register_core_types.cpp`: one place that registers all core classes with `ClassDB` | Explicit registration (no static initializers) keeps wasm startup deterministic. |

## What sits above `core/` in Godot, and our equivalents

| Godot | Phoenix |
|---|---|
| `servers/` | `engine/src/servers/`: see [Server architecture](servers.md) |
| `scene/` (Node tree) | `engine/src/scene/`: ECS world + Node-style hierarchy view + prefabs ([ECS](ecs-components.md)) |
| `drivers/` (Vulkan, D3D12, Metal, GLES3) | `engine/src/rhi/`: SDL_GPU now, slang-rhi later ([Shaders and RHI](shaders-rhi.md)) |
| `platform/` | SDL3 plus a thin per-platform layer (web PWA, desktop, mobile, consoles) |
| `modules/` (optional features) | `engine/modules/`: CMake options (jolt, recast, assimp-import, godot-import, …) |
| `editor/` | `editor/`: a Phoenix app ([Editor](editor.md)) |

## Ordering inside Phase 1

1. `StringName`, `RID`/`RIDOwner`, `CommandQueue`: no dependencies, needed by servers.
2. `Variant` + `ClassDB`: needed by serialization and the editor.
3. VFS + `ResourceLoader`/`Saver` + UIDs + `.import` sidecars.
4. `ProjectSettings`, `InputMap`, `UndoRedo`.
5. Move the existing `JobSystem`, `LinearArena` and `VramAllocator` under the same conventions.
