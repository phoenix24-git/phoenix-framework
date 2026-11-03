// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Diagnostics;

namespace Phoenix.Tools.Infrastructure;

/// <summary>Result of a captured process run.</summary>
public sealed record ProcessResult(int ExitCode, string StdOut, string StdErr)
{
    public bool Success => ExitCode == 0;
}

/// <summary>Spawns child processes (cmake, emsdk, chrome, ...) with streaming or captured output.</summary>
public sealed class ProcessRunner(Terminal terminal)
{
    /// <summary>Runs to completion, streaming output lines to the terminal.</summary>
    public async Task<int> StreamAsync(string fileName, IEnumerable<string> args, string? workingDirectory = null,
        IReadOnlyDictionary<string, string?>? environment = null, Action<string>? onLine = null,
        CancellationToken ct = default)
    {
        var psi = CreateStartInfo(fileName, args, workingDirectory, environment);
        terminal.Trace($"$ {fileName} {string.Join(' ', psi.ArgumentList)}");
        using var process = new Process { StartInfo = psi, EnableRaisingEvents = true };
        void Handle(string? line, bool isError)
        {
            if (line is null)
            {
                return;
            }
            if (onLine is not null)
            {
                onLine(line);
            }
            else if (isError)
            {
                terminal.Warning(line);
            }
            else
            {
                terminal.Line(line);
            }
        }
        process.OutputDataReceived += (_, e) => Handle(e.Data, false);
        process.ErrorDataReceived += (_, e) => Handle(e.Data, true);
        if (!process.Start())
        {
            throw new ToolExitException($"Failed to start {fileName}");
        }
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();
        try
        {
            await process.WaitForExitAsync(ct).ConfigureAwait(false);
        }
        catch (OperationCanceledException)
        {
            TryKill(process);
            throw;
        }
        return process.ExitCode;
    }

    /// <summary>Runs to completion and captures stdout/stderr.</summary>
    public static async Task<ProcessResult> CaptureAsync(string fileName, IEnumerable<string> args,
        string? workingDirectory = null, CancellationToken ct = default)
    {
        var psi = CreateStartInfo(fileName, args, workingDirectory, null);
        using var process = Process.Start(psi) ?? throw new ToolExitException($"Failed to start {fileName}");
        var stdout = process.StandardOutput.ReadToEndAsync(ct);
        var stderr = process.StandardError.ReadToEndAsync(ct);
        await process.WaitForExitAsync(ct).ConfigureAwait(false);
        return new ProcessResult(process.ExitCode, await stdout.ConfigureAwait(false), await stderr.ConfigureAwait(false));
    }

    /// <summary>Starts a long-running background process (e.g. Chrome or a desktop app).</summary>
    public Process StartDetached(string fileName, IEnumerable<string> args, string? workingDirectory = null,
        IReadOnlyDictionary<string, string?>? environment = null, bool redirect = true)
    {
        var psi = CreateStartInfo(fileName, args, workingDirectory, environment);
        psi.RedirectStandardOutput = redirect;
        psi.RedirectStandardError = redirect;
        terminal.Trace($"$ {fileName} {string.Join(' ', psi.ArgumentList)} &");
        return Process.Start(psi) ?? throw new ToolExitException($"Failed to start {fileName}");
    }

    public static void TryKill(Process process)
    {
        try
        {
            if (!process.HasExited)
            {
                process.Kill(entireProcessTree: true);
            }
        }
        catch (InvalidOperationException)
        {
            // Already exited.
        }
    }

    private static ProcessStartInfo CreateStartInfo(string fileName, IEnumerable<string> args, string? workingDirectory,
        IReadOnlyDictionary<string, string?>? environment)
    {
        var psi = new ProcessStartInfo(fileName)
        {
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            WorkingDirectory = workingDirectory ?? Environment.CurrentDirectory,
        };
        foreach (var arg in args)
        {
            psi.ArgumentList.Add(arg);
        }
        if (environment is not null)
        {
            foreach (var (key, value) in environment)
            {
                psi.Environment[key] = value;
            }
        }
        return psi;
    }
}
