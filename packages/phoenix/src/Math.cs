// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Unity-style math value types. Their memory layout matches the engine's foundation types
// (Vector3, Quaternion, Color: 32-bit floats), so they can be read and written in place inside
// ECS component memory without conversion.
using System.Runtime.InteropServices;

namespace Phoenix;

#pragma warning disable CA1707, IDE1006, CA2225, CA1051 // Unity's API shape, on purpose.

/// <summary>A 3D vector (x, y, z). Phoenix space: right-handed, Y up, -Z forward.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct Vector3(float x, float y, float z) : IEquatable<Vector3>
{
    public float x = x, y = y, z = z;

    public static Vector3 zero => default;
    public static Vector3 one => new(1, 1, 1);
    public static Vector3 up => new(0, 1, 0);
    public static Vector3 right => new(1, 0, 0);
    public static Vector3 forward => new(0, 0, -1);

    public readonly float magnitude => MathF.Sqrt((x * x) + (y * y) + (z * z));

    public static Vector3 operator +(Vector3 a, Vector3 b) => new(a.x + b.x, a.y + b.y, a.z + b.z);
    public static Vector3 operator -(Vector3 a, Vector3 b) => new(a.x - b.x, a.y - b.y, a.z - b.z);
    public static Vector3 operator -(Vector3 a) => new(-a.x, -a.y, -a.z);
    public static Vector3 operator *(Vector3 a, float s) => new(a.x * s, a.y * s, a.z * s);
    public static Vector3 operator *(float s, Vector3 a) => a * s;
    public static Vector3 operator /(Vector3 a, float s) => new(a.x / s, a.y / s, a.z / s);
    public static bool operator ==(Vector3 a, Vector3 b) => a.Equals(b);
    public static bool operator !=(Vector3 a, Vector3 b) => !a.Equals(b);

    public static Vector3 Lerp(Vector3 a, Vector3 b, float t) => a + ((b - a) * Mathf.Clamp01(t));

    public readonly bool Equals(Vector3 other) => x == other.x && y == other.y && z == other.z;
    public override readonly bool Equals(object? obj) => obj is Vector3 v && Equals(v);
    public override readonly int GetHashCode() => HashCode.Combine(x, y, z);
    public override readonly string ToString() => $"({x:0.###}, {y:0.###}, {z:0.###})";
}

/// <summary>A rotation (x, y, z, w).</summary>
[StructLayout(LayoutKind.Sequential)]
public struct Quaternion(float x, float y, float z, float w) : IEquatable<Quaternion>
{
    public float x = x, y = y, z = z, w = w;

    public static Quaternion identity => new(0, 0, 0, 1);

    /// <summary>Rotation from Euler angles in degrees, applied Z, then X, then Y (Unity's order).</summary>
    public static Quaternion Euler(float x, float y, float z)
    {
        var d = MathF.PI / 360f; // half angle in radians per degree
        var (sx, cx) = MathF.SinCos(x * d);
        var (sy, cy) = MathF.SinCos(y * d);
        var (sz, cz) = MathF.SinCos(z * d);
        // q = qy * qx * qz
        return new Quaternion(
            (cy * sx * cz) + (sy * cx * sz),
            (sy * cx * cz) - (cy * sx * sz),
            (cy * cx * sz) - (sy * sx * cz),
            (cy * cx * cz) + (sy * sx * sz));
    }

    public static Quaternion Euler(Vector3 degrees) => Euler(degrees.x, degrees.y, degrees.z);

    public static Quaternion operator *(Quaternion a, Quaternion b) => new(
        (a.w * b.x) + (a.x * b.w) + (a.y * b.z) - (a.z * b.y),
        (a.w * b.y) + (a.y * b.w) + (a.z * b.x) - (a.x * b.z),
        (a.w * b.z) + (a.z * b.w) + (a.x * b.y) - (a.y * b.x),
        (a.w * b.w) - (a.x * b.x) - (a.y * b.y) - (a.z * b.z));

    public static bool operator ==(Quaternion a, Quaternion b) => a.Equals(b);
    public static bool operator !=(Quaternion a, Quaternion b) => !a.Equals(b);

    public readonly bool Equals(Quaternion other) => x == other.x && y == other.y && z == other.z && w == other.w;
    public override readonly bool Equals(object? obj) => obj is Quaternion q && Equals(q);
    public override readonly int GetHashCode() => HashCode.Combine(x, y, z, w);
}

/// <summary>An RGBA color with 0..1 channels.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct Color(float r, float g, float b, float a = 1f) : IEquatable<Color>
{
    public float r = r, g = g, b = b, a = a;

    public static Color white => new(1, 1, 1);
    public static Color black => new(0, 0, 0);

    /// <summary>Hue, saturation and value in 0..1 (hue wraps).</summary>
    public static Color HSVToRGB(float h, float s, float v)
    {
        h -= MathF.Floor(h);
        var i = (int)(h * 6f);
        var f = (h * 6f) - i;
        float p = v * (1 - s), q = v * (1 - (f * s)), t = v * (1 - ((1 - f) * s));
        return (i % 6) switch
        {
            0 => new Color(v, t, p),
            1 => new Color(q, v, p),
            2 => new Color(p, v, t),
            3 => new Color(p, q, v),
            4 => new Color(t, p, v),
            _ => new Color(v, p, q),
        };
    }

    public static Color Lerp(Color x, Color y, float t)
    {
        t = Mathf.Clamp01(t);
        return new Color(x.r + ((y.r - x.r) * t), x.g + ((y.g - x.g) * t), x.b + ((y.b - x.b) * t), x.a + ((y.a - x.a) * t));
    }

    /// <summary>0xRRGGBBAA, the engine's draw color format.</summary>
    public readonly uint ToRgba()
    {
        static uint C(float v) => (uint)((Math.Clamp(v, 0f, 1f) * 255f) + 0.5f);
        return (C(r) << 24) | (C(g) << 16) | (C(b) << 8) | C(a);
    }

    public static bool operator ==(Color x, Color y) => x.Equals(y);
    public static bool operator !=(Color x, Color y) => !x.Equals(y);

    public readonly bool Equals(Color other) => r == other.r && g == other.g && b == other.b && a == other.a;
    public override readonly bool Equals(object? obj) => obj is Color c && Equals(c);
    public override readonly int GetHashCode() => HashCode.Combine(r, g, b, a);
}

/// <summary>Unity's Mathf helpers.</summary>
public static class Mathf
{
    public static float Clamp01(float v) => Math.Clamp(v, 0f, 1f);
    public static float Lerp(float a, float b, float t) => a + ((b - a) * Clamp01(t));
    public static float PingPong(float t, float length)
    {
        var m = t % (2 * length);
        if (m < 0)
        {
            m += 2 * length;
        }

        return length - MathF.Abs(m - length);
    }
}
#pragma warning restore CA1707, IDE1006, CA2225, CA1051
