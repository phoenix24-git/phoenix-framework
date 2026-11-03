// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Net;
using Microsoft.AspNetCore.Http;
using Phoenix.Tools.Web;
using Xunit;

namespace Phoenix.Tools.Tests;

public sealed class DevServerTests
{
    private static async Task<(DevServer Server, TempDir Web, TempDir App)> StartAsync()
    {
        var web = new TempDir();
        var app = new TempDir();
        File.WriteAllText(Path.Combine(web.Path, "index.html"), "<html>phoenix</html>");
        File.WriteAllBytes(Path.Combine(web.Path, "app.wasm"), [0, 97, 115, 109, 1, 0, 0, 0]);
        Directory.CreateDirectory(Path.Combine(app.Path, "lib"));
        File.WriteAllText(Path.Combine(app.Path, "lib", "main.cpp"), "int x;");
        File.WriteAllText(Path.Combine(app.Path, "secret.txt"), "nope");
        var server = await DevServer.StartAsync(new DevServerOptions(web.Path, TestSupport.Sdk.DevToolsDir, AppSourceRoot: app.Path),
            TestContext.Current.CancellationToken);
        return (server, web, app);
    }

    [Fact]
    public async Task ServesCrossOriginIsolationHeadersAndWasmMime()
    {
        var (server, web, app) = await StartAsync();
        await using var _ = server;
        using var __ = web;
        using var ___ = app;
        using var http = new HttpClient { BaseAddress = server.Address };

        var index = await http.GetAsync("/", TestContext.Current.CancellationToken);
        Assert.Equal(HttpStatusCode.OK, index.StatusCode);
        Assert.Equal("same-origin", index.Headers.GetValues("Cross-Origin-Opener-Policy").Single());
        Assert.Equal("require-corp", index.Headers.GetValues("Cross-Origin-Embedder-Policy").Single());

        var wasm = await http.GetAsync("/app.wasm", TestContext.Current.CancellationToken);
        Assert.Equal("application/wasm", wasm.Content.Headers.ContentType!.MediaType);

        var devtools = await http.GetStringAsync(DevServer.DevToolsPath + "/", TestContext.Current.CancellationToken);
        Assert.Contains("Phoenix DevTools", devtools, StringComparison.Ordinal);
    }

    [Fact]
    public async Task SourceMountsOnlyExposeSourceFiles()
    {
        var (server, web, app) = await StartAsync();
        await using var _ = server;
        using var __ = web;
        using var ___ = app;
        using var http = new HttpClient { BaseAddress = server.Address };
        Assert.Equal(HttpStatusCode.OK, (await http.GetAsync("/__phoenix/app/lib/main.cpp", TestContext.Current.CancellationToken)).StatusCode);
        Assert.Equal(HttpStatusCode.NotFound, (await http.GetAsync("/__phoenix/app/secret.txt", TestContext.Current.CancellationToken)).StatusCode);
    }

    [Theory]
    [InlineData("/lib/main.cpp", true, true)]
    [InlineData("/src/a.h", true, true)]
    [InlineData("/../etc/passwd.h", true, false)]
    [InlineData("/phoenix.json", true, false)]
    [InlineData("/anything.txt", false, true)]
    public void AllowedFilter(string path, bool sourcesOnly, bool expected) =>
        Assert.Equal(expected, DevServer.Allowed(new PathString(path), sourcesOnly));

    [Fact]
    public async Task HotRestartIsBroadcastOverServerSentEvents()
    {
        var (server, web, app) = await StartAsync();
        await using var _ = server;
        using var __ = web;
        using var ___ = app;
        using var http = new HttpClient { BaseAddress = server.Address };
        using var response = await http.GetAsync(DevServer.EventsPath, HttpCompletionOption.ResponseHeadersRead, TestContext.Current.CancellationToken);
        Assert.Equal("text/event-stream", response.Content.Headers.ContentType!.MediaType);
        using var reader = new StreamReader(await response.Content.ReadAsStreamAsync(TestContext.Current.CancellationToken));
        Assert.Equal("event: connected", await reader.ReadLineAsync(TestContext.Current.CancellationToken));

        var deadline = DateTime.UtcNow.AddSeconds(5);
        while (server.ConnectedClients == 0 && DateTime.UtcNow < deadline)
        {
            await Task.Delay(20, TestContext.Current.CancellationToken);
        }
        Assert.Equal(1, await server.BroadcastReloadAsync());
        string? line;
        do
        {
            line = await reader.ReadLineAsync(TestContext.Current.CancellationToken);
        }
        while (line is not null && !line.StartsWith("event: reload", StringComparison.Ordinal));
        Assert.Equal("event: reload", line);
    }
}
