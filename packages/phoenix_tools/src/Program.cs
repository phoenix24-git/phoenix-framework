// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.CommandLine;
using Phoenix.Tools.Commands;
using Phoenix.Tools.Infrastructure;

namespace Phoenix.Tools;

public static class Program
{
    public static Task<int> Main(string[] args) => RunAsync(args, new ToolContext(Terminal.Default));

    /// <summary>Entry point used by tests with an injected context.</summary>
    public static async Task<int> RunAsync(string[] args, ToolContext ctx)
    {
        var root = BuildRootCommand(ctx);
        try
        {
            return await root.Parse(args).InvokeAsync().ConfigureAwait(false);
        }
        catch (ToolExitException e)
        {
            ctx.Terminal.Error(e.Message);
            return e.ExitCode;
        }
    }

    internal static RootCommand BuildRootCommand(ToolContext ctx)
    {
        var verbose = new Option<bool>("--verbose", "-v") { Description = "Noisy logging, including all shell commands executed.", Recursive = true };
        var root = new RootCommand("Manage your Phoenix app development.\n\nCommon commands:\n\n  phoenix create <output directory>\n    Create a new Phoenix project in the specified directory.\n\n  phoenix run [options]\n    Run your Phoenix application on an attached device or in a browser.")
        {
            verbose,
        };
        root.Subcommands.Add(CreateCommand.Build(ctx));
        root.Subcommands.Add(RunCommand.Build(ctx));
        root.Subcommands.Add(BuildCommand.Build(ctx));
        root.Subcommands.Add(TestCommand.Build(ctx));
        root.Subcommands.Add(CleanCommand.Build(ctx));
        root.Subcommands.Add(DevicesCommand.Build(ctx));
        root.Subcommands.Add(DoctorCommand.Build(ctx));
        root.Subcommands.Add(PrecacheCommand.Build(ctx));

        // Apply -v before any command action runs. ToolExitException is rethrown so
        // RunAsync can print it without a stack trace.
        foreach (var command in root.Subcommands.SelectMany(Flatten))
        {
            if (command.Action is { } action)
            {
                command.Action = new WrappedAction(action, verbose, ctx);
            }
        }
        return root;
    }

    private static IEnumerable<Command> Flatten(Command c) => c.Subcommands.SelectMany(Flatten).Prepend(c);

    private sealed class WrappedAction(System.CommandLine.Invocation.CommandLineAction inner, Option<bool> verbose, ToolContext ctx)
        : System.CommandLine.Invocation.AsynchronousCommandLineAction
    {
        public override async Task<int> InvokeAsync(ParseResult parseResult, CancellationToken cancellationToken = default)
        {
            ctx.Terminal.Verbose = parseResult.GetValue(verbose);
            try
            {
                return inner switch
                {
                    System.CommandLine.Invocation.AsynchronousCommandLineAction a => await a.InvokeAsync(parseResult, cancellationToken).ConfigureAwait(false),
                    System.CommandLine.Invocation.SynchronousCommandLineAction s => s.Invoke(parseResult),
                    _ => 1,
                };
            }
            catch (ToolExitException e)
            {
                ctx.Terminal.Error(e.Message);
                return e.ExitCode;
            }
        }
    }
}
