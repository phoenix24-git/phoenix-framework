// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.CommandLine;
using Phoenix.Tools.Build;

namespace Phoenix.Tools.Commands;

internal static class DevicesCommand
{
    internal sealed record Device(string Name, string Id, string Platform, string Detail);

    internal static List<Device> Discover()
    {
        var devices = new List<Device>();
        if (Toolchain.FindChrome() is { } chrome)
        {
            devices.Add(new Device("Chrome", "chrome", "web-javascript", chrome));
        }
        devices.Add(new Device("Web Server", "web-server", "web-javascript", "serves the app; open it in any wasm64 browser"));
        var host = BuildTargetExtensions.HostDesktop();
        devices.Add(new Device(host.ToString() + " (desktop)", host.CliName(), $"{host.CliName()}-{System.Runtime.InteropServices.RuntimeInformation.OSArchitecture.ToString().ToLowerInvariant()}",
            System.Runtime.InteropServices.RuntimeInformation.OSDescription));
        return devices;
    }

    public static Command Build(ToolContext ctx)
    {
        var command = new Command("devices", "List all connected devices.");
        command.SetAction(_ =>
        {
            var devices = Discover();
            ctx.Terminal.Line($"Found {devices.Count} connected devices:");
            foreach (var d in devices)
            {
                ctx.Terminal.Line($"  {d.Name,-20} • {d.Id,-11} • {d.Platform,-15} • {d.Detail}");
            }
            return 0;
        });
        return command;
    }
}
