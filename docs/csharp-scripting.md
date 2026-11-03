---
title: C# scripting
nav_order: 5
---

# C# scripting (NativeAOT, zero overhead)
{: .no_toc }

Gameplay scripts can be written in C#. They compile with **NativeAOT** to a native shared library
that the C++ engine loads and calls like C — there is no CLR host, no reflection and no marshalling.

{: .important }
C# scripts currently run on **desktop**. .NET 10 cannot emit wasm64 NativeAOT code, so web apps use
C++ for now. See the [roadmap](roadmap.md) for the .NET 11 plan.

1. TOC
{:toc}

## The three rules

1. **No runtime marshalling.** `Phoenix.Framework` sets `[assembly: DisableRuntimeMarshalling]`;
   a non-blittable signature is a compile error.
2. **Raw memory, not copies.** The physics snapshot is a `ReadOnlySpan<PhxBody2D>` over engine
   memory; `Engine.DrawRects(n)` returns a `Span<PhxDrawRect>` inside the engine's frame arena.
3. **Native exports.** `[UnmanagedCallersOnly(EntryPoint = "...")]` methods become C symbols.
   Engine services are `delegate* unmanaged<...>` function pointers in a versioned C struct.

## A script

```csharp
using System.Runtime.InteropServices;
using Phoenix;
using Phoenix.Interop;

public sealed class RainScript : MetaBehaviour
{
    private double _nextDrop;

    protected override void Start() => Engine.Log("RainScript attached");

    protected override void Update()   // Unity lifecycle names; Time.deltaTime etc. available
    {
        if (Time.time >= _nextDrop)
        {
            _nextDrop = Time.time + 0.5;
            Engine.SpawnBox(0, 14, 0.3f, 0.3f);
        }
        var bodies = Engine.Bodies;                  // span over engine memory
        var rects = Engine.DrawRects(bodies.Length); // span into this frame's display list
        for (var i = 0; i < rects.Length; i++)
        {
            rects[i] = new PhxDrawRect { X = 8 + i * 6, Y = Time.frame.ViewportHeight - 12,
                                         Width = 4, Height = 4, Rgba = bodies[i].Rgba };
        }
    }
}

// The one export a script module writes itself: it names the script type (AOT-safe).
public static unsafe class Exports
{
    [UnmanagedCallersOnly(EntryPoint = "phoenix_script_init")]
    public static int Init(PhxEngineApi* api) => ScriptHost.Init<RainScript>(api);
}
```

## Project file

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>
    <PublishAot>true</PublishAot>
    <NativeLib>Shared</NativeLib>
  </PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="path/to/packages/phoenix/src/Phoenix.Framework.csproj" />
    <!-- exports phoenix_script_update / phoenix_script_shutdown from the framework -->
    <UnmanagedEntryPointsAssembly Include="Phoenix.Framework" />
  </ItemGroup>
</Project>
```

## Build and run

```bash
dotnet publish -c Release -r linux-x64 -o out      # -> out/MyScript.so (.dll / .dylib)
PHOENIX_SCRIPT_MODULE=$PWD/out/MyScript.so phoenix run -d linux
# or: build/linux/my_app --script=out/MyScript.so
```

## The ABI

Defined in [`script_abi.h`](https://github.com/phoenix24-git/phoenix-framework/blob/main/engine/include/phoenix/script_abi.h)
and mirrored in [`ScriptAbi.cs`](https://github.com/phoenix24-git/phoenix-framework/blob/main/packages/phoenix/src/Interop/ScriptAbi.cs).
Both sides assert sizes and offsets in tests; the C# side refuses a table whose `abi_version`
differs. Exceptions thrown by scripts are caught and logged — they never unwind into C++.

| Struct | Size | Purpose |
|---|---|---|
| `PhxBody2D` | 32 B | one rigid body in the physics snapshot |
| `PhxDrawRect` | 32 B | one display-list rectangle (also the GPU instance format) |
| `PhxFrameInfo` | 48 B | time, viewport, pointer |
| `PhxEngineApi` | 8 + 6 pointers | engine service table |
