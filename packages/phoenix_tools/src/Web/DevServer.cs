// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Collections.Concurrent;
using System.Net;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Hosting.Server;
using Microsoft.AspNetCore.Hosting.Server.Features;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.StaticFiles;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.FileProviders;
using Microsoft.Extensions.Logging;

namespace Phoenix.Tools.Web;

/// <summary>Configuration for the `phoenix run` web server.</summary>
public sealed record DevServerOptions(
    string WebRoot,
    string DevToolsRoot,
    string Hostname = "127.0.0.1",
    int Port = 0,
    string? AppSourceRoot = null,
    string? SdkSourceRoot = null);

/// <summary>
/// The development web server: serves build/web with the cross-origin
/// isolation headers wasm64 pthreads need, pushes hot-restart events over
/// Server-Sent Events, serves Phoenix DevTools and (in debug) the C++ sources
/// referenced by the wasm source map so Chrome DevTools can step through them.
/// </summary>
public sealed class DevServer : IAsyncDisposable
{
    public const string EventsPath = "/__phoenix/events";
    public const string DevToolsPath = "/__phoenix/devtools";

    private static readonly string[] SourceExtensions = [".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".inl"];
    private readonly ConcurrentDictionary<Guid, Func<string, Task>> _clients = new();
    private readonly WebApplication _app;

    private DevServer(WebApplication app) => _app = app;

    public Uri Address { get; private set; } = new("http://127.0.0.1/");

    public int ConnectedClients => _clients.Count;

    public static async Task<DevServer> StartAsync(DevServerOptions options, CancellationToken ct = default)
    {
        var builder = WebApplication.CreateSlimBuilder(new WebApplicationOptions { ContentRootPath = options.WebRoot });
        builder.Logging.ClearProviders();
        builder.WebHost.UseKestrel(k =>
        {
            if (options.Hostname is "localhost" or "127.0.0.1")
            {
                k.Listen(IPAddress.Loopback, options.Port);
            }
            else
            {
                k.Listen(IPAddress.Parse(options.Hostname == "0.0.0.0" ? "0.0.0.0" : options.Hostname), options.Port);
            }
        });
        var app = builder.Build();
        var server = new DevServer(app);
        server.Configure(options);
        await app.StartAsync(ct).ConfigureAwait(false);
        var address = app.Services.GetRequiredService<IServer>().Features.Get<IServerAddressesFeature>()!.Addresses.First();
        server.Address = new Uri(address.Replace("[::]", "localhost", StringComparison.Ordinal) + "/");
        return server;
    }

    /// <summary>Content types the browser must see for wasm/source maps.</summary>
    public static FileExtensionContentTypeProvider ContentTypes()
    {
        var provider = new FileExtensionContentTypeProvider();
        provider.Mappings[".wasm"] = "application/wasm";
        provider.Mappings[".map"] = "application/json";
        provider.Mappings[".js"] = "text/javascript";
        provider.Mappings[".mjs"] = "text/javascript";
        foreach (var ext in SourceExtensions)
        {
            provider.Mappings[ext] = "text/plain; charset=utf-8";
        }
        return provider;
    }

    private void Configure(DevServerOptions options)
    {
        _app.Use(async (context, next) =>
        {
            var headers = context.Response.Headers;
            headers["Cross-Origin-Opener-Policy"] = "same-origin";
            headers["Cross-Origin-Embedder-Policy"] = "require-corp";
            headers["Cross-Origin-Resource-Policy"] = "same-origin";
            headers.CacheControl = "no-store";
            await next(context).ConfigureAwait(false);
        });

        _app.MapGet(EventsPath, ServeEventsAsync);
        _app.Use(async (context, next) =>
        {
            // /__phoenix/devtools -> /__phoenix/devtools/ so relative URLs resolve.
            if (context.Request.Path.Value == DevToolsPath)
            {
                context.Response.Redirect(DevToolsPath + "/");
                return;
            }
            await next(context).ConfigureAwait(false);
        });

        var contentTypes = ContentTypes();
        MountStatic(DevToolsPath, options.DevToolsRoot, contentTypes, sourcesOnly: false);
        if (options.AppSourceRoot is not null)
        {
            MountStatic("/__phoenix/app", options.AppSourceRoot, contentTypes, sourcesOnly: true);
        }
        if (options.SdkSourceRoot is not null)
        {
            MountStatic("/__phoenix/sdk", options.SdkSourceRoot, contentTypes, sourcesOnly: true);
        }
        _app.UseDefaultFiles(new DefaultFilesOptions { FileProvider = new PhysicalFileProvider(options.WebRoot) });
        _app.UseStaticFiles(new StaticFileOptions
        {
            FileProvider = new PhysicalFileProvider(options.WebRoot),
            ContentTypeProvider = contentTypes,
            ServeUnknownFileTypes = false,
        });
    }

    private void MountStatic(string path, string root, FileExtensionContentTypeProvider types, bool sourcesOnly)
    {
        if (!Directory.Exists(root))
        {
            return;
        }
        var provider = new PhysicalFileProvider(root);
        _app.UseWhen(c => c.Request.Path.StartsWithSegments(path, out var rest) && Allowed(rest, sourcesOnly), branch =>
        {
            branch.UseDefaultFiles(new DefaultFilesOptions { FileProvider = provider, RequestPath = path });
            branch.UseStaticFiles(new StaticFileOptions { FileProvider = provider, RequestPath = path, ContentTypeProvider = types });
        });
    }

    /// <summary>Source mounts only expose C/C++ sources, never arbitrary files.</summary>
    internal static bool Allowed(PathString rest, bool sourcesOnly)
    {
        if (!sourcesOnly)
        {
            return true;
        }
        var value = rest.Value ?? string.Empty;
        return !value.Contains("..", StringComparison.Ordinal) &&
               SourceExtensions.Contains(Path.GetExtension(value), StringComparer.OrdinalIgnoreCase);
    }

    private async Task ServeEventsAsync(HttpContext context)
    {
        context.Response.Headers.ContentType = "text/event-stream";
        var id = Guid.NewGuid();
        var gate = new SemaphoreSlim(1, 1);
        async Task Send(string evt)
        {
            await gate.WaitAsync().ConfigureAwait(false);
            try
            {
                await context.Response.WriteAsync($"event: {evt}\ndata: {{}}\n\n").ConfigureAwait(false);
                await context.Response.Body.FlushAsync().ConfigureAwait(false);
            }
            finally
            {
                gate.Release();
            }
        }
        _clients[id] = Send;
        try
        {
            await Send("connected").ConfigureAwait(false);
            await Task.Delay(Timeout.Infinite, context.RequestAborted).ConfigureAwait(false);
        }
        catch (OperationCanceledException)
        {
            // Client went away.
        }
        finally
        {
            _clients.TryRemove(id, out _);
        }
    }

    /// <summary>Asks every connected page to reload (hot restart). Returns the number notified.</summary>
    public async Task<int> BroadcastReloadAsync()
    {
        var count = 0;
        foreach (var (_, send) in _clients)
        {
            try
            {
                await send("reload").ConfigureAwait(false);
                count++;
            }
            catch (Exception e) when (e is IOException or ObjectDisposedException or InvalidOperationException)
            {
                // Disconnected client; it is removed when its request ends.
            }
        }
        return count;
    }

    public async ValueTask DisposeAsync()
    {
        await _app.StopAsync().ConfigureAwait(false);
        await _app.DisposeAsync().ConfigureAwait(false);
    }
}
