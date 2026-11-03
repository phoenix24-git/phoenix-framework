// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/widgets.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "framework/font5x7.h"
#include "phoenix/engine.h"

namespace phoenix {

namespace {
constexpr float kInf = std::numeric_limits<float>::infinity();

float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}
}  // namespace

// ---------------------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------------------
Color Color::darker(float f) const {
    auto ch = [&](int shift) {
        return static_cast<std::uint32_t>(((rgba >> shift) & 0xFFu) * f) & 0xFFu;
    };
    return {(ch(24) << 24) | (ch(16) << 16) | (ch(8) << 8) | (rgba & 0xFFu)};
}

Size BoxConstraints::constrain(Size s) const {
    return {clampf(s.width, minWidth, maxWidth), clampf(s.height, minHeight, maxHeight)};
}

BoxConstraints BoxConstraints::deflate(const EdgeInsets& e) const {
    const float h = e.left + e.right;
    const float v = e.top + e.bottom;
    const float max_w = std::max(0.0f, maxWidth - h);
    const float max_h = std::max(0.0f, maxHeight - v);
    return {std::min(std::max(0.0f, minWidth - h), max_w), max_w,
            std::min(std::max(0.0f, minHeight - v), max_h), max_h};
}

namespace {
Rect intersect(const Rect& a, const Rect& b) {
    const float x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
    const float x1 = std::min(a.x + a.width, b.x + b.width);
    const float y1 = std::min(a.y + a.height, b.y + b.height);
    return {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
}
}  // namespace

PaintContext::PaintContext(std::vector<PhxDrawRect>& out, Offset pointer, bool pointer_down)
    : out_(out), input_(&owned_input_) {
    owned_input_.pointer = pointer;
    owned_input_.pointerDown = pointer_down;
}

PaintContext::PaintContext(std::vector<PhxDrawRect>& out, const FrameInput& input)
    : out_(out), input_(&input) {}

void PaintContext::pushClip(Rect r) {
    if (!clips_.empty())
        r = intersect(r, clips_.back());
    clips_.push_back(r);
}

void PaintContext::popClip() {
    if (!clips_.empty())
        clips_.pop_back();
}

Rect PaintContext::clip() const {
    return clips_.empty() ? Rect{-1e9f, -1e9f, 2e9f, 2e9f} : clips_.back();
}

void PaintContext::drawRect(Rect r, Color c, float rotation) {
    if ((c.rgba & 0xFFu) == 0 || r.width <= 0 || r.height <= 0) {
        return;
    }
    if (!clips_.empty()) {
        const Rect& clip = clips_.back();
        if (rotation != 0.0f) {
            if (!clip.contains({r.x + r.width * 0.5f, r.y + r.height * 0.5f}))
                return;
        } else {
            r = intersect(r, clip);
            if (r.width <= 0 || r.height <= 0)
                return;
        }
    }
    out_.push_back(PhxDrawRect{r.x, r.y, r.width, r.height, c.rgba, rotation, 0.0f, 0u});
}

void PaintContext::drawBorder(Rect r, Color c, float t) {
    drawRect({r.x, r.y, r.width, t}, c);
    drawRect({r.x, r.y + r.height - t, r.width, t}, c);
    drawRect({r.x, r.y + t, t, r.height - 2 * t}, c);
    drawRect({r.x + r.width - t, r.y + t, t, r.height - 2 * t}, c);
}

void PaintContext::addHitTarget(Rect r, std::function<void(Offset)> on_tap) {
    if (on_tap) {
        addInteraction(r, {.onTap = std::move(on_tap)});
    }
}

void PaintContext::addInteraction(Rect r, PointerHandlers handlers) {
    if (!clips_.empty())
        r = intersect(r, clips_.back());
    if (r.width <= 0 || r.height <= 0)
        return;
    hits_.push_back({r, std::move(handlers)});
}

void PaintContext::addKeyHandler(FocusNode* focus, std::function<bool(const KeyEvent&)> on_key,
                                 std::function<void(const std::string&)> on_text) {
    keys_.push_back({focus, std::move(on_key), std::move(on_text)});
}

void PaintContext::addOverlay(std::function<void(PaintContext&)> painter) {
    if (painter)
        overlays_.push_back(std::move(painter));
}

bool PaintContext::hovered(Rect r) const {
    const WidgetsBinding& b = WidgetsBinding::instance();
    if (!r.contains(input_->pointer))
        return false;
    if (!b.pointerCaptured())
        return true;
    // While a region holds the press, only that region shows hover.
    const Rect overlap = intersect(r, b.captureRect());
    return overlap.width > 0 && overlap.height > 0;
}

// ---------------------------------------------------------------------------
// Focus
// ---------------------------------------------------------------------------
FocusNode::~FocusNode() {
    onBlur = nullptr;
    unfocus();
}

bool FocusNode::hasFocus() const {
    return WidgetsBinding::instance().focused() == this;
}

void FocusNode::requestFocus() {
    WidgetsBinding::instance().setFocus(this);
}

void FocusNode::unfocus() {
    if (hasFocus())
        WidgetsBinding::instance().setFocus(nullptr);
}

void WidgetsBinding::setFocus(FocusNode* node) {
    if (focused_ == node)
        return;
    FocusNode* old = focused_;
    focused_ = node;
    if (old && old->onBlur) {
        auto on_blur = old->onBlur;  // the callback may destroy or refocus the node
        on_blur();
    }
}

// ---------------------------------------------------------------------------
// Stateless / Stateful
// ---------------------------------------------------------------------------
Size StatelessWidget::layout(const BoxConstraints& constraints) {
    if (!child_ || WidgetsBinding::instance().rebuilding()) {
        child_ = build();
    }
    size_ = child_ ? child_->layout(constraints) : constraints.constrain({});
    return size_;
}

void StatelessWidget::paint(PaintContext& context, Offset origin) {
    if (child_) {
        child_->paint(context, origin);
    }
}

void StatefulWidget::setState(const VoidCallback& fn) {
    if (fn) {
        fn();
    }
    WidgetsBinding::instance().markNeedsBuild();
}

namespace {

// ---------------------------------------------------------------------------
// Layout widgets
// ---------------------------------------------------------------------------
class ContainerWidget final : public Widget {
  public:
    explicit ContainerWidget(ContainerProps p) : p_(std::move(p)) {}

    Size layout(const BoxConstraints& c) override {
        BoxConstraints inner = c.deflate(p_.margin);
        if (p_.width >= 0) {
            const float w = clampf(p_.width, inner.minWidth, inner.maxWidth);
            inner.minWidth = inner.maxWidth = w;
        }
        if (p_.height >= 0) {
            const float h = clampf(p_.height, inner.minHeight, inner.maxHeight);
            inner.minHeight = inner.maxHeight = h;
        }
        Size box{};
        if (p_.child) {
            const Size child = p_.child->layout(inner.deflate(p_.padding));
            box = inner.constrain({child.width + p_.padding.left + p_.padding.right,
                                   child.height + p_.padding.top + p_.padding.bottom});
        } else {
            box = inner.constrain({p_.width >= 0 ? p_.width : 0, p_.height >= 0 ? p_.height : 0});
        }
        box_ = box;
        size_ = c.constrain({box.width + p_.margin.left + p_.margin.right,
                             box.height + p_.margin.top + p_.margin.bottom});
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const Offset box_origin{o.x + p_.margin.left, o.y + p_.margin.top};
        ctx.drawRect({box_origin.x, box_origin.y, box_.width, box_.height}, p_.color);
        if (p_.child) {
            p_.child->paint(ctx, {box_origin.x + p_.padding.left, box_origin.y + p_.padding.top});
        }
    }

  private:
    ContainerProps p_;
    Size box_;
};

class CenterWidget final : public Widget {
  public:
    explicit CenterWidget(WidgetPtr child) : child_(std::move(child)) {}

    Size layout(const BoxConstraints& c) override {
        const Size child = child_ ? child_->layout(c.loosen()) : Size{};
        size_ = c.constrain({std::isfinite(c.maxWidth) ? c.maxWidth : child.width,
                             std::isfinite(c.maxHeight) ? c.maxHeight : child.height});
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        if (child_) {
            const Size child = child_->size();
            child_->paint(ctx, {o.x + (size_.width - child.width) * 0.5f,
                                o.y + (size_.height - child.height) * 0.5f});
        }
    }

  private:
    WidgetPtr child_;
};

class ExpandedWidget final : public Widget {
  public:
    ExpandedWidget(int flex, WidgetPtr child)
        : flex_(std::max(1, flex)), child_(std::move(child)) {}
    int flex() const override { return flex_; }
    Size layout(const BoxConstraints& c) override {
        size_ = child_ ? child_->layout(c) : c.constrain({c.maxWidth, c.maxHeight});
        size_ = c.constrain(size_);
        return size_;
    }
    void paint(PaintContext& ctx, Offset o) override {
        if (child_) {
            child_->paint(ctx, o);
        }
    }

  private:
    int flex_;
    WidgetPtr child_;
};

class FlexWidget final : public Widget {
  public:
    FlexWidget(FlexProps p, bool vertical) : p_(std::move(p)), vertical_(vertical) {}

    Size layout(const BoxConstraints& c) override {
        const float max_main = vertical_ ? c.maxHeight : c.maxWidth;
        const float max_cross = vertical_ ? c.maxWidth : c.maxHeight;
        const bool stretch = p_.crossAxisAlignment == CrossAxisAlignment::stretch;
        const float min_cross = stretch && std::isfinite(max_cross) ? max_cross : 0.0f;
        const float gaps = p_.children.empty() ? 0.0f : p_.spacing * (p_.children.size() - 1);

        auto make = [&](float min_main, float main_max) {
            return vertical_ ? BoxConstraints{min_cross, max_cross, min_main, main_max}
                             : BoxConstraints{min_main, main_max, min_cross, max_cross};
        };

        float allocated = gaps;
        float cross = 0;
        int total_flex = 0;
        for (auto& child : p_.children) {
            if (!child)
                continue;
            if (child->flex() > 0) {
                total_flex += child->flex();
                continue;
            }
            const Size s = child->layout(make(0, kInf));
            allocated += vertical_ ? s.height : s.width;
            cross = std::max(cross, vertical_ ? s.width : s.height);
        }
        const float free_space =
            std::isfinite(max_main) ? std::max(0.0f, max_main - allocated) : 0.0f;
        for (auto& child : p_.children) {
            if (!child || child->flex() == 0)
                continue;
            const float share = total_flex ? free_space * child->flex() / total_flex : 0.0f;
            const Size s = child->layout(make(share, share));
            allocated += vertical_ ? s.height : s.width;
            cross = std::max(cross, vertical_ ? s.width : s.height);
        }
        const float main = std::isfinite(max_main) ? max_main : allocated;
        content_main_ = allocated;
        size_ = c.constrain(vertical_ ? Size{std::max(cross, min_cross), main}
                                      : Size{main, std::max(cross, min_cross)});
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const float main_size = vertical_ ? size_.height : size_.width;
        const float cross_size = vertical_ ? size_.width : size_.height;
        const float free_space = std::max(0.0f, main_size - content_main_);
        const auto n = static_cast<float>(p_.children.size());
        float lead = 0, between = p_.spacing;
        switch (p_.mainAxisAlignment) {
            case MainAxisAlignment::start:
                break;
            case MainAxisAlignment::center:
                lead = free_space * 0.5f;
                break;
            case MainAxisAlignment::end:
                lead = free_space;
                break;
            case MainAxisAlignment::spaceBetween:
                between += n > 1 ? free_space / (n - 1) : 0;
                break;
            case MainAxisAlignment::spaceAround:
                lead = n > 0 ? free_space / n * 0.5f : 0;
                between += n > 0 ? free_space / n : 0;
                break;
            case MainAxisAlignment::spaceEvenly:
                lead = free_space / (n + 1);
                between += free_space / (n + 1);
                break;
        }
        float cursor = lead;
        for (auto& child : p_.children) {
            if (!child)
                continue;
            const Size s = child->size();
            const float child_cross = vertical_ ? s.width : s.height;
            float cross_off = 0;
            switch (p_.crossAxisAlignment) {
                case CrossAxisAlignment::center:
                    cross_off = (cross_size - child_cross) * 0.5f;
                    break;
                case CrossAxisAlignment::end:
                    cross_off = cross_size - child_cross;
                    break;
                default:
                    break;
            }
            child->paint(ctx, vertical_ ? Offset{o.x + cross_off, o.y + cursor}
                                        : Offset{o.x + cursor, o.y + cross_off});
            cursor += (vertical_ ? s.height : s.width) + between;
        }
    }

  private:
    FlexProps p_;
    bool vertical_;
    float content_main_ = 0;
};

class TextWidget final : public Widget {
  public:
    TextWidget(std::string text, TextStyle style) : text_(std::move(text)), style_(style) {}

    Size layout(const BoxConstraints& c) override {
        size_ = c.constrain(measureText(text_, style_.fontSize));
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        ctx.addSemantics(text_, {o.x, o.y, size_.width, size_.height});
        drawText(ctx, o, text_, style_);
    }

  private:
    std::string text_;
    TextStyle style_;
};

class GestureDetectorWidget final : public Widget {
  public:
    explicit GestureDetectorWidget(GestureDetectorProps p) : p_(std::move(p)) {}
    Size layout(const BoxConstraints& c) override {
        size_ = p_.child ? p_.child->layout(c) : c.constrain({});
        return size_;
    }
    void paint(PaintContext& ctx, Offset o) override {
        if (p_.child)
            p_.child->paint(ctx, o);
        if (p_.onTap) {
            ctx.addHitTarget({o.x, o.y, size_.width, size_.height},
                             [cb = p_.onTap](Offset) { cb(); });
        }
    }

  private:
    GestureDetectorProps p_;
};

/// A solid button surface that darkens while pressed.
class ButtonSurface final : public Widget {
  public:
    ButtonSurface(VoidCallback on_pressed, std::string tooltip, Color color, WidgetPtr child,
                  Size fixed, EdgeInsets pad)
        : on_pressed_(std::move(on_pressed)),
          tooltip_(std::move(tooltip)),
          color_(color),
          child_(std::move(child)),
          fixed_(fixed),
          pad_(pad) {}

    Size layout(const BoxConstraints& c) override {
        const Size child = child_ ? child_->layout(c.loosen().deflate(pad_)) : Size{};
        Size s{child.width + pad_.left + pad_.right, child.height + pad_.top + pad_.bottom};
        if (fixed_.width > 0)
            s = fixed_;
        size_ = c.constrain(s);
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const Rect r{o.x, o.y, size_.width, size_.height};
        const bool pressed = ctx.pointerDown() && r.contains(ctx.pointer());
        ctx.drawRect({r.x + 2, r.y + 3, r.width, r.height}, Color{0x00000040u});  // shadow
        ctx.drawRect(r, pressed ? color_.darker() : color_);
        if (child_) {
            const Size cs = child_->size();
            child_->paint(ctx, {o.x + (size_.width - cs.width) * 0.5f,
                                o.y + (size_.height - cs.height) * 0.5f});
        }
        if (on_pressed_) {
            ctx.addHitTarget(r, [cb = on_pressed_](Offset) { cb(); });
        }
        if (!tooltip_.empty()) {
            ctx.addSemantics(tooltip_, r);
        }
    }

  private:
    VoidCallback on_pressed_;
    std::string tooltip_;
    Color color_;
    WidgetPtr child_;
    Size fixed_;
    EdgeInsets pad_;
};

class ScaffoldWidget final : public Widget {
  public:
    explicit ScaffoldWidget(ScaffoldProps p) : p_(std::move(p)) {}

    Size layout(const BoxConstraints& c) override {
        size_ = c.constrain({std::isfinite(c.maxWidth) ? c.maxWidth : 800,
                             std::isfinite(c.maxHeight) ? c.maxHeight : 600});
        bar_h_ = p_.appBar ? p_.appBar->layout({size_.width, size_.width, 0, size_.height}).height
                           : 0.0f;
        if (p_.body) {
            const Size body{size_.width, std::max(0.0f, size_.height - bar_h_)};
            p_.body->layout(BoxConstraints::tight(body));
        }
        if (p_.floatingActionButton) {
            p_.floatingActionButton->layout(BoxConstraints::loose(size_));
        }
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        ctx.drawRect({o.x, o.y, size_.width, size_.height}, p_.backgroundColor);
        if (p_.body)
            p_.body->paint(ctx, {o.x, o.y + bar_h_});
        if (p_.appBar)
            p_.appBar->paint(ctx, o);
        if (p_.floatingActionButton) {
            const Size f = p_.floatingActionButton->size();
            p_.floatingActionButton->paint(
                ctx, {o.x + size_.width - f.width - 16, o.y + size_.height - f.height - 16});
        }
    }

  private:
    ScaffoldProps p_;
    float bar_h_ = 0;
};

class PhysicsViewWidget final : public Widget {
  public:
    explicit PhysicsViewWidget(PhysicsViewProps p) : p_(std::move(p)) {}

    Size layout(const BoxConstraints& c) override {
        size_ = c.constrain({std::isfinite(c.maxWidth) ? c.maxWidth : 400,
                             std::isfinite(c.maxHeight) ? c.maxHeight : 300});
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const Rect view{o.x, o.y, size_.width, size_.height};
        ctx.drawRect(view, p_.backgroundColor);
        ctx.pushClip(view);
        const float ppm = p_.pixelsPerMeter;
        const float cx = o.x + size_.width * 0.5f;
        const float by = o.y + size_.height;
        for (const PhxBody2D& b : bodies()) {
            const float w = b.half_w * 2 * ppm;
            const float h = b.half_h * 2 * ppm;
            const float sx = cx + (b.x - p_.originX) * ppm;
            const float sy = by - (b.y - p_.originY) * ppm;
            if (sx + w < view.x || sx - w > view.x + view.width || sy + h < view.y ||
                sy - h > view.y + view.height) {
                continue;  // culled
            }
            ctx.drawRect({sx - w * 0.5f, sy - h * 0.5f, w, h}, Color{b.rgba}, -b.angle);
        }
        ctx.popClip();
        if (p_.onTapWorld) {
            ctx.addHitTarget(
                view, [cb = p_.onTapWorld, cx, by, ppm, ox = p_.originX, oy = p_.originY](
                          Offset tap) { cb(ox + (tap.x - cx) / ppm, oy + (by - tap.y) / ppm); });
        }
    }

  private:
    PhysicsViewProps p_;
};

}  // namespace

// ---------------------------------------------------------------------------
// Factories
// ---------------------------------------------------------------------------
WidgetPtr Container(ContainerProps props) {
    return std::make_shared<ContainerWidget>(std::move(props));
}

WidgetPtr SizedBox(SizedBoxProps props) {
    return Container(
        {.width = props.width, .height = props.height, .child = std::move(props.child)});
}

WidgetPtr Padding(PaddingProps props) {
    return Container({.padding = props.padding, .child = std::move(props.child)});
}

WidgetPtr Center(CenterProps props) {
    return std::make_shared<CenterWidget>(std::move(props.child));
}

WidgetPtr Column(FlexProps props) {
    return std::make_shared<FlexWidget>(std::move(props), true);
}
WidgetPtr Row(FlexProps props) {
    return std::make_shared<FlexWidget>(std::move(props), false);
}

WidgetPtr Expanded(ExpandedProps props) {
    return std::make_shared<ExpandedWidget>(props.flex, std::move(props.child));
}

WidgetPtr Text(std::string text, TextStyle style) {
    return std::make_shared<TextWidget>(std::move(text), style);
}

Size measureText(const std::string& text, float font_size) {
    if (text.empty()) {
        return {0, font_size};
    }
    const float px = font_size / font5x7::kGlyphHeight;
    return {(static_cast<float>(text.size()) * font5x7::kAdvance - 1) * px, font_size};
}

void drawText(PaintContext& ctx, Offset o, const std::string& text, TextStyle style) {
    const float px = style.fontSize / font5x7::kGlyphHeight;
    const Rect clip = ctx.clip();
    float pen_x = o.x;
    for (char ch : text) {
        const float advance = font5x7::kAdvance * px;
        if (pen_x > clip.x + clip.width)
            break;  // the rest is clipped away
        if (pen_x + advance >= clip.x && ch != ' ') {
            for (int row = 0; row < font5x7::kGlyphHeight; ++row) {
                int col = 0;
                while (col < font5x7::kGlyphWidth) {
                    if (!font5x7::pixel(ch, col, row)) {
                        ++col;
                        continue;
                    }
                    const int start = col;
                    while (col < font5x7::kGlyphWidth && font5x7::pixel(ch, col, row))
                        ++col;
                    ctx.drawRect({pen_x + start * px, o.y + row * px, (col - start) * px, px},
                                 style.color);
                }
            }
        }
        pen_x += advance;
    }
}

WidgetPtr GestureDetector(GestureDetectorProps props) {
    return std::make_shared<GestureDetectorWidget>(std::move(props));
}

WidgetPtr AppBar(AppBarProps props) {
    return Container({
        .height = 56,
        .color = props.backgroundColor,
        .padding = EdgeInsets::symmetric(16, 18),
        .child = Text(props.title, {.fontSize = 20, .color = props.foregroundColor}),
    });
}

WidgetPtr FloatingActionButton(FloatingActionButtonProps props) {
    return std::make_shared<ButtonSurface>(std::move(props.onPressed), std::move(props.tooltip),
                                           props.backgroundColor, std::move(props.child),
                                           Size{56, 56}, EdgeInsets{});
}

WidgetPtr ElevatedButton(ElevatedButtonProps props) {
    return std::make_shared<ButtonSurface>(std::move(props.onPressed), std::move(props.tooltip),
                                           props.backgroundColor, std::move(props.child), Size{},
                                           EdgeInsets::symmetric(16, 10));
}

WidgetPtr Scaffold(ScaffoldProps props) {
    return std::make_shared<ScaffoldWidget>(std::move(props));
}

WidgetPtr PhysicsView(PhysicsViewProps props) {
    return std::make_shared<PhysicsViewWidget>(std::move(props));
}

// ---------------------------------------------------------------------------
// Binding
// ---------------------------------------------------------------------------
WidgetsBinding& WidgetsBinding::instance() {
    static WidgetsBinding binding;
    return binding;
}

void runApp(WidgetPtr root) {
    WidgetsBinding::instance().setRoot(std::move(root));
    WidgetsBinding::instance().markNeedsBuild();
}

void WidgetsBinding::drawFrame(Size viewport, Offset pointer, bool pointer_down, bool tapped,
                               std::vector<PhxDrawRect>& out) {
    FrameInput input;
    input.pointer = pointer;
    input.pointerDown = pointer_down;
    input.pressed = pointer_down && !was_down_;
    input.released = tapped;
    drawFrame(viewport, input, out);
}

namespace {
void flush_overlays(PaintContext& ctx) {
    // Overlay painters may add more overlays (a submenu inside a menu).
    for (int guard = 0; guard < 16 && !ctx.overlays().empty(); ++guard) {
        auto painters = std::move(ctx.overlays());
        ctx.overlays().clear();
        for (auto& painter : painters)
            painter(ctx);
    }
}

constexpr float kDragSlop = 3.0f;
}  // namespace

void WidgetsBinding::drawFrame(Size viewport, const FrameInput& input,
                               std::vector<PhxDrawRect>& out) {
    ++frames_;
    if (!root_ && !overlay_) {
        was_down_ = input.pointerDown;
        return;
    }
    rebuilding_ = needs_build_;
    needs_build_ = false;
    // Keep the trees alive for the whole frame: callbacks may replace them.
    const WidgetPtr root = root_;
    const WidgetPtr overlay = overlay_;
    if (root)
        root->layout(BoxConstraints::tight(viewport));
    if (overlay)
        overlay->layout(BoxConstraints::tight(viewport));
    rebuilding_ = false;

    PaintContext ctx(out, input);
    if (root) {
        root->paint(ctx, {});
        flush_overlays(ctx);
    }
    const std::size_t overlay_first_hit = ctx.hitTargets().size();
    if (overlay) {
        overlay->paint(ctx, {});
        flush_overlays(ctx);
    }
    semantics_ = std::move(ctx.semantics());
    dispatch(ctx, input, overlay_first_hit);
    was_down_ = input.pointerDown;
}

void WidgetsBinding::dispatch(PaintContext& ctx, const FrameInput& input,
                              std::size_t overlay_first_hit) {
    auto& hits = ctx.hitTargets();
    const Offset p = input.pointer;

    // Topmost (last painted) region under the pointer, like Flutter's hit testing.
    PaintContext::HitTarget* top = nullptr;
    over_overlay_ = false;
    for (std::size_t i = hits.size(); i-- > 0;) {
        if (hits[i].rect.contains(p)) {
            top = &hits[i];
            over_overlay_ = i >= overlay_first_hit;
            break;
        }
    }

    // A node that had focus while this frame was painted but registered no key handler
    // has left the screen. (Checked before the press: a press may focus a new node.)
    auto& key_targets = ctx.keyTargets();
    if (focused_) {
        const bool registered = std::any_of(key_targets.begin(), key_targets.end(),
                                            [&](const auto& t) { return t.focus == focused_; });
        if (!registered)
            setFocus(nullptr);
    }

    if (input.pressed) {
        setFocus(top ? top->handlers.focus : nullptr);
        if (top) {
            captured_ = true;
            dragging_ = false;
            capture_ = top->handlers;
            capture_rect_ = top->rect;
            press_pos_ = last_pos_ = p;
            if (capture_.onPress)
                capture_.onPress(p);
        } else {
            captured_ = false;
        }
    }

    if (captured_ && input.pointerDown) {
        if (!dragging_ && (std::fabs(p.x - press_pos_.x) > kDragSlop ||
                           std::fabs(p.y - press_pos_.y) > kDragSlop)) {
            dragging_ = true;
        }
        if (dragging_ && (p.x != last_pos_.x || p.y != last_pos_.y)) {
            if (capture_.onDrag)
                capture_.onDrag(p, {p.x - last_pos_.x, p.y - last_pos_.y});
            last_pos_ = p;
        }
    }

    if (input.released) {
        if (captured_) {
            const PointerHandlers handlers = std::move(capture_);
            const bool was_drag = dragging_;
            const bool inside = capture_rect_.contains(p);
            captured_ = dragging_ = false;
            capture_ = {};
            if (was_drag) {
                if (handlers.onDragEnd)
                    handlers.onDragEnd(p);
            } else if (inside && handlers.onTap) {
                handlers.onTap(p);
            }
        } else if (!input.pressed && top && top->handlers.onTap) {
            // A release without a press we saw (a click shorter than one frame).
            top->handlers.onTap(p);
        }
    } else if (captured_ && !input.pointerDown) {
        captured_ = dragging_ = false;  // the press ended without a release event
        capture_ = {};
    }

    if (input.scrollX != 0 || input.scrollY != 0) {
        for (std::size_t i = hits.size(); i-- > 0;) {
            if (hits[i].rect.contains(p) && hits[i].handlers.onScroll) {
                hits[i].handlers.onScroll({input.scrollX, input.scrollY});
                break;
            }
        }
    }

    // Keyboard: the focused node's handlers first, then shortcut handlers.
    if (focused_ && !input.text.empty()) {
        for (std::size_t i = key_targets.size(); i-- > 0;) {
            if (key_targets[i].focus == focused_ && key_targets[i].on_text) {
                key_targets[i].on_text(input.text);
                break;
            }
        }
    }
    for (const KeyEvent& key : input.keys) {
        bool consumed = false;
        if (focused_) {
            for (std::size_t i = key_targets.size(); i-- > 0 && !consumed;) {
                if (key_targets[i].focus == focused_ && key_targets[i].on_key)
                    consumed = key_targets[i].on_key(key);
            }
            if (!consumed && key.key == Key::escape) {
                setFocus(nullptr);
                consumed = true;
            }
        }
        for (std::size_t i = key_targets.size(); i-- > 0 && !consumed;) {
            if (key_targets[i].focus == nullptr && key_targets[i].on_key)
                consumed = key_targets[i].on_key(key);
        }
    }
}

}  // namespace phoenix
