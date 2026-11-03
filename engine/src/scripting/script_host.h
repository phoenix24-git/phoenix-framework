// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

#include "phoenix/script_abi.h"

struct SDL_SharedObject;

namespace phoenix {

/// Loads a native script module (a C# assembly compiled with NativeAOT, or any
/// C/C++ shared library) that exports the Phoenix Script ABI entry points.
///
/// The engine calls straight through the exported C symbols: no CLR hosting,
/// no reflection, no marshalling stubs.
class ScriptHost {
  public:
    ~ScriptHost();
    bool load(const std::string& path, const PhxEngineApi* api);
    void unload();
    void update(const PhxFrameInfo& frame);
    bool loaded() const { return module_ != nullptr; }
    const std::string& path() const { return path_; }

  private:
    SDL_SharedObject* module_ = nullptr;
    PhxScriptUpdateFn update_ = nullptr;
    PhxScriptShutdownFn shutdown_ = nullptr;
    std::string path_;
};

}  // namespace phoenix
