// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Diagnostics;
using System.Net.Sockets;
using Phoenix.Tools.Infrastructure;

namespace Phoenix.Tools.Web;

/// <summary>A Chrome instance launched for debugging (like Flutter's web device).</summary>
public sealed class ChromeSession(Process process, int debugPort) : IDisposable
{
    public Process Process { get; } = process;
    public int DebugPort { get; } = debugPort;
    public Uri DebugEndpoint => new($"http://127.0.0.1:{DebugPort}/json/version");

    public void Dispose() => ProcessRunner.TryKill(Process);
}

/// <summary>Launches Chrome with an isolated profile and the remote debugging protocol enabled.</summary>
public sealed class ChromeLauncher(ProcessRunner runner)
{
    public static List<string> Arguments(string userDataDir, int debugPort, IEnumerable<Uri> urls, bool headless = false)
    {
        var args = new List<string>
        {
            $"--user-data-dir={userDataDir}",
            $"--remote-debugging-port={debugPort}",
            "--no-first-run",
            "--no-default-browser-check",
            "--disable-default-apps",
            "--disable-translate",
        };
        if (OperatingSystem.IsLinux())
        {
            // WebGPU is behind a flag on Linux Chrome; Phoenix falls back to WebGL otherwise.
            args.Add("--enable-unsafe-webgpu");
            args.Add("--enable-features=Vulkan");
        }
        if (headless)
        {
            args.Add("--headless=new");
        }
        args.AddRange(urls.Select(u => u.ToString()));
        return args;
    }

    public ChromeSession Launch(string chrome, string userDataDir, IEnumerable<Uri> urls, bool headless = false)
    {
        Directory.CreateDirectory(userDataDir);
        var port = FreePort();
        var process = runner.StartDetached(chrome, Arguments(userDataDir, port, urls, headless), redirect: true);
        // Drain Chrome's chatty stderr so it never blocks on a full pipe.
        process.ErrorDataReceived += (_, _) => { };
        process.OutputDataReceived += (_, _) => { };
        process.BeginErrorReadLine();
        process.BeginOutputReadLine();
        return new ChromeSession(process, port);
    }

    public static int FreePort()
    {
        using var listener = new TcpListener(System.Net.IPAddress.Loopback, 0);
        listener.Start();
        return ((System.Net.IPEndPoint)listener.LocalEndpoint).Port;
    }
}
