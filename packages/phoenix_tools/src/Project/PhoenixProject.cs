// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;
using Phoenix.Tools.Infrastructure;

namespace Phoenix.Tools.Project;

/// <summary>Virtual-console hardware profile (phoenix.json "console").</summary>
public sealed record ConsoleSettings
{
    public int VramMB { get; set; } = 3584;
    public bool VramPreallocate { get; set; } = true;
    public int WorkerThreads { get; set; } = 6;
    public int CpuArenaMB { get; set; } = 256;
}

/// <summary>Web-specific settings (phoenix.json "web").</summary>
public sealed record WebSettings
{
    public int InitialMemoryMB { get; set; } = 1024;
    public int MaximumMemoryMB { get; set; } = 16384;
}

/// <summary>The phoenix.json manifest (Phoenix's pubspec.yaml).</summary>
public sealed record PhoenixManifest
{
    public string Name { get; set; } = string.Empty;
    public string Description { get; set; } = string.Empty;
    public string Version { get; set; } = "1.0.0+1";
    public string? Title { get; set; }
    public Dictionary<string, string> Environment { get; set; } = [];
    public ConsoleSettings Console { get; set; } = new();
    public WebSettings Web { get; set; } = new();
}

[JsonSourceGenerationOptions(PropertyNamingPolicy = JsonKnownNamingPolicy.CamelCase, WriteIndented = true,
    ReadCommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true)]
[JsonSerializable(typeof(PhoenixManifest))]
internal sealed partial class ManifestJsonContext : JsonSerializerContext;

/// <summary>A Phoenix app on disk: a directory containing phoenix.json.</summary>
public sealed partial class PhoenixProject
{
    public const string ManifestFileName = "phoenix.json";

    private PhoenixProject(string directory, PhoenixManifest manifest)
    {
        Directory = directory;
        Manifest = manifest;
    }

    public string Directory { get; }
    public PhoenixManifest Manifest { get; }
    public string Name => Manifest.Name;
    public string Title => string.IsNullOrWhiteSpace(Manifest.Title) ? Manifest.Name : Manifest.Title!;
    public string BuildDir => Path.Combine(Directory, "build");
    public string ToolStateDir => Path.Combine(Directory, ".phoenix_tool");
    public string ManifestPath => Path.Combine(Directory, ManifestFileName);

    [GeneratedRegex("^[a-z][a-z0-9_]{0,63}$")]
    private static partial Regex ValidName();

    public static bool IsValidName(string name) => ValidName().IsMatch(name) && !ReservedNames.Contains(name);

    private static readonly HashSet<string> ReservedNames = ["phoenix", "phoenix_test", "phoenix_tools", "test", "main"];

    /// <summary>Finds phoenix.json in <paramref name="start"/> or its parents (like pubspec discovery).</summary>
    public static PhoenixProject Find(string start)
    {
        for (var dir = new DirectoryInfo(Path.GetFullPath(start)); dir is not null; dir = dir.Parent)
        {
            if (File.Exists(Path.Combine(dir.FullName, ManifestFileName)))
            {
                return Load(dir.FullName);
            }
        }
        throw new ToolExitException($"No {ManifestFileName} found in {start} or any parent directory.\n" +
                                    "Run this command inside a Phoenix project, or create one with `phoenix create <name>`.");
    }

    public static PhoenixProject Load(string directory)
    {
        var path = Path.Combine(directory, ManifestFileName);
        PhoenixManifest? manifest;
        try
        {
            manifest = JsonSerializer.Deserialize(File.ReadAllText(path), ManifestJsonContext.Default.PhoenixManifest);
        }
        catch (JsonException e)
        {
            throw new ToolExitException($"{path}: invalid JSON: {e.Message}");
        }
        if (manifest is null)
        {
            throw new ToolExitException($"{path}: empty manifest");
        }
        // Explicit nulls (or omitted sections with some serializers) fall back to defaults.
        manifest = manifest with
        {
            Console = manifest.Console ?? new ConsoleSettings(),
            Web = manifest.Web ?? new WebSettings(),
            Environment = manifest.Environment ?? [],
            Name = manifest.Name ?? string.Empty,
            Description = manifest.Description ?? string.Empty,
        };
        var errors = Validate(manifest);
        if (errors.Count > 0)
        {
            throw new ToolExitException($"{path} has errors:\n  - " + string.Join("\n  - ", errors));
        }
        return new PhoenixProject(Path.GetFullPath(directory), manifest);
    }

    /// <summary>Returns human-readable validation errors (empty when valid).</summary>
    public static List<string> Validate(PhoenixManifest m)
    {
        var errors = new List<string>();
        if (!IsValidName(m.Name))
        {
            errors.Add($"name \"{m.Name}\" must be lower_snake_case (a-z, 0-9, _), start with a letter, and not be reserved");
        }
        if (m.Console.WorkerThreads is < 0 or > 64)
        {
            errors.Add("console.workerThreads must be between 0 and 64");
        }
        if (m.Console.VramMB is < 16 or > 65536)
        {
            errors.Add("console.vramMB must be between 16 and 65536");
        }
        if (m.Console.CpuArenaMB is < 1 or > 16384)
        {
            errors.Add("console.cpuArenaMB must be between 1 and 16384");
        }
        if (m.Web.InitialMemoryMB < 64 || m.Web.MaximumMemoryMB < m.Web.InitialMemoryMB)
        {
            errors.Add("web.initialMemoryMB must be >= 64 and <= web.maximumMemoryMB");
        }
        if (m.Web.MaximumMemoryMB > 65536)
        {
            errors.Add("web.maximumMemoryMB must be <= 65536 (16 GiB is the browser limit today)");
        }
        return errors;
    }
}
