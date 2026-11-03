---
title: 0008 Clean-room policy
parent: Decisions (ADRs)
nav_order: 8
---

# 0008. Borrow ideas, not code: clean-room policy

- Status: Accepted
- Date: 2026-11-03

## Context

The roadmap deliberately draws on Godot, Unity and Unreal. Their licenses differ. Godot is MIT.
Unity's source and documentation are proprietary. Unreal's EULA forbids using its source in other
engines. Several feature names are trademarks.

## Decision

- **Godot**: code may be studied and adapted, with MIT attribution kept in `THIRD_PARTY_NOTICES`.
- **Unity**: only public API concepts and names for familiarity. No source, documentation text or
  assets.
- **Unreal**: only techniques from public papers and talks. Contributors must not read or port
  Unreal source for Phoenix work.
- Features get descriptive names (Virtual Geometry, Virtual Shadow Maps, Dynamic GI, …), never
  other vendors' trademarks.
- Runtime dependencies must be permissively licensed (MIT/BSD/zlib/Apache-2.0). No GPL in the
  runtime.

## Consequences

Reviewers check new rendering or engine features for a citation (paper/talk) in the PR description.
