---
title: Widgets
nav_order: 4
---

# Building UI with widgets
{: .no_toc }

Phoenix's C++ API is modelled on Flutter's. Named arguments are C++20 designated initializers, so
code reads almost like Dart.

1. TOC
{:toc}

## Hello, Phoenix

```cpp
#include <phoenix/phoenix.h>
using namespace phoenix;

class MyHomePage : public StatefulWidget {
    int counter_ = 0;

public:
    WidgetPtr build() override {
        return Scaffold({
            .appBar = AppBar({.title = "Phoenix Demo Home Page"}),
            .body = Center({.child = Column({
                .mainAxisAlignment = MainAxisAlignment::center,
                .children = {
                    Text("You have pushed the button this many times:"),
                    Text(std::to_string(counter_), {.fontSize = 34}),
                },
            })}),
            .floatingActionButton = FloatingActionButton({
                .onPressed = [this] { setState([this] { ++counter_; }); },
                .tooltip = "Increment",
                .child = Text("+", {.fontSize = 28, .color = Colors::white}),
            }),
        });
    }
};

void phoenix_main() { runApp(std::make_shared<MyHomePage>()); }
```

Designated initializers must follow declaration order — the order shown in the tables below.

## Layout model

Exactly Flutter's: **constraints go down, sizes go up, parents set positions**. `BoxConstraints`
has `minWidth/maxWidth/minHeight/maxHeight`; `tight`, `loose`, `deflate` and `constrain` behave as
in Flutter.

## Widget catalogue

| Widget | Props (in order) |
|---|---|
| `Container` | `width`, `height`, `color`, `padding`, `margin`, `child` |
| `SizedBox` | `width`, `height`, `child` |
| `Padding` | `padding`, `child` |
| `Center` | `child` |
| `Column` / `Row` | `mainAxisAlignment`, `crossAxisAlignment`, `spacing`, `children` |
| `Expanded` | `flex`, `child` |
| `Text(text, {fontSize, color})` | 5×7 bitmap font for now |
| `GestureDetector` | `onTap`, `child` |
| `Scaffold` | `appBar`, `body`, `floatingActionButton`, `backgroundColor` |
| `AppBar` | `title`, `backgroundColor`, `foregroundColor` |
| `FloatingActionButton` | `onPressed`, `tooltip`, `backgroundColor`, `child` |
| `ElevatedButton` | `onPressed`, `tooltip`, `backgroundColor`, `child` |
| `PhysicsView` | `pixelsPerMeter`, `originX`, `originY`, `backgroundColor`, `onTapWorld` |

Colours are `0xRRGGBBAA` (`Color{0xFF5722FFu}`, `Color::rgb(0xFF5722)`, `Colors::indigo`).

## State

`StatefulWidget` combines Flutter's widget and `State`: keep the instance alive (as a member of its
parent, or as the root passed to `runApp`) and call `setState` to rebuild.

{: .note }
There is no Element tree yet, so a `StatefulWidget` created inside `build()` would lose its state
on every rebuild. Reconciliation with keys is on the [roadmap](roadmap.md).

## Input

Each frame the binding receives a `FrameInput`: pointer position and button state, the
press/release edges, wheel delta, key presses and typed text. Widgets register **regions**
while painting:

```cpp
ctx.addInteraction(rect, {
    .onTap = [](Offset p) { ... },                 // press + release without dragging
    .onDrag = [](Offset p, Offset delta) { ... },  // keeps going outside the rect until release
    .onScroll = [](Offset lines) { ... },          // wheel; bubbles to the topmost region with onScroll
    .focus = &focusNode,                           // pressing moves keyboard focus here
});
ctx.addKeyHandler(&focusNode, onKey, onText);      // keys while focusNode has focus
ctx.addKeyHandler(nullptr, onShortcut);            // keys no focused widget consumed
ctx.addOverlay([](PaintContext& top) { ... });     // paint last: menus, dropdowns
```

Regions are opaque. A press goes to the topmost region under the pointer, so a panel can register
an empty region to keep clicks from reaching the game below it (`InputBlocker`). `ctx.hovered(rect)`
tells a widget whether to draw its hover state.

`WidgetsBinding::setOverlayRoot()` installs a second tree painted above the app. Where the overlay
has no regions, input reaches the app below. The [in-game editor](editor.md) uses this.

## Tool widgets

`#include <phoenix/tool_widgets.h>` adds the controls editors are built from, in a dark theme
(`ToolTheme`). Their state lives in small controllers (`TextEditingController`,
`ScrollController`, `SplitController`, `PopupState`, `TreeState`) held by `shared_ptr`. Keep the
controllers in your `StatefulWidget` and pass the same ones to every build.

| Widget | Props (in order) |
|---|---|
| `Stack` / `Positioned` | `children` / `left`, `top`, `right`, `bottom`, `width`, `height`, `child` |
| `ScrollView` | `controller`, `child` (vertical; wheel and draggable scrollbar) |
| `SplitView` | `vertical`, `controller`, `first`, `second` (draggable divider) |
| `InputBlocker` | `child` |
| `ToolButton` | `label`, `onPressed`, `selected`, `tooltip`, `color`, `fontSize` |
| `Checkbox` | `value`, `onChanged`, `label` |
| `TextField` | `controller`, `hint`, `onChanged`, `onSubmitted`, `width`, `fontSize` |
| `NumberField` | `value`, `onChanged`, `controller`, `step`, `decimals`, `integer`, `label`, `labelColor` (drag to scrub, click to type) |
| `Dropdown` | `items`, `selected`, `onChanged`, `state` |
| `MenuBar` | `menus` (`Menu{title, items}`, `MenuItem{label, shortcut, onSelected, enabled}`), `state`, `trailing` |
| `TreeView` | `roots` (`TreeItem{id, label, color, children}`), `state`, `onSelected`, `rowHeight` |
| `TabView` | `tabs` (`Tab{title, child}`), `index` |
| `Panel` | `title`, `actions`, `child`, `background` |
| `Section` | `title`, `expanded`, `onToggle`, `actions`, `child` |
| `PropertyRow` | `label`, `editor`, `labelWidth` |

Text fields edit with the arrow keys, Home/End, Backspace/Delete, Enter (commit) and Escape
(cancel). While a field has focus the engine turns on text input, so IMEs and on-screen keyboards
work there.

`WidgetTester` drives all of this: `tap`, `tapAt`, `drag(from, to)`, `hoverAt`,
`scroll(at, lines)`, `enterText`, `sendKey`, plus `find(label)` for a widget's rect.

## Engine services

```cpp
#include <phoenix/phoenix.h>

auto id = phoenix::spawn_box(x, y, 0.5f, 0.5f);       // Jolt rigid body (meters, y up)
phoenix::apply_impulse(id, 0, 500);
for (const PhxBody2D& b : phoenix::bodies()) { ... }  // zero-copy snapshot

phoenix::storage::write("save", data);                 // OPFS on the web, app data natively
phoenix::storage::read("save", [](std::optional<std::string> v) { ... });
```

`PhysicsView` renders the physics world; its `onTapWorld` callback receives world coordinates.
