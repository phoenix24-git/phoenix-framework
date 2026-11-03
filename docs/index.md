---
title: Home
layout: home
nav_order: 1
description: Phoenix — Flutter's developer workflow on a native C++ engine.
permalink: /
---

# Phoenix
{: .fs-9 }

Flutter's developer workflow — `create`, `run`, hot restart, `build`, `test` — on a native C++
engine: **SDL3** + **WebGPU** (via SDL_wgpu) + **Jolt Physics**, shipping to the web as **wasm64 with
real threads**, with optional **zero-overhead C# scripting**.
{: .fs-5 .fw-300 }

[Try the live demo](demo/){: .btn .btn-primary .fs-5 .mb-4 .mb-md-0 .mr-2 }
[Get started](getting-started.md){: .btn .fs-5 .mb-4 .mb-md-0 .mr-2 }
[GitHub](https://github.com/phoenix24-git/phoenix-framework){: .btn .fs-5 .mb-4 .mb-md-0 }

---

## The live demo

The [demo](demo/) is the app `phoenix create` generates: Flutter's counter, plus a Jolt physics
world. Press **+** or tap the dark area to drop boxes; the counter is saved to the browser's Origin
Private File System and survives a reload. Open [DevTools](demo/devtools/) in a second tab to watch
FPS, worker threads, the VRAM reservation and live engine logs.

{: .note }
The demo needs **WebAssembly Memory64**: Chrome/Edge 133+ or Firefox 134+. The first visit reloads
once to enable cross-origin isolation (a service worker provides it on GitHub Pages).

## What's inside

| Area | What you get |
|---|---|
| **Workflow** | `phoenix create / run / build / test / doctor / devices / clean / precache`, mirroring the Flutter CLI ([reference](cli.md)) |
| **UI** | Flutter-style widgets in C++20 — `Scaffold`, `AppBar`, `Column`, `Text`, `setState`, `WidgetTester` ([guide](widgets.md)) |
| **Engine** | SDL3 platform layer, SDL_GPU on WebGPU, Jolt physics on a 6-thread SDL job system ([architecture](architecture.md)) |
| **Virtual console** | 3.5 GiB VRAM and 6 cores reserved at boot, configured in `phoenix.json` |
| **Web** | wasm64 + pthreads (Web Workers on one shared memory), OPFS storage, WebGL fallback ([web target](web.md)) |
| **C#** | NativeAOT scripts, runtime marshalling disabled, spans over engine memory ([C# scripting](csharp-scripting.md)) |

## Sixty-second tour

```bash
git clone https://github.com/phoenix24-git/phoenix-framework.git
export PATH="$PWD/phoenix-framework/bin:$PATH"
phoenix doctor
phoenix create my_app && cd my_app
phoenix run -d chrome        # press r to hot restart, v for DevTools
```
