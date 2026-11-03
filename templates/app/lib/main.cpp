// {{name}}: a Phoenix app.
//
// This is the Phoenix take on Flutter's counter app: press the button to
// increment the counter *and* drop a Jolt rigid body into the physics view.
// Tap anywhere in the dark area to spawn a box there. The counter is saved to
// the browser's Origin Private File System, so it survives a reload.
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include <phoenix/phoenix.h>

using namespace phoenix;

class MyHomePage : public StatefulWidget {
  public:
    explicit MyHomePage(std::string title) : title_(std::move(title)) {
        // Static ground and walls (meters; the view maps 40 px per meter).
        spawn_box(0, -0.5f, 30, 0.5f, 0, /*dynamic=*/false, 0x45475AFFu);
        spawn_box(-12, 6, 0.5f, 8, 0, false, 0x45475AFFu);
        spawn_box(12, 6, 0.5f, 8, 0, false, 0x45475AFFu);
        storage::read("counter", [this](std::optional<std::string> saved) {
            if (saved)
                setState(
                    [&] { counter_ = static_cast<int>(std::strtol(saved->c_str(), nullptr, 10)); });
        });
    }

    WidgetPtr build() override {
        return Scaffold({
            .appBar = AppBar({.title = title_}),
            .body = Column({
                .crossAxisAlignment = CrossAxisAlignment::stretch,
                .children =
                    {
                        Expanded({.child = PhysicsView({
                                      .onTapWorld = [](float x, float y) { dropBox(x, y); },
                                  })}),
                        Padding({
                            .padding = EdgeInsets::all(24),
                            .child = Column({
                                .spacing = 12,
                                .children =
                                    {
                                        Text("You have pushed the button this many times:"),
                                        Text(std::to_string(counter_), {.fontSize = 34}),
                                    },
                            }),
                        }),
                    },
            }),
            .floatingActionButton = FloatingActionButton({
                .onPressed = [this] { incrementCounter(); },
                .tooltip = "Increment",
                .child = Text("+", {.fontSize = 28, .color = Colors::white}),
            }),
        });
    }

    int counter() const { return counter_; }

  private:
    static void dropBox(float x, float y) {
        static const std::uint32_t kPalette[] = {0xF38BA8FFu, 0xFAB387FFu, 0xF9E2AFFFu,
                                                 0xA6E3A1FFu, 0x89B4FAFFu, 0xCBA6F7FFu};
        static int next = 0;
        spawn_box(x, y, 0.4f, 0.4f, 0.3f * static_cast<float>(next % 5), true,
                  kPalette[next++ % 6]);
    }

    void incrementCounter() {
        setState([this] { ++counter_; });
        dropBox(-4.0f + static_cast<float>(counter_ % 9), 12.0f);
        storage::write("counter", std::to_string(counter_));
    }

    std::string title_;
    int counter_ = 0;
};

void phoenix_main() {
    runApp(std::make_shared<MyHomePage>("{{title}}"));
}
