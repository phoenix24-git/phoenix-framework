// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "scripting/script_host.h"

#include <SDL3/SDL_loadso.h>

#include <cstdint>

#include "core/log.h"

namespace phoenix {

ScriptHost::~ScriptHost() {
    unload();
}

bool ScriptHost::load(const std::string& path, const PhxEngineApi* api) {
    unload();
    module_ = SDL_LoadObject(path.c_str());
    if (!module_) {
        PHX_LOG_ERROR("Scripts: cannot load '%s': %s", path.c_str(), SDL_GetError());
        return false;
    }
    auto init =
        reinterpret_cast<PhxScriptInitFn>(SDL_LoadFunction(module_, PHX_SCRIPT_INIT_SYMBOL));
    update_ =
        reinterpret_cast<PhxScriptUpdateFn>(SDL_LoadFunction(module_, PHX_SCRIPT_UPDATE_SYMBOL));
    shutdown_ = reinterpret_cast<PhxScriptShutdownFn>(
        SDL_LoadFunction(module_, PHX_SCRIPT_SHUTDOWN_SYMBOL));
    attach_ =
        reinterpret_cast<PhxScriptAttachFn>(SDL_LoadFunction(module_, PHX_SCRIPT_ATTACH_SYMBOL));
    if (!init) {
        PHX_LOG_ERROR("Scripts: '%s' does not export %s", path.c_str(), PHX_SCRIPT_INIT_SYMBOL);
        unload();
        return false;
    }
    const std::int32_t rc = init(api);
    if (rc != 0) {
        PHX_LOG_ERROR("Scripts: %s returned %d", PHX_SCRIPT_INIT_SYMBOL, rc);
        unload();
        return false;
    }
    path_ = path;
    PHX_LOG_INFO("Scripts: loaded native module %s", path.c_str());
    return true;
}

void ScriptHost::update(const PhxFrameInfo& frame) {
    if (update_) {
        update_(&frame);
    }
}

bool ScriptHost::attach(std::uint64_t object, const char* class_name, const char* properties) {
    if (!attach_) {
        PHX_LOG_ERROR("Scripts: cannot attach %s: no script module with %s is loaded", class_name,
                      PHX_SCRIPT_ATTACH_SYMBOL);
        return false;
    }
    const std::int32_t rc = attach_(object, class_name, properties ? properties : "");
    if (rc != 0) {
        PHX_LOG_ERROR("Scripts: %s(%s) returned %d", PHX_SCRIPT_ATTACH_SYMBOL, class_name, rc);
        return false;
    }
    return true;
}

void ScriptHost::unload() {
    if (!module_)
        return;
    if (shutdown_)
        shutdown_();
    SDL_UnloadObject(module_);
    module_ = nullptr;
    update_ = nullptr;
    shutdown_ = nullptr;
    attach_ = nullptr;
    path_.clear();
}

}  // namespace phoenix
