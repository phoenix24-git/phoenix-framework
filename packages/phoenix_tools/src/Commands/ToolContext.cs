// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using Phoenix.Tools.Build;
using Phoenix.Tools.Infrastructure;
using Phoenix.Tools.Sdk;

namespace Phoenix.Tools.Commands;

/// <summary>Services shared by all commands (a tiny hand-rolled composition root).</summary>
public sealed class ToolContext
{
    private PhoenixSdk? _sdk;

    public ToolContext(Terminal terminal, Func<PhoenixSdk>? sdkFactory = null)
    {
        Terminal = terminal;
        SdkFactory = sdkFactory ?? PhoenixSdk.Locate;
        Runner = new ProcessRunner(terminal);
    }

    public Terminal Terminal { get; }
    public ProcessRunner Runner { get; }
    private Func<PhoenixSdk> SdkFactory { get; }
    public PhoenixSdk Sdk => _sdk ??= SdkFactory();
    public Toolchain Toolchain => new(Sdk);
    public AppBuilder Builder => new(Sdk, Toolchain, Runner, Terminal);

    /// <summary>The directory commands operate in (overridable for tests).</summary>
    public string WorkingDirectory { get; set; } = Environment.CurrentDirectory;
}
