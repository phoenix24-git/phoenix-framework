---
title: CLI reference
nav_order: 3
---

# `phoenix` CLI reference
{: .no_toc }

The `phoenix` tool mirrors `flutter`. Add `-v` to any command to see every command it runs
(CMake, emcc, Chrome).

1. TOC
{:toc}

## phoenix create

```bash
phoenix create <output directory> [--project-name name] [--description text] [--title text] [--overwrite]
```

Renders the app template. The directory name becomes the project name unless `--project-name` is
given; names are `lower_snake_case`.

## phoenix run

```bash
phoenix run [-d chrome|web-server|linux|macos|windows] [--debug|--profile|--release]
            [--web-port 8080] [--web-hostname 127.0.0.1] [--no-resident]
```

| Device | Behaviour |
|---|---|
| `chrome` (default when found) | dev server + Chrome with an isolated profile and `--remote-debugging-port` |
| `web-server` | dev server only; open the printed URL in any Memory64 browser |
| `linux` / `macos` / `windows` | native build of the host desktop, logs streamed to the terminal |

The dev server sends `Cross-Origin-Opener-Policy: same-origin` and
`Cross-Origin-Embedder-Policy: require-corp`, serves `application/wasm`, pushes hot-restart events
on `/__phoenix/events` (Server-Sent Events) and hosts DevTools at `/__phoenix/devtools/`. In debug
mode it also serves C++ sources (only C/C++ files) for the source map. Set `CHROME_EXECUTABLE` to
choose the browser.

## phoenix build

```bash
phoenix build web [--debug|--profile|--release] [-o <dir>]
phoenix build linux|macos|windows [--debug|--profile|--release]
```

| Mode | Web flags | Use |
|---|---|---|
| `--debug` | `-O0`, assertions, source maps | stepping through C++ |
| `--profile` | `-O2`, function names | profiling |
| `--release` (default) | `-O3` | shipping |

Output goes to `build/web` (or `build/<os>`). `-o/--output` additionally publishes the web bundle
to a folder — for example the repository's `docs/demo` for GitHub Pages. The target folder is
replaced only if it is empty or contains a previous Phoenix bundle (`version.json`).

## phoenix test

```bash
phoenix test [--name "pattern"]
```

Compiles `test/*.cpp` natively with doctest and the framework, then runs them. See
[Testing](testing.md).

## phoenix doctor · devices · clean · precache

| Command | Does |
|---|---|
| `phoenix doctor` | checks .NET, CMake, Ninja, the pinned Emscripten version and Chrome |
| `phoenix devices` | lists Chrome, Web Server and the host desktop |
| `phoenix clean` | deletes `build/` and `.phoenix_tool/` |
| `phoenix precache --web` | installs the pinned Emscripten SDK into `bin/cache/emsdk` |

## phoenix.json

```json
{
  "name": "my_app",
  "description": "A new Phoenix project.",
  "version": "1.0.0+1",
  "title": "Phoenix Demo Home Page",
  "console": { "vramMB": 3584, "vramPreallocate": true, "workerThreads": 6, "cpuArenaMB": 256 },
  "web": { "initialMemoryMB": 1024, "maximumMemoryMB": 16384 }
}
```

Comments and trailing commas are allowed. Omitted fields use the defaults shown.
