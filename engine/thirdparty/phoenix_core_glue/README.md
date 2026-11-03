# phoenix_core_glue — building the Phoenix foundation layer

`../phoenix_core/` holds an **unmodified** copy of Godot Engine's `core/` (plus the libraries and unit tests
it needs), vendored with `dev/tools/vendor_foundation.py`. This directory holds everything Phoenix adds
to build and run it. Godot's sources are never edited: any change of behaviour lives here.

| Path | Purpose |
|---|---|
| `CMakeLists.txt` | `phoenix::foundation` static library (C++17, Godot's defines), plus `phoenix_foundation_tests` |
| `codegen/` | runs Godot's own `*.gen.*` generators without SCons; `methods.py` holds helpers copied from Godot (MIT) |
| `compat/` | small headers at the include paths `core/` expects from `scene/`, `servers/` and `main/` (`Node`, `PackedScene`, `RenderingServer::sync`, `DisplayServer`, `Main`). These are the seams where Phoenix's servers plug in |
| `platform/` | Phoenix's platform layer on SDL3: `OS_Phoenix`, `FileAccessSDL`, `DirAccessSDL`, `platform_config.h`, and `CoreRuntime` (start-up/shut-down following Godot's `Main::test_setup()`) |
| `tests/` | runner for Godot's own core test suite |

## Using it

```cpp
#include "platform/core_runtime.h"
#include "core/variant/variant.h"

phoenix::foundation::CoreRuntime::initialize({.app_name = "my_game", .resource_dir = "/path/to/project"});
Dictionary d;
d["hello"] = Vector3(1, 2, 3);
print_line(Variant(d).stringify());
phoenix::foundation::CoreRuntime::finalize();
```

## Verification

`phoenix_foundation_tests` runs Godot 4.7.2's own core test suite against this build, natively and on
wasm64 with pthreads under Node: **831 test cases, 197,500 assertions**. Excluded:

| Excluded | Why |
|---|---|
| `[SceneTree]`, `[Editor]` cases | need Godot's `scene/`/editor layers |
| `[PCKPacker]` | packs Godot's logo artwork, which is not vendored (CC-BY-4.0 + trademark) |
| `test_ip`, `test_tcp_server`, `test_udp_server`, `test_uds_server`, `test_stream_peer_tcp` | need a platform socket/IP driver (known gap) |
| 5 test files that `#include` `scene/` or `servers/` | dropped at vendoring time |
| `[OS] Execute` on the web | browsers cannot spawn processes |

## Known gaps

- Networking: there is no IP/socket driver yet, so `IP`, `StreamPeerTCP` and `TCPServer` are unavailable. On the
  web, `HTTPClient::create()` returns null (Godot's web platform uses the Fetch API).
- `FileAccessSDL`: Unix permissions, hidden/read-only attributes, symlinks and shrinking `resize()`
  are unsupported. SDL3 doesn't expose them.
- `PackedScene`/`SceneState` are placeholders until the `.tscn`/`.scn` importer lands.

## Updating Godot

```bash
git clone --depth 1 --branch <tag> https://github.com/godotengine/godot /tmp/godot
dev/tools/vendor_foundation.py /tmp/godot
cmake --build --preset native-debug && ctest --preset native-debug   # must stay green
```
