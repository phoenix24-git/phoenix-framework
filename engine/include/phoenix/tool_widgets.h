// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Tool widgets: the controls editors are made of (Godot's Control nodes, Flutter-style).
//
//   auto split = std::make_shared<SplitController>(SplitController{.size = 260});
//   auto name = std::make_shared<TextEditingController>();
//
//   SplitView({
//       .controller = split,
//       .first = Panel({.title = "Scene", .child = ScrollView({.controller = scroll,
//                                                               .child = TreeView({...})})}),
//       .second = Panel({.title = "Inspector", .child = Column({.children = {
//           PropertyRow({.label = "Name", .editor = TextField({.controller = name})}),
//           PropertyRow({.label = "Speed", .editor = NumberField({.value = speed,
//                                                    .onChanged = [&](double v) { speed = v; }})}),
//       }})}),
//   });
//
// State that must survive a rebuild (scroll offsets, caret, split sizes, which menu is
// open) lives in small controller objects held by shared_ptr: keep them in your
// StatefulWidget and pass the same ones every build, as with Flutter's controllers.
#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "phoenix/widgets.h"

namespace phoenix {

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------
/// The dark tool theme (close to Godot's default editor theme).
namespace ToolTheme {
inline constexpr Color background{0x1D2129FFu};
inline constexpr Color panel{0x252A33FFu};
inline constexpr Color header{0x2F3542FFu};
inline constexpr Color field{0x181B21FFu};
inline constexpr Color hover{0x3A4252FFu};
inline constexpr Color pressed{0x465066FFu};
inline constexpr Color accent{0x478CBFFFu};
inline constexpr Color selection{0x2C5679FFu};
inline constexpr Color text{0xDDE1E8FFu};
inline constexpr Color textDim{0x8C95A5FFu};
inline constexpr Color border{0x111318FFu};
inline constexpr Color danger{0xD9534FFFu};
inline constexpr Color axisX{0xE0605AFFu};
inline constexpr Color axisY{0x7BC85CFFu};
inline constexpr Color axisZ{0x5A8FE0FFu};
inline constexpr float fontSize = 14;
inline constexpr float rowHeight = 24;
}  // namespace ToolTheme

// ---------------------------------------------------------------------------
// Controllers (long-lived state)
// ---------------------------------------------------------------------------
/// Text, caret and focus of an editable field.
struct TextEditingController {
    std::string text{};
    std::size_t caret = 0;
    FocusNode focus{};
    float scroll = 0;        ///< horizontal scroll of the text inside the field
    bool cancelled = false;  ///< set by Escape: the edit is discarded on blur

    void setText(std::string value) {
        text = std::move(value);
        caret = text.size();
    }
};

struct ScrollController {
    float offset = 0;
    float viewport = 0;  ///< last laid-out viewport height
    float extent = 0;    ///< last laid-out content height
    float maxOffset() const { return extent > viewport ? extent - viewport : 0.0f; }
    void scrollBy(float dy);
};

/// Size of a SplitView's fixed pane, in logical pixels.
struct SplitController {
    float size = 250;
    bool fromEnd = false;  ///< the fixed pane is the second child (right/bottom)
    float minSize = 60;
};

/// Which entry of a menu bar or dropdown is open.
struct PopupState {
    int open = -1;
};

struct TreeState {
    std::set<std::uint64_t> collapsed{};
    std::uint64_t selected = 0;  ///< 0 = nothing selected
};

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
struct StackProps {
    std::vector<WidgetPtr> children{};
};
/// Children painted on top of each other; Positioned children are placed by their edges,
/// the others at the top-left. Expands to fill bounded constraints.
WidgetPtr Stack(StackProps props);

struct PositionedProps {
    float left = -1, top = -1, right = -1, bottom = -1;  ///< -1 = unset
    float width = -1, height = -1;
    WidgetPtr child{};
};
WidgetPtr Positioned(PositionedProps props);

struct ScrollViewProps {
    std::shared_ptr<ScrollController> controller{};
    WidgetPtr child{};
};
/// Vertical scrolling with the wheel or the scrollbar; clips its child.
WidgetPtr ScrollView(ScrollViewProps props);

struct SplitViewProps {
    bool vertical = false;  ///< false: side by side; true: stacked
    std::shared_ptr<SplitController> controller{};
    WidgetPtr first{};
    WidgetPtr second{};
};
/// Two panes and a draggable divider.
WidgetPtr SplitView(SplitViewProps props);

struct InputBlockerProps {
    WidgetPtr child{};
};
/// Keeps presses on empty parts of `child` from reaching what is painted below.
WidgetPtr InputBlocker(InputBlockerProps props);

// ---------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------
struct ToolButtonProps {
    std::string label{};
    VoidCallback onPressed{};
    bool selected = false;
    std::string tooltip{};
    Color color = ToolTheme::text;
    float fontSize = ToolTheme::fontSize;
};
/// A flat, compact button that highlights on hover.
WidgetPtr ToolButton(ToolButtonProps props);

struct CheckboxProps {
    bool value = false;
    std::function<void(bool)> onChanged{};
    std::string label{};
};
WidgetPtr Checkbox(CheckboxProps props);

struct TextFieldProps {
    std::shared_ptr<TextEditingController> controller{};
    std::string hint{};
    std::function<void(const std::string&)> onChanged{};
    std::function<void(const std::string&)> onSubmitted{};  ///< Enter, or losing focus
    float width = -1;
    float fontSize = ToolTheme::fontSize;
};
/// Single-line text input. Click to focus; Enter submits, Escape cancels.
WidgetPtr TextField(TextFieldProps props);

struct NumberFieldProps {
    double value = 0;
    std::function<void(double)> onChanged{};
    /// Holds the text while typing. Without one the field can only be dragged.
    std::shared_ptr<TextEditingController> controller{};
    double step = 0.1;  ///< change per pixel dragged
    int decimals = 3;
    bool integer = false;
    std::string label{};  ///< short prefix, e.g. "x"
    Color labelColor = ToolTheme::textDim;
};
/// Godot's spin slider: drag sideways to scrub the value, click to type one.
WidgetPtr NumberField(NumberFieldProps props);

struct DropdownProps {
    std::vector<std::string> items{};
    int selected = 0;
    std::function<void(int)> onChanged{};
    std::shared_ptr<PopupState> state{};
};
WidgetPtr Dropdown(DropdownProps props);

struct MenuItem {
    std::string label{};
    std::string shortcut{};  ///< display only, e.g. "Ctrl+D"
    VoidCallback onSelected{};
    bool enabled = true;
};
struct Menu {
    std::string title{};
    std::vector<MenuItem> items{};
};
struct MenuBarProps {
    std::vector<Menu> menus{};
    std::shared_ptr<PopupState> state{};
    std::vector<WidgetPtr> trailing{};  ///< widgets on the right of the bar (toolbar)
};
WidgetPtr MenuBar(MenuBarProps props);

struct TreeItem {
    std::uint64_t id = 0;  ///< non-zero, unique
    std::string label{};
    Color color = ToolTheme::text;
    std::vector<TreeItem> children{};
};
struct TreeViewProps {
    std::vector<TreeItem> roots{};
    std::shared_ptr<TreeState> state{};
    std::function<void(std::uint64_t)> onSelected{};
    float rowHeight = ToolTheme::rowHeight;
};
/// Expandable rows; tap a row to select it, tap its arrow to fold it.
WidgetPtr TreeView(TreeViewProps props);

struct Tab {
    std::string title{};
    WidgetPtr child{};
};
struct TabViewProps {
    std::vector<Tab> tabs{};
    std::shared_ptr<int> index{};
};
WidgetPtr TabView(TabViewProps props);

// ---------------------------------------------------------------------------
// Composition helpers
// ---------------------------------------------------------------------------
struct PanelProps {
    std::string title{};
    std::vector<WidgetPtr> actions{};  ///< small buttons on the right of the header
    WidgetPtr child{};
    Color background = ToolTheme::panel;
};
/// A dock: a titled header over its content, blocking input to what is below.
WidgetPtr Panel(PanelProps props);

struct SectionProps {
    std::string title{};
    bool expanded = true;
    VoidCallback onToggle{};
    std::vector<WidgetPtr> actions{};
    WidgetPtr child{};
};
/// A foldable group (an inspector component).
WidgetPtr Section(SectionProps props);

struct PropertyRowProps {
    std::string label{};
    WidgetPtr editor{};
    float labelWidth = 120;
};
/// "label | editor" on one row.
WidgetPtr PropertyRow(PropertyRowProps props);

/// Text in the tool theme.
WidgetPtr Label(std::string text, Color color = ToolTheme::text,
                float fontSize = ToolTheme::fontSize);

}  // namespace phoenix
