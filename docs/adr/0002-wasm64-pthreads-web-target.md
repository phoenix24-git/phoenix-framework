---
title: 0002 wasm64 web target
parent: Decisions (ADRs)
nav_order: 2
---

# 0002. wasm64 + pthreads + WebGPU as the web target

- Status: Accepted
- Date: 2026-11-03

## Context

The engine is designed like a console: a fixed worker pool and large memory budgets. wasm32 caps
linear memory at 4 GiB; Memory64 (shipped in Chrome/Edge 133 and Firefox 134) removes that cap.
Real threads on the web are Web Workers sharing a `SharedArrayBuffer`, which requires cross-origin
isolation.

## Decision

- Compile with Emscripten (version pinned in `bin/internal/emscripten.version`) using `-m64 -pthread`
  for every translation unit, via `engine/cmake/toolchains/emscripten-wasm64.cmake`.
- Force `SDL_PTHREADS=ON` (SDL defaults to thread-less on Emscripten) so SDL threads are pthreads.
- Render through SDL_GPU's WebGPU backend from the SDL_wgpu fork (emdawnwebgpu), falling back to
  SDL_Renderer/WebGL2 when no adapter is available.
- Persist data with WasmFS + OPFS, owned by a dedicated IO thread.
- Provide cross-origin isolation via server headers (`phoenix run`) or a service worker (static hosts).

## Consequences

- Safari is not supported until it ships Memory64.
- The WebGPU backend uses ASYNCIFY for adapter and device requests, which costs some code size.
  JSPI is the planned replacement.
- The same unit tests run natively and under Node 24 with real worker threads, so CI exercises the
  threading model in both environments.
