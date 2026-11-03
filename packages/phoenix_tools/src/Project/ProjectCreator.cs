// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using Phoenix.Tools.Infrastructure;
using Phoenix.Tools.Sdk;

namespace Phoenix.Tools.Project;

/// <summary>Parameters for `phoenix create`.</summary>
public sealed record CreateOptions(string Directory, string? ProjectName = null, string? Description = null,
    string? Title = null, bool Overwrite = false);

/// <summary>Renders templates/app into a new project directory.</summary>
public sealed class ProjectCreator(PhoenixSdk sdk)
{
    public const string DefaultDescription = "A new Phoenix project.";
    public const string DefaultTitle = "Phoenix Demo Home Page";

    /// <summary>Creates the project and returns the list of written files (relative paths).</summary>
    public List<string> Create(CreateOptions options)
    {
        var target = Path.GetFullPath(options.Directory);
        var name = options.ProjectName ?? Path.GetFileName(target.TrimEnd(Path.DirectorySeparatorChar, '/'));
        if (!PhoenixProject.IsValidName(name))
        {
            throw new ToolExitException(
                $"\"{name}\" is not a valid Phoenix project name.\n" +
                "Use lower_snake_case (e.g. my_app), start with a letter, and avoid reserved names. " +
                "Pass --project-name to choose a name different from the directory.");
        }
        if (Directory.Exists(target) && Directory.EnumerateFileSystemEntries(target).Any() && !options.Overwrite &&
            File.Exists(Path.Combine(target, PhoenixProject.ManifestFileName)))
        {
            throw new ToolExitException($"{target} already contains a Phoenix project. Use --overwrite to replace template files.");
        }

        var values = new Dictionary<string, string>
        {
            ["name"] = name,
            ["description"] = options.Description ?? DefaultDescription,
            ["title"] = options.Title ?? DefaultTitle,
        };
        var templateDir = Path.Combine(sdk.TemplatesDir, "app");
        var written = new List<string>();
        foreach (var file in Directory.EnumerateFiles(templateDir, "*", SearchOption.AllDirectories).Order(StringComparer.Ordinal))
        {
            var relative = Path.GetRelativePath(templateDir, file);
            if (relative == "gitignore.tmpl")
            {
                relative = ".gitignore";
            }
            var destination = Path.Combine(target, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            File.WriteAllText(destination, Render(File.ReadAllText(file), values));
            written.Add(relative.Replace('\\', '/'));
        }
        return written;
    }

    /// <summary>Replaces {{key}} placeholders. Unknown keys are left untouched.</summary>
    public static string Render(string template, IReadOnlyDictionary<string, string> values)
    {
        foreach (var (key, value) in values)
        {
            template = template.Replace("{{" + key + "}}", value, StringComparison.Ordinal);
        }
        return template;
    }
}
