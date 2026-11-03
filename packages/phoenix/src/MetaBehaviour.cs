// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Runtime.InteropServices;
using Phoenix.Interop;

namespace Phoenix;

/// <summary>
/// Phoenix's MonoBehaviour: derive from it and override the Unity lifecycle methods you need.
/// The engine calls Awake and Start when the script module attaches, Update every frame, and
/// OnDestroy before the module unloads. Frame timing is available through <see cref="Time"/>.
/// </summary>
public abstract class MetaBehaviour
{
    /// <summary>Called once, first, when the script attaches.</summary>
    protected internal virtual void Awake() { }

    /// <summary>Called once after Awake, before the first Update.</summary>
    protected internal virtual void Start() { }

    /// <summary>Called every frame (see <see cref="Time.deltaTime"/>).</summary>
    protected internal virtual void Update() { }

    /// <summary>Called once before the script module unloads.</summary>
    protected internal virtual void OnDestroy() { }
}

/// <summary>Unity-style frame timing, updated by the engine before each Update.</summary>
public static class Time
{
#pragma warning disable CA1707, IDE1006 // Unity's lowerCamelCase names, on purpose: ported code compiles unchanged.
    /// <summary>Seconds since the previous frame.</summary>
    public static float deltaTime { get; internal set; }

    /// <summary>Seconds since the engine started.</summary>
    public static double time { get; internal set; }

    /// <summary>Frames rendered so far.</summary>
    public static long frameCount { get; internal set; }

    /// <summary>The raw frame info the engine passed for the current frame.</summary>
    public static PhxFrameInfo frame { get; internal set; }
#pragma warning restore CA1707, IDE1006
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
    private static MetaBehaviour? s_script;

    /// <summary>Attaches to the engine and starts <typeparamref name="T"/>. Returns 0 on success.</summary>
    public static int Init<T>(PhxEngineApi* api)
        where T : MetaBehaviour, new()
    {
        if (!IsCompatible(api))
        {
            return -1;
        }
        s_script = new T();
        return Start(api);
    }

    /// <summary>Attaches with an existing script instance (tests, custom hosting).</summary>
    public static int Init(PhxEngineApi* api, MetaBehaviour script)
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
            s_script!.Awake();
            s_script.Start();
            return 0;
        }
        catch (Exception e)
        {
            Engine.Log("Phoenix C#: Awake/Start threw " + e.GetType().Name + ": " + e.Message, ScriptAbi.LogError);
            return -3;
        }
    }

    /// <summary>phoenix_script_update. Exceptions never cross into C++.</summary>
    public static void Update(PhxFrameInfo* frame)
    {
        try
        {
            Time.frame = *frame;
            Time.deltaTime = frame->DeltaSeconds;
            Time.time = frame->TimeSeconds;
            Time.frameCount = frame->FrameIndex;
            s_script?.Update();
        }
        catch (Exception e)
        {
            Engine.Log("Phoenix C#: Update threw " + e.GetType().Name + ": " + e.Message, ScriptAbi.LogError);
        }
    }

    /// <summary>phoenix_script_shutdown.</summary>
    public static void Shutdown()
    {
        try
        {
            s_script?.OnDestroy();
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
