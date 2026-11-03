/* Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
 *
 * Phoenix Script ABI — the zero-overhead contract between the C++ engine and
 * script modules (C# compiled with NativeAOT, or C/C++ shared libraries).
 *
 * Rules that keep this boundary free of marshalling:
 *   - Only blittable types: fixed-width integers, floats, pointers, and
 *     structs made of those. No strings-by-value, no bools, no managed types.
 *   - Engine data is shared by pointer (e.g. the body snapshot is a
 *     `const PhxBody2D*` the script views as Span<PhxBody2D> without copying).
 *   - Scripts write draw commands straight into engine-owned frame memory
 *     (`draw_rects` returns a pointer to reserved display-list slots).
 *   - Calls are plain C function pointers; C# sees them as
 *     `delegate* unmanaged<...>` with [assembly: DisableRuntimeMarshalling].
 *
 * Any layout change MUST bump PHX_SCRIPT_ABI_VERSION. The C# mirror lives in
 * packages/phoenix/src/Interop/ScriptAbi.cs and is size-checked by tests.
 */
#ifndef PHOENIX_SCRIPT_ABI_H_
#define PHOENIX_SCRIPT_ABI_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PHX_SCRIPT_ABI_VERSION 1u

#define PHX_LOG_LEVEL_INFO 0
#define PHX_LOG_LEVEL_WARN 1
#define PHX_LOG_LEVEL_ERROR 2

/* One rigid body in the published physics snapshot. 32 bytes. */
typedef struct PhxBody2D {
    float x, y;           /* center, meters (y up) */
    float angle;          /* radians, counter-clockwise */
    float half_w, half_h; /* meters */
    uint32_t id;
    uint32_t rgba;  /* 0xRRGGBBAA */
    uint32_t flags; /* bit 0: dynamic */
} PhxBody2D;

/* One rectangle in the frame display list. 32 bytes. Logical pixels, y down. */
typedef struct PhxDrawRect {
    float x, y, w, h;
    uint32_t rgba;
    float rotation;      /* radians around the rect center */
    float corner_radius; /* reserved */
    uint32_t reserved;
} PhxDrawRect;

/* Per-frame input handed to scripts. 48 bytes. */
typedef struct PhxFrameInfo {
    double time_seconds;
    float delta_seconds;
    uint32_t frame_index;
    float viewport_width, viewport_height;
    float pointer_x, pointer_y;
    uint32_t pointer_buttons;
    uint32_t body_count;
    uint64_t reserved;
} PhxFrameInfo;

/* Engine services exposed to scripts. Pointers stay valid until shutdown. */
typedef struct PhxEngineApi {
    uint32_t abi_version;
    uint32_t struct_size;
    void* engine;
    void (*log)(void* engine, int32_t level, const char* utf8);
    uint32_t (*spawn_box)(void* engine, float x, float y, float half_w, float half_h, float angle,
                          uint32_t dynamic, uint32_t rgba);
    void (*apply_impulse)(void* engine, uint32_t body, float ix, float iy);
    const PhxBody2D* (*bodies)(void* engine, uint32_t* out_count);
    PhxDrawRect* (*draw_rects)(void* engine, uint32_t count);
} PhxEngineApi;

/* Symbols a script module exports (C# via [UnmanagedCallersOnly(EntryPoint=...)]). */
typedef int32_t (*PhxScriptInitFn)(const PhxEngineApi* api);
typedef void (*PhxScriptUpdateFn)(const PhxFrameInfo* frame);
typedef void (*PhxScriptShutdownFn)(void);

#define PHX_SCRIPT_INIT_SYMBOL "phoenix_script_init"
#define PHX_SCRIPT_UPDATE_SYMBOL "phoenix_script_update"
#define PHX_SCRIPT_SHUTDOWN_SYMBOL "phoenix_script_shutdown"

#ifdef __cplusplus
} /* extern "C" */

static_assert(sizeof(PhxBody2D) == 32, "PhxBody2D layout is ABI");
static_assert(sizeof(PhxDrawRect) == 32, "PhxDrawRect layout is ABI");
static_assert(sizeof(PhxFrameInfo) == 48, "PhxFrameInfo layout is ABI");
#endif

#endif /* PHOENIX_SCRIPT_ABI_H_ */
