// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix compat seam for Godot core/ (see ../README.md). Declares only what the vendored
// core/ references; the API shape mirrors Godot (MIT) so upstream core/ compiles unmodified.
#include "scene/main/node.h"
#include "servers/display/display_server.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

namespace {
RenderingServer g_null_rendering_server;
DisplayServer g_null_display_server;
RenderingDevice *g_rendering_device = nullptr;
RenderingServer *g_rendering_server = &g_null_rendering_server;
DisplayServer *g_display_server = &g_null_display_server;
} // namespace

RenderingDevice *RenderingDevice::get_singleton() { return g_rendering_device; }
void RenderingDevice::set_singleton(RenderingDevice *p_device) { g_rendering_device = p_device; }
RenderingServer *RenderingServer::get_singleton() { return g_rendering_server; }
void RenderingServer::set_singleton(RenderingServer *p_server) { g_rendering_server = p_server ? p_server : &g_null_rendering_server; }
DisplayServer *DisplayServer::get_singleton() { return g_display_server; }
void DisplayServer::set_singleton(DisplayServer *p_server) { g_display_server = p_server ? p_server : &g_null_display_server; }
