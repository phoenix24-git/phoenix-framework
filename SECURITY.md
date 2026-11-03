# Security Policy

## Supported versions

Phoenix is pre-1.0. Only the latest commit on `main` receives security fixes.

## Reporting a vulnerability

Please **do not** open a public issue. Report it privately through
[GitHub security advisories](https://github.com/phoenix24-git/phoenix-framework/security/advisories/new).
We aim to acknowledge reports within 72 hours and to agree on a disclosure timeline with you.

Areas of particular interest:

- the `phoenix run` dev server. It binds to 127.0.0.1 by default and serves source files only under
  `/__phoenix/app` and `/__phoenix/sdk`.
- the Script ABI boundary.
- the cross-origin-isolation service worker.
