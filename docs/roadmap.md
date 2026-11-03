---
title: Roadmap
nav_order: 10
---

# Roadmap

Honest status of the MVP and what comes next. Items are ordered roughly by priority.

## Works today (tested in CI)

- `phoenix create | run | build web/linux | test | clean | doctor | devices | precache`
- wasm64 + pthreads web builds:
  - 6 SDL worker threads as Web Workers
  - Jolt physics on the worker pool
  - OPFS persistence
  - WebGPU rendering with a preallocated VRAM pool, plus a WebGL2 fallback
- `phoenix run`:
  - dev server with COOP/COEP headers
  - Chrome launched with remote debugging
  - `r` hot restart over SSE
  - Phoenix DevTools (live stats and logs)
  - C++ source maps in Chrome DevTools
- GitHub Pages hosting through the cross-origin-isolation service worker
- C# scripts via NativeAOT with zero-copy spans (desktop)
- Flutter-style widgets and `WidgetTester`

## Next

1. **Text:** replace the 5×7 bitmap font with an SDF glyph atlas (HarfBuzz shaping) behind `Text`.
2. **Real Element tree:** reconcile widgets like Flutter (keys, element reuse) so `StatefulWidget`
   instances no longer need to be held by their parents.
3. **Desktop SDL_GPU:** compile shaders offline (WGSL → SPIR-V/MSL/DXIL with Tint or Naga) so desktop
   uses SDL_GPU on Vulkan/Metal/D3D12 instead of SDL_Renderer.
4. **JSPI instead of ASYNCIFY** for WebGPU device creation: smaller and faster wasm.
5. **Hot reload (state-preserving):** reload `lib/` as a side module (`dlopen` on wasm) while keeping
   engine state; C# scripts can be reloaded the same way on desktop.
6. **DevTools:** widget inspector (semantics tree), frame timeline, a job-system/worker view, and a
   CDP bridge so DevTools can pause and step the app.
7. **Assets pipeline:** `assets:` in phoenix.json, textures packed into the VRAM pool.
8. **Audio** (SDL3 audio streams), **gamepad input**, **3D** (Jolt is already 3D; the renderer is not yet).

## .NET roadmap

- **2026 (.NET 10 LTS):** the CLI, C# framework and desktop NativeAOT scripts.
- **wasm64 C# (in progress):** runtime sources are pinned and fetched with `dev/tools/dotnet_runtime.sh`.
  The plan is to add wasm64 to NativeAOT-LLVM; see [ADR 0005](adr/0005-dotnet-runtime-sources-for-wasm64-csharp.md).
- **2027 (.NET 11):** bump `PhoenixTargetFramework` and `global.json`. Switch to official WebAssembly
  NativeAOT when it supports **wasm64**, for compiling C# scripts into the same module as the
  engine. The Script ABI already works there: calls are plain C, data is shared by pointer, and the
  memory is one shared linear memory.

## Platforms

| Target | Status |
|---|---|
| Web (Chrome/Edge 133+, Firefox 134+) | ✅ primary |
| Linux desktop | ✅ (SDL_Renderer) |
| Windows / macOS desktop | 🧪 builds in CI as experimental |
| Android / iOS | planned (SDL3 supports both) |
