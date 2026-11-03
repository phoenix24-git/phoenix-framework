// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Diagnostics;
using System.Text.Json;
using Phoenix.Tools.Infrastructure;
using Phoenix.Tools.Project;
using Phoenix.Tools.Sdk;

namespace Phoenix.Tools.Build;

/// <summary>Options shared by `phoenix build` and `phoenix run`.</summary>
public sealed record BuildOptions(BuildTarget Target, BuildMode Mode, bool DevServer = false);

/// <summary>Result of a successful build.</summary>
public sealed record BuildResult(string OutputDir, string? Executable, TimeSpan Elapsed);

/// <summary>
/// Drives CMake for an app: configures the engine with the app's phoenix.json
/// translated into cache variables, builds, then assembles the output bundle
/// (build/web or build/&lt;os&gt;), like flutter_tools' assemble step.
/// </summary>
public sealed class AppBuilder(PhoenixSdk sdk, Toolchain toolchain, ProcessRunner runner, Terminal terminal)
{
    public static string OutputDir(PhoenixProject project, BuildTarget target) =>
        Path.Combine(project.BuildDir, target.CliName());

    public static string CMakeBinaryDir(PhoenixProject project, BuildOptions o) =>
        Path.Combine(project.BuildDir, ".phoenix", $"{o.Target.CliName()}-{o.Mode.CliName()}");

    /// <summary>Pure function: the CMake configure arguments for a build (unit tested).</summary>
    public static List<string> ConfigureArguments(PhoenixSdk sdk, PhoenixProject project, BuildOptions o,
        bool withTests = false)
    {
        var m = project.Manifest;
        var args = new List<string>
        {
            "-S", sdk.EngineDir,
            "-B", withTests ? Path.Combine(project.BuildDir, ".phoenix", "test") : CMakeBinaryDir(project, o),
            "-G", "Ninja",
            $"-DCMAKE_BUILD_TYPE={o.Mode.CMakeBuildType()}",
            $"-DPHOENIX_BUILD_TESTS={(withTests ? "ON" : "OFF")}",
            $"-DPHOENIX_APP_TESTS={(withTests ? "ON" : "OFF")}",
            $"-DPHOENIX_APP_DIR={project.Directory}",
            $"-DPHOENIX_APP_OUTPUT_DIR={OutputDir(project, o.Target)}",
            $"-DPHOENIX_APP_NAME={project.Name}",
            $"-DPHOENIX_APP_TITLE={project.Title}",
            $"-DPHOENIX_BUILD_MODE={o.Mode.CliName()}",
            $"-DPHOENIX_VRAM_MB={m.Console.VramMB}",
            $"-DPHOENIX_VRAM_PREALLOCATE={(m.Console.VramPreallocate ? "ON" : "OFF")}",
            $"-DPHOENIX_WORKER_THREADS={m.Console.WorkerThreads}",
            $"-DPHOENIX_CPU_ARENA_MB={m.Console.CpuArenaMB}",
            $"-DPHOENIX_WEB_INITIAL_MEMORY_MB={m.Web.InitialMemoryMB}",
            $"-DPHOENIX_WEB_MAXIMUM_MEMORY_MB={m.Web.MaximumMemoryMB}",
        };
        if (o.Target == BuildTarget.Web)
        {
            args.Add($"-DCMAKE_TOOLCHAIN_FILE={sdk.WasmToolchainFile}");
        }
        return args;
    }

    public async Task<BuildResult> BuildAsync(PhoenixProject project, BuildOptions options, CancellationToken ct)
    {
        var stopwatch = Stopwatch.StartNew();
        var cmake = Toolchain.RequireCMake();
        var env = options.Target == BuildTarget.Web ? toolchain.RequireEmsdk().Environment() : null;
        var binaryDir = CMakeBinaryDir(project, options);

        terminal.Status($"Compiling lib/main.cpp for the {options.Target.CliName()} target ({options.Mode.CliName()})...");
        var log = new List<string>();
        void Capture(string line)
        {
            log.Add(line);
            terminal.Trace(line);
        }

        var configure = await runner.StreamAsync(cmake, ConfigureArguments(sdk, project, options), project.Directory,
            env, Capture, ct).ConfigureAwait(false);
        if (configure != 0)
        {
            Fail("CMake configure", log);
        }
        var build = await runner.StreamAsync(cmake, ["--build", binaryDir, "--target", "phoenix_app"],
            project.Directory, env, Capture, ct).ConfigureAwait(false);
        if (build != 0)
        {
            Fail("Compilation", log);
        }

        var output = OutputDir(project, options.Target);
        string? executable;
        if (options.Target == BuildTarget.Web)
        {
            AssembleWeb(project, options, output);
            executable = null;
        }
        else
        {
            executable = Path.Combine(output, project.Name + (OperatingSystem.IsWindows() ? ".exe" : string.Empty));
        }
        stopwatch.Stop();
        terminal.Success($"✓ Built {Path.GetRelativePath(Environment.CurrentDirectory, output)} " +
                         $"({stopwatch.Elapsed.TotalSeconds:F1}s)");
        return new BuildResult(output, executable, stopwatch.Elapsed);
    }

    private void Fail(string stage, List<string> log)
    {
        if (!terminal.Verbose)
        {
            // Show the tail of the log: the actual compiler errors are at the end.
            foreach (var line in log.TakeLast(60))
            {
                terminal.Line(line);
            }
        }
        throw new ToolExitException($"{stage} failed. Re-run with -v for the full log.");
    }

    /// <summary>Copies web/ and the engine web runtime into build/web.</summary>
    internal void AssembleWeb(PhoenixProject project, BuildOptions options, string output)
    {
        var webDir = Path.Combine(project.Directory, "web");
        if (Directory.Exists(webDir))
        {
            FileSystemUtil.CopyDirectory(webDir, output);
        }
        if (options.Mode != BuildMode.Debug)
        {
            // Never ship a stale debug source map next to an optimized binary.
            File.Delete(Path.Combine(output, project.Name + ".wasm.map"));
        }
        File.Copy(Path.Combine(sdk.WebRuntimeDir, "phoenix_sw.js"), Path.Combine(output, "phoenix_sw.js"), true);
        var bootstrap = File.ReadAllText(Path.Combine(sdk.WebRuntimeDir, "phoenix_bootstrap.js"));
        File.WriteAllText(Path.Combine(output, "phoenix_bootstrap.js"),
            InjectBootstrapConfig(bootstrap, project, options, sdk.Version));
        File.WriteAllText(Path.Combine(output, "version.json"), JsonSerializer.Serialize(new Dictionary<string, string>
        {
            ["app_name"] = project.Name,
            ["version"] = project.Manifest.Version,
            ["engine_version"] = sdk.Version,
            ["build_mode"] = options.Mode.CliName(),
        }));
    }

    internal static string InjectBootstrapConfig(string bootstrap, PhoenixProject project, BuildOptions options,
        string engineVersion)
    {
        const string marker = "/*PHOENIX_CONFIG*/";
        var start = bootstrap.IndexOf(marker, StringComparison.Ordinal);
        if (start < 0)
        {
            throw new ToolExitException("phoenix_bootstrap.js is missing the /*PHOENIX_CONFIG*/ marker.");
        }
        var end = bootstrap.IndexOf(';', start);
        var config = JsonSerializer.Serialize(new Dictionary<string, object>
        {
            ["appJs"] = project.Name + ".js",
            ["buildMode"] = options.Mode.CliName(),
            ["devServer"] = options.DevServer,
            ["engineVersion"] = engineVersion,
        });
        return string.Concat(bootstrap.AsSpan(0, start), config, bootstrap.AsSpan(end));
    }
}
