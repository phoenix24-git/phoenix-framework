// Widget tests for {{name}}, run with `phoenix test`.
//
// Like flutter_test, WidgetTester pumps frames headlessly, finds widgets by
// their text/tooltip, and taps them. No window or GPU is needed.
#include <doctest/doctest.h>

#include <memory>

#include <phoenix/testing.h>

// Pull in the app's widgets.
#include "main.cpp"

TEST_CASE("Counter increments smoke test") {
    phoenix::WidgetTester tester;
    auto page = std::make_shared<MyHomePage>("Test");
    tester.pumpWidget(page);

    // Verify that our counter starts at 0.
    CHECK(tester.findText("0") == 1);
    CHECK(tester.findText("1") == 0);

    // Tap the '+' button and trigger a frame.
    REQUIRE(tester.tap("Increment"));
    tester.pump();

    // Verify that our counter has incremented.
    CHECK(tester.findText("0") == 0);
    CHECK(tester.findText("1") == 1);
    CHECK(page->counter() == 1);
}
