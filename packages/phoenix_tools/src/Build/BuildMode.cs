// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
namespace Phoenix.Tools.Build;

/// <summary>Flutter's three build modes.</summary>
public enum BuildMode
{
    /// <summary>Unoptimized, assertions on, source maps for the debugger.</summary>
    Debug,

    /// <summary>Optimized with symbol names for profiling.</summary>
    Profile,

    /// <summary>Fully optimized for shipping.</summary>
    Release,
}

public static class BuildModeExtensions
{
    public static string CliName(this BuildMode mode) => mode.ToString().ToLowerInvariant();

    public static string CMakeBuildType(this BuildMode mode) => mode switch
    {
        BuildMode.Debug => "Debug",
        BuildMode.Profile => "RelWithDebInfo",
        _ => "Release",
    };
}
