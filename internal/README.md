# internal/

`roadmap.7z` is the private engine roadmap: architecture plans, the feature catalogue and
design decisions that are not public yet. It is encrypted with AES-256, and the file names inside
are hidden.

```bash
export PHOENIX_ROADMAP_KEY='<ask the maintainers>'
dev/tools/roadmap.sh unlock      # → internal/roadmap/ (git-ignored)
dev/tools/roadmap.sh lock        # after editing; then commit internal/roadmap.7z
```

Never commit the passphrase or anything under `internal/roadmap/`.
