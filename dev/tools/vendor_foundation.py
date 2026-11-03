#!/usr/bin/env python3
# Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
"""Vendor Phoenix's foundation layer from upstream Godot Engine.

Godot is MIT-licensed (Copyright (c) 2014-present Godot Engine contributors,
(c) 2007-2014 Juan Linietsky, Ariel Manzur). We copy it verbatim, so its license
and per-file headers are kept, and every update is a reviewable diff.

  git clone --depth 1 --branch 4.7.2-stable https://github.com/godotengine/godot /tmp/godot
  dev/tools/vendor_foundation.py /tmp/godot

Copies:
  core/                           -> engine/thirdparty/phoenix_core/core/
  the thirdparty/ libraries core/ needs -> engine/thirdparty/phoenix_core/thirdparty/
  LICENSE.txt, COPYRIGHT.txt, AUTHORS.md, DONORS.md
  editor/icons/*.svg (without the Godot logo, which is CC-BY-4.0 + trademark)
                                  -> engine/assets/editor/icons/
  thirdparty/fonts (+ licenses)   -> engine/assets/editor/fonts/
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEST = REPO / "engine" / "thirdparty" / "phoenix_core"
ASSETS = REPO / "engine" / "assets" / "editor"

# Third-party code that Godot's core/ compiles (see core/SCsub, core/crypto/SCsub).
THIRDPARTY_DIRS = ["misc", "zlib", "zstd", "minizip", "clipper2", "brotli", "grisu2", "gamepadmotionhelpers", "doctest"]
MBEDTLS_SOURCES = [
    "aes.c", "base64.c", "constant_time.c", "ctr_drbg.c", "entropy.c",
    "md.c", "md5.c", "sha1.c", "sha256.c", "godot_core_mbedtls_platform.c",
]
TOP_LEVEL_FILES = ["LICENSE.txt", "COPYRIGHT.txt", "AUTHORS.md", "DONORS.md"]
# Logo artwork: CC-BY-4.0 (Andrea Calabró) and a Godot Foundation trademark. Not ours to ship.
EXCLUDED_ICONS = {"Godot.svg", "GodotFile.svg", "GodotMonochrome.svg"}


def copytree(src: Path, dst: Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst, ignore=shutil.ignore_patterns("SCsub", "*.pyc", "__pycache__"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("upstream", type=Path, help="path to an upstream Godot source checkout")
    args = parser.parse_args()
    src: Path = args.upstream.resolve()
    if not (src / "core" / "typedefs.h").exists():
        print(f"error: {src} is not a Godot checkout", file=sys.stderr)
        return 1

    sha = subprocess.run(["git", "-C", str(src), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    version = {}
    exec((src / "version.py").read_text(), version)
    tag = f'{version["major"]}.{version["minor"]}' + (f'.{version["patch"]}' if version.get("patch") else "") + f'-{version["status"]}'

    DEST.mkdir(parents=True, exist_ok=True)
    copytree(src / "core", DEST / "core")
    for name in TOP_LEVEL_FILES:
        shutil.copy2(src / name, DEST / name)
    shutil.copy2(src / "version.py", DEST / "version.py")

    third = DEST / "thirdparty"
    third.mkdir(exist_ok=True)
    for d in THIRDPARTY_DIRS:
        copytree(src / "thirdparty" / d, third / d)
    mbed = third / "mbedtls"
    if mbed.exists():
        shutil.rmtree(mbed)
    shutil.copytree(src / "thirdparty" / "mbedtls" / "include", mbed / "include")
    (mbed / "library").mkdir(parents=True)
    for f in (src / "thirdparty" / "mbedtls" / "library").iterdir():
        if f.suffix == ".h" or f.name in MBEDTLS_SOURCES:
            shutil.copy2(f, mbed / "library" / f.name)
    shutil.copy2(src / "thirdparty" / "mbedtls" / "LICENSE", mbed / "LICENSE")

    # Godot's own core/ unit tests (doctest), minus the few that need scene/ or servers/.
    tests = DEST / "tests"
    if tests.exists():
        shutil.rmtree(tests)
    tests.mkdir()
    for name in ["test_macros.h", "test_macros.cpp", "test_utils.h", "test_utils.cpp", "test_tools.h",
                 "signal_watcher.h", "signal_watcher.cpp"]:
        shutil.copy2(src / "tests" / name, tests / name)
    copytree(src / "tests" / "core", tests / "core")
    copytree(src / "tests" / "data", tests / "data")
    for test in tests.glob("core/**/*.cpp"):
        text = test.read_text(encoding="utf-8", errors="replace")
        if any(f'#include "{d}/' in text for d in ("scene", "servers", "editor", "modules", "main")):
            test.unlink()

    icons = ASSETS / "icons"
    if icons.exists():
        shutil.rmtree(icons)
    icons.mkdir(parents=True)
    count = 0
    for svg in sorted((src / "editor" / "icons").glob("*.svg")):
        if svg.name not in EXCLUDED_ICONS:
            shutil.copy2(svg, icons / svg.name)
            count += 1
    copytree(src / "thirdparty" / "fonts", ASSETS / "fonts")

    (DEST / "UPSTREAM").write_text(f"repository: https://github.com/godotengine/godot\ntag: {tag}\ncommit: {sha}\n")
    print(f"Vendored foundation from Godot {tag} ({sha[:12]}): core/, {len(THIRDPARTY_DIRS) + 1} thirdparty libs, {count} icons, fonts.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
