// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Phoenix.Interop;
using Xunit;

namespace Phoenix.Framework.Tests;

/// <summary>Mirrors engine/test/script_abi_test.cpp: both sides must agree byte for byte.</summary>
public sealed class ScriptAbiTests
{
    [Fact]
    public void StructSizesMatchTheCAbi()
    {
        Assert.Equal(1u, ScriptAbi.AbiVersion);
        Assert.Equal(32, Unsafe.SizeOf<PhxBody2D>());
        Assert.Equal(32, Unsafe.SizeOf<PhxDrawRect>());
        Assert.Equal(48, Unsafe.SizeOf<PhxFrameInfo>());
        Assert.Equal(8 + (6 * IntPtr.Size), Unsafe.SizeOf<PhxEngineApi>());
    }

    [Theory]
    [InlineData(typeof(PhxBody2D), nameof(PhxBody2D.Id), 20)]
    [InlineData(typeof(PhxBody2D), nameof(PhxBody2D.Rgba), 24)]
    [InlineData(typeof(PhxDrawRect), nameof(PhxDrawRect.Rgba), 16)]
    [InlineData(typeof(PhxDrawRect), nameof(PhxDrawRect.Rotation), 20)]
    [InlineData(typeof(PhxFrameInfo), nameof(PhxFrameInfo.ViewportWidth), 16)]
    [InlineData(typeof(PhxFrameInfo), nameof(PhxFrameInfo.PointerButtons), 32)]
    [InlineData(typeof(PhxEngineApi), nameof(PhxEngineApi.Engine), 8)]
    public void FieldOffsetsMatchTheCAbi(Type type, string field, int offset) =>
        Assert.Equal(offset, (int)Marshal.OffsetOf(type, field));

    [Fact]
    public void AbiTypesAreBlittable()
    {
        Assert.False(RuntimeHelpers.IsReferenceOrContainsReferences<PhxBody2D>());
        Assert.False(RuntimeHelpers.IsReferenceOrContainsReferences<PhxDrawRect>());
        Assert.False(RuntimeHelpers.IsReferenceOrContainsReferences<PhxFrameInfo>());
        Assert.False(RuntimeHelpers.IsReferenceOrContainsReferences<PhxEngineApi>());
    }

    [Fact]
    public void RuntimeMarshallingIsDisabled() =>
        Assert.True(typeof(Engine).Assembly.GetCustomAttributes(typeof(DisableRuntimeMarshallingAttribute), false).Length == 1);
}
