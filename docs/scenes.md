---
title: Scenes and scripts
nav_order: 5
---

# Scenes and scripts
{: .no_toc }

A Phoenix game is a **scene file** plus **scripts attached to its objects**:
- The file format is Godot's `.tscn` text syntax.
- The objects are Unity-style GameObjects with components.
- The scripts are C# `MetaBehaviour`s, Phoenix's `MonoBehaviour`.

[Skyline](https://github.com/phoenix24-git/phoenix-framework/tree/main/examples/skyline) is a
complete example.

1. TOC
{:toc}

## Starting a scene

```cpp
// lib/main.cpp
#include <phoenix/phoenix.h>

void phoenix_main() {
    phoenix::runScene("res://scenes/main.pscn");
}
```

`res://` is the app's `res/` folder. It is copied next to the executable, or packaged into the
web build (see [File system](file-system.md)).

## Scene files (`.pscn`)

```ini
[phoenix_scene format=1]

[node name="World"]                       ; the first node is the root

[node name="Main Camera" parent="."]      ; parent: "." = root, or a path like "Tower/Top"
tag = "MainCamera"                        ; node keys: tag, layer, active

[component type="Transform"]              ; components belong to the node above
position = Vector3(6, 6.5, 6)
euler = Vector3(-35.264, 45, 0)           ; degrees, Unity's Z-X-Y order

[component type="Camera"]
projection = "Orthographic"               ; enums by name (or number)
orthographicSize = 5.5
backgroundColor = Color(0.09, 0.1, 0.18, 1)

[node name="Base" parent="."]

[component type="Transform"]
position = Vector3(0, -4, 0)
scale = Vector3(3, 8, 3)

[component type="MeshFilter"]             ; a cube by default
[component type="MeshRenderer"]
color = Color(0.36, 0.42, 0.62, 1)

[node name="Game" parent="."]

[script class="Skyline.StackGame"]        ; attach a C# script
speed = 2.4                               ; its properties (Unity's serialized fields)
```

- **Values** use Godot's Variant syntax: `Vector3()`, `Color()`, `"strings"`, numbers, `true`, and
  arrays.
- **Comments** start with `;`.
- **Component names and fields** are the ones in [Unity components](components.md). `Transform`
  also accepts `euler`.
- **Errors** name the file and line (`main.pscn:14: Camera has no field "zoom"`). A scene that
  fails to load leaves nothing half-built behind.

## C# scripts

```csharp
using Phoenix;

public sealed class Spinner : MetaBehaviour
{
    private float _speed;

    protected override void Awake() => _speed = properties.GetFloat("speed", 90);   // from the scene file

    protected override void Update()
    {
        transform.Rotate(new Vector3(0, _speed * Time.deltaTime, 0));
        if (Input.GetMouseButtonDown(0))
        {
            var cube = GameObject.Create("Cube");
            cube.AddComponent<MeshFilter>();
            cube.AddComponent<MeshRenderer>().color = Color.HSVToRGB(0.6f, 0.5f, 1f);
            cube.transform.localPosition = new Vector3(0, 2, 0);
        }
    }
}
```

| Unity | Phoenix C# |
|---|---|
| `MonoBehaviour` + `Awake`/`Start`/`Update`/`OnDestroy` | `MetaBehaviour`, same lifecycle |
| `gameObject`, `transform.localPosition` / `localRotation` / `localScale` | same; the properties are `ref`s into engine memory |
| `GameObject.Find`, `new GameObject(name)`, `Destroy(go)` | `GameObject.Find`, `GameObject.Create(name, parent)`, `Destroy(go)` (end of frame) |
| `GetComponent<MeshRenderer>().material.color` | `GetComponent<MeshRenderer>().color` |
| `AddComponent<T>()` for built-ins / for scripts | `AddComponent<T>()` / `AddScript<T>()`; `GetScript<T>()` |
| `[SerializeField] float speed` | `properties.GetFloat("speed")` (values from the scene file) |
| `Input.GetMouseButtonDown(0)`, `Input.mousePosition` | same (mouse or first touch) |
| `Time.deltaTime`, `Time.time`, `Time.frameCount` | same |
| `Vector3`, `Quaternion.Euler`, `Color.HSVToRGB`, `Mathf.PingPong` | same names |
| any component field | `go.Field<float>("Camera", "orthographicSize")` (a `ref`, size-checked) |

The script module registers the classes that scene files may attach. NativeAOT has no
reflection to look classes up by name, so this has to be explicit:

```csharp
public static unsafe class Exports
{
    [UnmanagedCallersOnly(EntryPoint = "phoenix_script_init")]
    public static int Init(PhxEngineApi* api) =>
        ScriptHost.Init(api, scripts => scripts.Add<StackGame>().Add<CameraRig>());
}
```

Publish it with NativeAOT (`dotnet publish -c Release -r linux-x64`) and run the app with
`PHOENIX_SCRIPT_MODULE=path/to/Game.so`, or pass `--script=...`.

### How it stays zero-overhead

- **Object handles:** a `GameObject` is a 64-bit handle.
- **Component data:** `transform.localPosition` returns a `ref` into the ECS chunk where the
  engine stores that entity's `LocalTransform`. No copying, no marshalling.
- **Field checks:** `Field<T>` checks the field's size before handing out a `ref`, so reading a
  `Color` as a `float` is an error, not memory corruption.
- **When pointers expire:** a component's location changes when components are added or removed,
  so a `ref` is valid only until the next such change. The views (`transform`,
  `GetComponent<MeshRenderer>()`) fetch it fresh on every access.

## Rendering today

The engine renders every active object that has a `MeshFilter` (cube), a `MeshRenderer`
(`color`) and a world transform:

- **Camera:** the main camera (tagged `MainCamera`, otherwise the first one). It's orthographic;
  perspective cameras render orthographically for now.
- **Lighting:** one `Directional` light.
- **Draw order:** whole boxes, back to front.

Under an orthographic projection each cube face is exactly a parallelogram, so this is real 3D
through the same 2D display list the UI uses. Meshes, materials and perspective arrive with the
Forward+ renderer on the [roadmap](roadmap.md).
