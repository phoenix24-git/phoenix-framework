// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <deque>
#include <functional>
#include <optional>
#include <string>

struct SDL_Thread;
struct SDL_Mutex;
struct SDL_Condition;

namespace phoenix {

/// Engine IO thread (Flutter has the same split: platform/UI/raster/IO).
///
/// On the web the thread mounts the Origin Private File System through WasmFS
/// at /persistent. OPFS calls block, which is only legal off the browser main
/// thread, so all file work happens here and results are posted back to the
/// main thread via `pump()`.
class StorageService {
  public:
    using ReadCallback = std::function<void(std::optional<std::string>)>;

    ~StorageService();
    /// `root` is the native directory to use (ignored on the web).
    bool start(std::string root);
    void stop();

    void write(std::string key, std::string value);
    void read(std::string key, ReadCallback on_done);

    /// Delivers finished reads; call once per frame on the main thread.
    void pump();
    /// Blocks until queued requests are done (tests / shutdown only).
    void flush();

    const char* backend() const { return backend_; }
    static bool valid_key(const std::string& key);

  private:
    struct Request {
        enum class Kind { kRead, kWrite } kind;
        std::string key;
        std::string value;
        ReadCallback callback;
    };
    struct Completion {
        ReadCallback callback;
        std::optional<std::string> value;
    };
    static int io_main(void* self);
    void mount();
    std::string path_for(const std::string& key) const;

    SDL_Thread* thread_ = nullptr;
    SDL_Mutex* mutex_ = nullptr;
    SDL_Condition* cond_ = nullptr;
    std::deque<Request> requests_;
    std::deque<Completion> completions_;
    std::string root_;
    const char* backend_ = "memory";
    bool running_ = false;
    int in_flight_ = 0;
};

}  // namespace phoenix
