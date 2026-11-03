---
title: Getting started
nav_order: 2
---

# Getting started
{: .no_toc }

1. TOC
{:toc}

## Install

Phoenix is used from a git checkout, like the Flutter SDK.

```bash
git clone https://github.com/phoenix24-git/phoenix-framework.git
export PATH="$PWD/phoenix-framework/bin:$PATH"     # Windows: add phoenix-framework\bin to PATH
```

Requirements:

| Tool | Why |
|---|---|
| .NET SDK 10 | runs the `phoenix` tool (rebuilt automatically when it changes) |
| CMake 3.25+ and Ninja | builds the engine and your app |
| Emscripten (pinned) | web builds — install with `phoenix precache --web` |
| Chrome or Edge 133+ | `phoenix run -d chrome` |
| Linux only | SDL3 system headers: run `dev/ci/install_linux_deps.sh` |

Check everything with:

```bash
phoenix doctor
```

## Create an app

```bash
phoenix create my_app
cd my_app
```

```
my_app/
  phoenix.json          # name, version, console profile (like pubspec.yaml)
  lib/main.cpp          # your app — phoenix_main() calls runApp(...)
  test/widget_test.cpp  # widget tests, run with `phoenix test`
  web/index.html        # HTML shell; loads phoenix_bootstrap.js
```

## Run it

```bash
phoenix run -d chrome      # or -d web-server, or -d linux
```

`phoenix run` builds in debug mode, starts a dev server with the cross-origin isolation headers
wasm threads need, opens Chrome, and stays resident:

| Key | Action |
|---|---|
| `r` / `R` | hot restart (incremental rebuild + reload) |
| `v` | open Phoenix DevTools |
| `h` | help |
| `q` | quit |

## Edit, test, ship

```bash
phoenix test                         # widget tests (headless, no GPU)
phoenix build web --release          # optimized bundle in build/web
phoenix build web --release -o ../site/app   # …and publish it to another folder
```

Next: the [widget guide](widgets.md), or how to [host on the web](web.md).
