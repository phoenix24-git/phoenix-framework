// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/tool_widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include "framework/font5x7.h"

namespace phoenix {

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr Rect kEverywhere{-1e9f, -1e9f, 2e9f, 2e9f};

float char_width(float font_size) {
    return font5x7::kAdvance * font_size / font5x7::kGlyphHeight;
}

float finite_or(float v, float fallback) {
    return std::isfinite(v) ? v : fallback;
}

/// y that vertically centres text of `font_size` in a box at `y` of height `h`.
float text_y(float y, float h, float font_size) {
    return std::floor(y + (h - font_size) * 0.5f);
}

std::string format_number(double v, int decimals, bool integer) {
    char buf[64];
    if (integer) {
        std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(std::llround(v)));
        return buf;
    }
    std::snprintf(buf, sizeof buf, "%.*f", std::max(0, decimals), v);
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0')
            s.pop_back();
        if (!s.empty() && s.back() == '.')
            s.pop_back();
    }
    if (s == "-0")
        s = "0";
    return s;
}

/// Paints a single-line editable text box and handles its keys while focused.
void paint_editable(PaintContext& ctx, Rect r, const std::shared_ptr<TextEditingController>& c,
                    float font_size, const std::string& hint,
                    const std::function<void(const std::string&)>& on_changed) {
    const bool focused = c->focus.hasFocus();
    ctx.drawRect(r, ctx.hovered(r) && !focused ? ToolTheme::hover.darker(0.7f) : ToolTheme::field);
    ctx.drawBorder(r, focused ? ToolTheme::accent : ToolTheme::border);

    const Rect inner{r.x + 6, r.y, std::max(0.0f, r.width - 12), r.height};
    const float cw = char_width(font_size);
    c->caret = std::min(c->caret, c->text.size());
    if (focused) {
        const float caret_x = static_cast<float>(c->caret) * cw;
        if (caret_x - c->scroll > inner.width - 2)
            c->scroll = caret_x - inner.width + 2;
        if (caret_x < c->scroll)
            c->scroll = caret_x;
    } else {
        c->scroll = 0;
    }

    const float ty = text_y(r.y, r.height, font_size);
    ctx.pushClip(inner);
    if (c->text.empty() && !focused) {
        drawText(ctx, {inner.x, ty}, hint, {.fontSize = font_size, .color = ToolTheme::textDim});
    } else {
        drawText(ctx, {inner.x - c->scroll, ty}, c->text,
                 {.fontSize = font_size, .color = ToolTheme::text});
    }
    if (focused) {
        const float x = inner.x + static_cast<float>(c->caret) * cw - c->scroll;
        ctx.drawRect({x - 1, ty - 2, 2, font_size + 4}, ToolTheme::accent);
    }
    ctx.popClip();

    ctx.addInteraction(r, {.onPress =
                               [c, x0 = inner.x - c->scroll, cw](Offset p) {
                                   const long idx = std::lround((p.x - x0) / cw);
                                   c->caret = static_cast<std::size_t>(
                                       std::clamp(idx, 0L, static_cast<long>(c->text.size())));
                               },
                           .focus = &c->focus});
    if (!focused)
        return;

    ctx.addKeyHandler(
        &c->focus,
        [c, on_changed](const KeyEvent& k) {
            auto changed = [&] {
                if (on_changed)
                    on_changed(c->text);
            };
            switch (k.key) {
                case Key::left:
                    if (c->caret > 0)
                        --c->caret;
                    return true;
                case Key::right:
                    if (c->caret < c->text.size())
                        ++c->caret;
                    return true;
                case Key::home:
                    c->caret = 0;
                    return true;
                case Key::end:
                    c->caret = c->text.size();
                    return true;
                case Key::backspace:
                    if (c->caret > 0) {
                        c->text.erase(--c->caret, 1);
                        changed();
                    }
                    return true;
                case Key::del:
                    if (c->caret < c->text.size()) {
                        c->text.erase(c->caret, 1);
                        changed();
                    }
                    return true;
                case Key::enter:
                case Key::tab:
                    c->focus.unfocus();  // commits through onBlur
                    return true;
                case Key::escape:
                    c->cancelled = true;
                    c->focus.unfocus();
                    return true;
                case Key::character:
                    return !k.ctrl;  // typed text arrives as text; let Ctrl shortcuts through
                default:
                    return false;
            }
        },
        [c, on_changed](const std::string& typed) {
            bool any = false;
            for (char ch : typed) {
                if (ch >= 0x20 && ch < 0x7F) {  // the bitmap font is ASCII
                    c->text.insert(c->caret++, 1, ch);
                    any = true;
                }
            }
            if (any && on_changed)
                on_changed(c->text);
        });
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
class PositionedWidget final : public Widget {
  public:
    explicit PositionedWidget(PositionedProps p) : p_(std::move(p)) {}

    /// Laid out by the Stack, which knows its own size.
    void layoutIn(Size stack) {
        auto axis = [](float start, float end, float extent, float total, float& pos, float& min_s,
                       float& max_s) {
            if (extent >= 0) {
                min_s = max_s = extent;
            } else if (start >= 0 && end >= 0) {
                min_s = max_s = std::max(0.0f, total - start - end);
            } else {
                min_s = 0;
                max_s = total;
            }
            pos = start >= 0 ? start : 0;
        };
        float min_w, max_w, min_h, max_h;
        axis(p_.left, p_.right, p_.width, stack.width, pos_.x, min_w, max_w);
        axis(p_.top, p_.bottom, p_.height, stack.height, pos_.y, min_h, max_h);
        size_ = p_.child ? p_.child->layout({min_w, max_w, min_h, max_h}) : Size{min_w, min_h};
        if (p_.left < 0 && p_.right >= 0)
            pos_.x = stack.width - p_.right - size_.width;
        if (p_.top < 0 && p_.bottom >= 0)
            pos_.y = stack.height - p_.bottom - size_.height;
    }

    Size layout(const BoxConstraints& c) override {
        layoutIn({finite_or(c.maxWidth, 0), finite_or(c.maxHeight, 0)});
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        if (p_.child)
            p_.child->paint(ctx, {o.x + pos_.x, o.y + pos_.y});
    }

  private:
    PositionedProps p_;
    Offset pos_;
};

class StackWidget final : public Widget {
  public:
    explicit StackWidget(StackProps p) : p_(std::move(p)) {}

    Size layout(const BoxConstraints& c) override {
        Size biggest{};
        for (auto& child : p_.children) {
            if (child && !dynamic_cast<PositionedWidget*>(child.get())) {
                const Size s = child->layout(c.loosen());
                biggest = {std::max(biggest.width, s.width), std::max(biggest.height, s.height)};
            }
        }
        size_ = c.constrain(
            {finite_or(c.maxWidth, biggest.width), finite_or(c.maxHeight, biggest.height)});
        for (auto& child : p_.children) {
            if (auto* positioned = dynamic_cast<PositionedWidget*>(child.get()))
                positioned->layoutIn(size_);
        }
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        for (auto& child : p_.children) {
            if (child)
                child->paint(ctx, o);
        }
    }

  private:
    StackProps p_;
};

class ScrollViewWidget final : public Widget {
  public:
    explicit ScrollViewWidget(ScrollViewProps p)
        : p_(std::move(p)),
          controller_(p_.controller ? p_.controller : std::make_shared<ScrollController>()) {}

    static constexpr float kBar = 8;

    Size layout(const BoxConstraints& c) override {
        size_ = c.constrain({finite_or(c.maxWidth, 300), finite_or(c.maxHeight, 300)});
        const float w = std::max(0.0f, size_.width - kBar);
        const Size content = p_.child ? p_.child->layout({w, w, 0, kInf}) : Size{};
        controller_->viewport = size_.height;
        controller_->extent = content.height;
        controller_->offset = std::clamp(controller_->offset, 0.0f, controller_->maxOffset());
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const Rect view{o.x, o.y, size_.width, size_.height};
        auto c = controller_;
        ctx.addInteraction(view, {.onScroll = [c](Offset d) { c->scrollBy(d.y * 48); }});
        ctx.pushClip(view);
        if (p_.child)
            p_.child->paint(ctx, {o.x, o.y - c->offset});
        ctx.popClip();

        if (c->maxOffset() <= 0)
            return;
        const float track_h = size_.height;
        const float thumb_h = std::max(24.0f, track_h * track_h / c->extent);
        const float thumb_y = (track_h - thumb_h) * (c->offset / c->maxOffset());
        const Rect thumb{o.x + size_.width - kBar + 2, o.y + thumb_y, kBar - 4, thumb_h};
        ctx.drawRect(thumb, ctx.hovered(thumb) ? ToolTheme::textDim : ToolTheme::hover);
        const float px_per_offset = (track_h - thumb_h) / c->maxOffset();
        ctx.addInteraction({thumb.x - 2, thumb.y, kBar, thumb.height},
                           {.onDrag = [c, px_per_offset](Offset, Offset d) {
                               c->scrollBy(d.y / std::max(px_per_offset, 0.0001f));
                           }});
    }

  private:
    ScrollViewProps p_;
    std::shared_ptr<ScrollController> controller_;
};

class SplitViewWidget final : public Widget {
  public:
    explicit SplitViewWidget(SplitViewProps p)
        : p_(std::move(p)),
          controller_(p_.controller ? p_.controller : std::make_shared<SplitController>()) {}

    static constexpr float kDivider = 4;

    Size layout(const BoxConstraints& c) override {
        size_ = c.constrain({finite_or(c.maxWidth, 800), finite_or(c.maxHeight, 600)});
        const float total = p_.vertical ? size_.height : size_.width;
        const float cross = p_.vertical ? size_.width : size_.height;
        const float max_fixed = std::max(0.0f, total - kDivider - controller_->minSize);
        const float fixed =
            std::clamp(controller_->size, std::min(controller_->minSize, max_fixed), max_fixed);
        first_ = controller_->fromEnd ? std::max(0.0f, total - kDivider - fixed) : fixed;
        const float second = std::max(0.0f, total - kDivider - first_);
        auto tight = [&](float main) {
            return p_.vertical ? BoxConstraints::tight({cross, main})
                               : BoxConstraints::tight({main, cross});
        };
        if (p_.first)
            p_.first->layout(tight(first_));
        if (p_.second)
            p_.second->layout(tight(second));
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const Offset second_o = p_.vertical ? Offset{o.x, o.y + first_ + kDivider}
                                            : Offset{o.x + first_ + kDivider, o.y};
        if (p_.first)
            p_.first->paint(ctx, o);
        if (p_.second)
            p_.second->paint(ctx, second_o);
        const Rect divider = p_.vertical ? Rect{o.x, o.y + first_, size_.width, kDivider}
                                         : Rect{o.x + first_, o.y, kDivider, size_.height};
        ctx.drawRect(divider, ctx.hovered(divider) ? ToolTheme::accent : ToolTheme::border);
        auto c = controller_;
        const bool vertical = p_.vertical;
        ctx.addInteraction(divider, {.onDrag = [c, vertical](Offset, Offset d) {
                               const float delta = vertical ? d.y : d.x;
                               c->size += c->fromEnd ? -delta : delta;
                               c->size = std::max(c->size, c->minSize);
                           }});
    }

  private:
    SplitViewProps p_;
    std::shared_ptr<SplitController> controller_;
    float first_ = 0;
};

/// Registers its region before painting its child, so the child's own targets stay on top.
class TapRegion final : public Widget {
  public:
    TapRegion(PointerHandlers handlers, WidgetPtr child, std::string semantics = {})
        : handlers_(std::move(handlers)),
          child_(std::move(child)),
          semantics_(std::move(semantics)) {}

    Size layout(const BoxConstraints& c) override {
        size_ = child_ ? child_->layout(c) : c.constrain({});
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const Rect r{o.x, o.y, size_.width, size_.height};
        ctx.addInteraction(r, handlers_);
        if (!semantics_.empty())
            ctx.addSemantics(semantics_, r);
        if (child_)
            child_->paint(ctx, o);
    }

  private:
    PointerHandlers handlers_;
    WidgetPtr child_;
    std::string semantics_;
};

/// Clips its child to its own bounds (long labels in narrow columns).
class ClipWidget final : public Widget {
  public:
    explicit ClipWidget(WidgetPtr child) : child_(std::move(child)) {}
    Size layout(const BoxConstraints& c) override {
        size_ = child_ ? c.constrain(child_->layout(c)) : c.constrain({});
        return size_;
    }
    void paint(PaintContext& ctx, Offset o) override {
        if (!child_)
            return;
        ctx.pushClip({o.x, o.y, size_.width, size_.height});
        child_->paint(ctx, o);
        ctx.popClip();
    }

  private:
    WidgetPtr child_;
};

// ---------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------
class ToolButtonWidget final : public Widget {
  public:
    explicit ToolButtonWidget(ToolButtonProps p) : p_(std::move(p)) {}

    Size layout(const BoxConstraints& c) override {
        const Size t = measureText(p_.label, p_.fontSize);
        size_ = c.constrain({t.width + 16, std::max(t.height + 8, ToolTheme::rowHeight - 2)});
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const Rect r{o.x, o.y, size_.width, size_.height};
        const bool hover = ctx.hovered(r) && p_.onPressed;
        Color bg = Colors::transparent;
        if (p_.selected)
            bg = ToolTheme::selection;
        if (hover)
            bg = ctx.pointerDown() ? ToolTheme::pressed : ToolTheme::hover;
        ctx.drawRect(r, bg);
        const Size t = measureText(p_.label, p_.fontSize);
        const Color fg = p_.onPressed ? p_.color : ToolTheme::textDim;
        drawText(ctx,
                 {std::floor(o.x + (size_.width - t.width) * 0.5f),
                  text_y(o.y, size_.height, p_.fontSize)},
                 p_.label, {.fontSize = p_.fontSize, .color = fg});
        if (p_.onPressed)
            ctx.addHitTarget(r, [cb = p_.onPressed](Offset) { cb(); });
        ctx.addSemantics(p_.label, r);
        if (!p_.tooltip.empty())
            ctx.addSemantics(p_.tooltip, r);
    }

  private:
    ToolButtonProps p_;
};

class CheckboxWidget final : public Widget {
  public:
    explicit CheckboxWidget(CheckboxProps p) : p_(std::move(p)) {}
    static constexpr float kBox = 16;

    Size layout(const BoxConstraints& c) override {
        const float label_w = p_.label.empty() ? 0 : measureText(p_.label, 14).width + 8;
        size_ = c.constrain({kBox + label_w + 2, ToolTheme::rowHeight - 2});
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const Rect r{o.x, o.y, size_.width, size_.height};
        const Rect box{o.x + 1, std::floor(o.y + (size_.height - kBox) * 0.5f), kBox, kBox};
        ctx.drawRect(box, ctx.hovered(r) ? ToolTheme::hover : ToolTheme::field);
        ctx.drawBorder(box, p_.value ? ToolTheme::accent : ToolTheme::textDim);
        if (p_.value)
            ctx.drawRect({box.x + 4, box.y + 4, kBox - 8, kBox - 8}, ToolTheme::accent);
        if (!p_.label.empty()) {
            drawText(ctx, {box.x + kBox + 8, text_y(o.y, size_.height, 14)}, p_.label,
                     {.fontSize = 14, .color = ToolTheme::text});
        }
        if (p_.onChanged) {
            ctx.addHitTarget(r, [cb = p_.onChanged, v = p_.value](Offset) { cb(!v); });
        }
        ctx.addSemantics(p_.label.empty() ? std::string(p_.value ? "[x]" : "[ ]") : p_.label, r);
    }

  private:
    CheckboxProps p_;
};

class TextFieldWidget final : public Widget {
  public:
    explicit TextFieldWidget(TextFieldProps p)
        : p_(std::move(p)),
          controller_(p_.controller ? p_.controller : std::make_shared<TextEditingController>()) {}

    Size layout(const BoxConstraints& c) override {
        const float w = p_.width >= 0 ? p_.width : finite_or(c.maxWidth, 160);
        size_ = c.constrain({w, p_.fontSize + 10});
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        auto c = controller_;
        c->focus.onBlur = [c, on_submitted = p_.onSubmitted] {
            if (!c->cancelled && on_submitted)
                on_submitted(c->text);
            c->cancelled = false;
        };
        const Rect r{o.x, o.y, size_.width, size_.height};
        paint_editable(ctx, r, c, p_.fontSize, p_.hint, p_.onChanged);
        if (!p_.hint.empty())
            ctx.addSemantics(p_.hint, r);
        if (!c->text.empty())
            ctx.addSemantics(c->text, r);
    }

  private:
    TextFieldProps p_;
    std::shared_ptr<TextEditingController> controller_;
};

class NumberFieldWidget final : public Widget {
  public:
    explicit NumberFieldWidget(NumberFieldProps p) : p_(std::move(p)) {}

    Size layout(const BoxConstraints& c) override {
        size_ = c.constrain({finite_or(c.maxWidth, 80), ToolTheme::fontSize + 10});
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const Rect r{o.x, o.y, size_.width, size_.height};
        const std::string text = format_number(p_.value, p_.decimals, p_.integer);
        auto c = p_.controller;
        if (c && c->focus.hasFocus()) {
            c->focus.onBlur = [c, cb = p_.onChanged, integer = p_.integer] {
                if (!c->cancelled && cb) {
                    char* end = nullptr;
                    const double v = std::strtod(c->text.c_str(), &end);
                    if (end != c->text.c_str())
                        cb(integer ? std::round(v) : v);
                }
                c->cancelled = false;
            };
            paint_editable(ctx, r, c, ToolTheme::fontSize, {}, {});
            ctx.addSemantics(text, r);
            return;
        }

        const bool hover = ctx.hovered(r);
        ctx.drawRect(r, hover ? ToolTheme::hover.darker(0.7f) : ToolTheme::field);
        ctx.drawBorder(r, ToolTheme::border);
        const float ty = text_y(o.y, size_.height, ToolTheme::fontSize);
        float x = o.x + 6;
        ctx.pushClip(r);
        if (!p_.label.empty()) {
            drawText(ctx, {x, ty}, p_.label,
                     {.fontSize = ToolTheme::fontSize, .color = p_.labelColor});
            x += measureText(p_.label, ToolTheme::fontSize).width + 6;
        }
        drawText(ctx, {x, ty}, text, {.fontSize = ToolTheme::fontSize, .color = ToolTheme::text});
        ctx.popClip();
        ctx.addSemantics(text, r);

        auto accumulated = std::make_shared<double>(p_.value);
        PointerHandlers handlers;
        if (p_.onChanged) {
            handlers.onDrag = [accumulated, cb = p_.onChanged, step = p_.step,
                               integer = p_.integer](Offset, Offset d) {
                *accumulated += d.x * step;
                cb(integer ? std::round(*accumulated) : *accumulated);
            };
        }
        if (c) {
            handlers.onTap = [c, text](Offset) {
                c->setText(text);
                c->cancelled = false;
                c->focus.requestFocus();
            };
        }
        ctx.addInteraction(r, std::move(handlers));
    }

  private:
    NumberFieldProps p_;
};

class DropdownWidget final : public Widget {
  public:
    explicit DropdownWidget(DropdownProps p)
        : p_(std::move(p)), state_(p_.state ? p_.state : std::make_shared<PopupState>()) {}

    Size layout(const BoxConstraints& c) override {
        size_ = c.constrain({finite_or(c.maxWidth, 140), ToolTheme::fontSize + 10});
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const Rect r{o.x, o.y, size_.width, size_.height};
        ctx.drawRect(r, ctx.hovered(r) ? ToolTheme::hover : ToolTheme::header);
        ctx.drawBorder(r, ToolTheme::border);
        const float ty = text_y(o.y, size_.height, ToolTheme::fontSize);
        const std::string current =
            p_.selected >= 0 && p_.selected < static_cast<int>(p_.items.size())
                ? p_.items[static_cast<std::size_t>(p_.selected)]
                : std::string();
        ctx.pushClip({r.x, r.y, std::max(0.0f, r.width - 18), r.height});
        drawText(ctx, {o.x + 6, ty}, current, {.color = ToolTheme::text});
        ctx.popClip();
        drawText(ctx, {o.x + size_.width - 14, ty}, "v",
                 {.fontSize = ToolTheme::fontSize, .color = ToolTheme::textDim});
        ctx.addSemantics(current, r);
        auto state = state_;
        ctx.addHitTarget(r, [state](Offset) { state->open = state->open >= 0 ? -1 : 0; });
        if (state->open < 0)
            return;

        ctx.addOverlay([state, r, items = p_.items, selected = p_.selected,
                        cb = p_.onChanged](PaintContext& overlay) {
            overlay.addInteraction(kEverywhere, {.onPress = [state](Offset) { state->open = -1; }});
            const float row = ToolTheme::rowHeight;
            float w = r.width;
            for (const auto& item : items)
                w = std::max(w, measureText(item, ToolTheme::fontSize).width + 20);
            const Rect list{r.x, r.y + r.height, w, row * static_cast<float>(items.size())};
            overlay.drawRect({list.x + 3, list.y + 3, list.width, list.height}, Color{0x00000060u});
            overlay.drawRect(list, ToolTheme::header);
            overlay.drawBorder(list, ToolTheme::border);
            for (std::size_t i = 0; i < items.size(); ++i) {
                const Rect item{list.x, list.y + row * static_cast<float>(i), list.width, row};
                const int index = static_cast<int>(i);
                if (overlay.hovered(item))
                    overlay.drawRect(item, ToolTheme::hover);
                else if (index == selected)
                    overlay.drawRect(item, ToolTheme::selection);
                drawText(overlay, {item.x + 8, text_y(item.y, row, ToolTheme::fontSize)}, items[i],
                         {.color = ToolTheme::text});
                overlay.addSemantics(items[i], item);
                overlay.addHitTarget(item, [state, cb, index](Offset) {
                    state->open = -1;
                    if (cb)
                        cb(index);
                });
            }
        });
    }

  private:
    DropdownProps p_;
    std::shared_ptr<PopupState> state_;
};

class MenuBarWidget final : public Widget {
  public:
    explicit MenuBarWidget(MenuBarProps p)
        : p_(std::move(p)), state_(p_.state ? p_.state : std::make_shared<PopupState>()) {}

    static constexpr float kHeight = 28;

    Size layout(const BoxConstraints& c) override {
        size_ = c.constrain({finite_or(c.maxWidth, 800), kHeight});
        for (auto& w : p_.trailing) {
            if (w)
                w->layout({0, size_.width, 0, kHeight});
        }
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const Rect bar{o.x, o.y, size_.width, size_.height};
        ctx.drawRect(bar, ToolTheme::header);
        ctx.addInteraction(bar, {});  // the bar itself is opaque
        auto state = state_;
        float x = o.x + 4;
        for (std::size_t i = 0; i < p_.menus.size(); ++i) {
            const Menu& menu = p_.menus[i];
            const float w = measureText(menu.title, ToolTheme::fontSize).width + 20;
            const Rect title{x, o.y + 2, w, kHeight - 4};
            const int index = static_cast<int>(i);
            if (state->open >= 0 && state->open != index && ctx.hovered(title))
                state->open = index;  // slide between open menus
            const bool open = state->open == index;
            if (open || ctx.hovered(title))
                ctx.drawRect(title, open ? ToolTheme::selection : ToolTheme::hover);
            drawText(ctx, {x + 10, text_y(title.y, title.height, ToolTheme::fontSize)}, menu.title,
                     {.color = ToolTheme::text});
            ctx.addSemantics(menu.title, title);
            ctx.addHitTarget(
                title, [state, index](Offset) { state->open = state->open == index ? -1 : index; });
            if (open) {
                ctx.addOverlay([state, menu, title, bar_bottom = o.y + kHeight](PaintContext& ov) {
                    paintMenu(ov, state, menu, {title.x, bar_bottom});
                });
            }
            x += w;
        }

        // Trailing widgets (the toolbar), right-aligned.
        float tx = o.x + size_.width - 6;
        for (auto it = p_.trailing.rbegin(); it != p_.trailing.rend(); ++it) {
            if (!*it)
                continue;
            const Size s = (*it)->size();
            tx -= s.width;
            (*it)->paint(ctx, {tx, o.y + (kHeight - s.height) * 0.5f});
            tx -= 4;
        }
    }

  private:
    static void paintMenu(PaintContext& ov, const std::shared_ptr<PopupState>& state,
                          const Menu& menu, Offset at) {
        // Clicking anywhere below the bar closes the menu.
        ov.addInteraction({-1e9f, at.y, 2e9f, 2e9f},
                          {.onPress = [state](Offset) { state->open = -1; }});
        const float row = ToolTheme::rowHeight;
        float w = 160;
        for (const MenuItem& item : menu.items) {
            const float label = measureText(item.label, ToolTheme::fontSize).width;
            const float shortcut = item.shortcut.empty()
                                       ? 0.0f
                                       : measureText(item.shortcut, ToolTheme::fontSize).width + 24;
            w = std::max(w, label + shortcut + 24);
        }
        const Rect list{at.x, at.y, w, row * static_cast<float>(menu.items.size()) + 4};
        ov.drawRect({list.x + 3, list.y + 3, list.width, list.height}, Color{0x00000060u});
        ov.drawRect(list, ToolTheme::header);
        ov.drawBorder(list, ToolTheme::border);
        for (std::size_t i = 0; i < menu.items.size(); ++i) {
            const MenuItem& item = menu.items[i];
            const Rect r{list.x + 2, list.y + 2 + row * static_cast<float>(i), list.width - 4, row};
            const bool enabled = item.enabled && item.onSelected;
            if (enabled && ov.hovered(r))
                ov.drawRect(r, ToolTheme::hover);
            const float ty = text_y(r.y, row, ToolTheme::fontSize);
            drawText(ov, {r.x + 10, ty}, item.label,
                     {.color = enabled ? ToolTheme::text : ToolTheme::textDim});
            if (!item.shortcut.empty()) {
                const float sw = measureText(item.shortcut, ToolTheme::fontSize).width;
                drawText(ov, {r.x + r.width - sw - 10, ty}, item.shortcut,
                         {.color = ToolTheme::textDim});
            }
            ov.addSemantics(item.label, r);
            ov.addInteraction(r, {.onTap = [state, cb = item.onSelected, enabled](Offset) {
                                  state->open = -1;
                                  if (enabled)
                                      cb();
                              }});
        }
    }

    MenuBarProps p_;
    std::shared_ptr<PopupState> state_;
};

class TreeViewWidget final : public Widget {
  public:
    explicit TreeViewWidget(TreeViewProps p)
        : p_(std::move(p)), state_(p_.state ? p_.state : std::make_shared<TreeState>()) {
        for (const TreeItem& root : p_.roots)
            flatten(root, 0);
    }

    Size layout(const BoxConstraints& c) override {
        float widest = 0;
        if (!std::isfinite(c.maxWidth)) {
            for (const Row& row : rows_) {
                widest = std::max(widest, 20 + 16 * static_cast<float>(row.depth) +
                                              measureText(row.item->label, 14).width);
            }
        }
        size_ = c.constrain(
            {finite_or(c.maxWidth, widest), p_.rowHeight * static_cast<float>(rows_.size())});
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        const Rect clip = ctx.clip();
        auto state = state_;
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            const Row& row = rows_[i];
            const Rect r{o.x, o.y + p_.rowHeight * static_cast<float>(i), size_.width,
                         p_.rowHeight};
            if (r.y + r.height < clip.y || r.y > clip.y + clip.height)
                continue;  // off screen
            const TreeItem& item = *row.item;
            const bool selected = state->selected == item.id;
            if (selected)
                ctx.drawRect(r, ToolTheme::selection);
            else if (ctx.hovered(r))
                ctx.drawRect(r, ToolTheme::hover);
            const float indent = 4 + 16 * static_cast<float>(row.depth);
            const float ty = text_y(r.y, r.height, 14);
            ctx.addInteraction(r, {.onTap = [state, id = item.id, cb = p_.onSelected](Offset) {
                                   state->selected = id;
                                   if (cb)
                                       cb(id);
                               }});
            if (!item.children.empty()) {
                const bool collapsed = state->collapsed.count(item.id) != 0;
                drawText(ctx, {r.x + indent, ty}, collapsed ? ">" : "v",
                         {.color = ToolTheme::textDim});
                ctx.addInteraction({r.x + indent - 4, r.y, 18, r.height},
                                   {.onTap = [state, id = item.id](Offset) {
                                       if (!state->collapsed.erase(id))
                                           state->collapsed.insert(id);
                                   }});
            }
            drawText(ctx, {r.x + indent + 16, ty}, item.label, {.color = item.color});
            ctx.addSemantics(item.label, r);
        }
    }

  private:
    struct Row {
        const TreeItem* item;
        int depth;
    };

    void flatten(const TreeItem& item, int depth) {
        rows_.push_back({&item, depth});
        if (state_->collapsed.count(item.id))
            return;
        for (const TreeItem& child : item.children)
            flatten(child, depth + 1);
    }

    TreeViewProps p_;
    std::shared_ptr<TreeState> state_;
    std::vector<Row> rows_;
};

class TabViewWidget final : public Widget {
  public:
    explicit TabViewWidget(TabViewProps p)
        : p_(std::move(p)), index_(p_.index ? p_.index : std::make_shared<int>(0)) {}

    static constexpr float kHeader = 26;

    Size layout(const BoxConstraints& c) override {
        size_ = c.constrain({finite_or(c.maxWidth, 400), finite_or(c.maxHeight, 300)});
        *index_ = std::clamp(*index_, 0, std::max(0, static_cast<int>(p_.tabs.size()) - 1));
        if (WidgetPtr body = current())
            body->layout(
                BoxConstraints::tight({size_.width, std::max(0.0f, size_.height - kHeader)}));
        return size_;
    }

    void paint(PaintContext& ctx, Offset o) override {
        ctx.drawRect({o.x, o.y, size_.width, kHeader}, ToolTheme::header);
        float x = o.x + 4;
        auto index = index_;
        for (std::size_t i = 0; i < p_.tabs.size(); ++i) {
            const std::string& title = p_.tabs[i].title;
            const float w = measureText(title, ToolTheme::fontSize).width + 20;
            const Rect r{x, o.y + 3, w, kHeader - 3};
            const int tab = static_cast<int>(i);
            const bool active = *index == tab;
            if (active)
                ctx.drawRect(r, ToolTheme::panel);
            else if (ctx.hovered(r))
                ctx.drawRect(r, ToolTheme::hover);
            if (active)
                ctx.drawRect({r.x, r.y, r.width, 2}, ToolTheme::accent);
            drawText(ctx, {x + 10, text_y(r.y, r.height, ToolTheme::fontSize)}, title,
                     {.color = active ? ToolTheme::text : ToolTheme::textDim});
            ctx.addSemantics(title, r);
            ctx.addHitTarget(r, [index, tab](Offset) { *index = tab; });
            x += w + 2;
        }
        if (WidgetPtr body = current())
            body->paint(ctx, {o.x, o.y + kHeader});
    }

  private:
    WidgetPtr current() const {
        if (*index_ < 0 || *index_ >= static_cast<int>(p_.tabs.size()))
            return nullptr;
        return p_.tabs[static_cast<std::size_t>(*index_)].child;
    }

    TabViewProps p_;
    std::shared_ptr<int> index_;
};

}  // namespace

// ---------------------------------------------------------------------------
// Controllers
// ---------------------------------------------------------------------------
void ScrollController::scrollBy(float dy) {
    offset = std::clamp(offset + dy, 0.0f, maxOffset());
}

// ---------------------------------------------------------------------------
// Factories
// ---------------------------------------------------------------------------
WidgetPtr Stack(StackProps props) {
    return std::make_shared<StackWidget>(std::move(props));
}

WidgetPtr Positioned(PositionedProps props) {
    return std::make_shared<PositionedWidget>(std::move(props));
}

WidgetPtr ScrollView(ScrollViewProps props) {
    return std::make_shared<ScrollViewWidget>(std::move(props));
}

WidgetPtr SplitView(SplitViewProps props) {
    return std::make_shared<SplitViewWidget>(std::move(props));
}

WidgetPtr InputBlocker(InputBlockerProps props) {
    return std::make_shared<TapRegion>(PointerHandlers{}, std::move(props.child));
}

WidgetPtr ToolButton(ToolButtonProps props) {
    return std::make_shared<ToolButtonWidget>(std::move(props));
}

WidgetPtr Checkbox(CheckboxProps props) {
    return std::make_shared<CheckboxWidget>(std::move(props));
}

WidgetPtr TextField(TextFieldProps props) {
    return std::make_shared<TextFieldWidget>(std::move(props));
}

WidgetPtr NumberField(NumberFieldProps props) {
    return std::make_shared<NumberFieldWidget>(std::move(props));
}

WidgetPtr Dropdown(DropdownProps props) {
    return std::make_shared<DropdownWidget>(std::move(props));
}

WidgetPtr MenuBar(MenuBarProps props) {
    return std::make_shared<MenuBarWidget>(std::move(props));
}

WidgetPtr TreeView(TreeViewProps props) {
    return std::make_shared<TreeViewWidget>(std::move(props));
}

WidgetPtr TabView(TabViewProps props) {
    return std::make_shared<TabViewWidget>(std::move(props));
}

WidgetPtr Label(std::string text, Color color, float font_size) {
    return Text(std::move(text), {.fontSize = font_size, .color = color});
}

WidgetPtr Panel(PanelProps props) {
    std::vector<WidgetPtr> header{Label(props.title, ToolTheme::textDim),
                                  Expanded({.child = SizedBox({})})};
    for (auto& action : props.actions)
        header.push_back(std::move(action));
    return InputBlocker({.child = Column({
                             .crossAxisAlignment = CrossAxisAlignment::stretch,
                             .children =
                                 {
                                     Container({
                                         .height = 28,
                                         .color = ToolTheme::header,
                                         .padding = EdgeInsets{10, 0, 4, 0},
                                         .child = Row({.children = std::move(header)}),
                                     }),
                                     Expanded({.child = Container({
                                                   .color = props.background,
                                                   .child = std::move(props.child),
                                               })}),
                                 },
                         })});
}

WidgetPtr Section(SectionProps props) {
    std::vector<WidgetPtr> header{
        Label(props.expanded ? "v" : ">", ToolTheme::textDim),
        Label(props.title, ToolTheme::text),
        Expanded({.child = SizedBox({})}),
    };
    for (auto& action : props.actions)
        header.push_back(std::move(action));
    PointerHandlers toggle;
    if (props.onToggle)
        toggle.onTap = [cb = props.onToggle](Offset) { cb(); };
    std::vector<WidgetPtr> children{std::make_shared<TapRegion>(
        std::move(toggle), Container({
                               .height = ToolTheme::rowHeight + 2,
                               .color = ToolTheme::header,
                               .padding = EdgeInsets{8, 0, 4, 0},
                               .child = Row({.spacing = 8, .children = std::move(header)}),
                           }))};
    if (props.expanded && props.child) {
        children.push_back(Padding({.padding = EdgeInsets{10, 4, 6, 6}, .child = props.child}));
    }
    return Column(
        {.crossAxisAlignment = CrossAxisAlignment::stretch, .children = std::move(children)});
}

WidgetPtr PropertyRow(PropertyRowProps props) {
    return Padding({
        .padding = EdgeInsets{0, 1, 0, 1},
        .child = Row({
            .children =
                {
                    SizedBox({.width = props.labelWidth - 6,
                              .child = std::make_shared<ClipWidget>(
                                  Label(props.label, ToolTheme::textDim))}),
                    SizedBox({.width = 6}),
                    Expanded({.child = std::move(props.editor)}),
                },
        }),
    });
}

}  // namespace phoenix
