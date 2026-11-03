// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/arena.h"
#include "core/job_system.h"
#include "phoenix/console_profile.h"
#include "phoenix/script_abi.h"
#include "physics/physics_world.h"
#include "renderer/renderer.h"
#include "scripting/script_host.h"
#include "storage/storage_service.h"

struct SDL_Window;
union SDL_Event;

namespace phoenix {

struct EngineConfig {
    std::string app_name = "phoenix_app";
    std::string title = "Phoenix";
    std::string build_mode = "debug";
    ConsoleProfile profile{};
    /// Optional native script module (NativeAOT C# or C++ shared library).
    std::string script_module;
    /// Create no window/renderer (CI smoke runs, servers).
    bool headless = false;
    /// Quit after this many frames (0 = run forever).
    std::uint64_t max_frames = 0;
    int width = 1024;
    int height = 640;
};

/// Snapshot published to Phoenix DevTools a few times per second.
struct EngineStats {
    double fps = 0;
    double frame_ms = 0;
    std::uint64_t frame = 0;
    std::uint32_t bodies = 0;
    std::uint64_t physics_steps = 0;
    std::uint32_t workers = 0;
    std::uint64_t jobs_executed = 0;
    std::uint64_t arena_high_water = 0;
    std::uint64_t arena_capacity = 0;
    RendererStats renderer{};
};

/// The embedder: owns the window, the console resources and the frame loop.
/// One instance per process (like Flutter's Shell).
class Engine {
  public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    static Engine* current();

    bool init(const EngineConfig& config);
    /// Returns false when the app should quit.
    bool handle_event(const SDL_Event& event);
    /// Runs one frame. Returns false when the app should quit.
    bool iterate();
    void shutdown();

    const PhxEngineApi* api() const { return &api_; }
    PhysicsWorld& physics() { return physics_; }
    JobSystem& jobs() { return jobs_; }
    StorageService& storage() { return storage_; }
    const EngineStats& stats() const { return stats_; }
    const char* renderer_name() const { return renderer_ ? renderer_->name() : "none"; }

    /// Reserves `count` draw rects in this frame's arena (script ABI).
    PhxDrawRect* reserve_rects(std::uint32_t count);

  private:
    void publish_stats(double now);

    EngineConfig config_;
    SDL_Window* window_ = nullptr;
    std::unique_ptr<Renderer> renderer_;
    JobSystem jobs_;
    PhysicsWorld physics_;
    StorageService storage_;
    ScriptHost scripts_;
    LinearArena frame_arena_;
    std::vector<PhxDrawRect> display_list_;
    struct ScriptRects {
        PhxDrawRect* data;
        std::uint32_t count;
    };
    std::vector<ScriptRects> script_rects_;
    PhxEngineApi api_{};
    EngineStats stats_{};
    std::uint64_t start_ns_ = 0;
    std::uint64_t last_ns_ = 0;
    double last_publish_ = 0;
    std::uint64_t frames_since_publish_ = 0;
    float pointer_x_ = 0, pointer_y_ = 0;
    bool pointer_down_ = false;
    bool tapped_ = false;
    bool quit_ = false;
};

}  // namespace phoenix
