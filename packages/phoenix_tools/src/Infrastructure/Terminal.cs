// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
namespace Phoenix.Tools.Infrastructure;

/// <summary>Console output with optional ANSI colour, mirroring flutter_tools' terminal.</summary>
public sealed class Terminal
{
    private readonly TextWriter _out;
    private readonly TextWriter _err;
    private readonly bool _color;
    private readonly object _lock = new();

    public Terminal(TextWriter? output = null, TextWriter? error = null, bool? color = null)
    {
        _out = output ?? Console.Out;
        _err = error ?? Console.Error;
        _color = color ?? (!Console.IsOutputRedirected && Environment.GetEnvironmentVariable("NO_COLOR") is null);
    }

    public static Terminal Default { get; } = new();

    public bool Verbose { get; set; }

    public void Line(string text = "") { lock (_lock) { _out.WriteLine(text); } }

    public void Status(string text) => Line(Paint(text, "1"));

    public void Success(string text) => Line(Paint(text, "32"));

    public void Warning(string text) { lock (_lock) { _err.WriteLine(Paint(text, "33")); } }

    public void Error(string text) { lock (_lock) { _err.WriteLine(Paint(text, "31")); } }

    public void Trace(string text)
    {
        if (Verbose)
        {
            Line(Paint(text, "90"));
        }
    }

    public string Bold(string text) => Paint(text, "1");

    private string Paint(string text, string code) => _color ? $"\u001b[{code}m{text}\u001b[0m" : text;
}

/// <summary>A user-facing failure: printed without a stack trace, exit code 1.</summary>
public sealed class ToolExitException(string message, int exitCode = 1) : Exception(message)
{
    public int ExitCode { get; } = exitCode;
}
