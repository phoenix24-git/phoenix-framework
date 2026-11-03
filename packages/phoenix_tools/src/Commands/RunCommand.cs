// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.CommandLine;
using System.Diagnostics;
using Phoenix.Tools.Build;
using Phoenix.Tools.Infrastructure;
using Phoenix.Tools.Project;
using Phoenix.Tools.Web;

namespace Phoenix.Tools.Commands;

/// <summary>`phoenix run`: build, launch on a device, and stay resident for hot restart.</summary>
internal static class RunCommand
{
    public static Command Build(ToolContext ctx)
    {
        var device = new Option<string?>("--device-id", "-d") { Description = "Target device: chrome, web-server, linux, macos, windows." };
        var port = new Option<int>("--web-port") { Description = "Port for the web server (0 = pick a free port).", DefaultValueFactory = _ => 0 };
        var host = new Option<string>("--web-hostname") { Description = "Hostname the web server binds to.", DefaultValueFactory = _ => "127.0.0.1" };
        var noResident = new Option<bool>("--no-resident") { Description = "Build and launch, then exit without waiting (web-server: serve until Ctrl+C)." };
        var mode = ModeOptions.Create(defaultMode: BuildMode.Debug);
        var command = new Command("run", "Run your Phoenix app on an attached device.") { device, port, host, noResident };
        mode.AddTo(command);

        command.SetAction(async (parse, ct) =>
        {
            var project = PhoenixProject.Find(ctx.WorkingDirectory);
            var id = parse.GetValue(device) ?? (Toolchain.FindChrome() is not null ? "chrome" : "web-server");
            var buildMode = mode.Resolve(parse);
            return id switch
            {
                "chrome" or "web-server" => await RunWebAsync(ctx, project, buildMode, id == "chrome",
                    parse.GetValue(host)!, parse.GetValue(port), parse.GetValue(noResident), ct).ConfigureAwait(false),
                "linux" or "macos" or "windows" => await RunDesktopAsync(ctx, project, buildMode, ct).ConfigureAwait(false),
                _ => throw new ToolExitException($"No supported devices found with name or id matching '{id}'. Run `phoenix devices`."),
            };
        });
        return command;
    }

    private static async Task<int> RunWebAsync(ToolContext ctx, PhoenixProject project, BuildMode mode, bool chrome,
        string host, int port, bool noResident, CancellationToken ct)
    {
        var t = ctx.Terminal;
        var deviceName = chrome ? "Chrome" : "Web Server";
        t.Line($"Launching lib/main.cpp on {deviceName} in {mode.CliName()} mode...");
        var options = new BuildOptions(BuildTarget.Web, mode, DevServer: true);
        var result = await ctx.Builder.BuildAsync(project, options, ct).ConfigureAwait(false);

        await using var server = await DevServer.StartAsync(new DevServerOptions(
            result.OutputDir, ctx.Sdk.DevToolsDir, host, port,
            AppSourceRoot: mode == BuildMode.Debug ? project.Directory : null,
            SdkSourceRoot: mode == BuildMode.Debug ? ctx.Sdk.Root : null), ct).ConfigureAwait(false);
        var appUrl = server.Address;
        var devtoolsUrl = new Uri(appUrl, DevServer.DevToolsPath + "/");

        ChromeSession? session = null;
        var detached = false;
        if (chrome)
        {
            var chromePath = Toolchain.FindChrome() ?? throw new ToolExitException(
                "Chrome not found. Set CHROME_EXECUTABLE or use `-d web-server`.");
            session = new ChromeLauncher(ctx.Runner).Launch(chromePath,
                Path.Combine(project.ToolStateDir, "chrome-profile"), [appUrl]);
            t.Line($"Debug service listening on http://127.0.0.1:{session.DebugPort} (Chrome DevTools Protocol)");
        }
        else
        {
            t.Line($"lib/main.cpp is being served at {appUrl}");
            t.Line("The web-server device requires a browser with WebAssembly Memory64 support (Chrome/Edge 133+, Firefox 134+).");
        }
        t.Line();
        t.Line($"A Phoenix DevTools debugger and profiler is available at: {devtoolsUrl}");

        try
        {
            if (noResident && chrome)
            {
                detached = true;
                return 0;
            }
            var outcome = await ResidentLoopAsync(ctx, session?.Process,
                hotRestart: async () =>
                {
                    var sw = Stopwatch.StartNew();
                    t.Line("Performing hot restart...");
                    await ctx.Builder.BuildAsync(project, options, ct).ConfigureAwait(false);
                    var clients = await server.BroadcastReloadAsync().ConfigureAwait(false);
                    t.Success($"Restarted application in {sw.ElapsedMilliseconds:N0}ms ({clients} page(s) reloaded).");
                },
                openDevTools: () =>
                {
                    if (session is not null && Toolchain.FindChrome() is { } c)
                    {
                        ctx.Runner.StartDetached(c, [$"--user-data-dir={Path.Combine(project.ToolStateDir, "chrome-profile")}", devtoolsUrl.ToString()]);
                    }
                    else
                    {
                        t.Line($"Open {devtoolsUrl} in your browser.");
                    }
                }, ct).ConfigureAwait(false);
            detached = outcome == Outcome.Detach;
            return 0;
        }
        finally
        {
            if (!detached)
            {
                session?.Dispose();
            }
        }
    }

    private static async Task<int> RunDesktopAsync(ToolContext ctx, PhoenixProject project, BuildMode mode, CancellationToken ct)
    {
        var target = BuildTargetExtensions.HostDesktop();
        ctx.Terminal.Line($"Launching lib/main.cpp on {target} in {mode.CliName()} mode...");
        var options = new BuildOptions(target, mode);
        Process? app = null;
        async Task Launch()
        {
            var result = await ctx.Builder.BuildAsync(project, options, ct).ConfigureAwait(false);
            app = ctx.Runner.StartDetached(result.Executable!, [], project.Directory);
            app.OutputDataReceived += (_, e) => Forward(ctx, e.Data);
            app.ErrorDataReceived += (_, e) => Forward(ctx, e.Data);
            app.BeginOutputReadLine();
            app.BeginErrorReadLine();
        }
        await Launch().ConfigureAwait(false);
        var outcome = Outcome.Quit;
        try
        {
            outcome = await ResidentLoopAsync(ctx, app, hotRestart: async () =>
            {
                ProcessRunner.TryKill(app!);
                await Launch().ConfigureAwait(false);
            }, openDevTools: () => ctx.Terminal.Line("Phoenix DevTools for desktop targets is not available yet."), ct).ConfigureAwait(false);
            return 0;
        }
        finally
        {
            if (app is not null && outcome != Outcome.Detach)
            {
                ProcessRunner.TryKill(app);
            }
        }
    }

    private static void Forward(ToolContext ctx, string? line)
    {
        if (line is not null)
        {
            ctx.Terminal.Line(line);
        }
    }

    internal static void PrintHelp(Terminal t)
    {
        t.Line();
        t.Status("Phoenix run key commands.");
        t.Line("r Hot restart (rebuild and reload the app).");
        t.Line("R Hot restart.");
        t.Line("v Open Phoenix DevTools.");
        t.Line("h List all available interactive commands.");
        t.Line("d Detach (terminate \"phoenix run\" but leave application running).");
        t.Line("c Clear the screen");
        t.Line("q Quit (terminate the application on the device).");
        t.Line();
    }

    private enum Outcome
    {
        Quit,
        Detach,
    }

    /// <summary>Flutter-style resident runner: reacts to single key presses until quit.</summary>
    private static async Task<Outcome> ResidentLoopAsync(ToolContext ctx, Process? device,
        Func<Task> hotRestart, Action openDevTools, CancellationToken ct)
    {
        var t = ctx.Terminal;
        var interactive = !Console.IsInputRedirected;
        if (interactive)
        {
            PrintHelp(t);
        }
        else
        {
            t.Line("Running non-interactively; press Ctrl+C to stop.");
        }
        while (!ct.IsCancellationRequested)
        {
            if (device is { HasExited: true })
            {
                t.Line("Application finished.");
                return Outcome.Quit;
            }
            if (!interactive || !Console.KeyAvailable)
            {
                try
                {
                    await Task.Delay(100, ct).ConfigureAwait(false);
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                continue;
            }
            var key = Console.ReadKey(intercept: true).KeyChar;
            try
            {
                switch (key)
                {
                    case 'r' or 'R': await hotRestart().ConfigureAwait(false); break;
                    case 'v': openDevTools(); break;
                    case 'h' or '?': PrintHelp(t); break;
                    case 'c': Console.Clear(); break;
                    case 'd': t.Line("Detached."); return Outcome.Detach;
                    case 'q': t.Line("Application finished."); return Outcome.Quit;
                    default: break;
                }
            }
            catch (ToolExitException e)
            {
                // A failed hot restart keeps the session alive, like Flutter.
                t.Error(e.Message);
            }
        }
        return Outcome.Quit;
    }
}
