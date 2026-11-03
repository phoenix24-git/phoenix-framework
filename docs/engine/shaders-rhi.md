---
title: Slang, RHI and libraries
parent: Engine roadmap
nav_order: 7
---

# Slang shaders, SDL_GPU today, slang-rhi tomorrow
{: .no_toc }

1. TOC
{:toc}

## Why Slang

[Slang](https://github.com/shader-slang/slang) (Apache-2.0 with LLVM exception, hosted by Khronos)
compiles one source to **SPIR-V, DXIL, MSL, WGSL, HLSL, GLSL and CUDA**. It has modules,
generics, interfaces, automatic differentiation and full reflection. For Phoenix that means:

- **One shader language** for every backend: SDL_GPU on WebGPU (WGSL), Vulkan (SPIR-V), Metal (MSL)
  and D3D12 (DXIL). The hand-written WGSL in the MVP goes away.
- **Material interfaces** (`IMaterial`, `ILightingModel`) give us the "one material, three
  pipelines" story through specialization instead of text-macro permutations.
- **Reflection** drives resource binding, so the engine knows each shader's buffers and textures
  without hand-maintained tables.
- **The same struct definitions** shared by C++, C# (via codegen) and shaders, much like the
  layout-checked Script ABI.

## Shader pipeline

```
 *.slang ──► slangc (offline, `phoenix build`)  ──► per-backend blobs + reflection JSON
             │  WGSL (web)  SPIR-V (Vulkan)  MSL (Metal)  DXIL (D3D12)
             └► editor: libslang in-process (also compiled to wasm for the PWA editor) for live reload
```

SDL_GPU expects a fixed binding convention for each backend. A small Slang → SDL_GPU binding
adapter maps reflection onto it (resource sets 0–3 per stage). Pipeline states are cached on disk
and pre-warmed during loading screens.

## Two RHIs behind one RenderingServer

| | **SDL_GPU** (now) | **slang-rhi** (later) |
|---|---|---|
| Backends | Vulkan, D3D12, Metal, **WebGPU** (SDL_wgpu) | D3D12, Vulkan, Metal, WebGPU, CUDA, CPU |
| Compute, storage buffers, indirect draws | ✅ | ✅ |
| Bindless / descriptor indexing | ❌ | ✅ |
| Mesh / amplification shaders | ❌ | ✅ |
| Hardware ray tracing | ❌ | ✅ (DXR, VK_KHR_ray_tracing, Metal RT) |
| Async compute queues, 64-bit atomics, subgroups | limited | ✅ |
| Platforms | everything, including the web and mobile | desktop and consoles first |

[slang-rhi](https://github.com/shader-slang/slang-rhi) is a thin, modern RHI designed together with
Slang. We use it as our **second target** rather than writing a new RHI from scratch. The custom RHI
the roadmap mentions is therefore a Phoenix layer *on top of* slang-rhi (frame graph, residency,
bindless heaps). Platform backends that slang-rhi lacks, such as consoles, plug in at that layer.

Both RHIs sit **below the RenderingServer** ([servers](servers.md)), so the switch never touches
ECS, gameplay or the editor. A project can run SDL_GPU on web and mobile and slang-rhi on desktop in
the same build.

### Feasibility check

| Question | Answer |
|---|---|
| Can Slang target WGSL for SDL_GPU on the web? | Yes: Slang has a WGSL backend, and the Slang playground already compiles in the browser. |
| Can slang-rhi replace SDL_GPU on the web? | Partly. It has a WebGPU backend, but SDL_GPU stays the default on web/mobile because it's already integrated (windowing, swapchain, SDL_wgpu). |
| Is a ray-traced, mesh-shader, bindless renderer possible in the browser? | Not yet: WebGPU has no RT or mesh shaders. The architecture is ready for when the extensions ship. |

## Libraries

| Need | Library | License | Notes |
|---|---|---|---|
| Model import (broad) | **Assimp** | BSD-3 | 40+ formats; runs in the importer only, never in shipped games |
| glTF (primary) | fastgltf (or cgltf) | MIT | fast, spec-complete glTF 2.0 |
| FBX | ufbx | MIT / Unlicense | more robust than Assimp's FBX reader |
| Mesh optimization, meshlets, cluster LOD | **meshoptimizer** | MIT | `buildMeshlets`, `simplify`, `clusterlod` for Virtual Geometry |
| Shaders | **Slang** | Apache-2.0 + LLVM | |
| RHI (second target) | **slang-rhi** | Apache-2.0 | |
| Textures | KTX-Software + Basis Universal; stb_image; tinyexr | Apache-2.0 / MIT / BSD | GPU-compressed ASTC/BC7/ETC2 at import |
| Physics | Jolt | MIT | already integrated |
| Navigation | Recast/Detour | zlib | |
| Text | HarfBuzz, FreeType, msdfgen | MIT / FTL / MIT | SDF glyph atlas |
| Audio | SDL3 audio + miniaudio DSP (optional) + Steam Audio (optional) | zlib / MIT / Apache-2.0 | |
| Compression | zstd, lz4 | BSD | packs and streaming |
| Editor (early tooling) | Dear ImGui (debug overlays only) | MIT | the real editor UI uses Phoenix widgets |

All dependencies stay pinned by commit SHA in `engine/cmake/PhoenixDependencies.cmake`, behind CMake
options so web builds include only what they use.
