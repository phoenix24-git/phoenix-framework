// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/widgets.h"

#include <doctest/doctest.h>

#include <memory>
#include <vector>

using namespace phoenix;

namespace {

class Counter : public StatefulWidget {
  public:
    int taps = 0;
    WidgetPtr build() override {
        return Scaffold({
            .appBar = AppBar({.title = "Test"}),
            .body = Center({.child = Text(std::to_string(taps), {.fontSize = 14})}),
            .floatingActionButton = FloatingActionButton({
                .onPressed = [this] { setState([this] { ++taps; }); },
                .child = Text("+"),
            }),
        });
    }
};

std::vector<PhxDrawRect> frame(WidgetsBinding& b, Size vp, Offset p = {}, bool down = false,
                               bool tap = false) {
    std::vector<PhxDrawRect> out;
    b.drawFrame(vp, p, down, tap, out);
    return out;
}

}  // namespace

TEST_SUITE("Widgets") {
    TEST_CASE("BoxConstraints constrain and deflate") {
        BoxConstraints c{10, 100, 20, 200};
        Size s = c.constrain({500, 5});
        CHECK(s.width == 100);
        CHECK(s.height == 20);
        BoxConstraints d = c.deflate(EdgeInsets::all(10));
        CHECK(d.maxWidth == 80);
        CHECK(d.minHeight == 0);
    }

    TEST_CASE("text measures with the 5x7 font") {
        Size s = measureText("ab", 14);
        CHECK(s.height == 14);
        CHECK(s.width == doctest::Approx((2 * 6 - 1) * 2.0f));
    }

    TEST_CASE("Column with Expanded fills the remaining space") {
        auto fixed = Container({.height = 40});
        auto flex = Expanded({.child = Container({.color = Colors::indigo})});
        auto col = Column({.children = {fixed, flex}});
        col->layout(BoxConstraints::tight({300, 200}));
        CHECK(col->size().height == 200);
        CHECK(flex->size().height == doctest::Approx(160));
        CHECK(flex->size().width <= 300);
    }

    TEST_CASE("Row spaceBetween places children at both ends") {
        std::vector<PhxDrawRect> out;
        auto a = Container({.width = 10, .height = 10, .color = Colors::black});
        auto b = Container({.width = 10, .height = 10, .color = Colors::white});
        auto row = Row({.mainAxisAlignment = MainAxisAlignment::spaceBetween, .children = {a, b}});
        row->layout(BoxConstraints::tight({100, 10}));
        PaintContext ctx(out, {}, false);
        row->paint(ctx, {});
        REQUIRE(out.size() == 2);
        CHECK(out[0].x == 0);
        CHECK(out[1].x == doctest::Approx(90));
    }

    TEST_CASE("Scaffold + FAB: tapping the button triggers setState") {
        auto counter = std::make_shared<Counter>();
        auto& binding = WidgetsBinding::instance();
        runApp(counter);
        const Size vp{400, 300};
        auto first = frame(binding, vp);
        CHECK_FALSE(first.empty());
        // FAB is 56x56 at 16px from the bottom-right corner.
        const Offset fab{400 - 16 - 28, 300 - 16 - 28};
        frame(binding, vp, fab, /*down=*/true);
        frame(binding, vp, fab, false, /*tap=*/true);
        CHECK(counter->taps == 1);
        frame(binding, vp, {5, 5}, false, /*tap=*/true);  // tap on the app bar: no-op
        CHECK(counter->taps == 1);
        binding.setRoot(nullptr);
    }

    TEST_CASE("clipping trims axis-aligned rects and culls rotated ones") {
        std::vector<PhxDrawRect> out;
        PaintContext ctx(out, {}, false);
        ctx.pushClip({0, 0, 100, 100});
        ctx.drawRect({50, 50, 100, 100}, Colors::black);       // trimmed to 50x50
        ctx.drawRect({200, 200, 10, 10}, Colors::black);       // fully outside: dropped
        ctx.drawRect({140, 10, 10, 10}, Colors::black, 0.5f);  // rotated, centre outside
        ctx.popClip();
        ctx.drawRect({200, 200, 10, 10}, Colors::black);  // no clip any more
        REQUIRE(out.size() == 2);
        CHECK(out[0].w == 50);
        CHECK(out[0].h == 50);
        CHECK(out[1].x == 200);
    }

    TEST_CASE("Color helpers") {
        CHECK(Color::rgb(0x112233).rgba == 0x112233FFu);
        CHECK((Color{0xFFFFFFFFu}.darker(0.5f).rgba & 0xFFu) == 0xFFu);  // alpha preserved
    }
}
