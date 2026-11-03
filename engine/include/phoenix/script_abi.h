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

#define PHX_SCRIPT_ABI_VERSION 2u

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

/* PhxDrawRect.flags */
#define PHX_DRAW_PARALLELOGRAM 1u /* see PhxDrawRect */

/* One filled shape in the frame display list. 32 bytes. Logical pixels, y down.
 *
 * flags == 0: a rectangle (x, y, w, h) rotated by `rotation` radians around its center.
 * flags & PHX_DRAW_PARALLELOGRAM: the parallelogram with corner (x, y) and edges
 *   a = (w, h) and b = (rotation, corner_radius), i.e. corners o, o+a, o+a+b, o+b. Any 2D
 *   affine image of a square, so it draws box faces for isometric 3D, sheared panels, etc. */
typedef struct PhxDrawRect {
    float x, y, w, h;
    uint32_t rgba;
    float rotation;      /* radians; parallelogram: edge b.x */
    float corner_radius; /* reserved (0); parallelogram: edge b.y */
    uint32_t flags;      /* PHX_DRAW_* */
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

    /* --- Scene (GameObjects). Available when the app runs a scene (runScene); otherwise the
     * functions return 0/NULL. Objects are 64-bit handles, 0 = none; a destroyed object's
     * handle never comes back to life. Names are UTF-8. */
    uint64_t (*find_object)(void* engine, const char* name);
    uint64_t (*create_object)(void* engine, const char* name, uint64_t parent);
    /* Unity's Object.Destroy: deferred to the end of the frame, children included. */
    void (*destroy_object)(void* engine, uint64_t object);
    uint32_t (*object_alive)(void* engine, uint64_t object);
    void (*set_active)(void* engine, uint64_t object, uint32_t active);
    /* Zero-copy access to a component's data inside ECS chunk memory ("Transform" is the
     * LocalTransform struct). With add != 0 the component is added when missing. NULL if the
     * object or component doesn't exist. Valid until the next structural change (adding or
     * removing components, destroying objects): fetch it again each frame. */
    void* (*get_component)(void* engine, uint64_t object, const char* type, uint32_t add);
    /* Pointer to one field of a component (e.g. "MeshRenderer", "color"); *out_size receives
     * the field's size in bytes so callers can check the type they read it as. */
    void* (*get_field)(void* engine, uint64_t object, const char* type, const char* field,
                       uint32_t* out_size);
} PhxEngineApi;

/* Symbols a script module exports (C# via [UnmanagedCallersOnly(EntryPoint=...)]). */
typedef int32_t (*PhxScriptInitFn)(const PhxEngineApi* api);
typedef void (*PhxScriptUpdateFn)(const PhxFrameInfo* frame);
typedef void (*PhxScriptShutdownFn)(void);
/* Optional: attaches an instance of `class_name` (e.g. "Skyline.StackGame") to `object`, for
 * scene files' [script] sections. `properties` is "key=value" lines (the section's
 * assignments, values in Godot text syntax). Returns 0 on success. */
typedef int32_t (*PhxScriptAttachFn)(uint64_t object, const char* class_name,
                                     const char* properties);

#define PHX_SCRIPT_INIT_SYMBOL "phoenix_script_init"
#define PHX_SCRIPT_UPDATE_SYMBOL "phoenix_script_update"
#define PHX_SCRIPT_SHUTDOWN_SYMBOL "phoenix_script_shutdown"
#define PHX_SCRIPT_ATTACH_SYMBOL "phoenix_script_attach"

#ifdef __cplusplus
} /* extern "C" */

static_assert(sizeof(PhxBody2D) == 32, "PhxBody2D layout is ABI");
static_assert(sizeof(PhxDrawRect) == 32, "PhxDrawRect layout is ABI");
static_assert(sizeof(PhxFrameInfo) == 48, "PhxFrameInfo layout is ABI");
#endif

#endif /* PHOENIX_SCRIPT_ABI_H_ */
