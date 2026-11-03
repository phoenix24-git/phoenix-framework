# Debugging Phoenix apps

## Web (`phoenix run -d chrome`)

`phoenix run` builds in **debug** mode by default:

- `-O0`, with Emscripten assertions enabled.
- A source map. Paths in it are rewritten so that `/__phoenix/app/...` maps to your app's sources
  and `/__phoenix/sdk/...` maps to the engine's.

Chrome starts with an isolated profile and `--remote-debugging-port`. To debug:

1. In the app tab, open **Chrome DevTools → Sources**. Your C++ appears under `/__phoenix/app/lib/`.
2. Set breakpoints in `main.cpp` and step through them. Engine sources are under `/__phoenix/sdk/engine/`.
3. To inspect C++ variables, install the **C/C++ DevTools Support (DWARF)** extension in that profile.

Press `v` in the terminal (or open the URL `phoenix run` prints) for **Phoenix DevTools**. It shows:

- FPS and frame time
- rigid body count
- worker threads and jobs per second
- the VRAM reservation
- frame-arena peak
- draw-rect count
- live engine logs from every thread

Useful URL parameters:

| Parameter | Effect |
|---|---|
| `?vram=512` | Use a 512 MiB VRAM budget instead of the default from phoenix.json |
| `?workers=2` | Use 2 worker threads |
| `?frames=600` | Quit after 600 frames (benchmarks) |
| `?SDL_...=...` | Set any SDL hint |

## Desktop (`phoenix run -d linux`)

The app runs under the tool and its logs are streamed to the terminal. To attach a native debugger,
use `gdb --args build/linux/<app>` or your IDE. These flags help:

| Flag | Effect |
|---|---|
| `--headless` | No window (CI smoke runs) |
| `--frames=N` | Quit after N frames |
| `--vram-mb=N` | Set the VRAM budget |
| `--workers=N` | Set the number of worker threads |
| `--script=path.so` | Load a C# script module (same as `PHOENIX_SCRIPT_MODULE`) |

## Engine developers

```bash
cd engine
cmake --preset native-asan && cmake --build --preset native-asan && ctest --preset native-asan
cmake --preset native-tsan && cmake --build --preset native-tsan && ctest --preset native-tsan
cmake --preset wasm64-debug && cmake --build --preset wasm64-debug && ctest --preset wasm64-debug
```
