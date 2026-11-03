// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/tool_widgets.h"

#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "phoenix/testing.h"

using namespace phoenix;

namespace {

/// Rebuilds `make()` every frame (the way the editor does).
class Live : public Widget {
  public:
    explicit Live(std::function<WidgetPtr()> make) : make_(std::move(make)) {}
    Size layout(const BoxConstraints& c) override {
        child_ = make_();
        size_ = child_->layout(c);
        return size_;
    }
    void paint(PaintContext& ctx, Offset o) override { child_->paint(ctx, o); }

  private:
    std::function<WidgetPtr()> make_;
    WidgetPtr child_;
};

WidgetPtr live(std::function<WidgetPtr()> make) {
    return std::make_shared<Live>(std::move(make));
}

}  // namespace

TEST_SUITE("Input") {
    TEST_CASE("press, drag and release reach the region that was pressed") {
        WidgetTester tester;
        Offset total{};
        int taps = 0, ends = 0;
        tester.pumpWidget(live([&] {
            return Center({.child = std::make_shared<Live>(
                               [&] { return SizedBox({.width = 100, .height = 100}); })});
        }));
        // Register a region by hand through a tiny custom widget.
        struct Region : Widget {
            PointerHandlers h;
            Size layout(const BoxConstraints& c) override {
                size_ = c.constrain({100, 100});
                return size_;
            }
            void paint(PaintContext& ctx, Offset o) override {
                ctx.addInteraction({o.x, o.y, 100, 100}, h);
            }
        };
        auto region = std::make_shared<Region>();
        region->h.onDrag = [&](Offset, Offset d) {
            total.x += d.x;
            total.y += d.y;
        };
        region->h.onTap = [&](Offset) { ++taps; };
        region->h.onDragEnd = [&](Offset) { ++ends; };
        tester.pumpWidget(region);

        tester.drag({50, 50}, {250, 90});  // leaves the region: the drag keeps going
        CHECK(total.x == doctest::Approx(200));
        CHECK(total.y == doctest::Approx(40));
        CHECK(ends == 1);
        CHECK(taps == 0);  // a drag is not a tap

        tester.tapAt({10, 10});
        CHECK(taps == 1);
        tester.drag({10, 10}, {11, 11});  // within the drag slop: still a tap
        CHECK(taps == 2);
    }

    TEST_CASE("the overlay root is on top, and input falls through where it has no regions") {
        WidgetTester tester;
        int game = 0, editor = 0;
        tester.pumpWidget(GestureDetector(
            {.onTap = [&] { ++game; }, .child = Container({.color = Colors::black})}));
        WidgetsBinding::instance().setOverlayRoot(
            Stack({.children = {Positioned({
                       .left = 0,
                       .top = 0,
                       .width = 100,
                       .height = 600,
                       .child = ToolButton({.label = "Tool", .onPressed = [&] { ++editor; }}),
                   })}}));
        tester.pump();
        REQUIRE(tester.tap("Tool"));
        CHECK(editor == 1);
        CHECK(game == 0);
        CHECK(WidgetsBinding::instance().pointerOverOverlay());
        tester.tapAt({500, 300});
        CHECK(game == 1);
        CHECK_FALSE(WidgetsBinding::instance().pointerOverOverlay());
    }

    TEST_CASE("shortcut handlers see keys nobody focused consumed") {
        WidgetTester tester;
        int saves = 0;
        struct Shortcuts : Widget {
            int* saves;
            Size layout(const BoxConstraints& c) override {
                size_ = c.constrain({});
                return size_;
            }
            void paint(PaintContext& ctx, Offset) override {
                ctx.addKeyHandler(nullptr, [s = saves](const KeyEvent& k) {
                    if (!k.isShortcut('s'))
                        return false;
                    ++*s;
                    return true;
                });
            }
        };
        auto w = std::make_shared<Shortcuts>();
        w->saves = &saves;
        tester.pumpWidget(w);
        tester.sendKey({.key = Key::character, .ch = 's', .ctrl = true});
        tester.sendKey({.key = Key::character, .ch = 's'});
        CHECK(saves == 1);
    }
}

TEST_SUITE("Tool widgets") {
    TEST_CASE("TextField: focus, type, edit with keys, submit on Enter") {
        WidgetTester tester;
        auto controller = std::make_shared<TextEditingController>();
        std::string submitted;
        tester.pumpWidget(live([&] {
            return Center(
                {.child = TextField({.controller = controller,
                                     .hint = "Name",
                                     .onSubmitted = [&](const std::string& s) { submitted = s; },
                                     .width = 200})});
        }));
        REQUIRE(tester.tap("Name"));
        CHECK(controller->focus.hasFocus());
        CHECK(WidgetsBinding::instance().wantsTextInput());
        tester.enterText("Plaeyr");
        CHECK(controller->text == "Plaeyr");
        // Fix the typo: step left over "r", delete "ey", type "ye".
        tester.sendKey(Key::left);
        tester.sendKey(Key::backspace);
        tester.sendKey(Key::backspace);
        tester.enterText("ye");
        CHECK(controller->text == "Player");
        tester.sendKey(Key::enter);
        CHECK(submitted == "Player");
        CHECK_FALSE(controller->focus.hasFocus());
    }

    TEST_CASE("TextField: Escape cancels, clicking elsewhere commits") {
        WidgetTester tester;
        auto controller = std::make_shared<TextEditingController>();
        int submits = 0;
        tester.pumpWidget(live([&] {
            return Column(
                {.children = {TextField({.controller = controller,
                                         .hint = "Tag",
                                         .onSubmitted = [&](const std::string&) { ++submits; },
                                         .width = 200})}});
        }));
        tester.tap("Tag");
        tester.enterText("x");
        tester.sendKey(Key::escape);
        CHECK(submits == 0);
        tester.tap("x");
        tester.enterText("y");
        tester.tapAt({700, 500});  // empty space: focus is cleared and the edit commits
        CHECK(submits == 1);
        CHECK_FALSE(controller->focus.hasFocus());
    }

    TEST_CASE("NumberField: drag scrubs, click types a value") {
        WidgetTester tester;
        double value = 1.0;
        auto controller = std::make_shared<TextEditingController>();
        tester.pumpWidget(live([&] {
            return Column({.crossAxisAlignment = CrossAxisAlignment::stretch,
                           .children = {NumberField({.value = value,
                                                     .onChanged = [&](double v) { value = v; },
                                                     .controller = controller,
                                                     .step = 0.5})}});
        }));
        const Rect* r = tester.find("1");
        REQUIRE(r);
        const Offset c = WidgetTester::centre(*r);
        tester.drag(c, {c.x + 20, c.y});
        CHECK(value == doctest::Approx(11.0));

        tester.tap("11");
        CHECK(controller->focus.hasFocus());
        CHECK(controller->text == "11");
        tester.sendKey(Key::backspace);
        tester.sendKey(Key::backspace);
        tester.enterText("-2.25");
        tester.sendKey(Key::enter);
        CHECK(value == doctest::Approx(-2.25));
    }

    TEST_CASE("Checkbox toggles") {
        WidgetTester tester;
        bool on = false;
        tester.pumpWidget(live([&] {
            return Center(
                {.child = Checkbox(
                     {.value = on, .onChanged = [&](bool v) { on = v; }, .label = "Active"})});
        }));
        tester.tap("Active");
        CHECK(on);
        tester.tap("Active");
        CHECK_FALSE(on);
    }

    TEST_CASE("ScrollView scrolls with the wheel and clamps") {
        WidgetTester tester({400, 200});
        auto scroll = std::make_shared<ScrollController>();
        tester.pumpWidget(live([&] {
            std::vector<WidgetPtr> rows;
            for (int i = 0; i < 50; ++i)
                rows.push_back(
                    Container({.height = 20, .child = Label("row " + std::to_string(i))}));
            return ScrollView({.controller = scroll,
                               .child = Column({.crossAxisAlignment = CrossAxisAlignment::stretch,
                                                .children = rows})});
        }));
        CHECK(scroll->extent == doctest::Approx(1000));
        tester.scroll({100, 100}, 2);
        tester.pump();
        CHECK(scroll->offset == doctest::Approx(96));
        CHECK(tester.findText("row 0") == 1);  // semantics are not clipped...
        const Rect* r = tester.find("row 10");
        REQUIRE(r);
        CHECK(r->y == doctest::Approx(200 - 96));  // ...but positions follow the scroll
        tester.scroll({100, 100}, 1000);
        CHECK(scroll->offset == doctest::Approx(800));
    }

    TEST_CASE("SplitView divider drags") {
        WidgetTester tester({800, 600});
        auto split = std::make_shared<SplitController>(SplitController{.size = 200});
        tester.pumpWidget(live([&] {
            return SplitView({.controller = split,
                              .first = Container({.color = Colors::black}),
                              .second = Container({.color = Colors::white})});
        }));
        tester.drag({202, 300}, {302, 300});
        CHECK(split->size == doctest::Approx(300));
        split->fromEnd = true;
        split->size = 200;
        tester.pump();
        tester.drag({800 - 200 - 2, 300}, {800 - 250 - 2, 300});
        CHECK(split->size == doctest::Approx(250));
    }

    TEST_CASE("TreeView selects rows and folds branches") {
        WidgetTester tester;
        auto state = std::make_shared<TreeState>();
        std::uint64_t picked = 0;
        tester.pumpWidget(live([&] {
            return TreeView(
                {.roots = {TreeItem{
                     .id = 1,
                     .label = "World",
                     .children = {TreeItem{.id = 2, .label = "Camera"},
                                  TreeItem{.id = 3,
                                           .label = "Player",
                                           .children = {TreeItem{.id = 4, .label = "Sword"}}}}}},
                 .state = state,
                 .onSelected = [&](std::uint64_t id) { picked = id; }});
        }));
        CHECK(tester.findText("Sword") == 1);
        tester.tap("Player");
        CHECK(picked == 3);
        CHECK(state->selected == 3);
        // The fold arrow sits left of the label, one indent level in.
        const Rect* player = tester.find("Player");
        REQUIRE(player);
        tester.tapAt({player->x + 4 + 16 + 4, player->y + player->height * 0.5f});
        tester.pump();
        CHECK(tester.findText("Sword") == 0);
        CHECK(state->collapsed.count(3) == 1);
    }

    TEST_CASE("Dropdown opens an overlay list and picks an item") {
        WidgetTester tester;
        int picked = 0;
        auto popup = std::make_shared<PopupState>();
        tester.pumpWidget(live([&] {
            return Column({.children = {SizedBox({.width = 200,
                                                  .child = Dropdown({
                                                      .items = {"Perspective", "Orthographic"},
                                                      .selected = picked,
                                                      .onChanged = [&](int i) { picked = i; },
                                                      .state = popup,
                                                  })})}});
        }));
        CHECK(tester.findText("Orthographic") == 0);
        tester.tap("Perspective");
        tester.pump();
        CHECK(tester.findText("Orthographic") == 1);
        tester.tap("Orthographic");
        CHECK(picked == 1);
        CHECK(popup->open == -1);
    }

    TEST_CASE("MenuBar runs an item and closes; clicking outside closes too") {
        WidgetTester tester;
        int added = 0;
        auto state = std::make_shared<PopupState>();
        tester.pumpWidget(live([&] {
            return Column(
                {.crossAxisAlignment = CrossAxisAlignment::stretch,
                 .children = {
                     MenuBar({.menus = {Menu{
                                  .title = "Scene",
                                  .items = {MenuItem{.label = "Add Empty",
                                                     .onSelected = [&] { ++added; }},
                                            MenuItem{.label = "Disabled", .enabled = false}}}},
                              .state = state}),
                 }});
        }));
        tester.tap("Scene");
        tester.pump();
        tester.tap("Disabled");
        CHECK(state->open == -1);
        tester.tap("Scene");
        tester.pump();
        tester.tap("Add Empty");
        CHECK(added == 1);
        tester.tap("Scene");
        tester.pump();
        tester.tapAt({600, 400});
        CHECK(state->open == -1);
    }

    TEST_CASE("TabView switches tabs") {
        WidgetTester tester;
        auto index = std::make_shared<int>(0);
        tester.pumpWidget(live([&] {
            return TabView({.tabs = {Tab{.title = "Output", .child = Label("log line")},
                                     Tab{.title = "Files", .child = Label("res://")}},
                            .index = index});
        }));
        CHECK(tester.findText("log line") == 1);
        tester.tap("Files");
        tester.pump();
        CHECK(*index == 1);
        CHECK(tester.findText("res://") == 1);
        CHECK(tester.findText("log line") == 0);
    }
}
