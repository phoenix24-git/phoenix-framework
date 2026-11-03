// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using Xunit;

namespace Skyline.Tests;

public sealed class StackRulesTests
{
    private static readonly Footprint Base = new(0, 0, 3, 3);

    [Fact]
    public void OverhangIsCutOffAndFalls()
    {
        var moving = Base with { X = 1 };
        var r = StackRules.Drop(Base, moving, SlideAxis.X, 0.1f);
        Assert.False(r.Missed);
        Assert.False(r.Perfect);
        Assert.Equal(new Footprint(0.5f, 0, 2, 3), r.Placed);   // the overlapping part stays
        Assert.Equal(new Footprint(2f, 0, 1, 3), r.CutOff);      // the 1-unit overhang, beyond the edge
    }

    [Fact]
    public void OverhangOnTheOtherSideAndOtherAxis()
    {
        var moving = Base with { Z = -0.5f };
        var r = StackRules.Drop(Base, moving, SlideAxis.Z, 0.1f);
        Assert.Equal(new Footprint(0, -0.25f, 3, 2.5f), r.Placed);
        Assert.Equal(new Footprint(0, -1.75f, 3, 0.5f), r.CutOff);
    }

    [Fact]
    public void NearMissesSnapPerfectly()
    {
        var r = StackRules.Drop(Base, Base with { X = 0.05f }, SlideAxis.X, 0.1f);
        Assert.True(r.Perfect);
        Assert.Equal(Base, r.Placed);
        Assert.Null(r.CutOff);
    }

    [Fact]
    public void MissingCompletelyEndsTheGame()
    {
        var r = StackRules.Drop(Base, Base with { X = 3.2f }, SlideAxis.X, 0.1f);
        Assert.True(r.Missed);
        Assert.Equal(Base with { X = 3.2f }, r.CutOff);
    }

    [Fact]
    public void PerfectStreaksGrowTheSlabBackUpToTheStartSize()
    {
        var small = new Footprint(0, 0, 2, 3);
        Assert.Equal(small, StackRules.Reward(small, SlideAxis.X, perfectStreak: 2, startSize: 3));
        Assert.Equal(2.2f, StackRules.Reward(small, SlideAxis.X, 3, 3).Width, 4);
        Assert.Equal(3f, StackRules.Reward(new Footprint(0, 0, 2.95f, 3), SlideAxis.X, 5, 3).Width, 4);
    }

    [Fact]
    public void SpeedRisesWithTheTowerThenLevelsOff()
    {
        Assert.Equal(2f, StackRules.SpeedFor(0, 2f));
        Assert.True(StackRules.SpeedFor(20, 2f) > StackRules.SpeedFor(10, 2f));
        Assert.Equal(StackRules.SpeedFor(40, 2f), StackRules.SpeedFor(100, 2f));
    }
}
