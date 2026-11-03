// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using Phoenix.Tools.Infrastructure;
using Phoenix.Tools.Project;
using Xunit;

namespace Phoenix.Tools.Tests;

public sealed class ManifestTests
{
    [Theory]
    [InlineData("my_app", true)]
    [InlineData("a1", true)]
    [InlineData("MyApp", false)]
    [InlineData("1app", false)]
    [InlineData("my-app", false)]
    [InlineData("phoenix", false)]
    [InlineData("test", false)]
    [InlineData("", false)]
    public void ProjectNamesFollowPubRules(string name, bool valid) => Assert.Equal(valid, PhoenixProject.IsValidName(name));

    [Fact]
    public void DefaultsDescribeTheVirtualConsole()
    {
        var m = new PhoenixManifest { Name = "app" };
        Assert.Equal(3584, m.Console.VramMB);
        Assert.Equal(6, m.Console.WorkerThreads);
        Assert.True(m.Console.VramPreallocate);
        Assert.True(m.Web.MaximumMemoryMB > 4096, "wasm64 exists to exceed the 4 GiB wasm32 limit");
        Assert.Empty(PhoenixProject.Validate(m));
    }

    [Fact]
    public void ValidationReportsEveryProblem()
    {
        var m = new PhoenixManifest
        {
            Name = "Bad Name",
            Console = new ConsoleSettings { WorkerThreads = 500, VramMB = 1 },
            Web = new WebSettings { InitialMemoryMB = 2048, MaximumMemoryMB = 1024 },
        };
        var errors = PhoenixProject.Validate(m);
        Assert.Equal(4, errors.Count);
    }

    [Fact]
    public void LoadParsesJsonWithCommentsAndFindWalksUp()
    {
        using var dir = new TempDir();
        File.WriteAllText(Path.Combine(dir.Path, "phoenix.json"), """
            {
              // comments are allowed
              "name": "walker",
              "title": "Walker",
              "console": { "vramMB": 512, "workerThreads": 4 },
            }
            """);
        var nested = Directory.CreateDirectory(Path.Combine(dir.Path, "lib", "deep")).FullName;
        var project = PhoenixProject.Find(nested);
        Assert.Equal("walker", project.Name);
        Assert.Equal("Walker", project.Title);
        Assert.Equal(512, project.Manifest.Console.VramMB);
        Assert.Equal(4, project.Manifest.Console.WorkerThreads);
        Assert.Equal(256, project.Manifest.Console.CpuArenaMB); // default kept
    }

    [Fact]
    public void FindFailsOutsideAProject()
    {
        using var dir = new TempDir();
        var e = Assert.Throws<ToolExitException>(() => PhoenixProject.Find(dir.Path));
        Assert.Contains("phoenix create", e.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void InvalidJsonIsAFriendlyError()
    {
        using var dir = new TempDir();
        File.WriteAllText(Path.Combine(dir.Path, "phoenix.json"), "{ nope");
        Assert.Throws<ToolExitException>(() => PhoenixProject.Load(dir.Path));
    }
}
