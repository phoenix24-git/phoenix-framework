// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "storage/storage_service.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>

#include <algorithm>

#include "core/log.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/wasmfs.h>
#endif

namespace phoenix {

StorageService::~StorageService() {
    stop();
}

bool StorageService::valid_key(const std::string& key) {
    return !key.empty() && key.size() <= 128 && std::all_of(key.begin(), key.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-' || c == '.';
    }) && key.front() != '.';
}

bool StorageService::start(std::string root) {
    root_ = std::move(root);
    mutex_ = SDL_CreateMutex();
    cond_ = SDL_CreateCondition();
    running_ = true;
    thread_ = SDL_CreateThread(&StorageService::io_main, "phoenix.io", this);
    if (!thread_) {
        PHX_LOG_ERROR("Storage: cannot start IO thread: %s", SDL_GetError());
        running_ = false;
        return false;
    }
    return true;
}

void StorageService::stop() {
    if (!thread_) {
        return;
    }
    SDL_LockMutex(mutex_);
    running_ = false;
    SDL_BroadcastCondition(cond_);
    SDL_UnlockMutex(mutex_);
    SDL_WaitThread(thread_, nullptr);
    thread_ = nullptr;
    SDL_DestroyCondition(cond_);
    SDL_DestroyMutex(mutex_);
    cond_ = nullptr;
    mutex_ = nullptr;
}

void StorageService::mount() {
#ifdef __EMSCRIPTEN__
    backend_t opfs = wasmfs_create_opfs_backend();
    if (opfs && wasmfs_create_directory("/persistent", 0777, opfs) == 0) {
        root_ = "/persistent/";
        backend_ = "opfs";
    } else {
        root_ = "/tmp/";
        backend_ = "memory";
        PHX_LOG_WARN("Storage: OPFS unavailable, data will not persist");
    }
#else
    if (!root_.empty() &&
        (SDL_CreateDirectory(root_.c_str()) || SDL_GetPathInfo(root_.c_str(), nullptr))) {
        if (root_.back() != '/' && root_.back() != '\\')
            root_ += '/';
        backend_ = "native";
    } else {
        backend_ = "memory";
    }
#endif
    PHX_LOG_INFO("Storage: backend '%s' at %s", backend_, root_.c_str());
}

std::string StorageService::path_for(const std::string& key) const {
    return root_ + key;
}

void StorageService::write(std::string key, std::string value) {
    if (!valid_key(key)) {
        PHX_LOG_WARN("Storage: invalid key '%s'", key.c_str());
        return;
    }
    SDL_LockMutex(mutex_);
    requests_.push_back({Request::Kind::kWrite, std::move(key), std::move(value), {}});
    ++in_flight_;
    SDL_SignalCondition(cond_);
    SDL_UnlockMutex(mutex_);
}

void StorageService::read(std::string key, ReadCallback on_done) {
    if (!valid_key(key)) {
        if (on_done)
            on_done(std::nullopt);
        return;
    }
    SDL_LockMutex(mutex_);
    requests_.push_back({Request::Kind::kRead, std::move(key), {}, std::move(on_done)});
    ++in_flight_;
    SDL_SignalCondition(cond_);
    SDL_UnlockMutex(mutex_);
}

void StorageService::pump() {
    if (!mutex_)
        return;
    std::deque<Completion> done;
    SDL_LockMutex(mutex_);
    done.swap(completions_);
    SDL_UnlockMutex(mutex_);
    for (auto& c : done) {
        if (c.callback)
            c.callback(std::move(c.value));
    }
}

void StorageService::flush() {
    for (;;) {
        SDL_LockMutex(mutex_);
        const bool idle = in_flight_ == 0;
        SDL_UnlockMutex(mutex_);
        if (idle)
            break;
        SDL_Delay(1);
    }
}

int StorageService::io_main(void* self_ptr) {
    auto* self = static_cast<StorageService*>(self_ptr);
    self->mount();
    for (;;) {
        SDL_LockMutex(self->mutex_);
        while (self->running_ && self->requests_.empty()) {
            SDL_WaitCondition(self->cond_, self->mutex_);
        }
        if (self->requests_.empty()) {
            SDL_UnlockMutex(self->mutex_);
            return 0;
        }
        Request req = std::move(self->requests_.front());
        self->requests_.pop_front();
        SDL_UnlockMutex(self->mutex_);

        const std::string path = self->path_for(req.key);
        Completion completion{std::move(req.callback), std::nullopt};
        if (req.kind == Request::Kind::kWrite) {
            // Write-then-rename keeps the previous value if we crash mid-write.
            const std::string tmp = path + ".tmp";
            if (!SDL_SaveFile(tmp.c_str(), req.value.data(), req.value.size()) ||
                !SDL_RenamePath(tmp.c_str(), path.c_str())) {
                PHX_LOG_WARN("Storage: write '%s' failed: %s", req.key.c_str(), SDL_GetError());
            }
        } else {
            size_t size = 0;
            if (void* data = SDL_LoadFile(path.c_str(), &size)) {
                completion.value.emplace(static_cast<const char*>(data), size);
                SDL_free(data);
            }
        }
        SDL_LockMutex(self->mutex_);
        if (req.kind == Request::Kind::kRead) {
            self->completions_.push_back(std::move(completion));
        }
        --self->in_flight_;
        SDL_UnlockMutex(self->mutex_);
    }
}

}  // namespace phoenix
