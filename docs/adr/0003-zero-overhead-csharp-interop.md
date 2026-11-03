---
title: 0003 C# interop
parent: Decisions (ADRs)
nav_order: 3
---

# 0003. Zero-overhead C# interop via NativeAOT and a C Script ABI

- Status: Accepted
- Date: 2026-11-03

## Context

C# should be usable for gameplay scripts without paying for CLR hosting, reflection, or
marshalling on every engine call.

## Decision

1. `[assembly: DisableRuntimeMarshalling]` in `Phoenix.Framework`, so only blittable types can cross
   the boundary, and anything else fails to compile.
2. Data is shared by address: `ReadOnlySpan<PhxBody2D>` over the physics snapshot, and a
   `Span<PhxDrawRect>` over frame-arena memory the engine reserved.
3. Scripts are published with NativeAOT as shared libraries. `[UnmanagedCallersOnly(EntryPoint=...)]`
   methods become C exports that the engine loads with `SDL_LoadObject`. Engine services are a
   versioned C struct of function pointers (`delegate* unmanaged<...>`).
4. Script modules export `phoenix_script_init` themselves (one line naming the script type, so it is
   AOT-safe and needs no module initializers). `phoenix_script_update`/`_shutdown` are exported from
   `Phoenix.Framework` through `<UnmanagedEntryPointsAssembly>`.
5. Exceptions never cross into C++. They are caught and logged on the C# side.

## Consequences

- Layout drift is caught by mirrored tests (`script_abi_test.cpp`, `ScriptAbiTests.cs`), and CI loads
  a real NativeAOT library from C++.
- **Web:** .NET 10 has no wasm64 NativeAOT target, so C# scripts are desktop-only for now. The plan
  is to track NativeAOT-LLVM / .NET 11 WebAssembly work and link C# into the same wasm64 module
  (see the roadmap).
