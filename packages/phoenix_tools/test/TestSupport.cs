// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using Phoenix.Tools.Commands;
using Phoenix.Tools.Infrastructure;
using Phoenix.Tools.Sdk;

namespace Phoenix.Tools.Tests;

/// <summary>Shared helpers: the real SDK checkout and isolated temp directories.</summary>
internal static class TestSupport
{
    public static PhoenixSdk Sdk { get; } = PhoenixSdk.FromRoot(FindRepoRoot());

    private static string FindRepoRoot()
    {
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            if (PhoenixSdk.IsSdkRoot(dir.FullName))
            {
                return dir.FullName;
            }
        }
        throw new InvalidOperationException("Tests must run inside the Phoenix repository.");
    }

    public static (ToolContext Context, StringWriter Out, StringWriter Err) Context(string workingDirectory)
    {
        var output = new StringWriter();
        var error = new StringWriter();
        var ctx = new ToolContext(new Terminal(output, error, color: false), () => Sdk) { WorkingDirectory = workingDirectory };
        return (ctx, output, error);
    }
}

/// <summary>A temp directory deleted on dispose.</summary>
internal sealed class TempDir : IDisposable
{
    public TempDir() => Directory.CreateDirectory(Path);

    public string Path { get; } = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "phoenix-test-" + Guid.NewGuid().ToString("N"));

    public void Dispose()
    {
        try
        {
            Directory.Delete(Path, recursive: true);
        }
        catch (IOException)
        {
            // Best effort.
        }
    }
}
