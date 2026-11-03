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

/// Collects draw commands and hit targets while painting one frame.
class PaintContext {
  public:
    PaintContext(std::vector<PhxDrawRect>& out, Offset pointer, bool pointer_down)
        : out_(out), pointer_(pointer), pointer_down_(pointer_down) {}

    void drawRect(Rect r, Color c, float rotation = 0.0f);
    /// Restricts drawing to `r` (intersected with the current clip) until popClip().
    /// Axis-aligned rects are clipped exactly; rotated rects are culled by centre.
    void pushClip(Rect r);
    void popClip();
    /// Registers a tap target; the callback receives the tap position.
    void addHitTarget(Rect r, std::function<void(Offset)> on_tap);
    Offset pointer() const { return pointer_; }
    bool pointerDown() const { return pointer_down_; }

    struct HitTarget {
        Rect rect;
        std::function<void(Offset)> on_tap;
    };
    std::vector<HitTarget>& hitTargets() { return hits_; }

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
    std::vector<SemanticsNode> semantics_;
    std::vector<Rect> clips_;
    Offset pointer_;
    bool pointer_down_;
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
    void markNeedsBuild() { needs_build_ = true; }

    /// Lays out and paints the tree for a viewport, appending draw commands.
    /// `tapped` is true on the frame the pointer was released (a "tap").
    void drawFrame(Size viewport, Offset pointer, bool pointer_down, bool tapped,
                   std::vector<PhxDrawRect>& out);

    std::uint64_t frames() const { return frames_; }
    /// Semantics of the last painted frame.
    const std::vector<PaintContext::SemanticsNode>& semantics() const { return semantics_; }
    /// True while the current frame is rebuilding (StatelessWidget::build runs).
    bool rebuilding() const { return rebuilding_; }

  private:
    WidgetPtr root_;
    bool needs_build_ = true;
    bool rebuilding_ = false;
    std::vector<PaintContext::SemanticsNode> semantics_;
    std::uint64_t frames_ = 0;
};

}  // namespace phoenix
