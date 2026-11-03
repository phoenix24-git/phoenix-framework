# 0004. Virtual console profile: fixed VRAM/CPU budgets reserved at boot

- Status: Accepted
- Date: 2026-11-03

## Context

Consoles give games fixed, known budgets, so out-of-memory problems show up at boot instead of
in the middle of a session. The requested profile is 3.5 GiB of VRAM and 6 cores.

## Decision

- `phoenix.json → console` declares `vramMB` (default 3584), `vramPreallocate` (default true),
  `workerThreads` (default 6) and `cpuArenaMB` (default 256).
- The renderer reserves VRAM as 256 MiB `SDL_GPUBuffer` pages, because WebGPU caps per-buffer size.
  All subsequent GPU data is sub-allocated from those pages.
- If a GPU refuses part of the reservation, the engine logs how much it obtained and continues with
  that. It does not crash, so the app stays usable on smaller devices.
- The `?vram=MB` / `--vram-mb` and `?workers=N` / `--workers` overrides exist for testing.

## Consequences

- Reserving 3.5 GiB up front is heavy on integrated GPUs. Apps targeting the open web may want a
  lower `vramMB`. The default keeps the console semantics.
