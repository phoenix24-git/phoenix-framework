// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Text;
using Phoenix.Interop;

namespace Phoenix;

/// <summary>
/// Engine services for C# scripts. All data is shared, not copied: <see cref="Bodies"/>
/// is a span over the engine's physics snapshot and <see cref="DrawRects"/> hands out
/// engine-owned frame memory to write into directly.
/// </summary>
public static unsafe class Engine
{
    private static PhxEngineApi* s_api;

    /// <summary>True once the engine has called phoenix_script_init.</summary>
    public static bool IsAttached => s_api is not null;

    internal static void Attach(PhxEngineApi* api) => s_api = api;

    internal static void Detach() => s_api = null;

    internal static PhxEngineApi* Api => s_api is not null ? s_api : throw new InvalidOperationException("Phoenix engine is not attached.");

    /// <summary>Spawns a box (meters, y up). The id is valid immediately.</summary>
    public static uint SpawnBox(float x, float y, float halfWidth, float halfHeight, float angle = 0, bool dynamic = true,
        uint rgba = 0x4F8EF7FF) =>
        Api->SpawnBox(Api->Engine, x, y, halfWidth, halfHeight, angle, dynamic ? 1u : 0u, rgba);

    public static void ApplyImpulse(uint body, float ix, float iy) => Api->ApplyImpulse(Api->Engine, body, ix, iy);

    /// <summary>Zero-copy view of the latest physics snapshot. Valid until the next frame.</summary>
    public static ReadOnlySpan<PhxBody2D> Bodies
    {
        get
        {
            uint count;
            var data = Api->Bodies(Api->Engine, &count);
            return new ReadOnlySpan<PhxBody2D>(data, (int)count);
        }
    }

    /// <summary>
    /// Reserves <paramref name="count"/> rectangles in this frame's display list and
    /// returns them as a writable span over engine memory (empty if the frame arena is full).
    /// </summary>
    public static Span<PhxDrawRect> DrawRects(int count)
    {
        var data = Api->DrawRects(Api->Engine, (uint)count);
        return data is null ? Span<PhxDrawRect>.Empty : new Span<PhxDrawRect>(data, count);
    }

    /// <summary>Logs through the engine (UTF-8 encoded on the stack; no heap allocation for short messages).</summary>
    public static void Log(string message, int level = ScriptAbi.LogInfo)
    {
        var max = Encoding.UTF8.GetMaxByteCount(message.Length) + 1;
        var buffer = max <= 512 ? stackalloc byte[max] : new byte[max];
        var written = Encoding.UTF8.GetBytes(message, buffer);
        buffer[written] = 0;
        fixed (byte* p = buffer)
        {
            Api->Log(Api->Engine, level, p);
        }
    }
}
