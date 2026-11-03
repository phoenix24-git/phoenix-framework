// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Runtime.InteropServices;
using Phoenix.Interop;

namespace Phoenix;

/// <summary>Base class for gameplay scripts (like a Godot/Unity script, driven by the engine).</summary>
public abstract class PhoenixScript
{
    /// <summary>Called once after the engine attached.</summary>
    public virtual void OnStart() { }

    /// <summary>Called every frame before the UI is built.</summary>
    public virtual void OnUpdate(in PhxFrameInfo frame) { }

    /// <summary>Called before the module is unloaded.</summary>
    public virtual void OnShutdown() { }
}

/// <summary>
/// Hosts the active script. A script module exports <c>phoenix_script_init</c>
/// itself (one line, so the script type is known statically and fully AOT-safe):
/// <code>
/// [UnmanagedCallersOnly(EntryPoint = "phoenix_script_init")]
/// public static unsafe int Init(PhxEngineApi* api) => ScriptHost.Init&lt;MyScript&gt;(api);
/// </code>
/// <c>phoenix_script_update</c>/<c>phoenix_script_shutdown</c> come from
/// <see cref="ScriptExports"/> in this assembly.
/// </summary>
public static unsafe class ScriptHost
{
    private static PhoenixScript? s_script;

    /// <summary>Attaches to the engine and starts <typeparamref name="T"/>. Returns 0 on success.</summary>
    public static int Init<T>(PhxEngineApi* api)
        where T : PhoenixScript, new()
    {
        if (!IsCompatible(api))
        {
            return -1;
        }
        s_script = new T();
        return Start(api);
    }

    /// <summary>Attaches with an existing script instance (tests, custom hosting).</summary>
    public static int Init(PhxEngineApi* api, PhoenixScript script)
    {
        ArgumentNullException.ThrowIfNull(script);
        if (!IsCompatible(api))
        {
            return -1;
        }
        s_script = script;
        return Start(api);
    }

    internal static bool IsCompatible(PhxEngineApi* api) =>
        api is not null && api->AbiVersion == ScriptAbi.AbiVersion && api->StructSize >= (uint)sizeof(PhxEngineApi);

    private static int Start(PhxEngineApi* api)
    {
        Engine.Attach(api);
        try
        {
            s_script!.OnStart();
            return 0;
        }
        catch (Exception e)
        {
            Engine.Log("Phoenix C#: OnStart threw " + e.GetType().Name + ": " + e.Message, ScriptAbi.LogError);
            return -3;
        }
    }

    /// <summary>phoenix_script_update. Exceptions never cross into C++.</summary>
    public static void Update(PhxFrameInfo* frame)
    {
        try
        {
            s_script?.OnUpdate(in *frame);
        }
        catch (Exception e)
        {
            Engine.Log("Phoenix C#: OnUpdate threw " + e.GetType().Name + ": " + e.Message, ScriptAbi.LogError);
        }
    }

    /// <summary>phoenix_script_shutdown.</summary>
    public static void Shutdown()
    {
        try
        {
            s_script?.OnShutdown();
        }
        finally
        {
            s_script = null;
            Engine.Detach();
        }
    }
}

/// <summary>
/// Native exports. NativeAOT turns each [UnmanagedCallersOnly(EntryPoint=...)]
/// method into a real C symbol in the shared library, callable from C++ as if
/// it were written in C. Script projects opt in with
/// <c>&lt;UnmanagedEntryPointsAssembly Include="Phoenix.Framework" /&gt;</c>.
/// </summary>
public static unsafe class ScriptExports
{
    [UnmanagedCallersOnly(EntryPoint = "phoenix_script_update")]
    public static void Update(PhxFrameInfo* frame) => ScriptHost.Update(frame);

    [UnmanagedCallersOnly(EntryPoint = "phoenix_script_shutdown")]
    public static void Shutdown() => ScriptHost.Shutdown();
}
