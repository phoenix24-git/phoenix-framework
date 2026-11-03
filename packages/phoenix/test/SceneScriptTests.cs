// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Runtime.InteropServices;
using Phoenix.Interop;
using Xunit;

namespace Phoenix.Framework.Tests;

/// <summary>
/// GameObjects, component views, script attach and the Unity lifecycle, against a fake scene
/// whose objects live in native memory, exactly like ECS chunk memory in the real engine.
/// </summary>
[Collection("engine")]
public sealed unsafe class SceneScriptTests : IDisposable
{
    private sealed class FakeObject
    {
        public string Name = "";
        public bool Alive = true;
        public bool Active = true;
        public ulong Parent;
        public TransformData* Transform;
        public Color* MeshColor; // null until MeshRenderer is added
    }

    private static readonly Dictionary<ulong, FakeObject> Objects = [];
    private static readonly List<string> Logs = [];
    private static ulong s_next;

    private readonly PhxEngineApi* _api;

    public SceneScriptTests()
    {
        Objects.Clear();
        Logs.Clear();
        s_next = 100;
        Probe.Events.Clear();
        _api = (PhxEngineApi*)NativeMemory.AllocZeroed((nuint)sizeof(PhxEngineApi));
        _api->AbiVersion = ScriptAbi.AbiVersion;
        _api->StructSize = (uint)sizeof(PhxEngineApi);
        _api->Log = &FakeLog;
        _api->FindObject = &FakeFind;
        _api->CreateObject = &FakeCreate;
        _api->DestroyObject = &FakeDestroy;
        _api->ObjectAlive = &FakeAlive;
        _api->SetActive = &FakeSetActive;
        _api->GetComponent = &FakeGetComponent;
        _api->GetField = &FakeGetField;
        Assert.Equal(0, ScriptHost.Init(_api, scripts => scripts.Add<Probe>().Add<Spinner>()));
    }

    public void Dispose()
    {
        ScriptHost.Shutdown();
        foreach (var o in Objects.Values)
        {
            NativeMemory.Free(o.Transform);
            NativeMemory.Free(o.MeshColor);
        }

        NativeMemory.Free(_api);
    }

    // --- The fake engine (same calling convention the C++ engine uses) -------------------------
    [UnmanagedCallersOnly]
    private static void FakeLog(void* engine, int level, byte* text) => Logs.Add(Marshal.PtrToStringUTF8((nint)text)!);

    [UnmanagedCallersOnly]
    private static ulong FakeFind(void* engine, byte* name)
    {
        var n = Marshal.PtrToStringUTF8((nint)name);
        foreach (var (h, o) in Objects)
        {
            if (o.Alive && o.Name == n)
            {
                return h;
            }
        }

        return 0;
    }

    [UnmanagedCallersOnly]
    private static ulong FakeCreate(void* engine, byte* name, ulong parent) => CreateObject(Marshal.PtrToStringUTF8((nint)name)!, parent);

    private static ulong CreateObject(string name, ulong parent = 0)
    {
        var t = (TransformData*)NativeMemory.AllocZeroed((nuint)sizeof(TransformData));
        t->localRotation = Quaternion.identity;
        t->localScale = Vector3.one;
        Objects[++s_next] = new FakeObject { Name = name, Parent = parent, Transform = t };
        return s_next;
    }

    [UnmanagedCallersOnly]
    private static void FakeDestroy(void* engine, ulong h) => Objects[h].Alive = false;

    [UnmanagedCallersOnly]
    private static uint FakeAlive(void* engine, ulong h) => Objects.TryGetValue(h, out var o) && o.Alive ? 1u : 0u;

    [UnmanagedCallersOnly]
    private static void FakeSetActive(void* engine, ulong h, uint active) => Objects[h].Active = active != 0;

    [UnmanagedCallersOnly]
    private static void* FakeGetComponent(void* engine, ulong h, byte* type, uint add) => Component(h, Marshal.PtrToStringUTF8((nint)type)!, add != 0);

    private static void* Component(ulong h, string type, bool add)
    {
        if (!Objects.TryGetValue(h, out var o) || !o.Alive)
        {
            return null;
        }

        switch (type)
        {
            case "Transform":
                return o.Transform;
            case "MeshRenderer":
                if (o.MeshColor is null && add)
                {
                    o.MeshColor = (Color*)NativeMemory.AllocZeroed((nuint)sizeof(Color));
                    *o.MeshColor = Color.white;
                }

                return o.MeshColor;
            default:
                return null;
        }
    }

    [UnmanagedCallersOnly]
    private static void* FakeGetField(void* engine, ulong h, byte* type, byte* field, uint* size)
    {
        var t = Marshal.PtrToStringUTF8((nint)type);
        var f = Marshal.PtrToStringUTF8((nint)field);
        void* p = t == "MeshRenderer" && f == "color" ? Component(h, "MeshRenderer", false) : null;
        *size = p is null ? 0u : 16u;
        return p;
    }

    private static PhxFrameInfo Frame(float dt = 0.016f, uint buttons = 0)
    {
        var frame = new PhxFrameInfo { DeltaSeconds = dt, PointerButtons = buttons, PointerX = 10, PointerY = 20 };
        return frame;
    }

    private static void Tick(uint buttons = 0)
    {
        var frame = Frame(buttons: buttons);
        ScriptHost.Update(&frame);
    }

    // --- Scripts used by the tests --------------------------------------------------------------
    private sealed class Probe : MetaBehaviour
    {
        public static readonly List<string> Events = [];

        protected internal override void Awake() => Events.Add("Awake " + properties.GetString("label"));
        protected internal override void Start() => Events.Add("Start");
        protected internal override void Update() => Events.Add("Update");
        protected internal override void OnDestroy() => Events.Add("OnDestroy");
    }

    private sealed class Spinner : MetaBehaviour
    {
        protected internal override void Update() => transform.Translate(new Vector3(properties.GetFloat("speed", 1) * Time.deltaTime, 0, 0));
    }

    // --- Tests ----------------------------------------------------------------------------------
    [Fact]
    public void SceneScriptsAttachByNameAndFollowTheUnityLifecycle()
    {
        var h = CreateObject("Game");
        Assert.Equal(0, ScriptHost.AttachByName(h, "Phoenix.Framework.Tests.SceneScriptTests+Probe", "label=\"hi\"\n"));
        Assert.Equal(["Awake hi"], Probe.Events);

        Tick();
        Tick();
        Assert.Equal(["Awake hi", "Start", "Update", "Update"], Probe.Events);

        // Destroyed objects take their scripts with them (OnDestroy, then no more Updates).
        GameObject.Destroy(new GameObject(h));
        Tick();
        Tick();
        Assert.Equal(["Awake hi", "Start", "Update", "Update", "OnDestroy"], Probe.Events);
    }

    [Fact]
    public void ShortNamesWorkAndUnknownClassesAreReported()
    {
        Assert.Equal(0, ScriptHost.AttachByName(CreateObject("A"), "Probe", ""));
        Assert.Equal(-2, ScriptHost.AttachByName(CreateObject("B"), "Missing.Script", ""));
        Assert.Contains(Logs, l => l.Contains("Missing.Script", StringComparison.Ordinal));
    }

    [Fact]
    public void TransformWritesGoStraightIntoEngineMemory()
    {
        var h = CreateObject("Block");
        Assert.Equal(0, ScriptHost.AttachByName(h, "Spinner", "speed=2.0"));
        Tick();
        Assert.Equal(2f * 0.016f, Objects[h].Transform->localPosition.x, 5);

        var go = GameObject.Find("Block");
        go.transform.localScale = new Vector3(3, 1, 3);
        go.transform.localPosition.y += 5; // ref access: no copy
        Assert.Equal(new Vector3(3, 1, 3), Objects[h].Transform->localScale);
        Assert.Equal(5f, Objects[h].Transform->localPosition.y);
    }

    [Fact]
    public void ComponentViewsAndFields()
    {
        var go = GameObject.Create("Cube");
        Assert.True(go.IsValid);
        Assert.False(go.HasComponent("MeshRenderer"));
        Assert.Equal(default, go.GetComponent<MeshRenderer>());

        var renderer = go.AddComponent<MeshRenderer>();
        Assert.Equal(Color.white, renderer.color);
        renderer.color = new Color(1, 0, 0, 0.5f);
        Assert.Equal(new Color(1, 0, 0, 0.5f), *Objects[go.Handle].MeshColor);
        Assert.Equal(new Color(1, 0, 0, 0.5f), go.GetComponent<MeshRenderer>().color);

        // Reading a field as the wrong type is refused instead of corrupting memory.
        Assert.Throws<InvalidOperationException>(() => go.Field<float>("MeshRenderer", "color"));
        Assert.Throws<InvalidOperationException>(() => go.Field<Color>("Camera", "backgroundColor"));

        go.SetActive(false);
        Assert.False(Objects[go.Handle].Active);
    }

    [Fact]
    public void ScriptsCanAddScriptsAndFindThem()
    {
        var go = GameObject.Create("Runtime");
        var probe = go.AddScript<Probe>();
        Assert.Same(probe, go.GetScript<Probe>());
        Assert.Null(go.GetScript<Spinner>());
        Assert.Equal(go, probe.gameObject);
    }

    [Fact]
    public void InputReportsPressesOnceAndReleases()
    {
        Tick(buttons: 0);
        Assert.False(Input.GetMouseButtonDown(0));
        Tick(buttons: 1);
        Assert.True(Input.GetMouseButtonDown(0));
        Assert.True(Input.GetMouseButton(0));
        Assert.Equal(new Vector3(10, 20, 0), Input.mousePosition);
        Tick(buttons: 1);
        Assert.False(Input.GetMouseButtonDown(0)); // held, not pressed again
        Tick(buttons: 0);
        Assert.True(Input.GetMouseButtonUp(0));
    }

    [Fact]
    public void PropertiesUseGodotTextSyntax()
    {
        var p = ScriptProperties.Parse("speed=2.5\ncount=3\non=true\nname=\"Sky \\\"line\\\"\"\nstart=Vector3(1, 2.5, -3)\ntint=Color(1, 0.5, 0, 1)\n");
        Assert.Equal(2.5f, p.GetFloat("speed"));
        Assert.Equal(3, p.GetInt("count"));
        Assert.True(p.GetBool("on"));
        Assert.Equal("Sky \"line\"", p.GetString("name"));
        Assert.Equal(new Vector3(1, 2.5f, -3), p.GetVector3("start"));
        Assert.Equal(new Color(1, 0.5f, 0, 1), p.GetColor("tint"));
        Assert.Equal(7f, p.GetFloat("missing", 7f));
        Assert.Equal(Vector3.one, p.GetVector3("speed", Vector3.one)); // wrong shape -> fallback
    }

    [Fact]
    public void EulerMatchesTheEngineConvention()
    {
        // 90 degrees around Y turns forward (-Z) into -X, as in the engine (right-handed, Y up).
        var q = Quaternion.Euler(0, 90, 0);
        Assert.Equal(0f, q.x, 5);
        Assert.Equal(MathF.Sqrt(0.5f), q.y, 5);
        Assert.Equal(MathF.Sqrt(0.5f), q.w, 5);
        Assert.Equal(Quaternion.identity, Quaternion.Euler(0, 0, 0));
        Assert.Equal(0xFF8000FFu, new Color(1, 0.5f, 0).ToRgba());
    }
}
