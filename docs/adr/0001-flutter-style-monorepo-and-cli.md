# 0001. Flutter-style monorepo and CLI in C#

- Status: Accepted
- Date: 2026-11-03

## Context

The goal is Flutter's developer experience (`create`/`run`/`build`/`test`, hot restart, DevTools)
for a C++ engine. Flutter achieves it with an SDK checkout (`bin/flutter` + `packages/flutter_tools`)
that drives platform builds; apps contain no build-system files.

## Decision

- One repository holds the engine, framework, tool, templates and examples, laid out like Flutter's
  (`bin/`, `packages/`, `engine/`, `templates/`, `examples/`, `dev/`).
- The CLI is a .NET 10 console app (`packages/phoenix_tools`), launched through `bin/phoenix`, which
  rebuilds it when its sources change (as `bin/flutter` rebuilds the tool snapshot).
- Apps are described by `phoenix.json`; the tool translates it into CMake cache variables and drives
  `engine/CMakeLists.txt`. Apps never contain CMake.
- The target framework lives in one MSBuild property (`PhoenixTargetFramework`) plus `global.json`, so
  the planned move to .NET 11 is a two-line change.

## Consequences

- Contributors need the .NET SDK, CMake and Ninja; `phoenix doctor` checks this.
- Engine and tool versions never drift: they ship from the same commit.
