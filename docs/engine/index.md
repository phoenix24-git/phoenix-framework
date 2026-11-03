---
title: Engine roadmap
nav_order: 12
has_children: true
permalink: /engine/
---

# Engine roadmap: from MVP to a production engine
{: .no_toc }

This section turns Phoenix from the current MVP (2D widgets, Jolt, SDL_GPU, wasm64) into a full
3D engine. Each part borrows from the engine that does it best:

| Borrowed from | What we take | Where it's specified |
|---|---|---|
| **Godot** | `core/` foundations (Variant, RID, ClassDB, resources), the **server architecture**, the editor built with the engine itself, and the scene/resource file formats | [Godot core study](godot-core.md), [Server architecture](servers.md), [Godot formats](godot-formats.md) |
| **Unity** | GameObject/Component authoring, **nested prefabs and variants**, URP/HDRP on a **scriptable render pipeline**, a C# API that feels like home, and data-oriented ECS | [ECS and Unity components](ecs-components.md), [Render pipelines](render-pipelines.md) |
| **Unreal** | high-end rendering *techniques*: virtualized cluster geometry, virtual shadow maps, temporal upscaling, screen-space and ray-traced reflections, many-light sampling, dynamic GI | [Rendering features](rendering.md) |
| **Slang** | one shader language for every backend, plus **slang-rhi** as the second-generation RHI | [Shaders and RHI](shaders-rhi.md) |
| **The web** | the editor as an installable **PWA**, the same binary as the desktop editor | [Editor](editor.md) |

1. TOC
{:toc}

## Principles

1. **Servers own the hardware; scenes own the data.** Gameplay, ECS, the editor and importers talk
   to servers through handles (`RID`s) and commands, never to GPU or physics APIs directly. That is
   how we swap SDL_GPU for slang-rhi (mesh shaders, bindless, ray tracing) without rewriting
   anything above the server line. Godot does the same with `RenderingServer`.
2. **Data-oriented first, familiar API second.** Storage is archetype ECS (SoA chunks,
   job-scheduled systems). The Unity-style `GameObject`/`Component`/`MonoBehaviour` API is a thin
   façade over it, so Unity developers feel at home without paying an object-per-component cost.
3. **One feature, many tiers.** Every rendering feature declares which pipeline tier
   (Mobile / Desktop / Ultra) and which backend capability it needs. Web and mobile get the cheapest
   correct version, and consoles and high-end PCs get the full one. A frame graph wires features
   together per tier.
4. **Everything is a resource with an importer.** Source files (glTF, FBX, PNG, `.tscn`, …) are
   imported into engine-native resources, and a sidecar `.import` file records the settings, as in
   Godot. Builds ship only imported data.
5. **The editor is a Phoenix app.** It uses the engine's own widgets, servers and ECS. If the
   editor can't be built with Phoenix, Phoenix isn't good enough yet.

## Clean-room policy (read before contributing)

We borrow **ideas, published techniques and file formats**. We do not copy code we are not licensed
to copy.

| Source | Allowed | Not allowed |
|---|---|---|
| Godot (MIT) | studying and adapting code, with the MIT notice kept in `THIRD_PARTY_NOTICES` | removing attribution |
| Unity | public API *names and concepts* (Transform, Rigidbody, prefabs, URP/HDRP ideas); behaviour documented in public manuals | copying Unity source, documentation text, or assets; using Unity trademarks for our products |
| Unreal Engine | techniques from **public papers and talks** (SIGGRAPH, GDC, HPG) | reading, copying or porting Unreal source (its EULA forbids use in other engines); using the names Nanite, Lumen, MegaLights, Niagara, Chaos or MetaHuman for our features |
| Libraries | Assimp (BSD-3), meshoptimizer (MIT), Slang/slang-rhi (Apache-2.0 + LLVM exception), Jolt (MIT), Recast (zlib), HarfBuzz (MIT), FreeType (FTL), KTX/Basis (Apache-2.0), ufbx (MIT/Unlicense) | GPL libraries in the runtime |

Our feature names are descriptive: **Virtual Geometry**, **Virtual Shadow Maps**, **Dynamic GI**,
**Stochastic Many-Lights**, **Temporal Super Resolution**.

## Phases

The estimates assume a core team of 4–6 engineers. Halve the scope or double the time for one or
two people. A production-quality, AAA-level engine is a multi-year effort for any team; the phases
are ordered so that each one ships something usable.

### Phase 1: Foundations (2027 H1)

- [ ] **Core** ([Godot core study](godot-core.md)): `Variant`, `StringName`, `RID`/`RIDOwner`,
  `ClassDB` reflection, signals, `Resource` + `ResourceLoader/Saver`, `ProjectSettings`,
  `WorkerThreadPool` (on top of our SDL job system), `UndoRedo`.
- [ ] **Server split** ([servers](servers.md)): `RenderingServer`, `PhysicsServer3D` (Jolt),
  `AudioServer`, `DisplayServer` (SDL3), `TextServer`, `NavigationServer3D`, each with a command
  queue so it can run on its own thread.
- [ ] **ECS v1** ([ECS](ecs-components.md)): archetypes, queries, a system scheduler on the job
  graph, change detection, command buffers, and reflection-driven serialization.
- [ ] **Shaders** ([Slang](shaders-rhi.md)): a Slang → WGSL/SPIR-V/MSL/DXIL pipeline feeding
  SDL_GPU, plus shader reflection used to bind resources.
- [ ] **Asset import**: glTF 2.0 (fastgltf), FBX (ufbx), and everything else through Assimp.
  Textures go to KTX2/Basis. Meshes are optimized and clustered with meshoptimizer.
- [ ] **.NET 11** migration of the CLI and C# API (one-line `PhoenixTargetFramework` change).

### Phase 2: The 3D baseline and editor alpha (2027 H2)

- [ ] **Render pipelines** ([pipelines](render-pipelines.md)): a frame graph, the
  `RenderPipelineAsset` API, and the built-in **Mobile** and **Desktop** pipelines.
- [ ] **Rendering:**
  - PBR (metal/rough, clear coat, sheen)
  - clustered Forward+
  - cascaded shadow maps with PCSS
  - IBL with reflection probes
  - GTAO, bloom, ACES/AgX tonemapping, TAA, FXAA/SMAA, fog, decals
  - CPU LOD groups and GPU frustum culling
- [ ] **Scenes:** nested prefabs, variants and overrides; `.phscn` text format; Godot
  `.tscn`/`.tres` import ([formats](godot-formats.md)).
- [ ] **Unity component parity**, first wave: Transform, MeshRenderer, Camera, Light, Rigidbody and
  colliders, CharacterController, AudioSource, Animator (basic), Canvas UI.
- [ ] **Editor alpha** ([editor](editor.md)): scene tree, inspector, asset browser, 3D viewport
  with gizmos, play-in-editor, undo/redo, running as a PWA and as a native desktop app.

### Phase 3: GPU-driven, high-end on SDL_GPU (2028 H1)

- [ ] **Virtual Geometry v1**: a meshlet cluster DAG with LOD selection, two-pass HZB occlusion
  culling, a visibility buffer, and indirect draws (hardware raster only).
- [ ] **Virtual Shadow Maps v1**: page tables in storage buffers, cached pages, and a clipmap for
  the directional light.
- [ ] **SSR** (hierarchical tracing), **SSGI**, DDGI-style irradiance probes, volumetric fog and
  clouds, a physically based sky and atmosphere.
- [ ] **Temporal Super Resolution**: upscaling with history rectification and reactive masks.
- [ ] Skinning and morphs on GPU, an animation graph, a GPU particle system, terrain with virtual
  texturing, foliage instancing.
- [ ] **Ultra pipeline** (HDRP-like) preview; Godot import complete; editor beta.

### Phase 4: The custom RHI and next-gen features (2028 H2 →)

- [ ] **slang-rhi backend** behind `RenderingServer`: bindless descriptors, mesh/amplification
  shaders, hardware ray tracing (DXR / Vulkan RT / Metal RT), and async compute.
- [ ] **Virtual Geometry v2**: a software rasterizer for tiny triangles and streaming of cluster pages.
- [ ] **Ray-traced** shadows, reflections and AO; **Stochastic Many-Lights** (ReSTIR-style
  sampling + RT shadows + denoiser); **Dynamic GI** (screen traces → SDF/surface cache → RT).
- [ ] **Consoles** through partner SDKs. These require platform-holder approval and NDA, and are
  kept in private repositories.

## How each phase is validated

- Every server API has unit tests (native + wasm64) and a headless "null" implementation.
- Every rendering feature has a golden-image test per tier and a GPU timing budget:
  - **Mobile**: 16.6 ms on a mid-range phone
  - **Desktop**: 16.6 ms at 1440p on a mid-range GPU
  - **Ultra**: 16.6 ms at 4K output with TSR on a high-end GPU
- The editor is driven by Playwright e2e tests in its PWA form.
