// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.CommandLine;
using System.Runtime.InteropServices;
using Phoenix.Tools.Build;
using Phoenix.Tools.Infrastructure;

namespace Phoenix.Tools.Commands;

/// <summary>`phoenix doctor`: checks the host toolchain, like `flutter doctor`.</summary>
internal static class DoctorCommand
{
    internal sealed record Check(string Name, bool Ok, string Detail, string? Fix = null, bool Optional = false);

    public static Command Build(ToolContext ctx)
    {
        var command = new Command("doctor", "Show information about the installed tooling.");
        command.SetAction(async (_, ct) =>
        {
            var checks = await RunChecksAsync(ctx, ct).ConfigureAwait(false);
            var t = ctx.Terminal;
            t.Line("Doctor summary:");
            foreach (var c in checks)
            {
                var mark = c.Ok ? "[✓]" : c.Optional ? "[!]" : "[✗]";
                t.Line($"{mark} {c.Name} ({c.Detail})");
                if (!c.Ok && c.Fix is not null)
                {
                    t.Line($"    ✗ {c.Fix}");
                }
            }
            var issues = checks.Count(c => !c.Ok && !c.Optional);
            t.Line();
            if (issues == 0)
            {
                t.Success("• No issues found!");
                return 0;
            }
            t.Warning($"! Doctor found issues in {issues} categor{(issues == 1 ? "y" : "ies")}.");
            return 1;
        });
        return command;
    }

    internal static async Task<List<Check>> RunChecksAsync(ToolContext ctx, CancellationToken ct)
    {
        var sdk = ctx.Sdk;
        var checks = new List<Check>
        {
            new("Phoenix", true, $"{sdk.Version}, on {RuntimeInformation.OSDescription} {RuntimeInformation.OSArchitecture}, at {sdk.Root}"),
            new(".NET", true, $"runtime {Environment.Version}"),
        };
        checks.Add(await ToolVersionAsync("CMake", Toolchain.CMake, "--version", "Install CMake 3.25 or newer.", ct).ConfigureAwait(false));
        checks.Add(await ToolVersionAsync("Ninja", Toolchain.Ninja, "--version", "Install ninja-build.", ct).ConfigureAwait(false));

        var emsdk = ctx.Toolchain.FindEmsdk();
        var required = ctx.Toolchain.RequiredEmscriptenVersion;
        if (emsdk is null)
        {
            checks.Add(new Check("Emscripten - develop for the web", false, "not found",
                "Run `phoenix precache --web` to install the pinned emsdk."));
        }
        else
        {
            var version = await Toolchain.EmscriptenVersionAsync(emsdk).ConfigureAwait(false) ?? "unknown";
            checks.Add(new Check("Emscripten - develop for the web", version == required,
                $"{version} at {emsdk.Root}; wasm64 + pthreads",
                $"Phoenix pins Emscripten {required}. Run `phoenix precache --web`."));
        }
        var chrome = Toolchain.FindChrome();
        checks.Add(new Check("Chrome - develop for the web", chrome is not null, chrome ?? "not found",
            "Install Chrome, or set CHROME_EXECUTABLE.", Optional: true));
        return checks;
    }

    private static async Task<Check> ToolVersionAsync(string name, string? path, string flag, string fix, CancellationToken ct)
    {
        if (path is null)
        {
            return new Check(name, false, "not found on PATH", fix);
        }
        var result = await ProcessRunner.CaptureAsync(path, [flag], ct: ct).ConfigureAwait(false);
        var first = result.StdOut.Split('\n', StringSplitOptions.RemoveEmptyEntries).FirstOrDefault()?.Trim() ?? "?";
        return new Check(name, result.Success, first, fix);
    }
}
