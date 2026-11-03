---
title: Architecture
nav_order: 8
---

# Phoenix architecture

This page describes how Phoenix is put together and why. Decisions that are expensive to reverse are recorded as ADRs in [`adr/`](adr/index.md).

## Layers

| Layer | Location | Responsibility | Flutter analogue |
|---|---|---|---|
| Tool | `packages/phoenix_tools` (C#) | create, build, run, test; dev server; DevTools hosting | `flutter_tools` |
| Framework (C#) | `packages/phoenix` | scripting API, Script ABI mirror | — |
| Framework (C++) | `engine/include/phoenix/widgets.h`, `engine/src/framework` | widgets, layout, hit testing, semantics | `package:flutter` |
| Engine shell | `engine/src/shell` | process entry, frame loop, embedder | `flutter/shell` |
| Renderer | `engine/src/renderer` | display list → GPU | Impeller/Skia |
| Physics | `engine/src/physics` | Jolt world | — |
| Core | `engine/src/core` | memory, VRAM bookkeeping, jobs | `fml` |
| Platform | SDL3 (SDL_wgpu fork) | windows, input, threads, GPU, files | embedders |

Each engine module is a separate static library (`phoenix::core`, `::physics`, `::framework`, `::renderer`, `::engine`). Libraries may depend only on the ones above them in the list in `engine/src/CMakeLists.txt`. This modularity is borrowed from O3DE Gems: it keeps compile times and coupling under control as the engine grows.

## Threads

| Thread | Created by | Work |
|---|---|---|
| Main (browser main thread on the web) | SDL main callbacks | events, scripts, widget build/layout/paint, GPU submission |
| `phoenix.worker.0..5` | `JobSystem` via `SDL_CreateThread` | Jolt jobs, `parallel_for`, the physics step itself |
| `phoenix.io` | `StorageService` | file I/O, mounting OPFS (OPFS calls block, which the browser main thread is not allowed to do) |

On the web every SDL thread is a pthread backed by a Web Worker. All of them share one wasm64 `SharedArrayBuffer` memory, so the engine's memory pool is one pool on every platform.

**The browser main thread never blocks.** Physics runs one frame ahead (`begin_step`/`poll_step`) and storage completions are posted back to the main thread. `JobSystem::wait` makes the waiting thread execute queued jobs itself, so nested waits cannot deadlock.

## Memory: the virtual console profile

`phoenix.json → console` describes the hardware budget, the way a console SDK does:

```json
"console": { "vramMB": 3584, "vramPreallocate": true, "workerThreads": 6, "cpuArenaMB": 256 }
```

- **CPU arena:** reserved at boot. Per-frame allocations (for example, rectangles reserved by scripts) are bump-allocated and freed in O(1) with a reset at the end of the frame.
- **VRAM pool:** the budget is split into 256 MiB pages, one `SDL_GPUBuffer` each, because WebGPU caps the size of a single buffer. With `vramPreallocate` all pages are created at boot. Sub-allocation is a best-fit allocator with coalescing (`VramAllocator`), and it is unit tested without a GPU. The renderer's per-frame instance buffers are three slots carved out of the pool, one per frame in flight.
- **Linear memory (web):** `web.initialMemoryMB` (default 1 GiB) can grow up to `web.maximumMemoryMB` (default 16 GiB). Going past 4 GiB is only possible with wasm64.

Some devices refuse a 3.5 GiB VRAM reservation. The pool then logs how much it got and continues with that. For testing on small GPUs you can override the budget with `?vram=512` in the URL or `--vram-mb=512` natively.

## Frame pipeline

```
SDL events ─► Engine::handle_event (pointer state)
            ┌───────────────────────── Engine::iterate ─────────────────────────┐
            │ storage.pump()       deliver IO completions                        │
            │ physics.poll_step()  publish the finished snapshot (front/back swap)│
            │ physics.begin_step() kick the next step onto workers               │
            │ scripts.update()     C# reads bodies / writes draw rects (zero-copy)│
            │ WidgetsBinding.drawFrame() build if dirty → layout → paint → taps   │
            │ renderer.render()    blit display list → VRAM slot → 1 instanced draw│
            │ devtools::publish()  every 250 ms                                  │
            └────────────────────────────────────────────────────────────────────┘
```

## Rendering

- **SDL_GPU / WebGPU:** `PhxDrawRect` is also the vertex instance format (32 bytes). The WGSL vertex shader expands six vertices per instance, applies rotation, and maps logical pixels to NDC. Colour arrives as `UBYTE4_NORM` and is swizzled.
- **Fallback:** SDL_Renderer with `SDL_RenderGeometry`. That is WebGL2 in browsers without WebGPU and GL/D3D/Metal on desktop. Desktop SDL_GPU (Vulkan/D3D12/Metal) needs the shaders compiled to SPIR-V/DXIL/MSL; see the roadmap.

## Scripting

- **C++ scripts** are the app itself (`lib/*.cpp`). They are compiled into the executable and talk to the engine through `phoenix/engine.h`, which wraps the same `PhxEngineApi` table that C# uses.
- **C# scripts** are NativeAOT shared libraries. Rules ([ADR 0003](adr/0003-zero-overhead-csharp-interop.md)):
  1. `[assembly: DisableRuntimeMarshalling]`, so a non-blittable signature is a compile-time error.
  2. Data crosses the boundary as pointers or spans over engine memory, never as copies.
  3. Entry points are `[UnmanagedCallersOnly(EntryPoint=...)]` C symbols, and engine services are `delegate* unmanaged` pointers.
- The ABI is versioned (`PHX_SCRIPT_ABI_VERSION`), and the C# side rejects a mismatched table before touching it.

## Web runtime

- `phoenix_bootstrap.js`:
  - checks cross-origin isolation, Memory64, WebGPU and OPFS;
  - registers `phoenix_sw.js` when the host cannot send COOP/COEP headers;
  - loads the engine;
  - connects to the dev server's SSE stream for hot restart.
- `phoenix run` serves `build/web` with COOP/COEP/CORP headers, `application/wasm`, and SSE at `/__phoenix/events`. In debug mode it also serves the app and engine C++ sources under `/__phoenix/app` and `/__phoenix/sdk`, so Chrome DevTools can step through C++ using the source map.
- **Phoenix DevTools** (`/__phoenix/devtools/`) listens on the `phoenix-devtools` BroadcastChannel. The engine publishes stats and logs there from any thread.

## The `phoenix` tool

- `bin/phoenix` rebuilds the tool into `bin/cache/phoenix_tools` whenever its sources change, then runs it with `PHOENIX_ROOT` set. This is the same model Flutter uses for `bin/flutter`.
- An app build runs CMake on `engine/` with `-DPHOENIX_APP_DIR=<app>` plus cache variables generated from `phoenix.json`. Apps contain no CMake.
- Dependencies are pinned to exact commit SHAs in `engine/cmake/PhoenixDependencies.cmake`. They are fetched once into `bin/cache/deps` and shared by every app and target.
