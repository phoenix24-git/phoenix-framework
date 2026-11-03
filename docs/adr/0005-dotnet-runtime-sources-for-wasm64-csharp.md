---
title: 0005 .NET sources for wasm64 C#
parent: Decisions (ADRs)
nav_order: 5
---

# 0005. Pinned .NET runtime sources for wasm64 C# scripts

- Status: Accepted
- Date: 2026-11-03

## Context

C# scripts are NativeAOT shared libraries called through the C Script ABI ([ADR 0003](0003-zero-overhead-csharp-interop.md)).
They only run on desktop, because no shipping .NET can compile C# for the engine's web target:
**wasm64 (Memory64) + pthreads, linked by Emscripten into one module** ([ADR 0002](0002-wasm64-pthreads-web-target.md)).

We checked three candidate code bases at the commits pinned in `bin/internal/dotnet-runtime.lock`:

| Source | Runtime | wasm64 status (checked in source) | Fits Phoenix? |
|---|---|---|---|
| `dotnet/runtime` `main` | NativeAOT (RyuJIT) | WebAssembly output is **WASI only**, `wasm32-wasip2`. `Microsoft.NETCore.Native.Wasm.targets` rejects `browser`, `NativeLib=Shared` and multi-module builds. There is no wasm64. | No. It needs the browser (Emscripten), wasm64 and static linking into our module. |
| `dotnet/runtimelab` `feature/NativeAOT-LLVM` | NativeAOT with LLVM codegen | Produces `browser-wasm` and `wasi-wasm` with Emscripten, wasm32 only. The compiler already has a `TargetArchitecture.Wasm64` path (LLVM codegen, `llvm64` JIT config, instruction sets), but `eng/OSArch.props` says *"Will include wasm64 once we support it"*. The runtime, CoreLib native parts and build integration are not built for wasm64. | **Closest match.** It is the same NativeAOT model as desktop, uses Emscripten, and links as static objects. |
| `vulkan101/runtime` `wasm64_addressSpaceLimitationFix` ([dotnet/runtime#94108](https://github.com/dotnet/runtime/issues/94108), linked from there) | **Mono** interpreter | Working wasm64 Memory64 proof of concept on Emscripten 4.0.5. The Jiterpreter is stubbed out, ICU is removed and some values are hardcoded. It is an unreviewed community fork. | No as a dependency. It is useful as a reference for the Emscripten/Memory64 pitfalls it fixes in shared native code (`System.Native`, pointer-size assumptions, ICU, Brotli). |

Issue [dotnet/runtime#94108](https://github.com/dotnet/runtime/issues/94108) is about Mono. It is open with milestone *Future*, and no maintainer has committed to a date. It does not cover NativeAOT.

## Decision

1. Do **not** vendor or submodule any .NET runtime. The repositories are multi-GB and slow to build,
   and a submodule would make every clone of Phoenix pay for them.
2. Pin the sources in **`bin/internal/dotnet-runtime.lock`**, in the same way `emscripten.version` pins
   Emscripten. Fetch them on demand with **`dev/tools/dotnet_runtime.sh`**:
   - it makes a blobless, sparse clone into `bin/cache/dotnet-runtime/<name>` (git-ignored);
   - it checks out the exact pinned commit and verifies it;
   - `--full` gives a full checkout when you need to build;
   - `status` shows pins that have drifted, and `bump` moves a pin to its branch tip so the move is
     reviewed in a PR.
3. Target **NativeAOT-LLVM** for wasm64 work, not Mono. It keeps one execution model for C# on every
   platform: AOT code, a C ABI, no marshalling, and one linear memory shared with the engine.
4. Keep any Phoenix-side changes to the runtime as a **patch series** under
   `engine/thirdparty/dotnet-runtime-patches/` (to be added when the first patch exists). Apply it on
   top of the pinned commit, and send it upstream to runtimelab where it is accepted.

## Path to wasm64 C# on the web

Work list against `nativeaot-llvm`, in order:

1. Add `wasm64` to `eng/OSArch.props` / `TargetsWasm`, and add a `browser-wasm64` RID.
2. Build the NativeAOT runtime (`src/coreclr/nativeaot`) and the `System.*.Native` libs with
   `emcc -sMEMORY64=1 -pthread` at the Emscripten version in `bin/internal/emscripten.version`.
3. Emit the ILC output for `Wasm64` (`--targetarch:wasm64`), and fix pointer-size assumptions in the
   LLVM codegen and in the shadow-stack and exception-handling helpers.
4. Link the script's object files and static libraries into the engine module through
   `PhoenixApp.cmake`. Do not load a side module. The Script ABI stays the same.
5. Add a CI job that runs `ScriptHost*` tests under Node 24 with a wasm64 C# module.

Once upstream ships `browser-wasm64` NativeAOT (watch runtimelab and .NET 11 previews), drop the
patch series. Then replace the pins with the official ILCompiler package.

## Consequences

- A Phoenix clone stays small. Contributors working on the runtime run one command and get a
  reproducible tree.
- Pins can go stale, so `bump` is a deliberate, reviewed change.
- Web apps keep using C++ for gameplay until step 5 above passes in CI.
