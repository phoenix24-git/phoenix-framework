# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- **.NET runtime sources for wasm64 C#** ([ADR 0005](docs/adr/0005-dotnet-runtime-sources-for-wasm64-csharp.md)):
  `dotnet/runtime`, `dotnet/runtimelab` NativeAOT-LLVM and the Mono wasm64 fork are pinned in
  `bin/internal/dotnet-runtime.lock`. `dev/tools/dotnet_runtime.sh fetch|status|path|bump` makes
  verified, blobless, sparse clones into `bin/cache/` (git-ignored).
- **Scenes and scripts** (see `docs/scenes.md`):
  - `.pscn` scene files use Godot's `.tscn` syntax, with `[node]`, `[component]` and `[script]`
    sections; start one with `phoenix::runScene("res://…")`.
  - C# `MetaBehaviour`s attach to GameObjects, with the Unity lifecycle, `properties` from the
    scene file, `GameObject`/`Transform`, component views (`MeshRenderer`, `Camera`, `Light`) and
    `Input`. Their data is read and written as zero-copy references into ECS memory.
- **3D scene rendering:** orthographic camera, lit `MeshFilter` cubes, whole-box back-to-front
  ordering.
- **Script ABI v2:**
  - a parallelogram draw mode (`PHX_DRAW_PARALLELOGRAM`)
  - scene functions: find, create, destroy, `get_component`, `get_field`
  - a `phoenix_script_attach` export
- **Engine flags:**
  - `--screenshot=` saves the last frame.
  - `--res=` sets the folder behind `res://`.
  - `--fixed-dt=` advances every frame by a fixed amount of time, for deterministic headless runs.
- **Skyline example** (`examples/skyline`): an original 3D block-stacking game, built from a scene
  file and C# scripts. CI plays it headless.

- **Virtual file system** (`phoenix/vfs/vfs.h`), using Godot-style paths:
  - `res://` for project resources and `user://` for per-player data, plus `uid://`
  - path normalization that blocks paths escaping their root
  - atomic writes
  - `.pck` and `.zip` packs: a sealed main pack for exported games, and layered DLC and mod packs
  - on the web, `user://` is stored in OPFS
- `phoenix_pack` tool: packs a project folder into a `.pck`.

- **ECS** (`phoenix::ecs`, on foundation types):
  - archetype storage in 16 KiB SoA chunks
  - entity generations
  - queries with `with`/`without`/`changed`, `each`, `for_each_chunk`, and `par_each` on the
    WorkerThreadPool
  - per-column change versions
  - command buffers
  - a scheduler that runs non-conflicting systems in parallel waves
- **Reflection**: every component field converts to and from `Variant`/`Dictionary`.
- **Unity-style scene layer** (`phoenix::scene`):
  - `Scene`, `GameObject`, `Transform` (world-space getters are always current)
  - **`MetaBehaviour`** (Phoenix's `MonoBehaviour`) with the full lifecycle
  - `Time`
  - `Instantiate`, `Destroy` and `Find`
- **141 Unity components** generated from `engine/components/catalogue.py`, with the docs at
  `docs/components.md`.
- C# scripts derive from `MetaBehaviour` (`Awake`, `Start`, `Update`, `OnDestroy`) and read frame
  timing from `Time`.

- **Godot core foundation**:
  - Godot 4.7.2 `core/` is vendored verbatim in `engine/thirdparty/phoenix_core`, built as
    `phoenix::foundation` with CMake, natively and on wasm64.
  - It runs on a new SDL3 platform layer (`OS_Phoenix`, `FileAccessSDL`, `DirAccessSDL`) through
    `phoenix::foundation::CoreRuntime`.
  - Godot's own core test suite passes on both targets: 831 cases, 197,500 assertions.
- Godot editor icons (1,021 SVGs) and editor fonts in `engine/assets/editor`, plus
  `THIRD_PARTY_NOTICES.md`.
- `dev/tools/vendor_foundation.py` for reproducible Godot updates.


- `phoenix build web -o/--output <dir>`: publishes the finished bundle to a folder. It only replaces a
  folder that is empty or holds a previous Phoenix bundle.
- Documentation site in `docs/`, served by GitHub Pages from `main /docs` with Jekyll and
  just-the-docs. Pages: getting started, CLI reference, widgets, C# scripting, web hosting and
  testing.
- Live demo committed to `docs/demo` (plus DevTools), rebuilt by the new `Web demo` workflow.
- `.github/rulesets/protect-main.json`: an importable branch-protection ruleset for `main`.
- CI job that builds `docs/` with the GitHub Pages Jekyll toolchain.

### Changed

- GitHub Actions bumped to their Node 24 majors.
- Windows and macOS engine jobs are now required. They passed on GitHub runners.

### Removed

- `pages.yml`, the Actions-based Pages deploy. The repository serves Pages from `main /docs`.

### Fixed

- Directory listings in the SDL file layer no longer return nested files.

## [0.1.0] - 2026-11-03

### Added

- **Engine (C++20):**
  - SDL3 (SDL_wgpu fork) platform layer
  - SDL_GPU/WebGPU renderer with a preallocated 3.5 GiB VRAM pool, plus an SDL_Renderer fallback
  - Jolt 5.4 physics on a 6-thread SDL3 job system
  - frame arena
  - IO thread with OPFS persistence
  - DevTools bridge
- **Framework:**
  - Flutter-style widgets: Scaffold, AppBar, Column/Row/Expanded, Container, Text, buttons, PhysicsView
  - `setState`
  - semantics
  - `WidgetTester`
- **C# scripting:** `Phoenix.Framework` with runtime marshalling disabled, a function-pointer Script ABI, and zero-copy spans. Scripts compile with NativeAOT.
- **CLI (`phoenix`, .NET 10):** `create`, `run`, `build web|linux|macos|windows`, `test`, `clean`, `doctor`, `devices`, `precache`.
  - Dev server with COOP/COEP headers, SSE hot restart and C++ source serving.
  - Chrome launcher.
- **Web runtime:**
  - bootstrap with capability checks
  - cross-origin-isolation service worker for static hosting
  - Phoenix DevTools page
- **Quality:**
  - doctest suites, run natively and on wasm64 under Node
  - ASan/UBSan/TSan presets
  - xUnit suites
  - C++ ↔ NativeAOT C# integration test
  - Playwright e2e covering WebGPU and the WebGL fallback
  - CI, CodeQL, Dependabot, and GitHub Pages deployment
