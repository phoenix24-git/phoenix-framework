---
title: 0007 Slang + two RHIs
parent: Decisions (ADRs)
nav_order: 7
---

# 0007. Slang for all shaders; SDL_GPU now, slang-rhi as the second RHI

- Status: Accepted
- Date: 2026-11-03

## Context

The MVP's hand-written WGSL only works on WebGPU. We need SPIR-V, MSL and DXIL for SDL_GPU on native
platforms, and later a backend with bindless, mesh shaders and ray tracing.

## Decision

- All shaders are written in **Slang** and compiled offline by `phoenix build`. The editor (PWA
  included) compiles them live through libslang.
- **SDL_GPU** stays the renderer backend on every platform until slang-rhi is ready. It remains the
  long-term default for web and mobile.
- **slang-rhi** becomes the second backend, wrapped by a Phoenix layer (frame graph, bindless heaps,
  residency) under the same `RenderingServer` ([ADR 0005](0005-godot-style-servers.md)).

See [Slang, RHI and libraries](../engine/shaders-rhi.md).

## Consequences

- One shader source, reflection-driven binding, shared structs with C++/C#.
- Two backends to test. Golden-image tests run per backend and per tier in CI.
