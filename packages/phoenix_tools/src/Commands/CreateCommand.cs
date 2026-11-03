// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.CommandLine;
using Phoenix.Tools.Project;

namespace Phoenix.Tools.Commands;

internal static class CreateCommand
{
    public static Command Build(ToolContext ctx)
    {
        var output = new Argument<string>("output directory") { Description = "Where to create the project; its name becomes the project name." };
        var projectName = new Option<string?>("--project-name") { Description = "The project name (lower_snake_case). Defaults to the directory name." };
        var description = new Option<string?>("--description") { Description = "Description written to phoenix.json." };
        var title = new Option<string?>("--title") { Description = "Home page title for the template app." };
        var overwrite = new Option<bool>("--overwrite") { Description = "Overwrite existing template files." };

        var command = new Command("create", "Create a new Phoenix project.") { output, projectName, description, title, overwrite };
        command.SetAction(parse =>
        {
            var dir = Path.GetFullPath(Path.Combine(ctx.WorkingDirectory, parse.GetValue(output)!));
            var t = ctx.Terminal;
            t.Line($"Creating project {Path.GetFileName(dir)}...");
            var files = new ProjectCreator(ctx.Sdk).Create(new CreateOptions(dir, parse.GetValue(projectName),
                parse.GetValue(description), parse.GetValue(title), parse.GetValue(overwrite)));
            foreach (var f in files)
            {
                t.Trace($"  {f}");
            }
            t.Line($"Wrote {files.Count} files.");
            t.Line();
            t.Success("All done!");
            t.Line("In order to run your application, type:");
            t.Line();
            t.Line($"  $ cd {Path.GetRelativePath(ctx.WorkingDirectory, dir)}");
            t.Line("  $ phoenix run");
            t.Line();
            t.Line($"Your application code is in {Path.Combine(Path.GetRelativePath(ctx.WorkingDirectory, dir), "lib", "main.cpp")}.");
            return 0;
        });
        return command;
    }
}
