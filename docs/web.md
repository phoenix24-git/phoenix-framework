---
title: Web target & hosting
nav_order: 6
---

# Web target & hosting
{: .no_toc }

1. TOC
{:toc}

## What the browser runs

| Piece | Detail |
|---|---|
| Code | one **wasm64** (Memory64) module, built with Emscripten (pinned version) |
| Threads | 6 engine workers + an IO thread, each a pthread backed by a Web Worker |
| Memory | one shared linear memory: 1 GiB initially, growable to 16 GiB (beyond wasm32's 4 GiB) |
| GPU | SDL_GPU on **WebGPU** (WGSL); falls back to WebGL2 when no adapter exists |
| VRAM | the console budget (3.5 GiB default) reserved at boot as 256 MiB buffers |
| Storage | **OPFS** through WasmFS, mounted at `/persistent` by the IO thread |

## Browser support

Memory64 and SharedArrayBuffer are required: **Chrome/Edge 133+**, **Firefox 134+**. Safari is not
supported yet. WebGPU is optional.

## Cross-origin isolation

Threads need `SharedArrayBuffer`, which browsers only expose to cross-origin isolated pages:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

- `phoenix run` sends these headers.
- Your own server/CDN: add them (and serve `.wasm` as `application/wasm`).
- **Static hosts that cannot set headers (GitHub Pages)**: `phoenix_bootstrap.js` registers
  `phoenix_sw.js`, a service worker that re-serves the site with the headers, and reloads once.

## Deploying to GitHub Pages

This repository's own site is the example: Pages serves the `docs/` folder of `main`, and the demo
lives in `docs/demo`.

```bash
cd examples/hello_phoenix
phoenix build web --release -o ../../docs/demo
git add ../../docs/demo && git commit -m "chore(demo): rebuild" && git push
```

Here the `Web demo` workflow does that automatically whenever engine, framework or example sources
change on `main`. Everything in `build/web` uses relative URLs, so the bundle works from any
sub-path (`/phoenix-framework/demo/`).

## Tuning per page load

| URL parameter | Effect |
|---|---|
| `?vram=512` | smaller VRAM budget (MiB) for small GPUs |
| `?workers=2` | fewer worker threads |
| `?frames=600` | quit after N frames |

Try it: [demo/?vram=512](demo/?vram=512).
