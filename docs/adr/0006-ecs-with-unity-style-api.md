---
title: 0006 ECS + Unity-style API
parent: Decisions (ADRs)
nav_order: 6
---

# 0006. Archetype ECS storage with a Unity-style authoring API

- Status: Accepted
- Date: 2026-11-03

## Context

We want Unity developers to feel at home (GameObject, Transform, MonoBehaviour, nested prefabs). We
also want data-oriented performance (hundreds of thousands of entities, parallel systems) and
Godot scene import.

## Decision

- Storage is an **archetype ECS** (16 KiB SoA chunks, change versions, command buffers, a scheduler
  on the job system).
- `GameObject` and `Component` are **handles and views** over entities and components.
  `MonoBehaviour` is a managed component, for convenience. `IComponentData`/`ISystem` (C#) and
  native systems (C++) provide scale.
- Prefabs are entity hierarchies with GUID-addressed overrides and variants.
- Godot's node tree is imported as prefabs, using a node→component mapping table.

See [ECS and Unity components](../engine/ecs-components.md).

## Consequences

- One world serves prototyping and production. Users choose per feature, not per project.
- Reflection metadata (`ClassDB`) is shared by serialization, the inspector, Godot import and C#
  codegen, so components must be registered.
