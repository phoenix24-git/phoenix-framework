# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

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
