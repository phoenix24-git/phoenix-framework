// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Runtime.InteropServices;
using Phoenix;
using Phoenix.Interop;

namespace CSharpScript;

/// <summary>
/// Drops a coloured box every half second and draws a small HUD bar per body,
/// written straight into the engine's display list.
/// </summary>
public sealed class RainScript : MetaBehaviour
{
    private double _nextDrop;
    private uint _spawned;

    protected override void Start() => Engine.Log("RainScript (C#, NativeAOT) attached");

    protected override void Update()
    {
        var frame = Time.frame;
        if (frame.TimeSeconds >= _nextDrop && _spawned < 64)
        {
            _nextDrop = frame.TimeSeconds + 0.5;
            Engine.SpawnBox(-6f + (_spawned % 12), 14f, 0.3f, 0.3f, 0.2f * _spawned, true, 0x94E2D5FF);
            _spawned++;
        }

        // Zero-copy: read the physics snapshot and write HUD rects into engine memory.
        var bodies = Engine.Bodies;
        var rects = Engine.DrawRects(bodies.Length);
        for (var i = 0; i < rects.Length; i++)
        {
            rects[i] = new PhxDrawRect
            {
                X = 8 + i * 6,
                Y = frame.ViewportHeight - 12,
                Width = 4,
                Height = 4 + bodies[i].Y,
                Rgba = bodies[i].Rgba,
            };
        }
    }
}

/// <summary>The module's native entry point: the engine calls this C symbol first.</summary>
public static unsafe class Exports
{
    [UnmanagedCallersOnly(EntryPoint = "phoenix_script_init")]
    public static int Init(PhxEngineApi* api) => ScriptHost.Init<RainScript>(api);
}
