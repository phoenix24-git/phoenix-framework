# Repository rulesets

Rulesets are repository settings, so they can't be applied from a commit. Import this one once:

**Settings → Rules → Rulesets → New ruleset → Import a ruleset →** `protect-main.json`

What it does for `main`:

| Rule | Effect |
|---|---|
| Restrict deletions / block force pushes | `main` cannot be deleted or rewritten |
| Require a pull request | changes land through PRs (0 approvals required, so a solo maintainer can merge; raise it as the team grows) |
| Require status checks | every CI job in `.github/workflows/ci.yml` must pass |
| Bypass: GitHub Actions | lets the `Web demo` workflow commit the rebuilt `docs/demo` directly |

If you rename a CI job, update its name in `required_status_checks` too.
