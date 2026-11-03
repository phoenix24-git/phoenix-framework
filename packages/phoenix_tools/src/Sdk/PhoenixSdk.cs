// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using Phoenix.Tools.Infrastructure;

namespace Phoenix.Tools.Sdk;

/// <summary>
/// The Phoenix SDK checkout the tool runs from (like FLUTTER_ROOT): engine
/// sources, templates, web runtime, and the shared cache in bin/cache.
/// </summary>
public sealed class PhoenixSdk
{
    private PhoenixSdk(string root)
    {
        Root = root;
        Version = File.ReadAllText(Path.Combine(root, "VERSION")).Trim();
    }

    public string Root { get; }
    public string Version { get; }
    public string EngineDir => Path.Combine(Root, "engine");
    public string TemplatesDir => Path.Combine(Root, "templates");
    public string WebRuntimeDir => Path.Combine(EngineDir, "web");
    public string DevToolsDir => Path.Combine(WebRuntimeDir, "devtools");
    public string CacheDir => Path.Combine(Root, "bin", "cache");
    public string WasmToolchainFile => Path.Combine(EngineDir, "cmake", "toolchains", "emscripten-wasm64.cmake");

    /// <summary>Locates the SDK from PHOENIX_ROOT or by walking up from the tool binary.</summary>
    public static PhoenixSdk Locate()
    {
        var env = Environment.GetEnvironmentVariable("PHOENIX_ROOT");
        if (!string.IsNullOrEmpty(env))
        {
            return IsSdkRoot(env)
                ? new PhoenixSdk(Path.GetFullPath(env))
                : throw new ToolExitException($"PHOENIX_ROOT ({env}) is not a Phoenix SDK checkout.");
        }
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            if (IsSdkRoot(dir.FullName))
            {
                return new PhoenixSdk(dir.FullName);
            }
        }
        throw new ToolExitException("Could not locate the Phoenix SDK. Set PHOENIX_ROOT or run bin/phoenix from a checkout.");
    }

    /// <summary>Creates an SDK handle for an explicit root (tests).</summary>
    public static PhoenixSdk FromRoot(string root) =>
        IsSdkRoot(root) ? new PhoenixSdk(Path.GetFullPath(root)) : throw new ToolExitException($"{root} is not a Phoenix SDK.");

    public static bool IsSdkRoot(string dir) =>
        File.Exists(Path.Combine(dir, "VERSION")) && File.Exists(Path.Combine(dir, "engine", "CMakeLists.txt"));
}
