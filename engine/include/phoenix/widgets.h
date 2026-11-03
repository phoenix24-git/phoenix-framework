// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix widgets — a Flutter-style declarative UI layer for C++ scripts.
//
//   class CounterPage : public phoenix::StatefulWidget {
//       int counter_ = 0;
//       WidgetPtr build() override {
//           return Scaffold({
//               .appBar = AppBar({.title = "Phoenix"}),
//               .body = Center({.child = Text(std::to_string(counter_), {.fontSize = 48})}),
//               .floatingActionButton = FloatingActionButton({
//                   .onPressed = [this] { setState([this] { ++counter_; }); },
//                   .child = Text("+", {.fontSize = 32}),
//               }),
//           });
//       }
//   };
//   void phoenix_main() { phoenix::runApp(std::make_shared<CounterPage>()); }
//
// Named arguments use C++20 designated initializers, mirroring Dart's named
// parameters. Layout follows Flutter's protocol: constraints go down, sizes
// go up, the parent sets the position.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "phoenix/script_abi.h"

namespace phoenix {

// ---------------------------------------------------------------------------
// Geometry & painting primitives
// ---------------------------------------------------------------------------
struct Offset {
    float x = 0, y = 0;
};
struct Size {
    float width = 0, height = 0;
};
struct Rect {
    float x = 0, y = 0, width = 0, height = 0;
    bool contains(Offset p) const {
        return p.x >= x && p.y >= y && p.x < x + width && p.y < y + height;
    }
};
struct EdgeInsets {
    float left = 0, top = 0, right = 0, bottom = 0;
    static EdgeInsets all(float v) { return {v, v, v, v}; }
    static EdgeInsets symmetric(float h, float v) { return {h, v, h, v}; }
};

/// 0xRRGGBBAA.
struct Color {
    std::uint32_t rgba = 0x000000FFu;
    static constexpr Color rgb(std::uint32_t rrggbb) { return {(rrggbb << 8) | 0xFFu}; }
    Color darker(float f = 0.85f) const;
};

namespace Colors {
inline constexpr Color white{0xFFFFFFFFu};
inline constexpr Color black{0x000000FFu};
inline constexpr Color transparent{0x00000000u};
inline constexpr Color deepOrange{0xFF5722FFu};
inline constexpr Color indigo{0x3F51B5FFu};
inline constexpr Color blueGrey{0x607D8BFFu};
inline constexpr Color surface{0xFFF8F5FFu};
}  // namespace Colors

struct BoxConstraints {
    float minWidth = 0, maxWidth = 0, minHeight = 0, maxHeight = 0;
    static BoxConstraints tight(Size s) { return {s.width, s.width, s.height, s.height}; }
    static BoxConstraints loose(Size s) { return {0, s.width, 0, s.height}; }
    Size constrain(Size s) const;
    BoxConstraints deflate(const EdgeInsets& e) const;
    BoxConstraints loosen() const { return {0, maxWidth, 0, maxHeight}; }
};

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
/// Keys the framework understands. Printable keys arrive as `character` with `ch` set
/// (letters lower-case), so shortcuts like Ctrl+S are {character, 's', ctrl}.
enum class Key : std::uint8_t {
    unknown,
    character,
    enter,
    escape,
    backspace,
    del,
    tab,
    left,
    right,
    up,
    down,
    home,
    end,
    pageUp,
    pageDown,
    f1,
    f2,
    f3,
    f4,
    f5,
    f6,
    f7,
    f8,
    f9,
    f10,
    f11,
    f12,
};

struct KeyEvent {
    Key key = Key::unknown;
    char ch = 0;  ///< for Key::character: the ASCII character (letters lower-case)
    bool ctrl = false, shift = false, alt = false;
    /// True for Ctrl+<c> (or Cmd+<c> on macOS, which the engine reports as ctrl).
    bool isShortcut(char c) const { return key == Key::character && ctrl && ch == c; }
};

/// Everything the UI needs from one frame of input. Positions are logical pixels.
struct FrameInput {
    Offset pointer{-1, -1};
    bool pointerDown = false;
    bool pressed = false;            ///< the primary button went down this frame
    bool released = false;           ///< the primary button went up this frame
    float scrollX = 0, scrollY = 0;  ///< wheel delta this frame (lines; +y = towards the user)
    std::vector<KeyEvent> keys;      ///< key presses this frame, in order
    std::string text;                ///< UTF-8 text typed this frame
};

class WidgetsBinding;

/// Keyboard focus. Owned by long-lived state (a StatefulWidget member or a controller);
/// widgets register it while painting. Destroying a focused node clears the focus.
class FocusNode {
  public:
    FocusNode() = default;
    FocusNode(const FocusNode&) = delete;
    FocusNode& operator=(const FocusNode&) = delete;
    ~FocusNode();

    bool hasFocus() const;
    void requestFocus();
    void unfocus();

    /// Called when this node loses focus (another node takes it, it is unfocused, or its
    /// widget leaves the screen). Not called when the node is destroyed.
    std::function<void()> onBlur{};
};

/// Pointer callbacks for one interactive region. Callbacks may outlive the widget that
/// registered them (a drag keeps its handlers until release), so capture state that lives
/// at least as long as the interaction: a StatefulWidget, a controller or a shared_ptr.
struct PointerHandlers {
    std::function<void(Offset)> onTap{};    ///< press and release inside, without dragging
    std::function<void(Offset)> onPress{};  ///< the button went down on this region
    /// Called every frame the pointer moves while this region holds the press (after it
    /// moved past the drag slop): `position`, and `delta` since the previous call.
    std::function<void(Offset position, Offset delta)> onDrag{};
    std::function<void(Offset)> onDragEnd{};
    /// Wheel over the region (lines). Scroll bubbles: the topmost region with onScroll wins.
    std::function<void(Offset delta)> onScroll{};
    /// Pressing the region moves keyboard focus here (nullptr: pressing clears focus).
    FocusNode* focus = nullptr;
};

/// Collects draw commands and hit targets while painting one frame.
class PaintContext {
  public:
    PaintContext(std::vector<PhxDrawRect>& out, Offset pointer, bool pointer_down);
    PaintContext(std::vector<PhxDrawRect>& out, const FrameInput& input);

    void drawRect(Rect r, Color c, float rotation = 0.0f);
    /// Draws the outline of `r`, `thickness` pixels wide, inside the rect.
    void drawBorder(Rect r, Color c, float thickness = 1.0f);
    /// Restricts drawing to `r` (intersected with the current clip) until popClip().
    /// Axis-aligned rects are clipped exactly; rotated rects are culled by centre.
    void pushClip(Rect r);
    void popClip();
    /// The current clip, or a huge rect when nothing is clipped.
    Rect clip() const;

    /// Registers a tap target; the callback receives the tap position.
    void addHitTarget(Rect r, std::function<void(Offset)> on_tap);
    /// Registers an interactive region. Regions are opaque: a press goes to the topmost
    /// region under the pointer, even one with no handlers (use that to block the UI below).
    /// The region is clipped to the current clip.
    void addInteraction(Rect r, PointerHandlers handlers);
    /// Receives key presses and typed text while `focus` has focus. Return true to consume
    /// the key. With focus == nullptr the handler sees keys nobody focused consumed
    /// (shortcuts); later-registered handlers run first.
    void addKeyHandler(FocusNode* focus, std::function<bool(const KeyEvent&)> on_key,
                       std::function<void(const std::string&)> on_text = {});
    /// Paints `painter` after the whole tree (menus, dropdowns, tooltips): it draws on top
    /// and its hit targets win.
    void addOverlay(std::function<void(PaintContext&)> painter);

    Offset pointer() const { return input_->pointer; }
    bool pointerDown() const { return input_->pointerDown; }
    const FrameInput& input() const { return *input_; }
    /// The pointer is inside `r` and no other region holds the press.
    bool hovered(Rect r) const;

    struct HitTarget {
        Rect rect;
        PointerHandlers handlers;
    };
    std::vector<HitTarget>& hitTargets() { return hits_; }

    struct KeyTarget {
        FocusNode* focus;
        std::function<bool(const KeyEvent&)> on_key;
        std::function<void(const std::string&)> on_text;
    };
    std::vector<KeyTarget>& keyTargets() { return keys_; }
    std::vector<std::function<void(PaintContext&)>>& overlays() { return overlays_; }

    /// Semantics: labelled regions (text, tooltips) used by tests/accessibility.
    struct SemanticsNode {
        std::string label;
        Rect rect;
    };
    void addSemantics(std::string label, Rect r) { semantics_.push_back({std::move(label), r}); }
    std::vector<SemanticsNode>& semantics() { return semantics_; }

  private:
    std::vector<PhxDrawRect>& out_;
    std::vector<HitTarget> hits_;
    std::vector<KeyTarget> keys_;
    std::vector<std::function<void(PaintContext&)>> overlays_;
    std::vector<SemanticsNode> semantics_;
    std::vector<Rect> clips_;
    FrameInput owned_input_;
    const FrameInput* input_;
};

// ---------------------------------------------------------------------------
// Widget base classes
// ---------------------------------------------------------------------------
class Widget;
using WidgetPtr = std::shared_ptr<Widget>;
using VoidCallback = std::function<void()>;

class Widget {
  public:
    virtual ~Widget() = default;
    /// Computes and stores this widget's size under `constraints`.
    virtual Size layout(const BoxConstraints& constraints) = 0;
    /// Paints at `origin` (top-left, logical pixels) using the stored size.
    virtual void paint(PaintContext& context, Offset origin) = 0;
    /// Flex factor when placed in a Row/Column (0 = not flexible).
    virtual int flex() const { return 0; }
    Size size() const { return size_; }

  protected:
    Size size_;
};

/// A widget that describes part of the UI by composing other widgets.
class StatelessWidget : public Widget {
  public:
    virtual WidgetPtr build() = 0;
    Size layout(const BoxConstraints& constraints) override;
    void paint(PaintContext& context, Offset origin) override;

  private:
    WidgetPtr child_;
};

/// Like Flutter's StatefulWidget + State in one object: keep the instance
/// alive (e.g. as a member of its parent) and call setState() to rebuild.
class StatefulWidget : public StatelessWidget {
  public:
    void setState(const VoidCallback& fn);
};

// ---------------------------------------------------------------------------
// Basic widgets (factory functions take Flutter-style named arguments)
// ---------------------------------------------------------------------------
enum class MainAxisAlignment { start, center, end, spaceBetween, spaceAround, spaceEvenly };
enum class CrossAxisAlignment { start, center, end, stretch };

struct ContainerProps {
    float width = -1, height = -1;  // -1 = unconstrained
    Color color = Colors::transparent;
    EdgeInsets padding{};
    EdgeInsets margin{};
    WidgetPtr child{};
};
WidgetPtr Container(ContainerProps props);

struct SizedBoxProps {
    float width = -1, height = -1;
    WidgetPtr child{};
};
WidgetPtr SizedBox(SizedBoxProps props);

struct PaddingProps {
    EdgeInsets padding{};
    WidgetPtr child{};
};
WidgetPtr Padding(PaddingProps props);

struct CenterProps {
    WidgetPtr child{};
};
WidgetPtr Center(CenterProps props);

struct FlexProps {
    MainAxisAlignment mainAxisAlignment = MainAxisAlignment::start;
    CrossAxisAlignment crossAxisAlignment = CrossAxisAlignment::center;
    float spacing = 0;
    std::vector<WidgetPtr> children{};
};
WidgetPtr Column(FlexProps props);
WidgetPtr Row(FlexProps props);

struct ExpandedProps {
    int flex = 1;
    WidgetPtr child{};
};
WidgetPtr Expanded(ExpandedProps props);

struct TextStyle {
    float fontSize = 16;
    Color color = Colors::black;
};
WidgetPtr Text(std::string text, TextStyle style = {});
/// Measures text rendered with the built-in 5x7 bitmap font.
Size measureText(const std::string& text, float font_size);
/// Paints `text` with its top-left at `origin` (what Text paints, without a widget).
void drawText(PaintContext& context, Offset origin, const std::string& text, TextStyle style);

struct GestureDetectorProps {
    VoidCallback onTap{};
    WidgetPtr child{};
};
WidgetPtr GestureDetector(GestureDetectorProps props);

// ---------------------------------------------------------------------------
// Material-flavoured widgets
// ---------------------------------------------------------------------------
struct AppBarProps {
    std::string title;
    Color backgroundColor = Colors::deepOrange;
    Color foregroundColor = Colors::white;
};
WidgetPtr AppBar(AppBarProps props);

struct FloatingActionButtonProps {
    VoidCallback onPressed{};
    std::string tooltip{};
    Color backgroundColor = Colors::deepOrange;
    WidgetPtr child{};
};
WidgetPtr FloatingActionButton(FloatingActionButtonProps props);

struct ElevatedButtonProps {
    VoidCallback onPressed{};
    std::string tooltip{};
    Color backgroundColor = Colors::indigo;
    WidgetPtr child{};
};
WidgetPtr ElevatedButton(ElevatedButtonProps props);

struct ScaffoldProps {
    WidgetPtr appBar{};
    WidgetPtr body{};
    WidgetPtr floatingActionButton{};
    Color backgroundColor = Colors::surface;
};
WidgetPtr Scaffold(ScaffoldProps props);

// ---------------------------------------------------------------------------
// Engine-backed widgets
// ---------------------------------------------------------------------------
struct PhysicsViewProps {
    /// World units (meters) to logical pixels.
    float pixelsPerMeter = 40;
    /// World point that maps to the bottom-center of the view.
    float originX = 0, originY = 0;
    Color backgroundColor = Color::rgb(0x1E1E2E);
    /// Called with the tap position in world coordinates.
    std::function<void(float x, float y)> onTapWorld{};
};
/// Renders the live Jolt physics snapshot (expands to fill its constraints).
WidgetPtr PhysicsView(PhysicsViewProps props);

// ---------------------------------------------------------------------------
// App bootstrap
// ---------------------------------------------------------------------------
/// Installs `root` as the application widget (like Flutter's runApp).
void runApp(WidgetPtr root);

/// Framework driver used by the engine every frame (like WidgetsBinding).
class WidgetsBinding {
  public:
    static WidgetsBinding& instance();

    void setRoot(WidgetPtr root) { root_ = std::move(root); }
    bool hasRoot() const { return root_ != nullptr; }
    /// A second tree painted on top of the root (the in-game editor, debug overlays). It is
    /// laid out with the same viewport; where it registers no hit regions, input reaches
    /// the root below.
    void setOverlayRoot(WidgetPtr overlay) { overlay_ = std::move(overlay); }
    const WidgetPtr& overlayRoot() const { return overlay_; }
    void markNeedsBuild() { needs_build_ = true; }

    /// Lays out and paints the tree for a viewport, appending draw commands, then
    /// dispatches `input` (taps, drags, wheel, keys, text).
    void drawFrame(Size viewport, const FrameInput& input, std::vector<PhxDrawRect>& out);
    /// Pointer-only form: `tapped` is true on the frame the pointer was released.
    void drawFrame(Size viewport, Offset pointer, bool pointer_down, bool tapped,
                   std::vector<PhxDrawRect>& out);

    std::uint64_t frames() const { return frames_; }
    /// Semantics of the last painted frame.
    const std::vector<PaintContext::SemanticsNode>& semantics() const { return semantics_; }
    /// True while the current frame is rebuilding (StatelessWidget::build runs).
    bool rebuilding() const { return rebuilding_; }

    /// Keyboard focus (see FocusNode).
    FocusNode* focused() const { return focused_; }
    /// Moves focus to `node` (nullptr clears it), calling the old node's onBlur.
    void setFocus(FocusNode* node);
    /// True while something has keyboard focus: the engine turns on text input (IME,
    /// on-screen keyboards) only then.
    bool wantsTextInput() const { return focused_ != nullptr; }
    /// True while a region holds the press (a drag or a press in progress).
    bool pointerCaptured() const { return captured_; }
    /// The region holding the press (valid while pointerCaptured()).
    const Rect& captureRect() const { return capture_rect_; }
    /// True if the last frame's topmost region under the pointer belonged to the overlay
    /// root: the engine uses it to keep editor clicks out of the game.
    bool pointerOverOverlay() const { return over_overlay_; }

  private:
    void dispatch(PaintContext& ctx, const FrameInput& input, std::size_t overlay_first_hit);

    WidgetPtr root_;
    WidgetPtr overlay_;
    bool needs_build_ = true;
    bool rebuilding_ = false;
    std::vector<PaintContext::SemanticsNode> semantics_;
    std::uint64_t frames_ = 0;

    // Pointer state across frames.
    bool was_down_ = false;
    bool captured_ = false;
    bool dragging_ = false;
    bool over_overlay_ = false;
    Rect capture_rect_{};
    PointerHandlers capture_{};
    Offset press_pos_{};
    Offset last_pos_{};
    FocusNode* focused_ = nullptr;
};

}  // namespace phoenix
