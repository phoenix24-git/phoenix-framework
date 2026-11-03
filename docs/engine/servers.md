---
title: Server architecture
parent: Engine roadmap
nav_order: 2
---

# Server architecture (renderer-agnostic by design)
{: .no_toc }

Godot keeps its scene tree away from the GPU: nodes call `RenderingServer`, which owns every GPU
object and is addressed only through opaque `RID`s. Because of that, Godot runs three renderers
(Forward+, Mobile, Compatibility) behind one API. Phoenix adopts the same rule so that moving from
**SDL_GPU** to a **slang-rhi** renderer (bindless, mesh shaders, ray tracing) changes code below the
server line only.

1. TOC
{:toc}

## Layering

```
 editor/  ─┐
 game C#  ─┼─► scene/ (ECS world, prefabs, Unity-style API, Godot import)
 game C++ ─┘          │  extract (per frame, read-only views of ECS data)
                      ▼
 ┌──────────────────────────────── servers/ ───────────────────────────────────────┐
 │ RenderingServer  PhysicsServer3D/2D  AudioServer  NavigationServer3D  TextServer │
 │ DisplayServer    InputServer  XRServer (later)                                   │
 └───────────────┬───────────────────────────────────────────────────────────────────┘
                 │ implementations, chosen at boot
   RenderingServer ─► SdlGpuRenderer (now: WebGPU/Vulkan/Metal/D3D12 through SDL_GPU)
                   ─► RhiRenderer    (later: slang-rhi, bindless + mesh shaders + RT)
                   ─► NullRenderer   (headless tests, dedicated servers)
   PhysicsServer3D ─► JoltPhysics (now)    NavigationServer3D ─► Recast/Detour
   AudioServer     ─► SDL3 audio streams   TextServer         ─► HarfBuzz + FreeType + MSDF
```

## Rules

1. **Handles only.** Servers hand out `RID`s (64-bit: index + generation, owned by a `RIDOwner`).
   Callers never see pointers to server-internal objects.
2. **Commands, not calls.** Each server API call writes a command into a `CommandQueue`. A server
   runs either *inline* (web main thread, tests) or *threaded* (its own thread, drained once per
   frame). Callers can't tell the difference, the same way Godot's `RenderingServerDefault`
   wraps the threaded case.
3. **Plain data across the line.** Arguments are POD structs and spans (`MaterialParams`,
   `MeshSurfaceData`, `std::span<const Transform>`), never scene objects. That is the same zero-copy
   rule as the C# Script ABI.
4. **Capabilities, not backends.** Features query `rendering_server().caps()`, for example
   `mesh_shaders`, `ray_tracing`, `bindless`, `64bit_atomics`, `subgroups` and `timestamp_queries`.
   They never check backend names. This is how the Ultra pipeline uses mesh shaders when present and
   compute + indirect draws otherwise.
5. **Every server has a null implementation**, used by headless tests and dedicated servers.

## RenderingServer API sketch

```cpp
namespace phoenix::rs {

struct Caps { bool compute, indirect_count, mesh_shaders, ray_tracing, bindless,
              atomics64, subgroups, timestamps; uint64_t max_buffer_size; };

class RenderingServer {
public:
    virtual ~RenderingServer() = default;
    virtual const Caps& caps() const = 0;

    // Resources
    virtual RID mesh_create(const MeshDesc&) = 0;               // clusters/LODs built at import
    virtual RID texture_create(const TextureDesc&, std::span<const std::byte> mips) = 0;
    virtual RID material_create(RID shader, const MaterialParams&) = 0;
    virtual RID shader_create(const ShaderDesc&) = 0;             // Slang module + entry points

    // World ("scenario" in Godot)
    virtual RID scenario_create() = 0;
    virtual RID instance_create(RID scenario, RID mesh, RID material) = 0;
    virtual void instances_set_transforms(std::span<const RID>, std::span<const Mat4x3>) = 0; // bulk, from ECS
    virtual RID light_create(RID scenario, const LightDesc&) = 0;
    virtual RID reflection_probe_create(RID scenario, const ProbeDesc&) = 0;
    virtual RID environment_create(const EnvironmentDesc&) = 0;  // sky, fog, GI, exposure

    // Views and frame
    virtual RID camera_create() = 0;
    virtual RID viewport_create(const ViewportDesc&) = 0;       // window, editor panel, render target
    virtual void viewport_set_pipeline(RID viewport, RID render_pipeline) = 0;
    virtual void frame_draw() = 0;

    virtual void free(RID) = 0;
};

}  // namespace phoenix::rs
```

`instances_set_transforms` is the bridge to ECS. Once per frame a render-extract system iterates
`(LocalToWorld, MeshRenderer)` chunks and pushes dirty transforms in bulk, so the per-instance cost
is a memcpy rather than a virtual call.

## Migration from the current MVP

| Today (MVP) | Becomes |
|---|---|
| `Renderer` interface (`sdl_gpu` / `sdl_renderer`) | `RenderingServer` + `SdlGpuRenderer` / `CompatRenderer` |
| `PhysicsWorld` (Jolt, one frame ahead) | `PhysicsServer3D` / `PhysicsServer2D` (the Jolt implementation keeps the async step) |
| `StorageService` (IO thread, OPFS) | `ResourceLoader` threaded loads + VFS |
| `Engine::iterate` | `MainLoop`: input → ECS systems → extract → servers flush → `frame_draw` |
| `devtools::publish` | `EngineDebugger` (remote scene tree, profiler, logs) |

## Threading model

| Thread | Work |
|---|---|
| Main | input, ECS simulation scheduling, editor UI |
| Workers (6 on the console profile) | ECS systems, culling, animation, physics jobs, import jobs |
| Render thread (native) | drains the RenderingServer queue, records GPU work |
| IO thread | VFS, resource loading, OPFS |

On the web the WebGPU device lives on the browser main thread. The RenderingServer therefore runs
inline there, while heavy CPU work (culling, skinning prep, cluster selection) still fans out to
workers. Recording on a worker becomes possible once WebGPU-in-workers with `OffscreenCanvas` is
wired through SDL.
