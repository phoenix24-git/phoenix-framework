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
    ~WidgetTester() { WidgetsBinding::instance().setRoot(nullptr); }

    void pumpWidget(WidgetPtr root) {
        runApp(std::move(root));
        pump();
    }

    /// Lays out and paints one frame.
    void pump() { frame({-1, -1}, false, false); }

    /// Taps the centre of the first semantics node with `label` (text or tooltip).
    /// Returns false if nothing matched.
    bool tap(const std::string& label) {
        for (const auto& node : WidgetsBinding::instance().semantics()) {
            if (node.label == label) {
                tapAt(
                    {node.rect.x + node.rect.width * 0.5f, node.rect.y + node.rect.height * 0.5f});
                return true;
            }
        }
        return false;
    }

    void tapAt(Offset p) {
        frame(p, true, false);
        frame(p, false, true);
    }

    /// Number of semantics nodes whose label equals `label`.
    std::size_t findText(const std::string& label) const {
        std::size_t n = 0;
        for (const auto& node : WidgetsBinding::instance().semantics())
            n += node.label == label;
        return n;
    }

    const std::vector<PhxDrawRect>& lastFrame() const { return rects_; }

  private:
    void frame(Offset p, bool down, bool tap) {
        rects_.clear();
        WidgetsBinding::instance().drawFrame(surface_, p, down, tap, rects_);
    }

    Size surface_;
    std::vector<PhxDrawRect> rects_;
};

}  // namespace phoenix
