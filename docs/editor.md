---
title: In-game editor
nav_order: 5
---

# In-game editor
{: .no_toc }

Every Phoenix app has a Godot-style scene editor built into it. Press **F1** while the game runs
and the editor docks open over the game. The game keeps rendering in the middle and still gets
the input there. Press **F1** again to close the editor and resume the game.

1. TOC
{:toc}

## Layout

```text
+-----------------------------------------------------------------------------+
| Scene  Run  Editor                                 [Pause] [Step]  60 fps   |
+-------------+-----------------------------------------------+---------------+
| Scene    [+]|                                               | Inspector     |
|  v World    |          the running game shows through       | [x] [Crate  ] |
|    Camera   |          here and still gets the input        | Tag   Untagged|
|    Crate    |                                               | v Transform   |
|             +-----------------------------------------------+   Position .. |
|             | Output | FileSystem                           | > MeshRenderer|
|             | Editor: open (F1 to close, F5 play/pause...)  | [Add Component]|
+-------------+-----------------------------------------------+---------------+
```

| Dock | What it does |
|---|---|
| **Scene** | The GameObject hierarchy. Click a row to select it, and click the arrow to fold a branch. **+** adds an empty child under the selection. |
| **Inspector** | Edits the selection: the active checkbox, the name, tag, layer and Transform (position, rotation in degrees, scale). It also lists **every component on the object, with all of its reflected fields**. **x** removes a component. **Add Component** searches the component registry. |
| **Output** | The engine log (everything sent to `PHX_LOG_*` or SDL's log), coloured by level. It follows new lines while you are scrolled to the bottom. |
| **FileSystem** | The `res://` tree. Click a `.pscn` file to load it as the running scene. |
| **Toolbar** | **Pause/Play** and **Step** (one frame), plus fps and object count. |

The docks can be resized by dragging their dividers. Pausing stops the scene update, the C# script
update and the physics step together, so a paused game is fully frozen while you inspect it.

## Editing values

Fields are edited the way Godot's inspector edits them:

- **Numbers:** drag sideways to scrub the value. Click to type a value; **Enter** or clicking
  elsewhere commits it, and **Escape** cancels.
- **Vectors and colours:** one number field per axis (`x y z`, `r g b a`). Colours also show a swatch.
- **Enums:** a dropdown with the enum's value names.
- **Booleans:** a checkbox.
- **Strings:** a text field, committed on **Enter** or when it loses focus.
- **Entity and asset references:** shown read-only for now.

Every write goes through [component reflection](components.md) and marks the component as changed,
so systems that use change detection see the edit. Your own components show up with no extra code
once they are registered with fields (`register_component<T>(name, category, {PHX_FIELD(...)})`).
C++ `MetaBehaviour`s are listed under **Scripts**.

## Shortcuts

| Key | Action |
|---|---|
| F1 | Open/close the editor |
| F5 | Play / pause |
| F6 | Pause and advance one frame |
| Ctrl+N | Add an empty GameObject (child of the selection) |
| Ctrl+D | Duplicate the selection |
| Delete | Delete the selection |
| Escape | Leave the focused field without committing |

Shortcuts are ignored while a text field has focus. **Delete** then deletes a character, not the
GameObject.

## Command line and embedding

```bash
build/skyline/out/skyline --editor        # start with the editor open
build/skyline/out/skyline --no-editor     # disable it (F1 does nothing)
```

`EngineConfig::editor` (default `true`) controls the same setting from code. The editor is
built only into apps that use the scene layer (`PHOENIX_WITH_FOUNDATION`). While it is closed it
costs nothing: it is not laid out, painted, or hit-tested.

The editor is an ordinary widget, `phoenix::editor::SceneEditor`, installed as the binding's
*overlay root*: a second widget tree painted above the app. A tool or test can host it directly:

```cpp
#include <phoenix/editor/scene_editor.h>

auto editor = std::make_shared<phoenix::editor::SceneEditor>();
editor->setScene(&scene);
phoenix::WidgetsBinding::instance().setOverlayRoot(editor);
// every frame:
if (editor->shouldUpdateScene()) scene.Update(dt);
```

The editor's own tests (`engine/test/scene/editor_test.cpp`) drive it this way with `WidgetTester`
taps, drags and typing.

## Not there yet

- **Saving the scene** back to a `.pscn` file. Edits live only in the running game.
- **Undo/redo.**
- **Picking and gizmos in the game view:** select and move objects by clicking in the viewport.
- **Editing C# script properties.** C# scripts attached from `[script]` sections are not yet listed.
- **Reparenting** objects by dragging rows in the Scene dock.
