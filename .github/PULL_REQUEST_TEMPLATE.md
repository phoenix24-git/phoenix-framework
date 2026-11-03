## Description

<!-- What does this change and why? Link issues with "Fixes #123". -->

## Area

- [ ] Engine (C++)
- [ ] Framework / widgets
- [ ] C# scripting (Phoenix.Framework)
- [ ] CLI (phoenix_tools)
- [ ] Web runtime / DevTools
- [ ] Build, CI, docs

## Checklist

- [ ] Tests added or updated (engine doctest, xUnit, or Playwright e2e)
- [ ] `ctest --preset native-debug` and `dotnet test --solution phoenix.slnx` pass locally
- [ ] Script ABI unchanged, or `PHX_SCRIPT_ABI_VERSION` bumped with both C and C# mirrors updated
- [ ] User-facing changes noted in `CHANGELOG.md`
