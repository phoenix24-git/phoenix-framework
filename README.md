# Phoenix

[![CI](https://github.com/phoenix24-git/phoenix-framework/actions/workflows/ci.yml/badge.svg)](https://github.com/phoenix24-git/phoenix-framework/actions/workflows/ci.yml)
[![Pages](https://github.com/phoenix24-git/phoenix-framework/actions/workflows/pages.yml/badge.svg)](https://github.com/phoenix24-git/phoenix-framework/actions/workflows/pages.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

**Live demo:** <https://phoenix24-git.github.io/phoenix-framework/> · **Live DevTools:** <https://phoenix24-git.github.io/phoenix-framework/devtools/> (open next to the demo)

Phoenix is an app and game framework with **Flutter's developer workflow** (`create` → `run` → hot restart → `build` → `test`) on top of a **native C++ engine**:

- **SDL3** embedded as the platform layer, with **SDL_GPU on WebGPU** through [SDL_wgpu](https://github.com/TheeStickmahn/SDL_wgpu).
- **Jolt Physics**, running on the engine's own worker pool.
- Apps written in **C++** with a Flutter-style widget API. Gameplay scripts can also be written in **C#**, compiled with NativeAOT and called with no marshalling.
- The web target is **wasm64 (Memory64) with pthreads**: Web Workers share one linear memory that can grow past 4 GiB. Persistent storage uses **OPFS**.
- A **virtual console** profile reserves fixed hardware budgets at boot. By default that is **3.5 GiB of VRAM** and **6 worker cores**.

The CLI is written in C# (.NET 10, with .NET 11 planned for 2027) and plays the role of `flutter_tools`. Some ideas are borrowed from O3DE (modular engine libraries), Godot (scripts attached to an engine core) and Bevy (data in plain structs shared across systems).

```bash
git clone https://github.com/phoenix24-git/phoenix-framework.git
export PATH="$PWD/phoenix-framework/bin:$PATH"

phoenix doctor                 # checks .NET, CMake, Ninja, Emscripten and Chrome
phoenix precache --web         # installs the pinned Emscripten SDK if you don't have it
phoenix create my_app
cd my_app
phoenix run -d chrome          # debug build, dev server, Chrome and DevTools; press r to hot restart
phoenix test                   # widget tests (flutter_test style)
phoenix build web --release    # build/web, ready for any static host
```

## Flutter → Phoenix

| Flutter | Phoenix |
|---|---|
| `flutter create`, `run`, `build web`, `test`, `doctor`, `devices`, `clean`, `precache` | the same commands under `phoenix` |
| `pubspec.yaml` | `phoenix.json` (adds the `console` hardware profile) |
| `lib/main.dart` → `void main() => runApp(MyApp());` | `lib/main.cpp` → `void phoenix_main() { runApp(std::make_shared<MyApp>()); }` |
| `Scaffold(appBar: AppBar(title: Text('Hi')))` | `Scaffold({.appBar = AppBar({.title = "Hi"})})` (C++20 designated initializers act as named arguments) |
| `StatefulWidget` + `setState` | `StatefulWidget` + `setState` |
| `flutter_test` `WidgetTester`, `find.text`, `tester.tap` | `phoenix/testing.h` `WidgetTester`, `findText`, `tap` |
| `web/index.html` + `flutter_bootstrap.js` | `web/index.html` + `phoenix_bootstrap.js` |
| `r` hot reload / `R` hot restart | `r`/`R` hot restart (incremental rebuild, then page reload over SSE) |
| Dart DevTools | Phoenix DevTools at `/__phoenix/devtools/` plus Chrome DevTools with C++ source maps |
| `FLUTTER_ROOT`, `bin/cache`, `bin/internal/*.version` | `PHOENIX_ROOT`, `bin/cache`, `bin/internal/emscripten.version` |

## Architecture

```
 ┌────────────────────────── your app (lib/main.cpp, optional C# script) ──────────────────────────┐
 │  Widgets (Scaffold, Column, Text, PhysicsView…)       C# PhoenixScript (NativeAOT .so/.dll)       │
 └───────────────▲───────────────────────────────────────────────────▲──────────────────────────────┘
                 │ C++ API                                            │ Script ABI (C, blittable, versioned)
 ┌───────────────┴──────────── Phoenix engine (C++20, static libs) ──┴──────────────────────────────┐
 │ phoenix::engine     Shell: SDL3 main callbacks, frame loop, script host, DevTools bridge, IO thread │
 │ phoenix::renderer   SDL_GPU → WebGPU (WGSL, instanced quads, VRAM pool) │ SDL_Renderer fallback     │
 │ phoenix::framework  Widgets, layout (constraints down, sizes up), hit testing, semantics          │
 │ phoenix::physics    Jolt 5.4 world, one frame ahead, on the shared job system                       │
 │ phoenix::core       LinearArena, VramAllocator, JobSystem (SDL3 threads = Web Workers on wasm64)    │
 └──────────────── SDL3 (SDL_wgpu fork) · Jolt · Emscripten (wasm64, pthreads, WasmFS/OPFS) ─────────┘
```

Each frame:

1. Pending storage reads are delivered.
2. The physics step from the previous frame is collected and the next one starts on worker threads. The browser main thread never waits on physics.
3. C# scripts run.
4. Widgets are laid out and painted into a display list (`PhxDrawRect[]`).
5. The display list is copied into this frame's slot of the preallocated VRAM page and drawn with a single instanced draw call.

See [docs/architecture.md](docs/architecture.md) and the [ADRs](docs/adr/).

## Zero-overhead C# scripting

```csharp
[assembly: DisableRuntimeMarshalling]                     // 1. no runtime marshalling, enforced by the compiler

public sealed class RainScript : PhoenixScript
{
    public override void OnUpdate(in PhxFrameInfo frame)
    {
        ReadOnlySpan<PhxBody2D> bodies = Engine.Bodies;   // 2. a span over engine memory, no copy
        Span<PhxDrawRect> rects = Engine.DrawRects(bodies.Length); // writes go straight into the frame
        ...
    }
}

public static unsafe class Exports                        // 3. NativeAOT turns this into a C symbol
{
    [UnmanagedCallersOnly(EntryPoint = "phoenix_script_init")]
    public static int Init(PhxEngineApi* api) => ScriptHost.Init<RainScript>(api);
}
```

The engine table is a C struct of `delegate* unmanaged<...>` function pointers ([`script_abi.h`](engine/include/phoenix/script_abi.h) and [`ScriptAbi.cs`](packages/phoenix/src/Interop/ScriptAbi.cs)). Both sides check the layout byte for byte in their tests, and CI loads the real NativeAOT library from C++. See [examples/csharp_script](examples/csharp_script).

> C# scripts currently run on **desktop**. .NET 10 cannot produce wasm64 NativeAOT code, so web apps use C++ for now. See the [roadmap](docs/roadmap.md).

## Repository layout

```
bin/                 phoenix launcher (bash, PowerShell, cmd), pinned tool versions, cache/ (git-ignored)
engine/              C++ engine: CMake, public headers, sources, unit tests, web runtime, DevTools
packages/phoenix/        Phoenix.Framework: C# scripting API and interop (+ tests)
packages/phoenix_tools/  the phoenix CLI (+ tests)
templates/app/       project template rendered by `phoenix create`
examples/            hello_phoenix (generated by the CLI), csharp_script (NativeAOT)
dev/                 CI helpers and Playwright browser e2e tests
docs/                architecture, ADRs, roadmap, debugging guide
```

## Testing

| Suite | Command | What it covers |
|---|---|---|
| Engine unit tests (doctest) | `cd engine && cmake --preset native-debug && cmake --build --preset native-debug && ctest --preset native-debug` | arena, VRAM allocator, job system, Jolt, widgets, storage, ABI |
| The same tests on wasm64 | `--preset wasm64-debug` (Node 24, real worker threads) | pthreads plus Memory64 behaviour |
| Sanitizers | `--preset native-asan` / `native-tsan` | memory errors, UB, data races |
| .NET | `dotnet test --solution phoenix.slnx` | CLI (manifest, create, CMake args, dev server) and the C# interop |
| C++ ↔ C# NativeAOT | `PHOENIX_TEST_SCRIPT_MODULE=… phoenix_engine_tests -ts="ScriptHost*"` | a real exported C# library driven from C++ |
| App widget tests | `phoenix test` | the template's counter test |
| Browser e2e (Playwright) | `cd dev/e2e && npm ci && npx playwright test` | boot, WebGPU or WebGL, VRAM pool, physics, taps, OPFS persistence, DevTools, the GitHub Pages service worker |

## Browser requirements

The web target needs **WebAssembly Memory64** and **SharedArrayBuffer**. That means Chrome or Edge 133+, or Firefox 134+. Safari does not support it yet.

WebGPU is used when the browser has it. Otherwise Phoenix falls back to WebGL2. Cross-origin isolation comes from the dev server's headers. On static hosts like GitHub Pages, `phoenix_sw.js` (a service worker) supplies it instead.

## Status

This is an MVP: the architecture and workflow are real and tested from end to end, but large parts are still placeholders. Read [docs/roadmap.md](docs/roadmap.md) for the honest list. The main gaps:

- Text uses a 5×7 bitmap font.
- Hot reload means hot restart.
- Desktop rendering goes through SDL_Renderer.
- C# does not run on the web yet.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md). Report security issues as described in [SECURITY.md](SECURITY.md).

Licensed under [Apache-2.0](LICENSE). Third-party components keep their own licenses: SDL (zlib), Jolt (MIT) and doctest (MIT).
