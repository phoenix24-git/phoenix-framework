// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
namespace Phoenix.Tools.Build;

/// <summary>Platforms an app can be built for.</summary>
public enum BuildTarget
{
    Web,
    Linux,
    MacOS,
    Windows,
}

public static class BuildTargetExtensions
{
    public static string CliName(this BuildTarget target) => target switch
    {
        BuildTarget.MacOS => "macos",
        _ => target.ToString().ToLowerInvariant(),
    };

    public static BuildTarget HostDesktop() =>
        OperatingSystem.IsWindows() ? BuildTarget.Windows :
        OperatingSystem.IsMacOS() ? BuildTarget.MacOS : BuildTarget.Linux;
}
