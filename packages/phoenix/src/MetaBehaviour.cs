// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Globalization;
using System.Runtime.InteropServices;
using Phoenix.Interop;

namespace Phoenix;

#pragma warning disable CA1707, IDE1006 // Unity's lowerCamelCase names, on purpose: ported code compiles unchanged.

/// <summary>
/// Phoenix's MonoBehaviour: derive from it and override the Unity lifecycle methods you need.
/// A behaviour is attached to a <see cref="GameObject"/> (from a scene file's [script] section,
/// or <see cref="GameObject.AddScript{T}"/>): Awake runs on attach, Start before its first
/// Update, Update every frame, and OnDestroy when the object is destroyed or the module unloads.
/// </summary>
public abstract class MetaBehaviour
{
    /// <summary>The object this behaviour is attached to (default for a module-wide script).</summary>
    public GameObject gameObject { get; internal set; }

    /// <summary>The object's Transform.</summary>
    public Transform transform => gameObject.transform;

    /// <summary>Values from the scene file's [script] section (Unity's serialized fields).</summary>
    public ScriptProperties properties { get; internal set; } = ScriptProperties.Empty;

    internal bool Started { get; set; }

    /// <summary>Called once, first, when the script attaches.</summary>
    protected internal virtual void Awake() { }

    /// <summary>Called once after Awake, before the first Update.</summary>
    protected internal virtual void Start() { }

    /// <summary>Called every frame (see <see cref="Time.deltaTime"/>).</summary>
    protected internal virtual void Update() { }

    /// <summary>Called once when the object is destroyed or the script module unloads.</summary>
    protected internal virtual void OnDestroy() { }

    /// <summary>Unity's Object.Destroy (end of frame, children included).</summary>
    protected static void Destroy(GameObject target) => GameObject.Destroy(target);
}

/// <summary>Unity-style frame timing, updated by the engine before each Update.</summary>
public static class Time
{
    /// <summary>Seconds since the previous frame.</summary>
    public static float deltaTime { get; internal set; }

    /// <summary>Seconds since the engine started.</summary>
    public static double time { get; internal set; }

    /// <summary>Frames rendered so far.</summary>
    public static long frameCount { get; internal set; }

    /// <summary>The raw frame info the engine passed for the current frame.</summary>
    public static PhxFrameInfo frame { get; internal set; }
}
#pragma warning restore CA1707, IDE1006

/// <summary>
/// The script classes a module offers to scene files, by full name. NativeAOT has no reflection
/// to find classes by name, so a module registers them in <c>phoenix_script_init</c>:
/// <code>
/// [UnmanagedCallersOnly(EntryPoint = "phoenix_script_init")]
/// public static unsafe int Init(PhxEngineApi* api) =>
///     ScriptHost.Init(api, scripts => scripts.Add&lt;StackGame&gt;().Add&lt;CameraRig&gt;());
/// </code>
/// </summary>
public sealed class ScriptRegistry
{
    private readonly Dictionary<string, Func<MetaBehaviour>> _factories = new(StringComparer.Ordinal);

    /// <summary>Registers T under its full name ("Namespace.Class") and its short name.</summary>
    public ScriptRegistry Add<T>()
        where T : MetaBehaviour, new()
    {
        _factories[typeof(T).FullName!] = static () => new T();
        _factories.TryAdd(typeof(T).Name, static () => new T());
        return this;
    }

    internal MetaBehaviour? Create(string name) => _factories.TryGetValue(name, out var f) ? f() : null;

    internal void Clear() => _factories.Clear();
}

/// <summary>
/// Hosts a module's scripts. Entry points: <c>phoenix_script_init</c> (exported by the script
/// module, calling one of the Init overloads), and <c>phoenix_script_update</c>,
/// <c>phoenix_script_attach</c>, <c>phoenix_script_shutdown</c> from <see cref="ScriptExports"/>.
/// </summary>
public static unsafe class ScriptHost
{
    private static readonly ScriptRegistry Registry = new();
    private static readonly List<MetaBehaviour> Behaviours = [];
    private static MetaBehaviour? s_global;

    /// <summary>Attaches to the engine with one module-wide script T. Returns 0 on success.</summary>
    public static int Init<T>(PhxEngineApi* api)
        where T : MetaBehaviour, new() => Init(api, new T());

    /// <summary>Attaches with an existing module-wide script instance (tests, custom hosting).</summary>
    public static int Init(PhxEngineApi* api, MetaBehaviour script)
    {
        ArgumentNullException.ThrowIfNull(script);
        if (!Attach(api))
        {
            return -1;
        }

        s_global = script;
        return Run(script, static s => { s.Awake(); s.Start(); s.Started = true; }, "Awake/Start") ? 0 : -3;
    }

    /// <summary>Attaches to the engine and registers the classes scene files may attach.</summary>
    public static int Init(PhxEngineApi* api, Action<ScriptRegistry> register)
    {
        ArgumentNullException.ThrowIfNull(register);
        if (!Attach(api))
        {
            return -1;
        }

        register(Registry);
        return 0;
    }

    internal static bool IsCompatible(PhxEngineApi* api) =>
        api is not null && api->AbiVersion == ScriptAbi.AbiVersion && api->StructSize >= (uint)sizeof(PhxEngineApi);

    private static bool Attach(PhxEngineApi* api)
    {
        if (!IsCompatible(api))
        {
            return false;
        }

        Engine.Attach(api);
        Input.Reset();
        return true;
    }

    /// <summary>phoenix_script_attach: a scene's [script] section. 0 = attached, -2 = unknown class.</summary>
    public static int AttachByName(ulong handle, string className, string properties)
    {
        var script = Registry.Create(className);
        if (script is null)
        {
            Engine.Log($"Phoenix C#: no script class \"{className}\" is registered (ScriptRegistry.Add<T>)", ScriptAbi.LogError);
            return -2;
        }

        Attach(new GameObject(handle), script, ScriptProperties.Parse(properties));
        return 0;
    }

    /// <summary>Attaches a behaviour instance to an object and runs its Awake.</summary>
    public static T Attach<T>(GameObject gameObject, T script, ScriptProperties? properties)
        where T : MetaBehaviour
    {
        ArgumentNullException.ThrowIfNull(script);
        script.gameObject = gameObject;
        script.properties = properties ?? ScriptProperties.Empty;
        Behaviours.Add(script);
        Run(script, static s => s.Awake(), "Awake");
        return script;
    }

    /// <summary>The first live behaviour of type T on the object.</summary>
    public static T? Find<T>(GameObject gameObject)
        where T : MetaBehaviour
    {
        foreach (var b in Behaviours)
        {
            if (b is T t && b.gameObject == gameObject)
            {
                return t;
            }
        }

        return null;
    }

    /// <summary>phoenix_script_update. Exceptions never cross into C++.</summary>
    public static void Update(PhxFrameInfo* frame)
    {
        Time.frame = *frame;
        Time.deltaTime = frame->DeltaSeconds;
        Time.time = frame->TimeSeconds;
        Time.frameCount = frame->FrameIndex;
        Input.Update(*frame);

        if (s_global is not null)
        {
            Run(s_global, static s => s.Update(), "Update");
        }

        // Iterate over a snapshot: scripts may add or destroy objects while updating.
        var snapshot = Behaviours.ToArray();
        foreach (var b in snapshot)
        {
            if (!b.gameObject.IsValid)
            {
                Behaviours.Remove(b);
                Run(b, static s => s.OnDestroy(), "OnDestroy");
                continue;
            }

            if (!b.Started)
            {
                b.Started = true;
                if (!Run(b, static s => s.Start(), "Start"))
                {
                    continue;
                }
            }

            Run(b, static s => s.Update(), "Update");
        }
    }

    /// <summary>phoenix_script_shutdown.</summary>
    public static void Shutdown()
    {
        try
        {
            foreach (var b in Behaviours.ToArray())
            {
                Run(b, static s => s.OnDestroy(), "OnDestroy");
            }

            if (s_global is not null)
            {
                Run(s_global, static s => s.OnDestroy(), "OnDestroy");
            }
        }
        finally
        {
            Behaviours.Clear();
            Registry.Clear();
            s_global = null;
            Engine.Detach();
        }
    }

    private static bool Run(MetaBehaviour script, Action<MetaBehaviour> step, string what)
    {
        try
        {
            step(script);
            return true;
        }
        catch (Exception e)
        {
            if (Engine.IsAttached)
            {
                Engine.Log($"Phoenix C#: {script.GetType().Name}.{what} threw {e.GetType().Name}: {e.Message}", ScriptAbi.LogError);
            }

            return false;
        }
    }
}

/// <summary>
/// A scene file's [script] values. Values arrive in Godot's text syntax and are converted on
/// request: <c>properties.GetFloat("speed", 2f)</c>.
/// </summary>
public sealed class ScriptProperties
{
    private readonly Dictionary<string, string> _values;

    private ScriptProperties(Dictionary<string, string> values) => _values = values;

    public static ScriptProperties Empty { get; } = new(new Dictionary<string, string>(StringComparer.Ordinal));

    public IReadOnlyCollection<string> Keys => _values.Keys;

    public bool Has(string key) => _values.ContainsKey(key);

    /// <summary>Parses "key=value" lines.</summary>
    public static ScriptProperties Parse(string? text)
    {
        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var line in (text ?? string.Empty).Split('\n'))
        {
            var eq = line.IndexOf('=', StringComparison.Ordinal);
            if (eq > 0)
            {
                values[line[..eq].Trim()] = line[(eq + 1)..].Trim();
            }
        }

        return values.Count == 0 ? Empty : new ScriptProperties(values);
    }

    public string GetString(string key, string fallback = "")
    {
        if (!_values.TryGetValue(key, out var v))
        {
            return fallback;
        }

        return v.Length >= 2 && v[0] == '"' && v[^1] == '"'
            ? v[1..^1].Replace("\\\"", "\"", StringComparison.Ordinal).Replace("\\\\", "\\", StringComparison.Ordinal)
            : v;
    }

    public float GetFloat(string key, float fallback = 0) =>
        _values.TryGetValue(key, out var v) && float.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out var f) ? f : fallback;

    public int GetInt(string key, int fallback = 0) =>
        _values.TryGetValue(key, out var v) && int.TryParse(v, NumberStyles.Integer, CultureInfo.InvariantCulture, out var i) ? i : fallback;

    public bool GetBool(string key, bool fallback = false) =>
        _values.TryGetValue(key, out var v) ? v == "true" || (v != "false" && fallback) : fallback;

    /// <summary>"Vector3(1, 2, 3)".</summary>
    public Vector3 GetVector3(string key, Vector3 fallback = default)
    {
        var n = Numbers(key, "Vector3");
        return n is { Length: 3 } ? new Vector3(n[0], n[1], n[2]) : fallback;
    }

    /// <summary>"Color(r, g, b, a)" (alpha optional).</summary>
    public Color GetColor(string key, Color fallback = default)
    {
        var n = Numbers(key, "Color");
        return n switch
        {
            { Length: 4 } => new Color(n[0], n[1], n[2], n[3]),
            { Length: 3 } => new Color(n[0], n[1], n[2]),
            _ => fallback,
        };
    }

    private float[]? Numbers(string key, string constructor)
    {
        if (!_values.TryGetValue(key, out var v) || !v.StartsWith(constructor + "(", StringComparison.Ordinal) || !v.EndsWith(')'))
        {
            return null;
        }

        var parts = v[(constructor.Length + 1)..^1].Split(',');
        var result = new float[parts.Length];
        for (var i = 0; i < parts.Length; i++)
        {
            if (!float.TryParse(parts[i], NumberStyles.Float, CultureInfo.InvariantCulture, out result[i]))
            {
                return null;
            }
        }

        return result;
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

    [UnmanagedCallersOnly(EntryPoint = "phoenix_script_attach")]
    public static int Attach(ulong handle, byte* className, byte* properties)
    {
        try
        {
            return ScriptHost.AttachByName(handle, Utf8.Read(className), Utf8.Read(properties));
        }
        catch (Exception e)
        {
            Engine.Log("Phoenix C#: attach threw " + e.GetType().Name + ": " + e.Message, ScriptAbi.LogError);
            return -3;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "phoenix_script_shutdown")]
    public static void Shutdown() => ScriptHost.Shutdown();
}
