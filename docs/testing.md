---
title: Testing
nav_order: 7
---

# Testing
{: .no_toc }

1. TOC
{:toc}

## Widget tests (your app)

`phoenix test` builds `test/*.cpp` natively and runs them with [doctest](https://github.com/doctest/doctest).
`phoenix/testing.h` provides a `WidgetTester` modelled on `flutter_test`:

```cpp
#include <doctest/doctest.h>
#include <phoenix/testing.h>
#include "main.cpp"

TEST_CASE("Counter increments smoke test") {
    phoenix::WidgetTester tester;
    tester.pumpWidget(std::make_shared<MyHomePage>("Test"));
    CHECK(tester.findText("0") == 1);
    REQUIRE(tester.tap("Increment"));   // tooltip or text
    tester.pump();
    CHECK(tester.findText("1") == 1);
}
```

No window or GPU is needed; engine services are stubbed when the engine is not running.

## Framework test suites (contributors)

| Suite | Command |
|---|---|
| Engine (doctest) | `cd engine && cmake --preset native-debug && cmake --build --preset native-debug && ctest --preset native-debug` |
| Engine on wasm64 + threads (Node 24) | presets `wasm64-debug` |
| Sanitizers | presets `native-asan`, `native-tsan` |
| CLI + C# framework (xUnit) | `dotnet test --solution phoenix.slnx` |
| C++ ↔ C# NativeAOT | `PHOENIX_TEST_SCRIPT_MODULE=<CSharpScript.so> phoenix_engine_tests -ts="ScriptHost*"` |
| Browser e2e (Playwright) | `cd dev/e2e && npm ci && npx playwright test` |

All of these run in CI on every pull request.
