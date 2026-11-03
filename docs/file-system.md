---
title: File system (res://, user://)
nav_order: 6
---

# File system: `res://`, `user://` and packs
{: .no_toc }

Phoenix reads and writes files through **virtual paths**, the same scheme Godot uses. Game code
never deals with install folders, AppData, or the browser's storage APIs: it asks for
`res://levels/01.json` or `user://saves/slot1.json`, and the engine maps that path to the right
place on every platform.

1. TOC
{:toc}

## The roots

| Path | What it is | Writable | Where it lives |
|---|---|---|---|
| `res://` | Your project's resources: levels, textures, audio, scripts | In development, yes. In an exported game, **no** | Development: the project folder, plus any mounted packs. Exported game: the main `.pck` |
| `user://` | Per-player data: saves, settings, downloaded content, caches | Yes | **Linux:** `~/.local/share/phoenix/app_userdata/<project>`<br>**Windows:** `%APPDATA%\phoenix\app_userdata\<project>`<br>**macOS:** `~/Library/Application Support/phoenix/app_userdata/<project>`<br>**Web:** OPFS (`/persistent`), which survives reloads |
| `uid://` | Stable resource IDs that survive renames | No | Resolved to a `res://` path |

Any other absolute path refers to the host file system, for tools and the editor.

The `<project>` part is your app's name, or `application/config/name` if the project sets one. A
project can choose its own folder name with `application/config/use_custom_user_dir`, which gives
`<data dir>/<custom_user_dir_name>`.

## Using it

```cpp
#include <phoenix/vfs/vfs.h>
using namespace phoenix;

// Saves: written to a temporary file and swapped in only when complete,
// so a crash or power loss never leaves a half-written save.
vfs::write_text("user://saves/slot1.json", R"({"hp": 7})");
String save = vfs::read_text("user://saves/slot1.json");

// Resources
PackedByteArray png = vfs::read_bytes("res://ui/logo.png");
for (const String& level : vfs::list_files("res://levels")) { /* ... */ }

Error err;
String text = vfs::read_text("res://missing.txt", &err);  // err == ERR_FILE_NOT_FOUND
```

The full API is in [`vfs.h`](https://github.com/phoenix24-git/phoenix-framework/blob/main/engine/include/phoenix/vfs/vfs.h):

| Area | Functions |
|---|---|
| Read and write | `read_bytes`, `read_text`, `write_bytes`, `write_text`, `file_exists`, `file_size`, `modified_time` |
| Directories | `make_dir`, `list_files`, `list_dirs`, `dir_exists`, `remove`, `remove_recursive`, `rename`, `copy` |
| Paths | `normalize`, `root_of`, `globalize` (virtual → host), `localize` (host → virtual) |
| Packs | `mount_pack`, `using_packs`, `resources_read_only` |

Godot's `FileAccess` and `DirAccess` classes from the foundation work with the same paths, if
you need streaming or seeking.

## Safety

Every path is normalized before use. `..` is resolved, and any path that would climb out of its
root is **rejected** (`res://../secrets`, `user://../../etc/passwd`). Relative paths and unknown
schemes such as `http://` are rejected too. So a level file or a mod can't trick the game into
reading or writing outside `res://` or `user://`. Some operations are always refused:

- writing to `uid://`
- writing to `res://` in an exported game
- `remove_recursive` on a whole root (`res://` or `user://`)

## Packs: exported games, DLC and mods

A **pack** is an archive whose files appear under `res://`. Phoenix reads Godot's `.pck` format
and `.zip` files.

```bash
# Pack a project folder. Hidden files (.git, .phoenix) and build/ are skipped.
phoenix_pack my_game/ my_game.pck --exclude "res://raw/*" --exclude "*.psd"
```

There are three ways a pack gets used:

- **Exported game (main pack):**
  - Put `my_game.pck` next to the executable (`my_game` or `my_game.exe`), or pass
    `CoreOptions::main_pack`.
  - `res://` is then **only** the pack and is read-only. Stray files in the working directory are
    never visible.
- **DLC and patches:** call `vfs::mount_pack("user://dlc/forest.pck")` at runtime.
  - Its files layer over what is already in `res://`.
  - By default a later pack wins when it has the same file. Pass `replace_files = false` to keep
    the existing versions.
- **Mods:** mount a `.zip` the same way. Paths inside the zip are relative to `res://`.

{: .note }
Mount packs from the main thread before loading assets from them. Reading files is
thread-safe, and the loader and worker threads use it freely.

## Web

The browser has no real file system, so Phoenix uses WebAssembly's WasmFS:

- **`user://`** lives under `/persistent`, an [OPFS](https://developer.mozilla.org/docs/Web/API/File_System_API/Origin_private_file_system)
  mount that survives reloads. If the browser blocks OPFS (some private windows do), it falls back
  to an in-memory folder.
- **`res://`** comes from the files bundled with the build. Shipping a `.pck` next to the `.wasm`
  and mounting it is on the [roadmap](roadmap.md).
