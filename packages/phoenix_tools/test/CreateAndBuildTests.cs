// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using Phoenix.Tools.Build;
using Phoenix.Tools.Infrastructure;
using Phoenix.Tools.Project;
using Xunit;

namespace Phoenix.Tools.Tests;

public sealed class CreateAndBuildTests
{
    [Fact]
    public async Task CreateCommandRendersTheTemplate()
    {
        using var dir = new TempDir();
        var (ctx, output, _) = TestSupport.Context(dir.Path);
        var exit = await Program.RunAsync(["create", "space_game", "--description", "Lasers."], ctx);
        Assert.Equal(0, exit);
        var app = Path.Combine(dir.Path, "space_game");
        foreach (var file in new[] { "phoenix.json", "lib/main.cpp", "test/widget_test.cpp", "web/index.html", ".gitignore", "README.md" })
        {
            Assert.True(File.Exists(Path.Combine(app, file)), file);
        }
        var project = PhoenixProject.Load(app);
        Assert.Equal("space_game", project.Name);
        Assert.Equal("Lasers.", project.Manifest.Description);
        Assert.DoesNotContain("{{", File.ReadAllText(Path.Combine(app, "lib", "main.cpp")), StringComparison.Ordinal);
        Assert.Contains("phoenix run", output.ToString(), StringComparison.Ordinal);
    }

    [Fact]
    public async Task CreateRejectsInvalidNames()
    {
        using var dir = new TempDir();
        var (ctx, _, error) = TestSupport.Context(dir.Path);
        Assert.Equal(1, await Program.RunAsync(["create", "Bad-Name"], ctx));
        Assert.Contains("not a valid Phoenix project name", error.ToString(), StringComparison.Ordinal);
    }

    [Fact]
    public void CreateRefusesToClobberAnExistingProject()
    {
        using var dir = new TempDir();
        var creator = new ProjectCreator(TestSupport.Sdk);
        var app = Path.Combine(dir.Path, "app_one");
        creator.Create(new CreateOptions(app));
        Assert.Throws<ToolExitException>(() => creator.Create(new CreateOptions(app)));
        Assert.NotEmpty(creator.Create(new CreateOptions(app, Overwrite: true)));
    }

    [Fact]
    public void RenderReplacesOnlyKnownPlaceholders() =>
        Assert.Equal("hi app {{other}}", ProjectCreator.Render("hi {{name}} {{other}}", new Dictionary<string, string> { ["name"] = "app" }));

    [Fact]
    public void ConfigureArgumentsTranslatePhoenixJson()
    {
        using var dir = new TempDir();
        new ProjectCreator(TestSupport.Sdk).Create(new CreateOptions(Path.Combine(dir.Path, "cfg_app")));
        var project = PhoenixProject.Load(Path.Combine(dir.Path, "cfg_app"));

        var web = AppBuilder.ConfigureArguments(TestSupport.Sdk, project, new BuildOptions(BuildTarget.Web, BuildMode.Release));
        Assert.Contains("-DPHOENIX_VRAM_MB=3584", web);
        Assert.Contains("-DPHOENIX_WORKER_THREADS=6", web);
        Assert.Contains("-DPHOENIX_BUILD_MODE=release", web);
        Assert.Contains("-DCMAKE_BUILD_TYPE=Release", web);
        Assert.Contains(web, a => a.StartsWith("-DCMAKE_TOOLCHAIN_FILE=", StringComparison.Ordinal) && a.EndsWith("emscripten-wasm64.cmake", StringComparison.Ordinal));
        Assert.Contains("-DPHOENIX_BUILD_TESTS=OFF", web);

        var test = AppBuilder.ConfigureArguments(TestSupport.Sdk, project, new BuildOptions(BuildTarget.Linux, BuildMode.Debug), withTests: true);
        Assert.Contains("-DPHOENIX_APP_TESTS=ON", test);
        Assert.DoesNotContain(test, a => a.StartsWith("-DCMAKE_TOOLCHAIN_FILE", StringComparison.Ordinal));
    }

    [Fact]
    public void BootstrapConfigIsInjected()
    {
        using var dir = new TempDir();
        new ProjectCreator(TestSupport.Sdk).Create(new CreateOptions(Path.Combine(dir.Path, "boot_app")));
        var project = PhoenixProject.Load(Path.Combine(dir.Path, "boot_app"));
        var source = File.ReadAllText(Path.Combine(TestSupport.Sdk.WebRuntimeDir, "phoenix_bootstrap.js"));
        var js = AppBuilder.InjectBootstrapConfig(source, project, new BuildOptions(BuildTarget.Web, BuildMode.Debug, DevServer: true), "9.9.9");
        Assert.Contains("\"appJs\":\"boot_app.js\"", js, StringComparison.Ordinal);
        Assert.Contains("\"devServer\":true", js, StringComparison.Ordinal);
        Assert.Contains("\"engineVersion\":\"9.9.9\"", js, StringComparison.Ordinal);
        Assert.DoesNotContain("/*PHOENIX_CONFIG*/", js, StringComparison.Ordinal);
    }

    [Fact]
    public void PublishBundleReplacesAPreviousBundleOnly()
    {
        using var dir = new TempDir();
        var bundle = Directory.CreateDirectory(Path.Combine(dir.Path, "bundle")).FullName;
        File.WriteAllText(Path.Combine(bundle, "version.json"), "{}");
        File.WriteAllText(Path.Combine(bundle, "app.js"), "new");

        var target = Path.Combine(dir.Path, "docs", "demo");
        Assert.Equal(2, AppBuilder.PublishBundle(bundle, target));        // fresh directory
        File.WriteAllText(Path.Combine(target, "stale.wasm"), "old");
        Assert.Equal(2, AppBuilder.PublishBundle(bundle, target));        // previous bundle replaced
        Assert.False(File.Exists(Path.Combine(target, "stale.wasm")));

        var unrelated = Directory.CreateDirectory(Path.Combine(dir.Path, "notes")).FullName;
        File.WriteAllText(Path.Combine(unrelated, "important.md"), "keep me");
        Assert.Throws<ToolExitException>(() => AppBuilder.PublishBundle(bundle, unrelated));
        Assert.True(File.Exists(Path.Combine(unrelated, "important.md")));
    }

    [Fact]
    public async Task ModeFlagsAreMutuallyExclusive()
    {
        using var dir = new TempDir();
        var (ctx, _, _) = TestSupport.Context(dir.Path);
        var original = Console.Error;
        var captured = new StringWriter();
        Console.SetError(captured); // System.CommandLine reports parse errors on stderr.
        try
        {
            Assert.NotEqual(0, await Program.RunAsync(["build", "web", "--debug", "--release"], ctx));
        }
        finally
        {
            Console.SetError(original);
        }
        Assert.Contains("Only one of", captured.ToString(), StringComparison.Ordinal);
    }

    [Fact]
    public async Task BuildOutsideAProjectFailsHelpfully()
    {
        using var dir = new TempDir();
        var (ctx, _, error) = TestSupport.Context(dir.Path);
        Assert.Equal(1, await Program.RunAsync(["build", "web"], ctx));
        Assert.Contains("No phoenix.json", error.ToString(), StringComparison.Ordinal);
    }
}
