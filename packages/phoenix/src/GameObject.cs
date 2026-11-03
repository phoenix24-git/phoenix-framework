// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// GameObjects for C# scripts. A GameObject is a handle to an entity in the engine's ECS scene;
// component data is read and written in place (zero-copy) through pointers into ECS chunk
// memory. Built-in components are exposed as small "views" (Transform, MeshRenderer, Camera,
// Light) whose properties are refs into that memory.
using System.Runtime.InteropServices;
using System.Text;

namespace Phoenix;

#pragma warning disable CA1707, IDE1006, CA1720, CA1716 // Unity's API shape, on purpose.

/// <summary>A handle to an object in the running scene (0 = none).</summary>
public readonly unsafe struct GameObject(ulong handle) : IEquatable<GameObject>
{
    /// <summary>The engine handle (stable for the object's lifetime; never reused).</summary>
    public ulong Handle { get; } = handle;

    /// <summary>False once the object is destroyed (or for the default value).</summary>
    public bool IsValid => Handle != 0 && Engine.Api->ObjectAlive(Engine.Api->Engine, Handle) != 0;

    /// <summary>Finds an object by name (Unity's GameObject.Find).</summary>
    public static GameObject Find(string name)
    {
        using var n = new Utf8(name);
        return new GameObject(Engine.Api->FindObject(Engine.Api->Engine, n.Pointer));
    }

    /// <summary>Creates an empty object (Unity's <c>new GameObject(name)</c>), optionally as a child.</summary>
    public static GameObject Create(string name, GameObject parent = default)
    {
        using var n = new Utf8(name);
        var handle = Engine.Api->CreateObject(Engine.Api->Engine, n.Pointer, parent.Handle);
        return handle == 0 ? throw new InvalidOperationException("No scene is running (call runScene in phoenix_main).") : new GameObject(handle);
    }

    /// <summary>Unity's Object.Destroy: the object and its children go away at the end of the frame.</summary>
    public static void Destroy(GameObject gameObject) => Engine.Api->DestroyObject(Engine.Api->Engine, gameObject.Handle);

    public void SetActive(bool value) => Engine.Api->SetActive(Engine.Api->Engine, Handle, value ? 1u : 0u);

    /// <summary>This object's Transform (local position, rotation, scale).</summary>
    public Transform transform => new(this);

    /// <summary>A built-in component view, or default if the object doesn't have that component.</summary>
    public T GetComponent<T>()
        where T : struct, IComponentView<T> => HasComponent(T.TypeName) ? T.Create(this) : default;

    /// <summary>Adds a built-in component (or returns the existing one).</summary>
    public T AddComponent<T>()
        where T : struct, IComponentView<T>
    {
        RawComponent(T.TypeName, add: true);
        return T.Create(this);
    }

    /// <summary>Attaches a new C# script (Unity's AddComponent for MonoBehaviours).</summary>
    public T AddScript<T>()
        where T : MetaBehaviour, new() => ScriptHost.Attach(this, new T(), null);

    /// <summary>The first attached script of type T, or null.</summary>
    public T? GetScript<T>()
        where T : MetaBehaviour => ScriptHost.Find<T>(this);

    public bool HasComponent(string type) => RawComponent(type, add: false) is not null;

    /// <summary>
    /// A reference to one field of a component in engine memory, e.g.
    /// <c>ref var size = ref go.Field&lt;float&gt;("Camera", "orthographicSize")</c>. The type must
    /// match the field's size. Valid for this frame (re-fetch after adding or removing components).
    /// </summary>
    public ref T Field<T>(string component, string field)
        where T : unmanaged
    {
        using var c = new Utf8(component);
        using var f = new Utf8(field);
        return ref FieldAt<T>(c.Pointer, f.Pointer);
    }

    /// <summary>Allocation-free <see cref="Field{T}(string, string)"/> for u8 literals (NUL-terminated).</summary>
    internal ref T Field<T>(ReadOnlySpan<byte> component, ReadOnlySpan<byte> field)
        where T : unmanaged
    {
        fixed (byte* c = component)
        fixed (byte* f = field)
        {
            return ref FieldAt<T>(c, f);
        }
    }

    private ref T FieldAt<T>(byte* component, byte* field)
        where T : unmanaged
    {
        uint size;
        var p = Engine.Api->GetField(Engine.Api->Engine, Handle, component, field, &size);
        if (p is null)
        {
            throw new InvalidOperationException($"{Utf8.Read(component)}.{Utf8.Read(field)} is not available on this object.");
        }

        if (size != (uint)sizeof(T))
        {
            throw new InvalidOperationException(
                $"{Utf8.Read(component)}.{Utf8.Read(field)} is {size} bytes, not a {typeof(T).Name} ({sizeof(T)} bytes).");
        }

        return ref *(T*)p;
    }

    /// <summary>A pointer to a component's data in engine memory, or null.</summary>
    public void* RawComponent(string type, bool add)
    {
        using var t = new Utf8(type);
        return Engine.Api->GetComponent(Engine.Api->Engine, Handle, t.Pointer, add ? 1u : 0u);
    }

    /// <summary>Allocation-free <see cref="RawComponent(string, bool)"/> for u8 literals.</summary>
    internal void* RawComponent(ReadOnlySpan<byte> type, bool add)
    {
        fixed (byte* t = type)
        {
            return Engine.Api->GetComponent(Engine.Api->Engine, Handle, t, add ? 1u : 0u);
        }
    }

    public bool Equals(GameObject other) => Handle == other.Handle;
    public override bool Equals(object? obj) => obj is GameObject g && Equals(g);
    public override int GetHashCode() => Handle.GetHashCode();
    public static bool operator ==(GameObject a, GameObject b) => a.Equals(b);
    public static bool operator !=(GameObject a, GameObject b) => !a.Equals(b);
}

/// <summary>Built-in component views: <c>go.GetComponent&lt;MeshRenderer&gt;()</c>.</summary>
public interface IComponentView<TSelf>
    where TSelf : struct
{
    /// <summary>The engine component type name (see docs/components.md).</summary>
    static abstract string TypeName { get; }

    static abstract TSelf Create(GameObject gameObject);
}

/// <summary>The engine's LocalTransform layout (40 bytes): what Transform reads and writes.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct TransformData
{
    public Vector3 localPosition;
    public Quaternion localRotation;
    public Vector3 localScale;
}

/// <summary>Unity's Transform (local space). The engine computes world matrices after scripts run.</summary>
public readonly unsafe struct Transform(GameObject gameObject) : IComponentView<Transform>
{
    public static string TypeName => "Transform";
    public static Transform Create(GameObject gameObject) => new(gameObject);

    public GameObject gameObject { get; } = gameObject;

    private ref TransformData Data
    {
        get
        {
            var p = gameObject.RawComponent("Transform"u8, add: false);
            if (p is null)
            {
                throw new InvalidOperationException("The GameObject was destroyed.");
            }

            return ref *(TransformData*)p;
        }
    }

    public ref Vector3 localPosition => ref Data.localPosition;
    public ref Quaternion localRotation => ref Data.localRotation;
    public ref Vector3 localScale => ref Data.localScale;

    public void Translate(Vector3 delta) => Data.localPosition += delta;

    /// <summary>Rotates by Euler angles in degrees (local space).</summary>
    public void Rotate(Vector3 eulerDegrees)
    {
        ref var d = ref Data;
        d.localRotation *= Quaternion.Euler(eulerDegrees);
    }
}

/// <summary>Unity's MeshRenderer: <see cref="color"/> is the base color until materials land.</summary>
public readonly struct MeshRenderer(GameObject gameObject) : IComponentView<MeshRenderer>
{
    public static string TypeName => "MeshRenderer";
    public static MeshRenderer Create(GameObject gameObject) => new(gameObject);

    public GameObject gameObject { get; } = gameObject;
    public ref Color color => ref gameObject.Field<Color>("MeshRenderer"u8, "color"u8);
}

/// <summary>Unity's MeshFilter (built-in meshes for now: a cube by default).</summary>
public readonly struct MeshFilter(GameObject gameObject) : IComponentView<MeshFilter>
{
    public static string TypeName => "MeshFilter";
    public static MeshFilter Create(GameObject gameObject) => new(gameObject);

    public GameObject gameObject { get; } = gameObject;
}

/// <summary>Unity's Camera.</summary>
public readonly struct Camera(GameObject gameObject) : IComponentView<Camera>
{
    public static string TypeName => "Camera";
    public static Camera Create(GameObject gameObject) => new(gameObject);

    public GameObject gameObject { get; } = gameObject;
    public ref Color backgroundColor => ref gameObject.Field<Color>("Camera"u8, "backgroundColor"u8);
    public ref float orthographicSize => ref gameObject.Field<float>("Camera"u8, "orthographicSize"u8);
}

/// <summary>Unity's Light.</summary>
public readonly struct Light(GameObject gameObject) : IComponentView<Light>
{
    public static string TypeName => "Light";
    public static Light Create(GameObject gameObject) => new(gameObject);

    public GameObject gameObject { get; } = gameObject;
    public ref Color color => ref gameObject.Field<Color>("Light"u8, "color"u8);
    public ref float intensity => ref gameObject.Field<float>("Light"u8, "intensity"u8);
}

/// <summary>Unity's Input (pointer: mouse or first touch).</summary>
public static class Input
{
    private static uint s_buttons;
    private static uint s_previous;

    /// <summary>Pointer position in logical pixels (y down, like the draw API).</summary>
    public static Vector3 mousePosition { get; private set; }

    public static bool GetMouseButton(int button) => (s_buttons & (1u << button)) != 0;
    public static bool GetMouseButtonDown(int button) => GetMouseButton(button) && (s_previous & (1u << button)) == 0;
    public static bool GetMouseButtonUp(int button) => !GetMouseButton(button) && (s_previous & (1u << button)) != 0;

    internal static void Update(in Interop.PhxFrameInfo frame)
    {
        s_previous = s_buttons;
        s_buttons = frame.PointerButtons;
        mousePosition = new Vector3(frame.PointerX, frame.PointerY, 0);
    }

    internal static void Reset() => s_buttons = s_previous = 0;
}

/// <summary>A NUL-terminated UTF-8 copy of a string for native calls.</summary>
internal unsafe ref struct Utf8
{
    private readonly byte[] _buffer;
    private GCHandle _pin;

    public Utf8(string text)
    {
        _buffer = new byte[Encoding.UTF8.GetByteCount(text) + 1];
        Encoding.UTF8.GetBytes(text, _buffer);
        _pin = GCHandle.Alloc(_buffer, GCHandleType.Pinned);
    }

    public readonly byte* Pointer => (byte*)_pin.AddrOfPinnedObject();

    public static string Read(byte* text) => Marshal.PtrToStringUTF8((nint)text) ?? string.Empty;

    public void Dispose()
    {
        if (_pin.IsAllocated)
        {
            _pin.Free();
        }
    }
}
#pragma warning restore CA1707, IDE1006, CA1720, CA1716
