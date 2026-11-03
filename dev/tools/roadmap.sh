#!/usr/bin/env bash
# Private roadmap vault.
#
# The public repository only contains internal/roadmap.7z: AES-256 encrypted,
# with file names hidden. Plaintext lives in internal/roadmap/, which git ignores.
#
#   export PHOENIX_ROADMAP_KEY='<passphrase>'   # never commit this
#   dev/tools/roadmap.sh unlock    # roadmap.7z -> internal/roadmap/
#   ...edit...
#   dev/tools/roadmap.sh lock      # internal/roadmap/ -> roadmap.7z (then commit it)
#
# Requires 7-Zip (`7z`): apt install p7zip-full / brew install p7zip / 7-zip.org.
# The archive also opens in the 7-Zip GUI with the same passphrase.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARCHIVE="$ROOT/internal/roadmap.7z"
PLAIN="$ROOT/internal/roadmap"

command -v 7z >/dev/null || { echo "error: 7z not found (install p7zip-full)" >&2; exit 1; }

key() {
  if [[ -n "${PHOENIX_ROADMAP_KEY:-}" ]]; then
    printf '%s' "$PHOENIX_ROADMAP_KEY"
  else
    read -r -s -p "Roadmap passphrase: " k </dev/tty; echo >&2; printf '%s' "$k"
  fi
}

case "${1:-}" in
  unlock)
    [[ -f "$ARCHIVE" ]] || { echo "error: $ARCHIVE missing" >&2; exit 1; }
    if [[ -d "$PLAIN" ]] && [[ -n "$(ls -A "$PLAIN" 2>/dev/null)" ]] && [[ "${2:-}" != "--force" ]]; then
      echo "error: $PLAIN already has files (lock them first, or pass --force to overwrite)" >&2; exit 1
    fi
    mkdir -p "$PLAIN"
    7z x -y -bso0 -bsp0 -p"$(key)" -o"$PLAIN" "$ARCHIVE"
    echo "Unlocked into internal/roadmap/ (git-ignored)."
    ;;
  lock)
    [[ -d "$PLAIN" ]] || { echo "error: nothing to lock ($PLAIN missing)" >&2; exit 1; }
    tmp="$ARCHIVE.tmp.7z"; rm -f "$tmp"
    # -mhe=on encrypts file names too; AES-256 with a key derived from the passphrase.
    (cd "$PLAIN" && 7z a -t7z -mx=9 -mhe=on -bso0 -bsp0 -p"$(key)" "$tmp" .)
    mv "$tmp" "$ARCHIVE"
    echo "Locked -> internal/roadmap.7z. Commit that file; internal/roadmap/ stays local."
    ;;
  *)
    echo "usage: $0 unlock [--force] | lock" >&2; exit 2 ;;
esac
