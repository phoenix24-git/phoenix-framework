// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Runtime.InteropServices;
using Phoenix.Interop;
using Xunit;

namespace Phoenix.Framework.Tests;

/// <summary>
/// Drives Engine/ScriptHost through a fake engine whose function table points at
/// [UnmanagedCallersOnly] C# methods — the exact calling convention C++ uses.
/// </summary>
[Collection("engine")] // Engine is process-global state.
public sealed unsafe class EngineInteropTests : IDisposable
{
    private static PhxBody2D* s_bodies;
    private static int s_bodyCount;
    private static PhxDrawRect* s_rects;
    private static readonly List<string> Logs = [];
    private static uint s_spawned;

    private readonly PhxEngineApi* _api;

    public EngineInteropTests()
    {
        s_bodyCount = 0;
        s_spawned = 0;
        Logs.Clear();
        s_bodies = (PhxBody2D*)NativeMemory.AllocZeroed(16, (nuint)sizeof(PhxBody2D));
        s_rects = (PhxDrawRect*)NativeMemory.AllocZeroed(16, (nuint)sizeof(PhxDrawRect));
        _api = (PhxEngineApi*)NativeMemory.AllocZeroed((nuint)sizeof(PhxEngineApi));
        _api->AbiVersion = ScriptAbi.AbiVersion;
        _api->StructSize = (uint)sizeof(PhxEngineApi);
        _api->Log = &FakeLog;
        _api->SpawnBox = &FakeSpawn;
        _api->ApplyImpulse = &FakeImpulse;
        _api->Bodies = &FakeBodies;
        _api->DrawRects = &FakeDrawRects;
    }

    public void Dispose()
    {
        ScriptHost.Shutdown();
        NativeMemory.Free(s_bodies);
        NativeMemory.Free(s_rects);
        NativeMemory.Free(_api);
    }

    [UnmanagedCallersOnly]
    private static void FakeLog(void* engine, int level, byte* text) => Logs.Add(Marshal.PtrToStringUTF8((nint)text)!);

    [UnmanagedCallersOnly]
    private static uint FakeSpawn(void* engine, float x, float y, float hw, float hh, float angle, uint dynamic, uint rgba)
    {
        s_bodies[s_bodyCount] = new PhxBody2D { X = x, Y = y, HalfWidth = hw, HalfHeight = hh, Rgba = rgba, Id = s_spawned, Flags = dynamic };
        s_bodyCount++;
        return s_spawned++;
    }

    [UnmanagedCallersOnly]
    private static void FakeImpulse(void* engine, uint body, float ix, float iy) => s_bodies[body].X += ix;

    [UnmanagedCallersOnly]
    private static PhxBody2D* FakeBodies(void* engine, uint* count)
    {
        *count = (uint)s_bodyCount;
        return s_bodies;
    }

    [UnmanagedCallersOnly]
    private static PhxDrawRect* FakeDrawRects(void* engine, uint count) => count <= 16 ? s_rects : null;

    private sealed class SpawnOnStart : PhoenixScript
    {
        public int Updates { get; private set; }

        public override void OnStart() => Engine.SpawnBox(1, 2, 0.5f, 0.5f, rgba: 0x11223344);

        public override void OnUpdate(in PhxFrameInfo frame) => Updates++;
    }

    [Fact]
    public void InitStartsScriptAndSpansShareEngineMemory()
    {
        var script = new SpawnOnStart();
        Assert.Equal(0, ScriptHost.Init(_api, script));
        Assert.True(Engine.IsAttached);

        var bodies = Engine.Bodies;
        Assert.Equal(1, bodies.Length);
        Assert.Equal(0x11223344u, bodies[0].Rgba);
        Assert.True(bodies[0].IsDynamic);

        // Zero copy: the span aliases the engine's buffer.
        fixed (PhxBody2D* p = bodies)
        {
            Assert.True(p == s_bodies);
        }

        var rects = Engine.DrawRects(2);
        rects[1].Width = 42;
        Assert.Equal(42, s_rects[1].Width);
        Assert.True(Engine.DrawRects(1000).IsEmpty);

        var frame = new PhxFrameInfo { FrameIndex = 7 };
        ScriptHost.Update(&frame);
        Assert.Equal(1, script.Updates);
    }

    [Fact]
    public void LogEncodesUtf8()
    {
        Assert.Equal(0, ScriptHost.Init(_api, new SpawnOnStart()));
        Engine.Log("héllo 🔥");
        Assert.Contains("héllo 🔥", Logs);
    }

    [Fact]
    public void AbiMismatchIsRejected()
    {
        _api->AbiVersion = ScriptAbi.AbiVersion + 1;
        Assert.Equal(-1, ScriptHost.Init<SpawnOnStart>(_api));
        Assert.False(Engine.IsAttached);
        Assert.Equal(-1, ScriptHost.Init<SpawnOnStart>(null));
    }

    private sealed class Throwing : PhoenixScript
    {
        public override void OnUpdate(in PhxFrameInfo frame) => throw new InvalidOperationException("boom");
    }

    [Fact]
    public void ExceptionsNeverEscapeIntoNativeCode()
    {
        Assert.Equal(0, ScriptHost.Init(_api, new Throwing()));
        var frame = default(PhxFrameInfo);
        ScriptHost.Update(&frame);
        Assert.Contains(Logs, l => l.Contains("boom", StringComparison.Ordinal));
    }
}
