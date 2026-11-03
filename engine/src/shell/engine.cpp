// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "shell/engine.h"

#include <SDL3/SDL.h>

#include <algorithm>

#include "core/log.h"
#include "framework/engine_api.h"
#include "phoenix/storage.h"
#include "phoenix/widgets.h"
#include "shell/devtools_bridge.h"

namespace phoenix {

namespace {

Engine* g_engine = nullptr;

// --- Script ABI trampolines (plain C function pointers, no marshalling) ---
void api_log(void*, std::int32_t level, const char* text) {
    switch (level) {
        case PHX_LOG_LEVEL_WARN:
            PHX_LOG_WARN("%s", text);
            break;
        case PHX_LOG_LEVEL_ERROR:
            PHX_LOG_ERROR("%s", text);
            break;
        default:
            PHX_LOG_INFO("%s", text);
            break;
    }
}
std::uint32_t api_spawn_box(void* e, float x, float y, float hw, float hh, float angle,
                            std::uint32_t dynamic, std::uint32_t rgba) {
    return static_cast<Engine*>(e)->physics().add_box(x, y, hw, hh, angle, dynamic != 0, rgba);
}
void api_apply_impulse(void* e, std::uint32_t body, float ix, float iy) {
    static_cast<Engine*>(e)->physics().add_impulse(body, ix, iy);
}
const PhxBody2D* api_bodies(void* e, std::uint32_t* count) {
    auto span = static_cast<Engine*>(e)->physics().bodies();
    if (count)
        *count = static_cast<std::uint32_t>(span.size());
    return span.data();
}
PhxDrawRect* api_draw_rects(void* e, std::uint32_t count) {
    return static_cast<Engine*>(e)->reserve_rects(count);
}

}  // namespace

Engine::Engine() = default;
Engine::~Engine() {
    shutdown();
}
Engine* Engine::current() {
    return g_engine;
}

bool Engine::init(const EngineConfig& config) {
    config_ = config;
    g_engine = this;
    start_ns_ = last_ns_ = SDL_GetTicksNS();
    devtools::install_log_forwarding();

    SDL_SetAppMetadata(config_.title.c_str(), PHOENIX_ENGINE_VERSION, config_.app_name.c_str());
    if (config_.headless) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    }
#ifdef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, "#phoenix-canvas");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        PHX_LOG_ERROR("Engine: SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    PHX_LOG_INFO("Phoenix engine %s (%s mode), SDL %d.%d.%d, %d logical CPUs",
                 PHOENIX_ENGINE_VERSION, config_.build_mode.c_str(), SDL_MAJOR_VERSION,
                 SDL_MINOR_VERSION, SDL_MICRO_VERSION, SDL_GetNumLogicalCPUCores());

    // 1) Console CPU memory: reserve the arena up front.
    if (!frame_arena_.reserve(config_.profile.cpu_arena_bytes)) {
        PHX_LOG_ERROR("Engine: cannot reserve %llu MiB CPU arena",
                      static_cast<unsigned long long>(config_.profile.cpu_arena_bytes / kMiB));
        return false;
    }
    display_list_.reserve(16384);

    // 2) Console CPU cores: fixed worker pool on SDL threads.
    if (!jobs_.start(config_.profile.worker_threads)) {
        return false;
    }
    PHX_LOG_INFO("Engine: %u worker threads, %llu MiB CPU arena", jobs_.worker_count(),
                 static_cast<unsigned long long>(config_.profile.cpu_arena_bytes / kMiB));

    // 3) Window + GPU (VRAM pool is reserved by the GPU renderer).
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef __EMSCRIPTEN__
    flags |= SDL_WINDOW_FILL_DOCUMENT;
#endif
    if (config_.headless) {
        flags = SDL_WINDOW_HIDDEN;
    }
    window_ = SDL_CreateWindow(config_.title.c_str(), config_.width, config_.height, flags);
    if (!window_) {
        PHX_LOG_ERROR("Engine: SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    if (!config_.headless) {
        renderer_ = create_gpu_renderer(window_, config_.profile);
    }
    if (!renderer_) {
        renderer_ = create_sdl_renderer(window_);
    }
    PHX_LOG_INFO("Engine: renderer %s", renderer_name());

    // 4) Physics, storage, scripting.
    physics_.init(jobs_);
    char* pref = SDL_GetPrefPath("phoenix", config_.app_name.c_str());
    storage_.start(pref ? pref : "");
    SDL_free(pref);

    api_ = PhxEngineApi{PHX_SCRIPT_ABI_VERSION, sizeof(PhxEngineApi), this,       api_log,
                        api_spawn_box,          api_apply_impulse,    api_bodies, api_draw_rects};
    set_engine_api(&api_);
    if (!config_.script_module.empty()) {
        scripts_.load(config_.script_module, &api_);
    }
    devtools::announce(config_.app_name.c_str(), renderer_name(), config_.profile);
    PHX_LOG_INFO("Phoenix engine ready");
    return true;
}

bool Engine::handle_event(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
            quit_ = true;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            pointer_x_ = event.motion.x;
            pointer_y_ = event.motion.y;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            pointer_x_ = event.button.x;
            pointer_y_ = event.button.y;
            pointer_down_ = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            pointer_x_ = event.button.x;
            pointer_y_ = event.button.y;
            if (pointer_down_)
                tapped_ = true;
            pointer_down_ = false;
            break;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP: {
            int w = 0, h = 0;
            SDL_GetWindowSize(window_, &w, &h);
            pointer_x_ = event.tfinger.x * w;
            pointer_y_ = event.tfinger.y * h;
            const bool down = event.type == SDL_EVENT_FINGER_DOWN;
            if (!down && pointer_down_)
                tapped_ = true;
            pointer_down_ = down;
            break;
        }
        default:
            break;
    }
    return !quit_;
}

PhxDrawRect* Engine::reserve_rects(std::uint32_t count) {
    auto* rects = static_cast<PhxDrawRect*>(
        frame_arena_.allocate(sizeof(PhxDrawRect) * count, alignof(PhxDrawRect)));
    if (rects) {
        script_rects_.push_back({rects, count});
    }
    return rects;
}

bool Engine::iterate() {
    const std::uint64_t now_ns = SDL_GetTicksNS();
    const float dt = std::min(static_cast<float>(now_ns - last_ns_) / 1e9f, 1.0f / 15.0f);
    last_ns_ = now_ns;
    const double now = static_cast<double>(now_ns - start_ns_) / 1e9;

    storage_.pump();

    // Physics runs one frame ahead on workers; never block the main thread.
    physics_.poll_step();
    physics_.begin_step(dt);

    int w = config_.width, h = config_.height;
    SDL_GetWindowSize(window_, &w, &h);
    PhxFrameInfo& frame = mutable_frame_info();
    frame = PhxFrameInfo{now,
                         dt,
                         static_cast<std::uint32_t>(stats_.frame),
                         static_cast<float>(w),
                         static_cast<float>(h),
                         pointer_x_,
                         pointer_y_,
                         pointer_down_ ? 1u : 0u,
                         physics_.body_count(),
                         0};

    scripts_.update(frame);

    display_list_.clear();
    WidgetsBinding::instance().drawFrame({static_cast<float>(w), static_cast<float>(h)},
                                         {pointer_x_, pointer_y_}, pointer_down_, tapped_,
                                         display_list_);
    tapped_ = false;
    for (const auto& sr : script_rects_) {
        display_list_.insert(display_list_.end(), sr.data, sr.data + sr.count);
    }
    script_rects_.clear();

    if (renderer_) {
        renderer_->render(display_list_, static_cast<float>(w), static_cast<float>(h), 0x101018FFu);
    }

    stats_.arena_high_water = frame_arena_.high_water();
    stats_.arena_capacity = frame_arena_.capacity();
    frame_arena_.reset();

    ++stats_.frame;
    ++frames_since_publish_;
    if (now - last_publish_ >= 0.25) {
        publish_stats(now);
    }
    if (config_.max_frames && stats_.frame >= config_.max_frames) {
        quit_ = true;
    }
    return !quit_;
}

void Engine::publish_stats(double now) {
    const double elapsed = now - last_publish_;
    stats_.fps = elapsed > 0 ? frames_since_publish_ / elapsed : 0;
    stats_.frame_ms = stats_.fps > 0 ? 1000.0 / stats_.fps : 0;
    stats_.bodies = physics_.body_count();
    stats_.physics_steps = physics_.steps_completed();
    stats_.workers = jobs_.worker_count();
    stats_.jobs_executed = jobs_.jobs_executed();
    if (renderer_)
        stats_.renderer = renderer_->stats();
    last_publish_ = now;
    frames_since_publish_ = 0;
    devtools::publish(stats_, renderer_name(), storage_.backend());
}

void Engine::shutdown() {
    if (g_engine != this) {
        return;
    }
    PHX_LOG_INFO("Engine: shutting down after %llu frames",
                 static_cast<unsigned long long>(stats_.frame));
    scripts_.unload();
    set_engine_api(nullptr);
    storage_.stop();
    physics_.shutdown();
    jobs_.stop();
    renderer_.reset();
    if (window_)
        SDL_DestroyWindow(window_);
    window_ = nullptr;
    frame_arena_.release();
    g_engine = nullptr;
    SDL_Quit();
}

// --- phoenix/storage.h --------------------------------------------------------
namespace storage {

void write(std::string key, std::string value) {
    if (Engine* e = Engine::current())
        e->storage().write(std::move(key), std::move(value));
}

void read(std::string key, std::function<void(std::optional<std::string>)> on_done) {
    if (Engine* e = Engine::current()) {
        e->storage().read(std::move(key), std::move(on_done));
    } else if (on_done) {
        on_done(std::nullopt);
    }
}

const char* backend_name() {
    Engine* e = Engine::current();
    return e ? e->storage().backend() : "memory";
}

}  // namespace storage

}  // namespace phoenix
