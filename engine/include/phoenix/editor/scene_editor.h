// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// The in-game scene editor: Godot's editor docks, drawn by the running game itself.
//
// Press F1 in any Phoenix app (debug or release) to open it over the game:
//
//   +-----------------------------------------------------------------------------+
//   | Scene  Run  Editor                                 [Pause] [Step]  60 fps   |
//   +-------------+-----------------------------------------------+---------------+
//   | Scene       |                                               | Inspector     |
//   |  v World    |              the game keeps rendering         |  Name [Cube ] |
//   |    Camera   |              here (input reaches it)          |  > Transform  |
//   |    Cube     |                                               |  > MeshRender |
//   |             +-----------------------------------------------+  [Add compo.] |
//   |             | Output | FileSystem                           |               |
//   +-------------+-----------------------------------------------+---------------+
//
// The editor is a widget tree like any other (phoenix/tool_widgets.h) installed as the
// binding's overlay root, so it costs nothing while closed. Everything it edits goes
// through the scene API and component reflection: any component with reflected fields,
// including your own, is editable without extra code.
//
// To embed it yourself (the engine already does this):
//
//   auto editor = std::make_shared<phoenix::editor::SceneEditor>();
//   editor->setScene(&scene);
//   phoenix::WidgetsBinding::instance().setOverlayRoot(editor);
//   ...each frame: if (editor->shouldUpdateScene()) scene.Update(dt);
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "phoenix/scene/scene.h"
#include "phoenix/tool_widgets.h"

namespace phoenix::editor {

/// Recent log lines, captured from SDL's log (the engine's single log sink).
class LogBuffer {
  public:
    enum class Level : std::uint8_t { info, warning, error };
    struct Line {
        Level level;
        std::string text;
    };

    static LogBuffer& instance();
    /// Routes SDL's log output through the buffer as well as the previous sink. Idempotent.
    void install();
    void add(Level level, std::string text);
    void clear();
    std::vector<Line> snapshot() const;
    /// Increases every time a line is added (to auto-scroll the Output dock).
    std::uint64_t revision() const;

    static constexpr std::size_t kCapacity = 1000;

  private:
    mutable std::mutex mutex_;
    std::vector<Line> lines_;  // ring buffer once full
    std::size_t head_ = 0;
    std::uint64_t revision_ = 0;
    bool installed_ = false;
};

/// What the editor may ask of its host (the engine). All optional.
struct EditorHooks {
    /// Loads a scene file and makes it the running scene (FileSystem dock).
    std::function<bool(const std::string& path)> openScene{};
    /// True once res:// is mounted, so the FileSystem dock can list it.
    std::function<bool()> filesReady{};
    /// Short status for the toolbar, e.g. "60 fps".
    std::function<std::string()> status{};
    /// Closes the editor (Editor > Close).
    std::function<void()> close{};
};

class SceneEditor : public Widget {
  public:
    explicit SceneEditor(EditorHooks hooks = {});
    ~SceneEditor() override;

    /// The scene being edited (nullptr: none). Clears the selection if the scene changes.
    void setScene(scene::Scene* scene);
    scene::Scene* scene() const { return scene_; }

    // --- Play state -----------------------------------------------------------------------
    bool playing() const { return playing_; }
    void setPlaying(bool playing) { playing_ = playing; }
    /// Advances one frame on the next shouldUpdateScene() while paused.
    void step() { step_requested_ = true; }
    /// The host calls this once per frame: true if the game should advance this frame.
    bool shouldUpdateScene();

    // --- Selection and edits (also used by the menus and tests) ---------------------------
    scene::GameObject selection() const;
    void select(scene::GameObject go);
    /// Adds an empty GameObject, as a child of the selection if there is one, and selects it.
    scene::GameObject addGameObject(const std::string& name = "GameObject");
    /// Adds a lit cube (MeshFilter + MeshRenderer), like Unity's GameObject > 3D Object > Cube.
    scene::GameObject addCube();
    scene::GameObject duplicateSelection();
    void deleteSelection();

    // --- Widget ---------------------------------------------------------------------------
    Size layout(const BoxConstraints& constraints) override;
    void paint(PaintContext& context, Offset origin) override;

  private:
    WidgetPtr build();
    WidgetPtr buildMenuBar();
    WidgetPtr buildSceneDock();
    WidgetPtr buildInspector();
    WidgetPtr buildBottomDock();
    WidgetPtr buildOutput();
    WidgetPtr buildFileSystem();
    WidgetPtr buildViewport();
    WidgetPtr buildTransform(scene::GameObject go);
    WidgetPtr buildComponent(scene::GameObject go, std::uint32_t component_id);
    WidgetPtr buildField(scene::GameObject go, std::uint32_t component_id, std::size_t field);
    WidgetPtr buildAddComponent(scene::GameObject go);
    WidgetPtr vectorRow(const std::string& key, const std::vector<double>& values,
                        const std::vector<std::string>& labels, double step,
                        std::function<void(std::size_t axis, double value)> on_changed);

    std::shared_ptr<TextEditingController> controller(const std::string& key);
    std::shared_ptr<PopupState> popup(const std::string& key);
    void refreshFiles();

    EditorHooks hooks_;
    scene::Scene* scene_ = nullptr;
    WidgetPtr tree_;
    bool playing_ = true;
    bool step_requested_ = false;

    std::shared_ptr<PopupState> menu_ = std::make_shared<PopupState>();
    std::shared_ptr<SplitController> left_split_ =
        std::make_shared<SplitController>(SplitController{.size = 250});
    std::shared_ptr<SplitController> right_split_ =
        std::make_shared<SplitController>(SplitController{.size = 360, .fromEnd = true});
    std::shared_ptr<SplitController> bottom_split_ =
        std::make_shared<SplitController>(SplitController{.size = 190, .fromEnd = true});
    std::shared_ptr<TreeState> hierarchy_ = std::make_shared<TreeState>();
    std::shared_ptr<ScrollController> hierarchy_scroll_ = std::make_shared<ScrollController>();
    std::shared_ptr<ScrollController> inspector_scroll_ = std::make_shared<ScrollController>();
    std::shared_ptr<ScrollController> output_scroll_ = std::make_shared<ScrollController>();
    std::shared_ptr<ScrollController> files_scroll_ = std::make_shared<ScrollController>();
    std::shared_ptr<int> bottom_tab_ = std::make_shared<int>(0);
    std::shared_ptr<TreeState> files_state_ = std::make_shared<TreeState>();
    std::shared_ptr<TextEditingController> component_search_ =
        std::make_shared<TextEditingController>();

    std::uint64_t selected_ = 0;  ///< Entity::bits() + 1, 0 = none
    std::uint64_t log_seen_ = 0;
    std::map<std::string, bool> folded_;  ///< inspector sections the user folded
    std::unordered_map<std::string, std::shared_ptr<TextEditingController>> fields_;
    std::unordered_map<std::string, std::shared_ptr<PopupState>> popups_;
    std::vector<TreeItem> files_;
    std::unordered_map<std::uint64_t, std::string> file_paths_;
    bool files_loaded_ = false;
};

}  // namespace phoenix::editor
