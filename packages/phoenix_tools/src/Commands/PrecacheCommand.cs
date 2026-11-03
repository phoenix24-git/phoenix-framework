// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.CommandLine;
using Phoenix.Tools.Infrastructure;

namespace Phoenix.Tools.Commands;

/// <summary>`phoenix precache --web`: installs the pinned Emscripten SDK into bin/cache/emsdk.</summary>
internal static class PrecacheCommand
{
    private static readonly string[] Steps = ["install", "activate"];

    public static Command Build(ToolContext ctx)
    {
        var web = new Option<bool>("--web") { Description = "Install the web toolchain (Emscripten).", DefaultValueFactory = _ => true };
        var command = new Command("precache", "Populate the Phoenix tool's cache of binary artifacts.") { web };
        command.SetAction(async (parse, ct) =>
        {
            if (!parse.GetValue(web))
            {
                return 0;
            }
            var toolchain = ctx.Toolchain;
            var version = toolchain.RequiredEmscriptenVersion;
            var dir = toolchain.EmsdkCacheDir;
            var git = FileSystemUtil.FindOnPath("git") ?? throw new ToolExitException("git is required.");
            if (!Directory.Exists(Path.Combine(dir, ".git")))
            {
                ctx.Terminal.Status($"Downloading emsdk into {dir}...");
                if (await ctx.Runner.StreamAsync(git, ["clone", "--depth", "1", "https://github.com/emscripten-core/emsdk.git", dir],
                        ct: ct).ConfigureAwait(false) != 0)
                {
                    throw new ToolExitException("git clone emsdk failed.");
                }
            }
            var emsdk = Path.Combine(dir, OperatingSystem.IsWindows() ? "emsdk.bat" : "emsdk");
            foreach (var step in Steps)
            {
                ctx.Terminal.Status($"emsdk {step} {version}...");
                if (await ctx.Runner.StreamAsync(emsdk, [step, version], dir, ct: ct).ConfigureAwait(false) != 0)
                {
                    throw new ToolExitException($"emsdk {step} {version} failed.");
                }
            }
            ctx.Terminal.Success($"✓ Emscripten {version} ready.");
            return 0;
        });
        return command;
    }
}
