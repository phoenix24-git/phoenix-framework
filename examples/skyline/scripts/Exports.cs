// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Runtime.InteropServices;
using Phoenix;
using Phoenix.Interop;

namespace Skyline;

/// <summary>
/// The module's entry point. Registers the classes that scene files can attach by name
/// ([script class="Skyline.StackGame"]); NativeAOT has no reflection to find them otherwise.
/// </summary>
public static unsafe class Exports
{
    [UnmanagedCallersOnly(EntryPoint = "phoenix_script_init")]
    public static int Init(PhxEngineApi* api) =>
        ScriptHost.Init(api, scripts => scripts.Add<StackGame>().Add<CameraRig>().Add<ScoreHud>());
}
