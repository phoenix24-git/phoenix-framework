// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using Phoenix.Tools.Commands;
using Phoenix.Tools.Web;
using Xunit;

namespace Phoenix.Tools.Tests;

public sealed class CommandTests
{
    [Fact]
    public async Task HelpListsTheFlutterStyleCommands()
    {
        using var dir = new TempDir();
        var (ctx, output, _) = TestSupport.Context(dir.Path);
        var original = Console.Out;
        var captured = new StringWriter();
        Console.SetOut(captured);
        try
        {
            Assert.Equal(0, await Program.RunAsync(["--help"], ctx));
        }
        finally
        {
            Console.SetOut(original);
        }
        var help = captured + output.ToString();
        foreach (var command in new[] { "create", "run", "build", "test", "clean", "devices", "doctor", "precache" })
        {
            Assert.Contains(command, help, StringComparison.Ordinal);
        }
    }

    [Fact]
    public void DevicesAlwaysIncludeWebServerAndHostDesktop()
    {
        var devices = DevicesCommand.Discover();
        Assert.Contains(devices, d => d.Id == "web-server");
        Assert.Contains(devices, d => d.Id is "linux" or "macos" or "windows");
    }

    [Fact]
    public async Task DoctorReportsCoreTools()
    {
        using var dir = new TempDir();
        var (ctx, _, _) = TestSupport.Context(dir.Path);
        var checks = await DoctorCommand.RunChecksAsync(ctx, TestContext.Current.CancellationToken);
        Assert.Contains(checks, c => c.Name == "Phoenix" && c.Ok);
        Assert.Contains(checks, c => c.Name.StartsWith("Emscripten", StringComparison.Ordinal));
    }

    [Fact]
    public void ChromeArgumentsIsolateTheProfileAndEnableDebugging()
    {
        var args = ChromeLauncher.Arguments("/tmp/profile", 9333, [new Uri("http://127.0.0.1:8080/")]);
        Assert.Contains("--user-data-dir=/tmp/profile", args);
        Assert.Contains("--remote-debugging-port=9333", args);
        Assert.Equal("http://127.0.0.1:8080/", args[^1]);
    }

    [Fact]
    public async Task CleanRemovesBuildOutputs()
    {
        using var dir = new TempDir();
        var (ctx, _, _) = TestSupport.Context(dir.Path);
        Assert.Equal(0, await Program.RunAsync(["create", "clean_me"], ctx));
        var app = Path.Combine(dir.Path, "clean_me");
        Directory.CreateDirectory(Path.Combine(app, "build", "web"));
        ctx.WorkingDirectory = app;
        Assert.Equal(0, await Program.RunAsync(["clean"], ctx));
        Assert.False(Directory.Exists(Path.Combine(app, "build")));
    }
}
