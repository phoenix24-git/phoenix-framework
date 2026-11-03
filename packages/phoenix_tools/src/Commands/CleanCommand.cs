// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.CommandLine;
using Phoenix.Tools.Project;

namespace Phoenix.Tools.Commands;

internal static class CleanCommand
{
    public static Command Build(ToolContext ctx)
    {
        var command = new Command("clean", "Delete the build/ and .phoenix_tool/ directories.");
        command.SetAction(_ =>
        {
            var project = PhoenixProject.Find(ctx.WorkingDirectory);
            foreach (var dir in new[] { project.BuildDir, project.ToolStateDir })
            {
                if (Directory.Exists(dir))
                {
                    ctx.Terminal.Line($"Deleting {Path.GetFileName(dir)}...");
                    Directory.Delete(dir, recursive: true);
                }
            }
            return 0;
        });
        return command;
    }
}
