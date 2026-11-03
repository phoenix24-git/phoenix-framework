// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// C# mirror of engine/include/phoenix/script_abi.h (ABI v2). Layouts are
// asserted by tests on both sides; any change must bump AbiVersion.
using System.Runtime.InteropServices;

namespace Phoenix.Interop;

/// <summary>One rigid body in the engine's physics snapshot (32 bytes).</summary>
[StructLayout(LayoutKind.Sequential)]
public struct PhxBody2D
{
    public float X, Y;
    public float Angle;
    public float HalfWidth, HalfHeight;
    public uint Id;
    public uint Rgba;
    public uint Flags;

    public readonly bool IsDynamic => (Flags & 1u) != 0;
}

/// <summary>
/// One filled shape in the frame display list (32 bytes, logical pixels, y down): a rotated
/// rectangle, or with <see cref="ScriptAbi.DrawParallelogram"/> a parallelogram (corner
/// <c>(X, Y)</c>, edges <c>(Width, Height)</c> and <c>(Rotation, CornerRadius)</c>).
/// Use <see cref="Rect"/> and <see cref="Parallelogram"/> rather than setting fields by hand.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct PhxDrawRect
{
    public float X, Y, Width, Height;
    public uint Rgba;
    public float Rotation;
    public float CornerRadius;
    public uint Flags;

    /// <summary>An axis-aligned rectangle, optionally rotated around its center.</summary>
    public static PhxDrawRect Rect(float x, float y, float width, float height, uint rgba, float rotation = 0) =>
        new() { X = x, Y = y, Width = width, Height = height, Rgba = rgba, Rotation = rotation };

    /// <summary>The parallelogram with corner <c>o</c> and edges <c>a</c>, <c>b</c>: corners o, o+a, o+a+b, o+b.</summary>
    public static PhxDrawRect Parallelogram(float ox, float oy, float ax, float ay, float bx, float by, uint rgba) =>
        new()
        {
            X = ox,
            Y = oy,
            Width = ax,
            Height = ay,
            Rotation = bx,
            CornerRadius = by,
            Rgba = rgba,
            Flags = ScriptAbi.DrawParallelogram,
        };
}

/// <summary>Per-frame timing and input (48 bytes).</summary>
[StructLayout(LayoutKind.Sequential)]
public struct PhxFrameInfo
{
    public double TimeSeconds;
    public float DeltaSeconds;
    public uint FrameIndex;
    public float ViewportWidth, ViewportHeight;
    public float PointerX, PointerY;
    public uint PointerButtons;
    public uint BodyCount;
    public ulong Reserved;
}

/// <summary>
/// The engine service table. Function pointers are raw C pointers
/// (<c>delegate* unmanaged</c>): calling one is a direct native call with no
/// marshalling stub, no GC transition beyond the mandatory one, no allocation.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct PhxEngineApi
{
    public uint AbiVersion;
    public uint StructSize;
    public void* Engine;
    public delegate* unmanaged<void*, int, byte*, void> Log;
    public delegate* unmanaged<void*, float, float, float, float, float, uint, uint, uint> SpawnBox;
    public delegate* unmanaged<void*, uint, float, float, void> ApplyImpulse;
    public delegate* unmanaged<void*, uint*, PhxBody2D*> Bodies;
    public delegate* unmanaged<void*, uint, PhxDrawRect*> DrawRects;

    // Scene (GameObjects): handles are ulong, 0 = none. Strings are NUL-terminated UTF-8.
    public delegate* unmanaged<void*, byte*, ulong> FindObject;
    public delegate* unmanaged<void*, byte*, ulong, ulong> CreateObject;
    public delegate* unmanaged<void*, ulong, void> DestroyObject;
    public delegate* unmanaged<void*, ulong, uint> ObjectAlive;
    public delegate* unmanaged<void*, ulong, uint, void> SetActive;
    public delegate* unmanaged<void*, ulong, byte*, uint, void*> GetComponent;
    public delegate* unmanaged<void*, ulong, byte*, byte*, uint*, void*> GetField;
}

public static class ScriptAbi
{
    public const uint AbiVersion = 2;

    /// <summary><see cref="PhxDrawRect.Flags"/>: draw a parallelogram instead of a rectangle.</summary>
    public const uint DrawParallelogram = 1;
    public const int LogInfo = 0;
    public const int LogWarn = 1;
    public const int LogError = 2;
}
