# Contributing to Phoenix

Thanks for helping. This guide keeps the project maintainable for the long run.

## Setup

```bash
dev/ci/install_linux_deps.sh            # Linux system packages (SDL3 backends, toolchain)
bin/phoenix doctor                       # .NET 10, CMake ≥ 3.25, Ninja, Emscripten, Chrome
bin/phoenix precache --web               # pinned Emscripten into bin/cache/emsdk
```

## Before opening a pull request

```bash
# C++ engine
cd engine && cmake --preset native-debug && cmake --build --preset native-debug && ctest --preset native-debug
# C# CLI + framework
dotnet build phoenix.slnx && dotnet test --solution phoenix.slnx
# Formatting
git ls-files 'engine/*.cpp' 'engine/*.h' | xargs clang-format -i
dotnet format phoenix.slnx
# Browser e2e (optional locally, always in CI)
cd dev/e2e && npm ci && npx playwright test
```

## Ground rules

- **Tests come with changes.** Engine logic gets doctest coverage (it must also pass on wasm64).
  CLI behaviour gets xUnit tests. Anything user-visible on the web gets a Playwright test.
- **Layering:** `core ← physics/framework/renderer ← engine`. Lower layers never include higher ones.
- **The browser main thread never blocks.** Long work goes to the job system or the IO thread.
- **Script ABI:** any change to `script_abi.h` must update `ScriptAbi.cs` and both layout tests,
  and bump `PHX_SCRIPT_ABI_VERSION`.
- **Dependencies are pinned by commit SHA** in `engine/cmake/PhoenixDependencies.cmake`, by version
  in `Directory.Packages.props` and `dev/e2e/package.json`, and Emscripten in
  `bin/internal/emscripten.version`. Bumps get their own pull request.
- **Decisions that are hard to reverse** get an ADR in `docs/adr/`.
- **Style:** `.clang-format` (C++20, Google-based, 100 columns) and `.editorconfig` (C#: `_field`,
  `s_static`; warnings are errors).

## Commits and releases

- Use [Conventional Commits](https://www.conventionalcommits.org/) (`feat(engine): …`, `fix(cli): …`).
- `VERSION` is the single source of the SDK version (CMake, MSBuild, and the tool all read it).
  Releases are tagged `vX.Y.Z` and summarized in `CHANGELOG.md`.
