// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
namespace Phoenix.Tools.Infrastructure;

internal static class FileSystemUtil
{
    /// <summary>Recursively copies <paramref name="source"/> into <paramref name="destination"/>.</summary>
    public static int CopyDirectory(string source, string destination, Func<string, bool>? include = null)
    {
        var count = 0;
        Directory.CreateDirectory(destination);
        foreach (var file in Directory.EnumerateFiles(source, "*", SearchOption.AllDirectories))
        {
            var relative = Path.GetRelativePath(source, file);
            if (include is not null && !include(relative))
            {
                continue;
            }
            var target = Path.Combine(destination, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            File.Copy(file, target, overwrite: true);
            count++;
        }
        return count;
    }

    /// <summary>Searches PATH (and Windows PATHEXT) for an executable.</summary>
    public static string? FindOnPath(string name)
    {
        var paths = (Environment.GetEnvironmentVariable("PATH") ?? string.Empty).Split(Path.PathSeparator,
            StringSplitOptions.RemoveEmptyEntries);
        var extensions = OperatingSystem.IsWindows()
            ? (Environment.GetEnvironmentVariable("PATHEXT") ?? ".EXE;.CMD;.BAT").Split(';')
            : [string.Empty];
        foreach (var dir in paths)
        {
            foreach (var ext in extensions)
            {
                var candidate = Path.Combine(dir, name + ext.ToLowerInvariant());
                if (File.Exists(candidate))
                {
                    return candidate;
                }
                candidate = Path.Combine(dir, name + ext);
                if (File.Exists(candidate))
                {
                    return candidate;
                }
            }
        }
        return null;
    }
}
