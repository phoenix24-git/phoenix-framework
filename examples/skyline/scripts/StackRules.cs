// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Skyline's rules, with no engine dependency (unit-tested in tests/).
namespace Skyline;

/// <summary>Which horizontal axis the moving slab slides along.</summary>
public enum SlideAxis
{
    X,
    Z,
}

/// <summary>A slab's footprint on the XZ plane: center and size.</summary>
public readonly record struct Footprint(float X, float Z, float Width, float Depth)
{
    public float Center(SlideAxis axis) => axis == SlideAxis.X ? X : Z;

    public float Size(SlideAxis axis) => axis == SlideAxis.X ? Width : Depth;

    public Footprint With(SlideAxis axis, float center, float size) =>
        axis == SlideAxis.X ? this with { X = center, Width = size } : this with { Z = center, Depth = size };
}

/// <summary>What happened when a slab was dropped.</summary>
public readonly record struct DropResult(bool Missed, bool Perfect, Footprint Placed, Footprint? CutOff);

public static class StackRules
{
    /// <summary>
    /// Drops <paramref name="moving"/> onto <paramref name="below"/>: whatever hangs over the
    /// edge along <paramref name="axis"/> is cut off. Within <paramref name="perfectTolerance"/>
    /// the slab snaps onto the one below and nothing is cut.
    /// </summary>
    public static DropResult Drop(Footprint below, Footprint moving, SlideAxis axis, float perfectTolerance)
    {
        var delta = moving.Center(axis) - below.Center(axis);
        var size = moving.Size(axis);
        if (MathF.Abs(delta) <= perfectTolerance)
        {
            return new DropResult(false, true, moving.With(axis, below.Center(axis), size), null);
        }

        var overlap = size - MathF.Abs(delta);
        if (overlap <= 0)
        {
            return new DropResult(true, false, default, moving); // the whole slab falls
        }

        var sign = MathF.Sign(delta);
        var placed = moving.With(axis, below.Center(axis) + (delta / 2), overlap);
        var cutCenter = placed.Center(axis) + (sign * ((overlap / 2) + (MathF.Abs(delta) / 2)));
        return new DropResult(false, false, placed, moving.With(axis, cutCenter, MathF.Abs(delta)));
    }

    /// <summary>After a streak of perfect drops the slab grows back a little, up to its start size.</summary>
    public static Footprint Reward(Footprint placed, SlideAxis axis, int perfectStreak, float startSize)
    {
        if (perfectStreak < 3)
        {
            return placed;
        }

        var size = MathF.Min(placed.Size(axis) * 1.1f, startSize);
        return placed.With(axis, placed.Center(axis), size);
    }

    /// <summary>Slide speed for the n-th slab: a little faster as the tower grows.</summary>
    public static float SpeedFor(int level, float baseSpeed) => baseSpeed * (1f + MathF.Min(level, 40) * 0.015f);
}
