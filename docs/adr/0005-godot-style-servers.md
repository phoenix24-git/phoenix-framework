---
title: 0005 Server architecture
parent: Decisions (ADRs)
nav_order: 5
---

# 0005. Godot-style servers between scenes and hardware

- Status: Accepted
- Date: 2026-11-03

## Context

We render through SDL_GPU today and will need bindless descriptors, mesh shaders and ray tracing
through slang-rhi later. Physics, audio, navigation and text will also change implementations over
time. Rewriting gameplay, ECS or editor code for each change is not acceptable.

## Decision

All hardware-facing subsystems become **servers** (`RenderingServer`, `PhysicsServer3D/2D`,
`AudioServer`, `NavigationServer3D`, `TextServer`, `DisplayServer`), following Godot's `servers/`
design:

- Resources are addressed only through `RID`s.
- Calls are queued as commands so a server can run on its own thread.
- Arguments are plain data and spans.
- Features query capabilities, never backend names.
- Every server has a null implementation for tests and headless builds.

See [Server architecture](../engine/servers.md).

## Consequences

- A renderer swap (SDL_GPU → slang-rhi) touches only code under `servers/rendering/`.
- Some extra indirection; it is mitigated by bulk APIs (`instances_set_transforms`) fed by ECS
  change detection.
