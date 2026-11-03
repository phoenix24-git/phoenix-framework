// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/editor/scene_editor.h"

#include <SDL3/SDL_log.h>

#include <algorithm>
#include <cstdio>
#include <typeinfo>

#if defined(__GNUG__)
#include <cstdlib>

#include <cxxabi.h>
#endif

#include "core/variant/variant.h"
#include "phoenix/vfs/vfs.h"

namespace phoenix::editor {

using scene::GameObject;
using scene::Transform;

namespace {

std::string to_std(const ::String& s) {
    return s.utf8().get_data();
}

::String to_string(const std::string& s) {
    return ::String::utf8(s.c_str());
}

std::uint64_t object_id(GameObject go) {
    return go.entity().bits() + 1;
}

ecs::Entity entity_from_id(std::uint64_t id) {
    const std::uint64_t bits = id - 1;
    return ecs::Entity{static_cast<std::uint32_t>(bits & 0xFFFFFFFFu),
                       static_cast<std::uint32_t>(bits >> 32)};
}

std::string demangle(const std::type_info& type) {
#if defined(__GNUG__)
    int status = 0;
    char* name = abi::__cxa_demangle(type.name(), nullptr, nullptr, &status);
    if (status == 0 && name) {
        std::string out = name;
        std::free(name);
        return out;
    }
#endif
    return type.name();
}

/// Components every GameObject has, shown in the inspector header and Transform section.
bool is_core_component(ecs::ComponentId id) {
    using namespace scene;
    return id == ecs::component_id<Name>() || id == ecs::component_id<Tag>() ||
           id == ecs::component_id<Layer>() || id == ecs::component_id<Inactive>() ||
           id == ecs::component_id<LocalTransform>() || id == ecs::component_id<LocalToWorld>() ||
           id == ecs::component_id<Parent>() || id == ecs::component_id<Children>() ||
           id == ecs::component_id<ScriptHost>();
}

void build_hierarchy_item(GameObject go, TreeItem& out) {
    out.id = object_id(go);
    out.label = to_std(go.name());
    out.color = go.activeInHierarchy() ? ToolTheme::text : ToolTheme::textDim;
    const Transform t = go.transform();
    for (int i = 0; i < t.childCount(); ++i) {
        out.children.emplace_back();
        build_hierarchy_item(t.GetChild(i).gameObject(), out.children.back());
    }
}

/// The game view: transparent (the game shows through and gets the input), with a badge.
class ViewportWidget final : public Widget {
  public:
    explicit ViewportWidget(bool playing) : playing_(playing) {}
    Size layout(const BoxConstraints& c) override {
        size_ = c.constrain({c.maxWidth, c.maxHeight});
        return size_;
    }
    void paint(PaintContext& ctx, Offset o) override {
        ctx.drawBorder({o.x, o.y, size_.width, size_.height}, ToolTheme::border, 2);
        if (!playing_) {
            const std::string label = "PAUSED";
            const Size t = measureText(label, ToolTheme::fontSize);
            ctx.drawRect({o.x + 8, o.y + 8, t.width + 16, t.height + 10}, Color{0x000000A0u});
            drawText(ctx, {o.x + 16, o.y + 13}, label,
                     {.fontSize = ToolTheme::fontSize, .color = Color{0xF0C040FFu}});
            ctx.addSemantics(label, {o.x + 8, o.y + 8, t.width + 16, t.height + 10});
        }
    }

  private:
    bool playing_;
};

}  // namespace

// ---------------------------------------------------------------------------
// LogBuffer
// ---------------------------------------------------------------------------
namespace {
SDL_LogOutputFunction g_previous_log = nullptr;
void* g_previous_log_data = nullptr;

void SDLCALL capture_log(void*, int category, SDL_LogPriority priority, const char* message) {
    LogBuffer::Level level = LogBuffer::Level::info;
    if (priority >= SDL_LOG_PRIORITY_ERROR)
        level = LogBuffer::Level::error;
    else if (priority == SDL_LOG_PRIORITY_WARN)
        level = LogBuffer::Level::warning;
    LogBuffer::instance().add(level, message ? message : "");
    if (g_previous_log)
        g_previous_log(g_previous_log_data, category, priority, message);
}
}  // namespace

LogBuffer& LogBuffer::instance() {
    static LogBuffer buffer;
    return buffer;
}

void LogBuffer::install() {
    std::lock_guard lock(mutex_);
    if (installed_)
        return;
    installed_ = true;
    SDL_GetLogOutputFunction(&g_previous_log, &g_previous_log_data);
    SDL_SetLogOutputFunction(capture_log, nullptr);
}

void LogBuffer::add(Level level, std::string text) {
    std::lock_guard lock(mutex_);
    if (lines_.size() < kCapacity) {
        lines_.push_back({level, std::move(text)});
    } else {
        lines_[head_] = {level, std::move(text)};
        head_ = (head_ + 1) % kCapacity;
    }
    ++revision_;
}

void LogBuffer::clear() {
    std::lock_guard lock(mutex_);
    lines_.clear();
    head_ = 0;
    ++revision_;
}

std::vector<LogBuffer::Line> LogBuffer::snapshot() const {
    std::lock_guard lock(mutex_);
    std::vector<Line> out;
    out.reserve(lines_.size());
    for (std::size_t i = 0; i < lines_.size(); ++i)
        out.push_back(lines_[(head_ + i) % lines_.size()]);
    return out;
}

std::uint64_t LogBuffer::revision() const {
    std::lock_guard lock(mutex_);
    return revision_;
}

// ---------------------------------------------------------------------------
// SceneEditor: state and commands
// ---------------------------------------------------------------------------
SceneEditor::SceneEditor(EditorHooks hooks) : hooks_(std::move(hooks)) {
    LogBuffer::instance().install();
}

SceneEditor::~SceneEditor() = default;

void SceneEditor::setScene(scene::Scene* s) {
    if (s == scene_)
        return;
    scene_ = s;
    selected_ = 0;
    hierarchy_->selected = 0;
    fields_.clear();
    popups_.clear();
}

bool SceneEditor::shouldUpdateScene() {
    if (playing_)
        return true;
    const bool step = step_requested_;
    step_requested_ = false;
    return step;
}

GameObject SceneEditor::selection() const {
    if (!scene_ || selected_ == 0)
        return {};
    const GameObject go(scene_, entity_from_id(selected_));
    return go.valid() ? go : GameObject();
}

void SceneEditor::select(GameObject go) {
    const std::uint64_t id = go.valid() ? object_id(go) : 0;
    if (id != selected_) {
        fields_.clear();  // per-object editing state
        popups_.clear();
    }
    selected_ = id;
    hierarchy_->selected = id;
    // Unfold the parents so the selection is visible in the tree.
    if (go.valid()) {
        for (Transform t = go.transform().parent(); t.valid(); t = t.parent())
            hierarchy_->collapsed.erase(object_id(t.gameObject()));
    }
}

GameObject SceneEditor::addGameObject(const std::string& name) {
    if (!scene_)
        return {};
    GameObject go = scene_->CreateGameObject(to_string(name));
    if (const GameObject parent = selection(); parent.valid())
        go.transform().SetParent(parent.transform(), false);
    select(go);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Editor: added %s", name.c_str());
    return go;
}

GameObject SceneEditor::addCube() {
    GameObject go = addGameObject("Cube");
    if (go.valid()) {
        go.AddComponent<scene::MeshFilter>();
        go.AddComponent<scene::MeshRenderer>();
    }
    return go;
}

GameObject SceneEditor::duplicateSelection() {
    const GameObject original = selection();
    if (!original.valid())
        return {};
    GameObject copy = scene_->Instantiate(original);
    if (const Transform parent = original.transform().parent(); parent.valid())
        copy.transform().SetParent(parent, false);
    select(copy);
    return copy;
}

void SceneEditor::deleteSelection() {
    const GameObject go = selection();
    if (!go.valid())
        return;
    const Transform parent = go.transform().parent();
    const std::string name = to_std(go.name());
    scene_->DestroyImmediate(go);
    select(parent.valid() ? parent.gameObject() : GameObject());
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Editor: deleted %s", name.c_str());
}

std::shared_ptr<TextEditingController> SceneEditor::controller(const std::string& key) {
    auto& c = fields_[key];
    if (!c)
        c = std::make_shared<TextEditingController>();
    return c;
}

std::shared_ptr<PopupState> SceneEditor::popup(const std::string& key) {
    auto& p = popups_[key];
    if (!p)
        p = std::make_shared<PopupState>();
    return p;
}

// ---------------------------------------------------------------------------
// SceneEditor: widget
// ---------------------------------------------------------------------------
Size SceneEditor::layout(const BoxConstraints& c) {
    // The editor mirrors live data (the game keeps running), so it rebuilds every frame.
    tree_ = build();
    size_ = tree_->layout(c);
    return size_;
}

void SceneEditor::paint(PaintContext& ctx, Offset o) {
    tree_->paint(ctx, o);
    // Editor shortcuts: only reach here when no text field consumed the key.
    ctx.addKeyHandler(nullptr, [this](const KeyEvent& k) {
        if (k.isShortcut('d')) {
            duplicateSelection();
            return true;
        }
        if (k.isShortcut('n')) {
            addGameObject();
            return true;
        }
        switch (k.key) {
            case Key::del:
                deleteSelection();
                return true;
            case Key::f5:
                playing_ = !playing_;
                return true;
            case Key::f6:
                playing_ = false;
                step();
                return true;
            default:
                return false;
        }
    });
}

WidgetPtr SceneEditor::build() {
    return Column({
        .crossAxisAlignment = CrossAxisAlignment::stretch,
        .children =
            {
                buildMenuBar(),
                Expanded({.child = SplitView({
                              .controller = left_split_,
                              .first = buildSceneDock(),
                              .second = SplitView({
                                  .controller = right_split_,
                                  .first = SplitView({
                                      .vertical = true,
                                      .controller = bottom_split_,
                                      .first = buildViewport(),
                                      .second = buildBottomDock(),
                                  }),
                                  .second = buildInspector(),
                              }),
                          })}),
            },
    });
}

WidgetPtr SceneEditor::buildMenuBar() {
    const bool has_scene = scene_ != nullptr;
    const bool has_selection = selection().valid();
    std::vector<WidgetPtr> toolbar{
        ToolButton({.label = playing_ ? "Pause" : "Play",
                    .onPressed = [this] { playing_ = !playing_; },
                    .selected = !playing_,
                    .tooltip = "Play/Pause (F5)"}),
        ToolButton({.label = "Step",
                    .onPressed =
                        [this] {
                            playing_ = false;
                            step();
                        },
                    .tooltip = "Advance one frame (F6)"}),
    };
    if (hooks_.status)
        toolbar.push_back(Padding({.padding = EdgeInsets::symmetric(8, 0),
                                   .child = Label(hooks_.status(), ToolTheme::textDim)}));

    std::vector<MenuItem> scene_items{
        MenuItem{.label = "Add Empty",
                 .shortcut = "Ctrl+N",
                 .onSelected = [this] { addGameObject(); },
                 .enabled = has_scene},
        MenuItem{.label = "Add Cube", .onSelected = [this] { addCube(); }, .enabled = has_scene},
        MenuItem{.label = "Duplicate",
                 .shortcut = "Ctrl+D",
                 .onSelected = [this] { duplicateSelection(); },
                 .enabled = has_selection},
        MenuItem{.label = "Delete",
                 .shortcut = "Del",
                 .onSelected = [this] { deleteSelection(); },
                 .enabled = has_selection},
    };
    return MenuBar({
        .menus =
            {
                Menu{.title = "Scene", .items = std::move(scene_items)},
                Menu{.title = "Run",
                     .items =
                         {
                             MenuItem{.label = playing_ ? "Pause" : "Play",
                                      .shortcut = "F5",
                                      .onSelected = [this] { playing_ = !playing_; }},
                             MenuItem{.label = "Step Frame",
                                      .shortcut = "F6",
                                      .onSelected =
                                          [this] {
                                              playing_ = false;
                                              step();
                                          }},
                         }},
                Menu{.title = "Editor",
                     .items =
                         {
                             MenuItem{.label = "Clear Output",
                                      .onSelected = [] { LogBuffer::instance().clear(); }},
                             MenuItem{.label = "Close Editor",
                                      .shortcut = "F1",
                                      .onSelected = hooks_.close,
                                      .enabled = static_cast<bool>(hooks_.close)},
                         }},
            },
        .state = menu_,
        .trailing = std::move(toolbar),
    });
}

WidgetPtr SceneEditor::buildSceneDock() {
    WidgetPtr body;
    if (!scene_) {
        body = Padding({.padding = EdgeInsets::all(10),
                        .child = Label("No scene is running.", ToolTheme::textDim)});
    } else {
        // Roots in creation order (entity index), then each subtree in sibling order.
        std::vector<GameObject> roots;
        scene_->world().query<const scene::LocalTransform>().without<scene::Parent>().each(
            [&](ecs::Entity e, const scene::LocalTransform&) { roots.emplace_back(scene_, e); });
        std::sort(roots.begin(), roots.end(), [](const GameObject& a, const GameObject& b) {
            return a.entity().index < b.entity().index;
        });
        std::vector<TreeItem> items(roots.size());
        for (std::size_t i = 0; i < roots.size(); ++i)
            build_hierarchy_item(roots[i], items[i]);
        if (!selection().valid())
            hierarchy_->selected = 0;
        body = ScrollView({
            .controller = hierarchy_scroll_,
            .child = TreeView({
                .roots = std::move(items),
                .state = hierarchy_,
                .onSelected =
                    [this](std::uint64_t id) { select(GameObject(scene_, entity_from_id(id))); },
            }),
        });
    }
    return Panel({
        .title = "Scene",
        .actions =
            {
                ToolButton({.label = "+",
                            .onPressed =
                                scene_ ? VoidCallback([this] { addGameObject(); }) : VoidCallback(),
                            .tooltip = "Add GameObject"}),
            },
        .child = body,
    });
}

WidgetPtr SceneEditor::buildViewport() {
    return std::make_shared<ViewportWidget>(playing_);
}

WidgetPtr SceneEditor::buildBottomDock() {
    return InputBlocker({.child = Container({
                             .color = ToolTheme::panel,
                             .child = TabView({
                                 .tabs = {Tab{.title = "Output", .child = buildOutput()},
                                          Tab{.title = "FileSystem", .child = buildFileSystem()}},
                                 .index = bottom_tab_,
                             }),
                         })});
}

WidgetPtr SceneEditor::buildOutput() {
    const auto lines = LogBuffer::instance().snapshot();
    // Follow new output while the view is scrolled to the bottom.
    const std::uint64_t revision = LogBuffer::instance().revision();
    if (revision != log_seen_) {
        const bool at_bottom = output_scroll_->offset >= output_scroll_->maxOffset() - 4;
        if (at_bottom)
            output_scroll_->offset = 1e9f;  // clamped at layout
        log_seen_ = revision;
    }
    std::vector<WidgetPtr> rows;
    rows.reserve(lines.size());
    for (const auto& line : lines) {
        Color color = ToolTheme::text;
        if (line.level == LogBuffer::Level::warning)
            color = Color{0xE8C060FFu};
        else if (line.level == LogBuffer::Level::error)
            color = ToolTheme::danger;
        rows.push_back(Container({.height = 18, .child = Label(line.text, color)}));
    }
    if (rows.empty())
        rows.push_back(Label("No output yet.", ToolTheme::textDim));
    return ScrollView({
        .controller = output_scroll_,
        .child = Padding({.padding = EdgeInsets{8, 4, 8, 4},
                          .child = Column({.crossAxisAlignment = CrossAxisAlignment::start,
                                           .children = std::move(rows)})}),
    });
}

void SceneEditor::refreshFiles() {
    files_.clear();
    file_paths_.clear();
    std::uint64_t next_id = 1;
    // Depth-limited walk of res:// (folders first, then files, both sorted).
    std::function<void(const ::String&, std::vector<TreeItem>&, int)> walk =
        [&](const ::String& dir, std::vector<TreeItem>& out, int depth) {
            if (depth > 8)
                return;
            const ::String base = dir.ends_with("/") ? dir : dir + "/";
            for (const ::String& name : vfs::list_dirs(dir)) {
                TreeItem item{
                    .id = next_id++, .label = to_std(name) + "/", .color = ToolTheme::textDim};
                walk(base + name, item.children, depth + 1);
                out.push_back(std::move(item));
            }
            for (const ::String& name : vfs::list_files(dir)) {
                const std::uint64_t id = next_id++;
                const bool is_scene = name.ends_with(".pscn");
                out.push_back(TreeItem{.id = id,
                                       .label = to_std(name),
                                       .color = is_scene ? ToolTheme::accent : ToolTheme::text});
                file_paths_[id] = to_std(base + name);
            }
        };
    walk("res://", files_, 0);
    files_loaded_ = true;
}

WidgetPtr SceneEditor::buildFileSystem() {
    const bool ready = hooks_.filesReady && hooks_.filesReady();
    if (!ready) {
        return Padding({.padding = EdgeInsets::all(10),
                        .child = Label("res:// is not mounted (no scene has been loaded).",
                                       ToolTheme::textDim)});
    }
    if (!files_loaded_)
        refreshFiles();
    return Column({
        .crossAxisAlignment = CrossAxisAlignment::stretch,
        .children =
            {
                Row({.children = {Padding({.padding = EdgeInsets::symmetric(8, 2),
                                           .child = Label("res://  (click a .pscn to open it)",
                                                          ToolTheme::textDim)}),
                                  Expanded({.child = SizedBox({})}),
                                  ToolButton({.label = "Refresh",
                                              .onPressed = [this] { refreshFiles(); }})}}),
                Expanded({.child = ScrollView({
                              .controller = files_scroll_,
                              .child = TreeView({
                                  .roots = files_,
                                  .state = files_state_,
                                  .onSelected =
                                      [this](std::uint64_t id) {
                                          auto it = file_paths_.find(id);
                                          if (it == file_paths_.end() || !hooks_.openScene)
                                              return;
                                          if (it->second.ends_with(".pscn"))
                                              hooks_.openScene(it->second);
                                      },
                              }),
                          })}),
            },
    });
}

// ---------------------------------------------------------------------------
// Inspector
// ---------------------------------------------------------------------------
WidgetPtr SceneEditor::vectorRow(const std::string& key, const std::vector<double>& values,
                                 const std::vector<std::string>& labels, double step,
                                 std::function<void(std::size_t, double)> on_changed) {
    static const Color axis_colors[] = {ToolTheme::axisX, ToolTheme::axisY, ToolTheme::axisZ,
                                        ToolTheme::textDim};
    std::vector<WidgetPtr> fields;
    for (std::size_t i = 0; i < values.size(); ++i) {
        fields.push_back(Expanded({.child = NumberField({
                                       .value = values[i],
                                       .onChanged = [on_changed, i](double v) { on_changed(i, v); },
                                       .controller = controller(key + "." + std::to_string(i)),
                                       .step = step,
                                       .label = labels[i],
                                       .labelColor = axis_colors[std::min<std::size_t>(i, 3)],
                                   })}));
    }
    return Row({.spacing = 4, .children = std::move(fields)});
}

WidgetPtr SceneEditor::buildTransform(GameObject go) {
    Transform t = go.transform();
    const std::string key = std::to_string(object_id(go)) + ".transform";
    const ::Vector3 p = t.localPosition();
    const ::Vector3 r = t.localEulerAngles();
    const ::Vector3 s = t.localScale();
    const std::vector<std::string> xyz{"x", "y", "z"};
    auto set_axis = [](::Vector3 v, std::size_t axis, double value) {
        v[static_cast<int>(axis)] = static_cast<float>(value);
        return v;
    };
    const bool open = !folded_["Transform"];
    return Section({
        .title = "Transform",
        .expanded = open,
        .onToggle = [this] { folded_["Transform"] = !folded_["Transform"]; },
        .child = !open ? nullptr
                       : Column({
                             .crossAxisAlignment = CrossAxisAlignment::stretch,
                             .children =
                                 {
                                     PropertyRow({.label = "Position",
                                                  .editor = vectorRow(
                                                      key + ".p", {p.x, p.y, p.z}, xyz, 0.02,
                                                      [t, set_axis](std::size_t a, double v) mutable {
                                                          t.SetLocalPosition(
                                                              set_axis(t.localPosition(), a, v));
                                                      })}),
                                     PropertyRow({.label = "Rotation",
                                                  .editor = vectorRow(
                                                      key + ".r", {r.x, r.y, r.z}, xyz, 0.5,
                                                      [t, set_axis](std::size_t a, double v) mutable {
                                                          t.SetLocalEulerAngles(set_axis(
                                                              t.localEulerAngles(), a, v));
                                                      })}),
                                     PropertyRow({.label = "Scale",
                                                  .editor = vectorRow(
                                                      key + ".s", {s.x, s.y, s.z}, xyz, 0.01,
                                                      [t, set_axis](std::size_t a, double v) mutable {
                                                          t.SetLocalScale(
                                                              set_axis(t.localScale(), a, v));
                                                      })}),
                                 },
                         }),
    });
}

WidgetPtr SceneEditor::buildField(GameObject go, std::uint32_t id, std::size_t index) {
    const auto& info = ecs::ComponentRegistry::instance().info(id);
    const ecs::FieldInfo& field = info.fields[static_cast<std::uint32_t>(index)];
    ecs::World& world = scene_->world();
    const ecs::Entity e = go.entity();
    const void* data = world.get_component(e, id, false);
    if (!data)
        return nullptr;
    const ::Variant value = info.get(data, field);
    const std::string key =
        std::to_string(object_id(go)) + "." + to_std(info.name) + "." + field.name;
    // Writes go through reflection, marking the component changed for change-detecting systems.
    auto set = [scene = scene_, e, id, index](const ::Variant& v) {
        if (!GameObject(scene, e).valid())
            return;
        const auto& ci = ecs::ComponentRegistry::instance().info(id);
        if (void* storage = scene->world().get_component(e, id, true))
            ci.set(storage, ci.fields[static_cast<std::uint32_t>(index)], v);
    };

    using ecs::FieldKind;
    switch (field.kind) {
        case FieldKind::Bool:
            return Checkbox({.value = static_cast<bool>(value),
                             .onChanged = [set](bool v) { set(::Variant(v)); }});
        case FieldKind::Int32:
        case FieldKind::UInt32:
        case FieldKind::Int64:
        case FieldKind::UInt64:
        case FieldKind::LayerMask:
            return NumberField(
                {.value = static_cast<double>(static_cast<int64_t>(value)),
                 .onChanged =
                     [set](double v) { set(::Variant(static_cast<int64_t>(std::llround(v)))); },
                 .controller = controller(key),
                 .step = 0.2,
                 .integer = true});
        case FieldKind::Float:
        case FieldKind::Double:
            return NumberField({.value = static_cast<double>(value),
                                .onChanged = [set](double v) { set(::Variant(v)); },
                                .controller = controller(key),
                                .step = 0.02});
        case FieldKind::Vector2: {
            const ::Vector2 v = value;
            return vectorRow(key, {v.x, v.y}, {"x", "y"}, 0.02, [set, v](std::size_t a, double x) {
                ::Vector2 out = v;
                out[static_cast<int>(a)] = static_cast<float>(x);
                set(::Variant(out));
            });
        }
        case FieldKind::Vector3: {
            const ::Vector3 v = value;
            return vectorRow(key, {v.x, v.y, v.z}, {"x", "y", "z"}, 0.02,
                             [set, v](std::size_t a, double x) {
                                 ::Vector3 out = v;
                                 out[static_cast<int>(a)] = static_cast<float>(x);
                                 set(::Variant(out));
                             });
        }
        case FieldKind::Vector4: {
            const ::Vector4 v = value;
            return vectorRow(key, {v.x, v.y, v.z, v.w}, {"x", "y", "z", "w"}, 0.02,
                             [set, v](std::size_t a, double x) {
                                 ::Vector4 out = v;
                                 out[static_cast<int>(a)] = static_cast<float>(x);
                                 set(::Variant(out));
                             });
        }
        case FieldKind::Quaternion: {
            const ::Quaternion q = value;
            return vectorRow(key, {q.x, q.y, q.z, q.w}, {"x", "y", "z", "w"}, 0.01,
                             [set, q](std::size_t a, double x) {
                                 ::Quaternion out = q;
                                 out[static_cast<int>(a)] = static_cast<float>(x);
                                 set(::Variant(out.normalized()));
                             });
        }
        case FieldKind::Color: {
            const ::Color c = value;
            auto clamp01 = [](float f) { return std::clamp(f, 0.0f, 1.0f); };
            const phoenix::Color swatch{(static_cast<std::uint32_t>(clamp01(c.r) * 255) << 24) |
                                        (static_cast<std::uint32_t>(clamp01(c.g) * 255) << 16) |
                                        (static_cast<std::uint32_t>(clamp01(c.b) * 255) << 8) |
                                        0xFFu};
            return Row({
                .spacing = 4,
                .children =
                    {
                        Container({.width = 22, .height = 22, .color = swatch}),
                        Expanded({.child = vectorRow(key, {c.r, c.g, c.b, c.a},
                                                     {"r", "g", "b", "a"}, 0.005,
                                                     [set, c](std::size_t a, double x) {
                                                         ::Color out = c;
                                                         out.components[a] = static_cast<float>(x);
                                                         set(::Variant(out));
                                                     })}),
                    },
            });
        }
        case FieldKind::Enum: {
            std::vector<std::string> names;
            for (int i = 0; field.enum_names && field.enum_names[i]; ++i)
                names.emplace_back(field.enum_names[i]);
            return Dropdown({.items = std::move(names),
                             .selected = static_cast<int>(static_cast<int64_t>(value)),
                             .onChanged = [set](int i) { set(::Variant(static_cast<int64_t>(i))); },
                             .state = popup(key)});
        }
        case FieldKind::String:
        case FieldKind::StringName: {
            auto c = controller(key);
            if (!c->focus.hasFocus())
                c->setText(to_std(static_cast<::String>(value)));
            const bool name_kind = field.kind == FieldKind::StringName;
            return TextField(
                {.controller = c, .onSubmitted = [set, name_kind](const std::string& s) {
                     if (name_kind)
                         set(::Variant(::StringName(to_string(s))));
                     else
                         set(::Variant(to_string(s)));
                 }});
        }
        case FieldKind::Entity: {
            const auto bits = static_cast<std::uint64_t>(static_cast<int64_t>(value));
            const ecs::Entity target{static_cast<std::uint32_t>(bits & 0xFFFFFFFFu),
                                     static_cast<std::uint32_t>(bits >> 32)};
            const GameObject other(scene_, target);
            return Label(other.valid() ? to_std(other.name()) : std::string("None"),
                         ToolTheme::textDim);
        }
        case FieldKind::Asset: {
            const auto uid = static_cast<int64_t>(value);
            return Label(uid ? "uid:" + std::to_string(uid) : std::string("None"),
                         ToolTheme::textDim);
        }
        default:
            return Label(std::string("(") + ecs::field_kind_name(field.kind) + ")",
                         ToolTheme::textDim);
    }
}

WidgetPtr SceneEditor::buildComponent(GameObject go, std::uint32_t id) {
    const auto& info = ecs::ComponentRegistry::instance().info(id);
    const std::string name = to_std(info.name);
    const bool open = !folded_[name];
    std::vector<WidgetPtr> rows;
    if (open) {
        for (std::uint32_t i = 0; i < info.fields.size(); ++i) {
            if (WidgetPtr editor = buildField(go, id, i))
                rows.push_back(PropertyRow({.label = info.fields[i].name, .editor = editor}));
        }
        if (rows.empty())
            rows.push_back(Label("No fields.", ToolTheme::textDim));
        if (info.status == ecs::ComponentStatus::DataOnly)
            rows.push_back(Label("Data only: no system simulates it yet.", ToolTheme::textDim,
                                 ToolTheme::fontSize));
    }
    const ecs::Entity e = go.entity();
    return Section({
        .title = name,
        .expanded = open,
        .onToggle = [this, name] { folded_[name] = !folded_[name]; },
        .actions = {ToolButton({.label = "x",
                                .onPressed =
                                    [this, e, id] {
                                        if (scene_ && GameObject(scene_, e).valid())
                                            scene_->world().remove_component(e, id);
                                    },
                                .tooltip = "Remove " + name,
                                .color = ToolTheme::danger})},
        .child = open ? Column({.crossAxisAlignment = CrossAxisAlignment::stretch,
                                .children = std::move(rows)})
                      : nullptr,
    });
}

WidgetPtr SceneEditor::buildAddComponent(GameObject go) {
    std::vector<WidgetPtr> children{
        TextField({.controller = component_search_, .hint = "Add Component (type to search)"})};
    std::string query = component_search_->text;
    std::transform(query.begin(), query.end(), query.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (!query.empty()) {
        auto& reg = ecs::ComponentRegistry::instance();
        const ecs::Entity e = go.entity();
        int shown = 0;
        for (std::uint32_t id = 0; id < reg.count() && shown < 12; ++id) {
            const auto& info = reg.info(id);
            if (is_core_component(id) || scene_->world().has_component(e, id))
                continue;
            std::string lower = to_std(info.name);
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (lower.find(query) == std::string::npos)
                continue;
            ++shown;
            const std::string label =
                to_std(info.name) +
                (info.category.is_empty() ? std::string() : "  (" + to_std(info.category) + ")");
            children.push_back(ToolButton({.label = label, .onPressed = [this, e, id] {
                                               if (!scene_ || !GameObject(scene_, e).valid())
                                                   return;
                                               scene_->world().add_component(e, id);
                                               component_search_->setText("");
                                               component_search_->focus.unfocus();
                                           }}));
        }
        if (shown == 0)
            children.push_back(Label("No matching component.", ToolTheme::textDim));
    }
    return Padding({.padding = EdgeInsets{8, 10, 8, 10},
                    .child = Column({.crossAxisAlignment = CrossAxisAlignment::stretch,
                                     .spacing = 2,
                                     .children = std::move(children)})});
}

WidgetPtr SceneEditor::buildInspector() {
    const GameObject go = selection();
    WidgetPtr body;
    if (!go.valid()) {
        body = Padding({.padding = EdgeInsets::all(10),
                        .child = Label(scene_ ? "Select a GameObject in the Scene dock."
                                              : "No scene is running.",
                                       ToolTheme::textDim)});
    } else {
        const std::string key = std::to_string(object_id(go));
        auto name = controller(key + ".name");
        if (!name->focus.hasFocus())
            name->setText(to_std(go.name()));
        auto tag = controller(key + ".tag");
        if (!tag->focus.hasFocus())
            tag->setText(to_std(go.tag()));
        GameObject target = go;

        std::vector<WidgetPtr> children{
            Padding({.padding = EdgeInsets{8, 8, 8, 4},
                     .child = Row({
                         .spacing = 6,
                         .children =
                             {
                                 Checkbox({.value = go.activeSelf(),
                                           .onChanged =
                                               [target](bool v) mutable { target.SetActive(v); }}),
                                 Expanded({.child = TextField({
                                               .controller = name,
                                               .hint = "Name",
                                               .onSubmitted =
                                                   [target](const std::string& s) mutable {
                                                       target.SetName(to_string(s));
                                                   },
                                           })}),
                             },
                     })}),
            Padding({.padding = EdgeInsets{8, 0, 8, 6},
                     .child = Column({
                         .crossAxisAlignment = CrossAxisAlignment::stretch,
                         .children =
                             {
                                 PropertyRow({.label = "Tag",
                                              .editor = TextField({
                                                  .controller = tag,
                                                  .onSubmitted =
                                                      [target](const std::string& s) mutable {
                                                          target.SetTag(to_string(s));
                                                      },
                                              })}),
                                 PropertyRow({.label = "Layer",
                                              .editor = NumberField({
                                                  .value = static_cast<double>(go.layer()),
                                                  .onChanged =
                                                      [target](double v) mutable {
                                                          target.SetLayer(std::clamp(
                                                              static_cast<int>(std::lround(v)), 0,
                                                              31));
                                                      },
                                                  .controller = controller(key + ".layer"),
                                                  .step = 0.1,
                                                  .integer = true,
                                              })}),
                             },
                     })}),
            buildTransform(go),
        };

        // Data components in id order (registration order: the catalogue, then yours).
        std::vector<ecs::ComponentId> ids(scene_->world().components_of(go.entity()).begin(),
                                          scene_->world().components_of(go.entity()).end());
        std::sort(ids.begin(), ids.end());
        for (ecs::ComponentId id : ids) {
            if (!is_core_component(id))
                children.push_back(buildComponent(go, id));
        }

        // MetaBehaviours (C++ scripts) are listed; their fields are not reflected.
        if (const auto* host = scene_->world().get<scene::ScriptHost>(go.entity())) {
            std::vector<WidgetPtr> rows;
            for (const auto& behaviour : host->behaviours) {
                rows.push_back(Row({.spacing = 6,
                                    .children = {Label(demangle(typeid(*behaviour))),
                                                 Label(behaviour->enabled() ? "" : "(disabled)",
                                                       ToolTheme::textDim)}}));
            }
            const bool open = !folded_["Scripts"];
            children.push_back(Section({
                .title = "Scripts",
                .expanded = open,
                .onToggle = [this] { folded_["Scripts"] = !folded_["Scripts"]; },
                .child = open ? Column({.crossAxisAlignment = CrossAxisAlignment::start,
                                        .children = std::move(rows)})
                              : nullptr,
            }));
        }
        children.push_back(buildAddComponent(go));
        body = ScrollView({
            .controller = inspector_scroll_,
            .child = Column({.crossAxisAlignment = CrossAxisAlignment::stretch,
                             .children = std::move(children)}),
        });
    }
    return Panel({.title = "Inspector", .child = body});
}

}  // namespace phoenix::editor
