// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "shell/engine.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>

#include "core/log.h"
#include "framework/engine_api.h"
#include "phoenix/storage.h"
#include "phoenix/widgets.h"
#include "shell/devtools_bridge.h"

#if PHOENIX_HAS_SCENE
#include "core/variant/variant_parser.h"
#include "core_runtime.h"
#include "phoenix/editor/scene_editor.h"
#include "phoenix/scene/scene_file.h"
#include "phoenix/scene/scene_renderer.h"
#endif

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

// --- Scene ABI: GameObject handles are (generation + 1) << 32 | index, so 0 means "none" ---
#if PHOENIX_HAS_SCENE
std::uint64_t to_handle(ecs::Entity e) {
    return e.valid() ? ((std::uint64_t(e.generation) + 1) << 32) | e.index : 0;
}
scene::GameObject resolve(void* e, std::uint64_t handle) {
    scene::Scene* sc = static_cast<Engine*>(e)->scene();
    if (!sc || handle == 0) {
        return {};
    }
    const ecs::Entity ent{static_cast<std::uint32_t>(handle),
                          static_cast<std::uint32_t>((handle >> 32) - 1)};
    const scene::GameObject go(sc, ent);
    return go.valid() ? go : scene::GameObject();
}
const ecs::ComponentInfo* component_type(const char* type) {
    String name = String::utf8(type ? type : "");
    if (name == "Transform") {
        name = "LocalTransform";
    }
    const ecs::ComponentInfo* info = ecs::ComponentRegistry::instance().find(name);
    return info && info->size > 0 ? info : nullptr;
}
std::uint64_t api_find_object(void* e, const char* name) {
    scene::Scene* sc = static_cast<Engine*>(e)->scene();
    return sc && name ? to_handle(sc->Find(String::utf8(name)).entity()) : 0;
}
std::uint64_t api_create_object(void* e, const char* name, std::uint64_t parent) {
    scene::Scene* sc = static_cast<Engine*>(e)->scene();
    if (!sc) {
        return 0;
    }
    scene::GameObject go = sc->CreateGameObject(String::utf8(name ? name : "GameObject"));
    if (const scene::GameObject p = resolve(e, parent); p.valid()) {
        go.transform().SetParent(p.transform(), /*world_position_stays=*/false);
    }
    return to_handle(go.entity());
}
void api_destroy_object(void* e, std::uint64_t object) {
    if (resolve(e, object).valid()) {
        static_cast<Engine*>(e)->destroy_later(object);
    }
}
std::uint32_t api_object_alive(void* e, std::uint64_t object) {
    return resolve(e, object).valid() ? 1u : 0u;
}
void api_set_active(void* e, std::uint64_t object, std::uint32_t active) {
    if (scene::GameObject go = resolve(e, object); go.valid()) {
        go.SetActive(active != 0);
    }
}
void* api_get_component(void* e, std::uint64_t object, const char* type, std::uint32_t add) {
    const scene::GameObject go = resolve(e, object);
    const ecs::ComponentInfo* info = component_type(type);
    if (!go.valid() || !info) {
        return nullptr;
    }
    ecs::World& world = go.scene()->world();
    if (!world.has_component(go.entity(), info->id)) {
        if (!add || world.iterating()) {
            return nullptr;
        }
        world.add_component(go.entity(), info->id);
    }
    return world.get_component(go.entity(), info->id, /*mark_changed=*/true);
}
void* api_get_field(void* e, std::uint64_t object, const char* type, const char* field,
                    std::uint32_t* out_size) {
    const ecs::ComponentInfo* info = component_type(type);
    const ecs::FieldInfo* f = info && field ? info->field(field) : nullptr;
    auto* data = f ? static_cast<std::byte*>(api_get_component(e, object, type, 0)) : nullptr;
    if (out_size) {
        *out_size = data ? f->size : 0;
    }
    return data ? data + f->offset : nullptr;
}
#else
std::uint64_t api_find_object(void*, const char*) {
    return 0;
}
std::uint64_t api_create_object(void*, const char*, std::uint64_t) {
    return 0;
}
void api_destroy_object(void*, std::uint64_t) {}
std::uint32_t api_object_alive(void*, std::uint64_t) {
    return 0;
}
void api_set_active(void*, std::uint64_t, std::uint32_t) {}
void* api_get_component(void*, std::uint64_t, const char*, std::uint32_t) {
    return nullptr;
}
void* api_get_field(void*, std::uint64_t, const char*, const char*, std::uint32_t* out_size) {
    if (out_size)
        *out_size = 0;
    return nullptr;
}
#endif

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

    api_ = PhxEngineApi{PHX_SCRIPT_ABI_VERSION,
                        sizeof(PhxEngineApi),
                        this,
                        api_log,
                        api_spawn_box,
                        api_apply_impulse,
                        api_bodies,
                        api_draw_rects,
                        api_find_object,
                        api_create_object,
                        api_destroy_object,
                        api_object_alive,
                        api_set_active,
                        api_get_component,
                        api_get_field};
    set_engine_api(&api_);
    if (!config_.script_module.empty()) {
        scripts_.load(config_.script_module, &api_);
    }
    devtools::announce(config_.app_name.c_str(), renderer_name(), config_.profile);
    PHX_LOG_INFO("Phoenix engine ready");
    return true;
}

namespace {
KeyEvent translate_key(const SDL_KeyboardEvent& k) {
    KeyEvent e;
    e.ctrl = (k.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
    e.shift = (k.mod & SDL_KMOD_SHIFT) != 0;
    e.alt = (k.mod & SDL_KMOD_ALT) != 0;
    const SDL_Keycode code = k.key;
    switch (code) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            e.key = Key::enter;
            return e;
        case SDLK_ESCAPE:
            e.key = Key::escape;
            return e;
        case SDLK_BACKSPACE:
            e.key = Key::backspace;
            return e;
        case SDLK_DELETE:
            e.key = Key::del;
            return e;
        case SDLK_TAB:
            e.key = Key::tab;
            return e;
        case SDLK_LEFT:
            e.key = Key::left;
            return e;
        case SDLK_RIGHT:
            e.key = Key::right;
            return e;
        case SDLK_UP:
            e.key = Key::up;
            return e;
        case SDLK_DOWN:
            e.key = Key::down;
            return e;
        case SDLK_HOME:
            e.key = Key::home;
            return e;
        case SDLK_END:
            e.key = Key::end;
            return e;
        case SDLK_PAGEUP:
            e.key = Key::pageUp;
            return e;
        case SDLK_PAGEDOWN:
            e.key = Key::pageDown;
            return e;
        default:
            break;
    }
    if (code >= SDLK_F1 && code <= SDLK_F12) {
        e.key = static_cast<Key>(static_cast<int>(Key::f1) + static_cast<int>(code - SDLK_F1));
    } else if (code >= 0x20 && code < 0x7F) {
        // SDL keycodes for printable keys are their (lower-case) ASCII characters.
        e.key = Key::character;
        e.ch = static_cast<char>(code);
    }
    return e;
}
}  // namespace

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
            if (event.button.button == SDL_BUTTON_LEFT) {
                pointer_down_ = true;
                input_.pressed = true;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            pointer_x_ = event.button.x;
            pointer_y_ = event.button.y;
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (pointer_down_)
                    input_.released = true;
                pointer_down_ = false;
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            input_.scrollX += event.wheel.x;
            input_.scrollY -= event.wheel.y;  // SDL: +y away from the user; ours: +y scrolls down
            break;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP: {
            int w = 0, h = 0;
            SDL_GetWindowSize(window_, &w, &h);
            pointer_x_ = event.tfinger.x * w;
            pointer_y_ = event.tfinger.y * h;
            const bool down = event.type == SDL_EVENT_FINGER_DOWN;
            if (down)
                input_.pressed = true;
            if (!down && pointer_down_)
                input_.released = true;
            pointer_down_ = down;
            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            const KeyEvent key = translate_key(event.key);
            if (key.key != Key::unknown)
                input_.keys.push_back(key);
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            if (event.text.text)
                input_.text += event.text.text;
            break;
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
    float dt = std::min(static_cast<float>(now_ns - last_ns_) / 1e9f, 1.0f / 15.0f);
    last_ns_ = now_ns;
    double now = static_cast<double>(now_ns - start_ns_) / 1e9;
    if (config_.fixed_dt > 0.0f) {
        dt = config_.fixed_dt;
        now = static_cast<double>(stats_.frame + 1) * config_.fixed_dt;
    }

    storage_.pump();

    // F1 toggles the in-game editor; while it is paused, the game (scene, scripts, physics)
    // stands still but keeps rendering.
    if (config_.open_editor && stats_.frame == 0)
        set_editor_open(true);
    for (auto it = input_.keys.begin(); it != input_.keys.end();) {
        if (it->key == Key::f1 && config_.editor) {
            set_editor_open(!editor_open_);
            it = input_.keys.erase(it);
        } else {
            ++it;
        }
    }
    bool run_game = true;
#if PHOENIX_HAS_SCENE
    if (editor_open_ && editor_) {
        editor_->setScene(scene_.get());
        run_game = editor_->shouldUpdateScene();
    }
#endif

    // Physics runs one frame ahead on workers; never block the main thread.
    physics_.poll_step();
    if (run_game)
        physics_.begin_step(dt);

    int w = config_.width, h = config_.height;
    SDL_GetWindowSize(window_, &w, &h);
    PhxFrameInfo& frame = mutable_frame_info();
    frame = PhxFrameInfo{
        now, dt, static_cast<std::uint32_t>(stats_.frame), static_cast<float>(w),
        static_cast<float>(h), pointer_x_, pointer_y_,
        // Presses on editor docks belong to the editor, not the game.
        pointer_down_ && !(editor_open_ && WidgetsBinding::instance().pointerOverOverlay()) ? 1u
                                                                                            : 0u,
        physics_.body_count(), 0};

    if (run_game)
        scripts_.update(frame);
    flush_destroyed();

    // The 3D scene first, then the widget UI and script draws on top.
    display_list_.clear();
    std::uint32_t clear_rgba = 0x101018FFu;
#if PHOENIX_HAS_SCENE
    if (scene_) {
        if (run_game)
            scene_->Update(dt);
        clear_rgba = scene::render_scene(*scene_, static_cast<float>(w), static_cast<float>(h),
                                         display_list_);
    }
#endif
    input_.pointer = {pointer_x_, pointer_y_};
    input_.pointerDown = pointer_down_;
    WidgetsBinding& binding = WidgetsBinding::instance();
    binding.drawFrame({static_cast<float>(w), static_cast<float>(h)}, input_, display_list_);
    input_ = FrameInput{};
    // Text input (IME, on-screen keyboards) only while a text field has focus.
    if (window_ && binding.wantsTextInput() != text_input_on_) {
        text_input_on_ = binding.wantsTextInput();
        if (text_input_on_)
            SDL_StartTextInput(window_);
        else
            SDL_StopTextInput(window_);
    }
    for (const auto& sr : script_rects_) {
        display_list_.insert(display_list_.end(), sr.data, sr.data + sr.count);
    }
    script_rects_.clear();

    if (renderer_ && !config_.screenshot.empty() && config_.max_frames &&
        stats_.frame + 1 == config_.max_frames) {
        if (!renderer_->capture_next_frame(config_.screenshot.c_str()))
            PHX_LOG_WARN("Engine: renderer %s can't capture frames", renderer_name());
    }
    if (renderer_) {
        renderer_->render(display_list_, static_cast<float>(w), static_cast<float>(h), clear_rgba);
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
    scripts_.unload();  // scripts hold pointers into the scene: unload them first
#if PHOENIX_HAS_SCENE
    scene_.reset();
    if (core_started_) {
        foundation::CoreRuntime::finalize();
        core_started_ = false;
    }
#endif
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

void Engine::flush_destroyed() {
#if PHOENIX_HAS_SCENE
    for (const std::uint64_t handle : pending_destroy_) {
        if (const scene::GameObject go = resolve(this, handle); go.valid()) {
            scene_->Destroy(go);
        }
    }
#endif
    pending_destroy_.clear();
}

bool Engine::start_core() {
#if PHOENIX_HAS_SCENE
    if (core_started_) {
        return true;
    }
    foundation::CoreOptions options;
    options.app_name = String::utf8(config_.app_name.c_str());
    std::string res = config_.res_dir;
    if (res.empty()) {
#ifdef __EMSCRIPTEN__
        res = "/res";
#else
        const char* base = SDL_GetBasePath();
        res = std::string(base ? base : "") + "res";
#endif
    }
    options.resource_dir = String::utf8(res.c_str());
    options.worker_threads = static_cast<int>(std::max(1u, config_.profile.worker_threads));
    core_started_ = foundation::CoreRuntime::initialize(options);
    if (core_started_) {
        PHX_LOG_INFO("Engine: foundation core up, res:// = %s", res.c_str());
    }
    return core_started_;
#else
    return false;
#endif
}

void Engine::set_editor_open(bool open) {
#if PHOENIX_HAS_SCENE
    if (!config_.editor || open == editor_open_)
        return;
    if (open && !editor_) {
        editor::EditorHooks hooks;
        hooks.openScene = [this](const std::string& path) {
            editor_->setScene(nullptr);  // drop selection and edit state of the old scene
            const bool ok = run_scene(path);
            editor_->setScene(scene_.get());
            return ok;
        };
        hooks.filesReady = [this] { return core_started_; };
        hooks.status = [this] {
            char buf[64];
            std::snprintf(buf, sizeof buf, "%.0f fps  %llu objects", stats_.fps,
                          static_cast<unsigned long long>(scene_ ? scene_->gameObjectCount() : 0));
            return std::string(buf);
        };
        hooks.close = [this] { set_editor_open(false); };
        editor_ = std::make_shared<editor::SceneEditor>(std::move(hooks));
    }
    editor_open_ = open;
    if (open) {
        editor_->setScene(scene_.get());
        WidgetsBinding::instance().setOverlayRoot(editor_);
        PHX_LOG_INFO("Editor: open (F1 to close, F5 play/pause, F6 step)");
    } else {
        editor_->setPlaying(true);  // closing the editor resumes the game
        WidgetsBinding::instance().setOverlayRoot(nullptr);
        WidgetsBinding::instance().setFocus(nullptr);
    }
#else
    (void)open;
#endif
}

bool Engine::run_scene(const std::string& path) {
#if PHOENIX_HAS_SCENE
    if (!start_core()) {
        PHX_LOG_ERROR("Scene: the foundation core failed to start");
        return false;
    }
    scene_ = std::make_unique<scene::Scene>();
    const scene::SceneLoadResult result = scene::load_scene(*scene_, String::utf8(path.c_str()));
    if (!result) {
        PHX_LOG_ERROR("Scene: %s", result.message.utf8().get_data());
        scene_.reset();
        return false;
    }
    int attached = 0;
    for (const scene::SceneScript& script : result.scripts) {
        // Properties travel as "key=value" lines in Godot's text syntax.
        String props;
        for (const KeyValue<Variant, Variant>& kv : script.properties) {
            String text;
            VariantWriter::write_to_string(kv.value, text);
            props += String(kv.key) + "=" + text + "\n";
        }
        if (scripts_.attach(to_handle(script.gameObject.entity()),
                            script.class_name.utf8().get_data(), props.utf8().get_data())) {
            ++attached;
        }
    }
    PHX_LOG_INFO("Scene: %s loaded (%u objects, %d of %d scripts attached)", path.c_str(),
                 static_cast<unsigned>(scene_->gameObjectCount()), attached,
                 static_cast<int>(result.scripts.size()));
    return true;
#else
    PHX_LOG_ERROR("Scene: %s needs an engine built with PHOENIX_WITH_FOUNDATION", path.c_str());
    return false;
#endif
}

bool runScene(std::string_view path) {
    Engine* e = Engine::current();
    return e && e->run_scene(std::string(path));
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
