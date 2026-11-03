# Third-party notices

Phoenix is licensed under Apache-2.0. It includes or builds the third-party components below,
each under its own license. Full license texts ship next to the code:
`engine/thirdparty/phoenix_core/LICENSE.txt`, `engine/thirdparty/phoenix_core/COPYRIGHT.txt` (Debian-format,
covers everything Godot bundles), and the `LICENSE*` files in each directory.

## Godot Engine — `engine/thirdparty/phoenix_core/`, `engine/assets/editor/icons/`

Godot Engine 4.7.2-stable (commit in `engine/thirdparty/phoenix_core/UPSTREAM`), vendored verbatim with
`dev/tools/vendor_foundation.py`: `core/`, its unit tests, and the editor icons.

> Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md).
> Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.
> Licensed under the MIT (Expat) license.

`engine/thirdparty/phoenix_core_glue/codegen/methods.py` also contains helper functions copied from Godot's
`methods.py`, under the same license. The Godot logo is **not** included: it is CC-BY-4.0 artwork
and a trademark of the Godot Foundation, so the logo icons are excluded at vendoring time. Phoenix
is not affiliated with or endorsed by the Godot Foundation.

### Libraries bundled with Godot's core

| Component | License | Copyright |
|---|---|---|
| brotli (decoder) | MIT | The Brotli Authors |
| Clipper2 | BSL-1.0 | Angus Johnson |
| doctest (tests only) | MIT | Viktor Kirilov |
| GamepadMotionHelpers | MIT | Julian "Jibb" Smart |
| grisu2 | MIT and Apache-2.0 | Florian Loitsch |
| Mbed TLS (MD5/SHA/AES/base64 subset) | Apache-2.0 | The Mbed TLS Contributors |
| minizip | Zlib | Gilles Vollant |
| zlib | Zlib | Jean-loup Gailly and Mark Adler |
| Zstandard | BSD-3-Clause | Meta Platforms, Inc. and affiliates |
| misc: fastlz, polypartition, others | MIT | see `COPYRIGHT.txt` |
| misc: pcg | Apache-2.0 | M.E. O'Neill |
| misc: r128, smolv, stb_rect_pack | Unlicense or MIT | see `COPYRIGHT.txt` |
| misc: smaz | BSD-3-Clause | Salvatore Sanfilippo |
| `core/math/convex_hull.cpp` (from Bullet) | MIT and Zlib | Ole Kniemeyer, MAXON |

## Fonts — `engine/assets/editor/fonts/`

| Font | License | Copyright |
|---|---|---|
| Inter | SIL OFL 1.1 | The Inter Project Authors |
| JetBrains Mono | SIL OFL 1.1 | JetBrains s.r.o. |
| Noto Sans (several scripts) | SIL OFL 1.1 | Google Inc. |
| Open Sans | SIL OFL 1.1 | The Open Sans Project Authors |
| Vazirmatn | SIL OFL 1.1 | The Vazirmatn Project Authors |
| Droid Sans Fallback / Japanese | Apache-2.0 | The Android Open Source Project |

Fonts are redistributed unmodified with their license files. The OFL forbids selling them on their own.

## Fetched at build time (not in the repository)

| Component | License |
|---|---|
| SDL3 (SDL_wgpu fork) | Zlib |
| Jolt Physics | MIT |
| doctest | MIT |
| Emscripten, emdawnwebgpu | MIT / BSD-3-Clause |
