---
title: Editor (web-first PWA)
parent: Engine roadmap
nav_order: 8
---

# The Phoenix editor: built with Phoenix, web-first
{: .no_toc }

Godot's editor is a Godot application, using the same scene system, UI toolkit and servers as the
games it makes. Phoenix does the same, and goes **web-first**: the editor is an installable
**Progressive Web App**, and the same codebase also builds a native desktop editor.

1. TOC
{:toc}

## Is a web-first PWA editor possible?

Yes, with these building blocks, all of which ship in Chromium-based browsers today:

| Need | Web technology | Status |
|---|---|---|
| Big projects in memory | wasm64 (Memory64), up to 16 GiB | ✅ already used by the runtime |
| Multithreaded import, bake and build | pthreads / Web Workers + SharedArrayBuffer | ✅ already used |
| Open a project folder from disk | **File System Access API** (`showDirectoryPicker`), with handles persisted in IndexedDB | ✅ Chrome/Edge (Firefox/Safari: fall back to OPFS + zip import/export) |
| Fast project cache | **OPFS** (sync access handles in workers) | ✅ already used |
| Install and run offline | Web App Manifest + service worker (we already ship one) | ✅ |
| Shader compilation in the browser | Slang compiled to wasm | ✅ (the Slang playground does it) |
| Model import in the browser | Assimp / fastgltf / ufbx compiled to wasm | ✅ |
| GPU viewports | WebGPU through SDL_GPU | ✅ |
| Play-in-editor | game runs in an isolated iframe/worker with its own wasm instance; DevTools protocol for the scene tree and profiler | ✅ (protocol exists as Phoenix DevTools) |
| Desktop-only tasks (console SDKs, native builds) | the native desktop editor build, or a local "phoenix daemon" the PWA talks to over WebSocket | planned |

Limits to plan for: Safari lacks Memory64 and the File System Access API, so the PWA editor targets
Chrome/Edge first. Memory is per tab. Very large projects use the native build.

## Architecture

```
 editor/ (a Phoenix app)
   ├─ EditorApp            docking layout, menus, command palette, shortcuts
   ├─ panels/              SceneTree, Inspector (ClassDB reflection), AssetBrowser, Console,
   │                       Profiler, Animation timeline, Shader graph, Prefab mode
   ├─ viewport/            3D/2D viewports = RenderingServer viewports + gizmos + picking (ID buffer)
   ├─ import/              importers on the worker pool; writes .import sidecars + imported cache
   ├─ project/             VFS over File System Access / OPFS / native FS; git-friendly text assets
   ├─ undo/                UndoRedo (from core) over every inspector and scene edit
   └─ play/                launches the game (iframe/worker on web, child process native) + remote debugger
```

- **UI**: the editor uses Phoenix's own widget layer, upgraded in Phase 2: SDF text, docking,
  virtualized lists and trees, theming. That forces the UI layer to become production quality,
  which pays off for games too.
- **Inspector**: generated from `ClassDB` metadata (property hints: ranges, enums, colours, curves,
  resource pickers). Custom inspectors in C# work like Unity's `CustomEditor` and Godot's
  `EditorInspectorPlugin`.
- **Prefab mode**: edit a prefab in isolation with override highlighting, and apply/revert per field.
- **Hot reload**: C++ app code as a side module and C# scripts reloaded; assets re-imported on change
  (FileSystemObserver on the web, inotify/FSEvents natively).
- **Extensibility**: editor plugins in C# or C++ through the Script ABI, registered with `ClassDB`.
- **Collaboration (later)**: a CRDT-based scene document so several users can edit in the browser.

## Milestones

| Milestone | Content | Phase |
|---|---|---|
| Shell | PWA install, open folder, asset browser, console, project settings | 2 |
| Scene editing | scene tree, inspector, 3D viewport + gizmos, undo/redo, play-in-editor | 2 |
| Prefabs | nested prefabs, variants, prefab mode, overrides UI | 2 |
| Content tools | material editor, Slang shader graph, animation timeline, particle editor, terrain tools | 3 |
| Build & profile | `phoenix build` from the editor, GPU/CPU profiler, frame debugger | 3 |
| Godot import UX | "Open Godot project" with an import report | 3 |
| Native editor parity | same features in the desktop build plus console deploy | 4 |
