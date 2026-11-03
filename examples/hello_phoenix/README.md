# hello_phoenix

The Phoenix starter app: Flutter-style counter + Jolt physics on wasm64.

## Getting Started

This project is a starting point for a Phoenix application.

```bash
phoenix run -d chrome        # build (debug), serve, open Chrome + Phoenix DevTools
phoenix build web --release  # optimized build in build/web
phoenix test                 # run widget tests in test/
```

- `lib/main.cpp` — your app (C++ widgets, physics, storage).
- `phoenix.json` — app metadata and the virtual-console profile (VRAM, cores).
- `web/` — the HTML shell; edit `index.html` to customise loading.
