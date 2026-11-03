// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.CommandLine;
using Phoenix.Tools.Build;
using Phoenix.Tools.Infrastructure;
using Phoenix.Tools.Project;

namespace Phoenix.Tools.Commands;

/// <summary>`phoenix test`: builds test/*.cpp natively against the framework and runs them.</summary>
internal static class TestCommand
{
    public static Command Build(ToolContext ctx)
    {
        var filter = new Option<string?>("--name") { Description = "Only run tests whose name matches this doctest filter." };
        var command = new Command("test", "Run Phoenix unit tests for the current project.") { filter };
        command.SetAction(async (parse, ct) =>
        {
            var project = PhoenixProject.Find(ctx.WorkingDirectory);
            if (!Directory.Exists(Path.Combine(project.Directory, "test")))
            {
                throw new ToolExitException("Test directory \"test\" not found.");
            }
            var cmake = Toolchain.RequireCMake();
            var options = new BuildOptions(BuildTargetExtensions.HostDesktop(), BuildMode.Debug);
            var args = AppBuilder.ConfigureArguments(ctx.Sdk, project, options, withTests: true);
            var binaryDir = args[args.IndexOf("-B") + 1];
            ctx.Terminal.Status("Building tests...");
            var log = new List<string>();
            if (await ctx.Runner.StreamAsync(cmake, args, project.Directory, null, log.Add, ct).ConfigureAwait(false) != 0 ||
                await ctx.Runner.StreamAsync(cmake, ["--build", binaryDir, "--target", "phoenix_app_tests"], project.Directory,
                    null, log.Add, ct).ConfigureAwait(false) != 0)
            {
                foreach (var line in log.TakeLast(60))
                {
                    ctx.Terminal.Line(line);
                }
                throw new ToolExitException("Test build failed.");
            }
            var exe = Path.Combine(binaryDir, "phoenix_app_tests" + (OperatingSystem.IsWindows() ? ".exe" : string.Empty));
            var testArgs = new List<string>();
            if (parse.GetValue(filter) is { } name)
            {
                testArgs.Add($"--test-case=*{name}*");
            }
            return await ctx.Runner.StreamAsync(exe, testArgs, project.Directory, null, ct: ct).ConfigureAwait(false);
        });
        return command;
    }
}
