// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// C# mirror of engine/include/phoenix/script_abi.h (ABI v1). Layouts are
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

/// <summary>One rectangle in the frame display list (32 bytes, logical pixels).</summary>
[StructLayout(LayoutKind.Sequential)]
public struct PhxDrawRect
{
    public float X, Y, Width, Height;
    public uint Rgba;
    public float Rotation;
    public float CornerRadius;
    public uint Reserved;
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
}

public static class ScriptAbi
{
    public const uint AbiVersion = 1;
    public const int LogInfo = 0;
    public const int LogWarn = 1;
    public const int LogError = 2;
}
