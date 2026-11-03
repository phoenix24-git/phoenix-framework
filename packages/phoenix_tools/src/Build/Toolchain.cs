// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Text.RegularExpressions;
using Phoenix.Tools.Infrastructure;
using Phoenix.Tools.Sdk;

namespace Phoenix.Tools.Build;

/// <summary>An installed Emscripten SDK.</summary>
public sealed record Emsdk(string Root, string EmscriptenDir, string? NodePath)
{
    /// <summary>Environment for child processes (what `source emsdk_env.sh` would set).</summary>
    public Dictionary<string, string?> Environment()
    {
        var path = new List<string> { Root, EmscriptenDir };
        if (NodePath is not null)
        {
            path.Add(System.IO.Path.GetDirectoryName(NodePath)!);
        }
        path.Add(System.Environment.GetEnvironmentVariable("PATH") ?? string.Empty);
        return new Dictionary<string, string?>
        {
            ["EMSDK"] = Root,
            ["EMSDK_NODE"] = NodePath,
            ["PATH"] = string.Join(System.IO.Path.PathSeparator, path),
        };
    }
}

/// <summary>Discovers the host tools the build needs (cmake, ninja, emsdk, Chrome).</summary>
public sealed partial class Toolchain(PhoenixSdk sdk)
{
    public string RequiredEmscriptenVersion =>
        File.ReadAllText(Path.Combine(sdk.Root, "bin", "internal", "emscripten.version")).Trim();

    public static string? CMake => FileSystemUtil.FindOnPath("cmake");

    public static string? Ninja => FileSystemUtil.FindOnPath("ninja");

    public string EmsdkCacheDir => Path.Combine(sdk.CacheDir, "emsdk");

    public Emsdk? FindEmsdk()
    {
        var candidates = new List<string?>
        {
            System.Environment.GetEnvironmentVariable("EMSDK"),
            EmsdkCacheDir,
            Path.Combine(System.Environment.GetFolderPath(System.Environment.SpecialFolder.UserProfile), "emsdk"),
            "/opt/emsdk",
        };
        var emccOnPath = FileSystemUtil.FindOnPath("emcc");
        if (emccOnPath is not null)
        {
            // <emsdk>/upstream/emscripten/emcc
            var resolved = new FileInfo(emccOnPath).ResolveLinkTarget(true)?.FullName ?? emccOnPath;
            candidates.Add(Path.GetFullPath(Path.Combine(Path.GetDirectoryName(resolved)!, "..", "..")));
        }
        foreach (var root in candidates)
        {
            if (string.IsNullOrEmpty(root))
            {
                continue;
            }
            var emscripten = Path.Combine(root, "upstream", "emscripten");
            if (File.Exists(Path.Combine(emscripten, "emcc")) || File.Exists(Path.Combine(emscripten, "emcc.bat")))
            {
                return new Emsdk(Path.GetFullPath(root), emscripten, FindEmsdkNode(root));
            }
        }
        return null;
    }

    private static string? FindEmsdkNode(string root)
    {
        var env = System.Environment.GetEnvironmentVariable("EMSDK_NODE");
        if (!string.IsNullOrEmpty(env) && File.Exists(env))
        {
            return env;
        }
        var nodeDir = Path.Combine(root, "node");
        if (!Directory.Exists(nodeDir))
        {
            return null;
        }
        var exe = OperatingSystem.IsWindows() ? "node.exe" : "node";
        return Directory.EnumerateDirectories(nodeDir)
            .OrderDescending(StringComparer.Ordinal)
            .Select(d => Path.Combine(d, "bin", exe))
            .FirstOrDefault(File.Exists);
    }

    public static async Task<string?> EmscriptenVersionAsync(Emsdk emsdk)
    {
        var emcc = Path.Combine(emsdk.EmscriptenDir, OperatingSystem.IsWindows() ? "emcc.bat" : "emcc");
        try
        {
            var result = await ProcessRunner.CaptureAsync(emcc, ["--version"]).ConfigureAwait(false);
            var match = VersionRegex().Match(result.StdOut);
            return match.Success ? match.Groups[1].Value : null;
        }
        catch (System.ComponentModel.Win32Exception)
        {
            return null;
        }
    }

    [GeneratedRegex(@"emcc \(Emscripten[^)]*\) (\d+\.\d+\.\d+)")]
    private static partial Regex VersionRegex();

    /// <summary>Finds Chrome/Chromium; honours CHROME_EXECUTABLE like Flutter.</summary>
    public static string? FindChrome()
    {
        var env = System.Environment.GetEnvironmentVariable("CHROME_EXECUTABLE");
        if (!string.IsNullOrEmpty(env))
        {
            return File.Exists(env) ? env : FileSystemUtil.FindOnPath(env);
        }
        if (OperatingSystem.IsMacOS())
        {
            const string mac = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
            return File.Exists(mac) ? mac : null;
        }
        if (OperatingSystem.IsWindows())
        {
            foreach (var baseDir in new[] { "ProgramFiles", "ProgramFiles(x86)", "LOCALAPPDATA" })
            {
                var root = System.Environment.GetEnvironmentVariable(baseDir);
                if (root is null)
                {
                    continue;
                }
                var path = Path.Combine(root, "Google", "Chrome", "Application", "chrome.exe");
                if (File.Exists(path))
                {
                    return path;
                }
            }
            return null;
        }
        foreach (var name in new[] { "google-chrome", "google-chrome-stable", "chromium", "chromium-browser" })
        {
            if (FileSystemUtil.FindOnPath(name) is { } found)
            {
                return found;
            }
        }
        // Playwright's bundled Chromium (CI images, cloud dev boxes).
        const string playwright = "/opt/pw-browsers";
        if (Directory.Exists(playwright))
        {
            return Directory.EnumerateFiles(playwright, "chrome", SearchOption.AllDirectories)
                .FirstOrDefault(p => p.Contains("chrome-linux", StringComparison.Ordinal));
        }
        return null;
    }

    public Emsdk RequireEmsdk() => FindEmsdk() ?? throw new ToolExitException(
        "Emscripten SDK not found. Run `phoenix precache --web` to install the pinned version " +
        $"({RequiredEmscriptenVersion}) into {EmsdkCacheDir}, or set EMSDK.");

    public static string RequireCMake() => CMake ?? throw new ToolExitException("cmake (>= 3.25) not found on PATH.");
}
