#!/usr/bin/env python3
# Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
"""Run Godot's own code generators (core/**/*builders.py, make_*.py) without SCons.

  generate.py <vendored godot dir> <output dir>

Writes the *.gen.h / *.gen.cpp files that Godot's core/ expects into <output dir>,
mirroring Godot's paths (core/..., modules/...), so they resolve through an include path.
"""
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

sys.dont_write_bytecode = True  # never write __pycache__ into the vendored tree
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))  # our `methods` shim


class Value:
    """Stand-in for SCons env.Value(...)."""

    def __init__(self, value):
        self.value = value

    def read(self):
        return self.value

    def __str__(self):
        return str(self.value)


def load(path: Path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    godot = Path(sys.argv[1]).resolve()
    out = Path(sys.argv[2]).resolve()
    core = godot / "core"
    for sub in ("core/object", "core/extension", "core/input", "core/profiling", "modules"):
        (out / sub).mkdir(parents=True, exist_ok=True)

    version = load(godot / "version.py")
    builders = load(core / "core_builders.py")
    env = {"profiler": "", "profiler_sample_callstack": False, "profiler_track_memory": False,
           "profiler_record_on_demand": False}

    def gen(rel: str, fn, sources):
        fn([str(out / rel)], sources, env)

    gen("core/disabled_classes.gen.h", builders.disabled_class_builder, [Value([])])
    gen("core/version_generated.gen.h", builders.version_info_builder, [Value({
        # Engine identity reported by Engine::get_version_info() etc. is Phoenix's; the
        # numbers stay upstream's so compatibility checks keep working.
        "short_name": "phoenix", "name": "Phoenix Engine", "major": version.major,
        "minor": version.minor, "patch": version.patch, "status": version.status,
        "build": "phoenix", "module_config": version.module_config,
        "website": "https://github.com/phoenix24-git/phoenix-framework",
        "docs_branch": version.docs,
    })])
    upstream = dict(line.split(": ", 1) for line in (godot / "UPSTREAM").read_text().splitlines() if ": " in line)
    gen("core/version_hash.gen.cpp", builders.version_hash_builder,
        [Value({"git_hash": upstream.get("commit", ""), "git_timestamp": 0})])
    gen("core/script_encryption_key.gen.cpp", builders.encryption_key_builder, [Value("")])
    gen("core/authors.gen.h", builders.make_authors_header, [godot / "AUTHORS.md"])
    gen("core/donors.gen.h", builders.make_donors_header, [godot / "DONORS.md"])
    gen("core/license.gen.h", builders.make_license_header, [godot / "COPYRIGHT.txt", godot / "LICENSE.txt"])

    gen("core/object/gdvirtual.gen.h", load(core / "object" / "make_virtuals.py").run, [])
    ext = core / "extension"
    gen("core/extension/ext_wrappers.gen.h", load(ext / "make_wrappers.py").run, [])
    gen("core/extension/gdextension_interface_dump.gen.h", load(ext / "make_interface_dumper.py").run,
        [ext / "gdextension_interface.json"])
    gen("core/extension/gdextension_interface.gen.h", load(ext / "make_interface_header.py").run,
        [ext / "gdextension_interface.json"])
    gen("core/input/default_controller_mappings.gen.cpp",
        load(core / "input" / "input_builders.py").make_default_controller_mappings,
        [core / "input" / "gamecontrollerdb.txt", core / "input" / "godotcontrollerdb.txt"])
    gen("core/profiling/profiling.gen.h", load(core / "profiling" / "profiling_builders.py").profiler_gen_builder,
        [Value(""), Value(False), Value(False), Value(False)])

    # Phoenix builds no Godot modules.
    (out / "modules" / "modules_enabled.gen.h").write_text("#pragma once\n/* Phoenix: no Godot modules. */\n")
    (out / ".stamp").write_text("ok\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
