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
