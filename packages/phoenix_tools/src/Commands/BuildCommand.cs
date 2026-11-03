// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.CommandLine;
using Phoenix.Tools.Build;
using Phoenix.Tools.Project;

namespace Phoenix.Tools.Commands;

internal static class BuildCommand
{
    public static Command Build(ToolContext ctx)
    {
        var command = new Command("build", "Build an executable app or web bundle.");
        command.Subcommands.Add(Target(ctx, BuildTarget.Web, "Build a web application bundle (wasm64 + WebGPU)."));
        command.Subcommands.Add(Target(ctx, BuildTarget.Linux, "Build a Linux desktop application."));
        command.Subcommands.Add(Target(ctx, BuildTarget.MacOS, "Build a macOS desktop application."));
        command.Subcommands.Add(Target(ctx, BuildTarget.Windows, "Build a Windows desktop application."));
        return command;
    }

    private static Command Target(ToolContext ctx, BuildTarget target, string description)
    {
        var mode = ModeOptions.Create(defaultMode: BuildMode.Release);
        var command = new Command(target.CliName(), description);
        mode.AddTo(command);
        command.SetAction(async (parse, ct) =>
        {
            if (target != BuildTarget.Web && target != BuildTargetExtensions.HostDesktop())
            {
                ctx.Terminal.Error($"Cannot build for {target.CliName()} on this host; desktop builds are not cross-compiled.");
                return 1;
            }
            var project = PhoenixProject.Find(ctx.WorkingDirectory);
            await ctx.Builder.BuildAsync(project, new BuildOptions(target, mode.Resolve(parse)), ct).ConfigureAwait(false);
            return 0;
        });
        return command;
    }
}

/// <summary>--debug / --profile / --release flags (mutually exclusive, like Flutter).</summary>
internal sealed class ModeOptions
{
    private readonly BuildMode _default;

    private ModeOptions(BuildMode defaultMode) => _default = defaultMode;

    public Option<bool> Debug { get; } = new("--debug") { Description = "Build a debug version (assertions, source maps)." };
    public Option<bool> Profile { get; } = new("--profile") { Description = "Build an optimized version with profiling symbols." };
    public Option<bool> Release { get; } = new("--release") { Description = "Build a fully optimized release version." };

    public static ModeOptions Create(BuildMode defaultMode) => new(defaultMode);

    public void AddTo(Command command)
    {
        command.Options.Add(Debug);
        command.Options.Add(Profile);
        command.Options.Add(Release);
        command.Validators.Add(result =>
        {
            var count = new[] { Debug, Profile, Release }.Count(o => result.GetValue(o));
            if (count > 1)
            {
                result.AddError("Only one of --debug, --profile, or --release can be specified.");
            }
        });
    }

    public BuildMode Resolve(ParseResult parse) =>
        parse.GetValue(Debug) ? BuildMode.Debug :
        parse.GetValue(Profile) ? BuildMode.Profile :
        parse.GetValue(Release) ? BuildMode.Release : _default;
}
