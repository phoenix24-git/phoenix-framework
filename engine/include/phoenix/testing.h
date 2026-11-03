// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// phoenix_test: widget testing helpers in the spirit of flutter_test.
//
//   TEST_CASE("Counter increments smoke test") {
//       phoenix::WidgetTester tester;
//       tester.pumpWidget(std::make_shared<MyHomePage>());
//       CHECK(tester.findText("0") == 1);
//       tester.tap("Increment");
//       tester.pump();
//       CHECK(tester.findText("1") == 1);
//   }
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "phoenix/widgets.h"

namespace phoenix {

class WidgetTester {
  public:
    explicit WidgetTester(Size surface = {800, 600}) : surface_(surface) {}
    ~WidgetTester() {
        WidgetsBinding::instance().setRoot(nullptr);
        WidgetsBinding::instance().setOverlayRoot(nullptr);
        WidgetsBinding::instance().setFocus(nullptr);
    }

    void pumpWidget(WidgetPtr root) {
        runApp(std::move(root));
        pump();
    }

    /// Lays out and paints one frame (the pointer stays where it was).
    void pump() { send({}); }

    /// Taps the centre of the first semantics node with `label` (text or tooltip).
    /// Returns false if nothing matched.
    bool tap(const std::string& label) {
        const Rect* r = find(label);
        if (!r)
            return false;
        tapAt(centre(*r));
        return true;
    }

    void tapAt(Offset p) {
        pointer_ = p;
        send({.pressed = true, .down = true});
        send({.released = true});
    }

    /// Presses at `from`, moves to `to` in `steps` frames, and releases there.
    void drag(Offset from, Offset to, int steps = 4) {
        pointer_ = from;
        send({.pressed = true, .down = true});
        for (int i = 1; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            pointer_ = {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t};
            send({.down = true});
        }
        send({.released = true});
    }

    /// Moves the pointer without pressing (hover).
    void hoverAt(Offset p) {
        pointer_ = p;
        send({});
    }

    /// Turns the wheel by `lines` (+ = down) with the pointer at `at`.
    void scroll(Offset at, float lines) {
        pointer_ = at;
        send({.scroll = lines});
    }

    /// Types `text` into whatever has keyboard focus.
    void enterText(const std::string& text) { send({.text = text}); }

    void sendKey(KeyEvent key) { send({.keys = {key}}); }
    void sendKey(Key key) { sendKey(KeyEvent{.key = key}); }

    /// Number of semantics nodes whose label equals `label`.
    std::size_t findText(const std::string& label) const {
        std::size_t n = 0;
        for (const auto& node : WidgetsBinding::instance().semantics())
            n += node.label == label;
        return n;
    }

    /// Rect of the first semantics node with `label`, or nullptr.
    const Rect* find(const std::string& label) const {
        for (const auto& node : WidgetsBinding::instance().semantics()) {
            if (node.label == label)
                return &node.rect;
        }
        return nullptr;
    }

    static Offset centre(const Rect& r) { return {r.x + r.width * 0.5f, r.y + r.height * 0.5f}; }

    const std::vector<PhxDrawRect>& lastFrame() const { return rects_; }

  private:
    struct Step {
        bool pressed = false;
        bool released = false;
        bool down = false;
        float scroll = 0;
        std::string text{};
        std::vector<KeyEvent> keys{};
    };

    void send(Step step) {
        FrameInput input;
        input.pointer = pointer_;
        input.pointerDown = step.down;
        input.pressed = step.pressed;
        input.released = step.released;
        input.scrollY = step.scroll;
        input.text = std::move(step.text);
        input.keys = std::move(step.keys);
        rects_.clear();
        WidgetsBinding::instance().drawFrame(surface_, input, rects_);
    }

    Size surface_;
    Offset pointer_{-1, -1};
    std::vector<PhxDrawRect> rects_;
};

}  // namespace phoenix
