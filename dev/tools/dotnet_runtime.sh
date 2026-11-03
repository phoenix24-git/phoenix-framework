#!/usr/bin/env bash
# Fetch pinned .NET runtime sources for the wasm64 C# scripting work (ADR 0005).
#
# The sources are never committed. They are cloned into bin/cache/dotnet-runtime/<name>
# (git-ignored), checked out at the exact commit in bin/internal/dotnet-runtime.lock,
# and verified after checkout.
#
#   dev/tools/dotnet_runtime.sh fetch [name...]      # default: every pinned source
#   dev/tools/dotnet_runtime.sh fetch --full runtime # full checkout, needed to build
#   dev/tools/dotnet_runtime.sh status               # pinned vs. checked-out commits
#   dev/tools/dotnet_runtime.sh path <name>          # print the checkout directory
#   dev/tools/dotnet_runtime.sh bump <name>          # move a pin to its branch tip
#
# By default the clone is blobless (--filter=blob:none) and sparse: only the
# directories relevant to WebAssembly and NativeAOT are checked out, so a fetch takes
# a few hundred MB instead of several GB. Pass --full before building the runtime
# (./build.sh), because its build needs the whole tree.
#
# Set PHOENIX_DOTNET_RUNTIME_CACHE to put the checkouts somewhere else.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOCK="$ROOT/bin/internal/dotnet-runtime.lock"
CACHE="${PHOENIX_DOTNET_RUNTIME_CACHE:-$ROOT/bin/cache/dotnet-runtime}"

# Directories checked out in sparse mode (cone mode; parent files come along).
SPARSE_PATHS=(
  docs/design
  docs/workflow
  eng
  src/coreclr/nativeaot
  src/coreclr/tools/Common
  src/coreclr/tools/aot
  src/mono/browser
  src/mono/wasm
  src/mono/wasi
  src/native/libs
)

die() { echo "error: $*" >&2; exit 1; }

command -v git >/dev/null || die "git not found"
[[ -f "$LOCK" ]] || die "$LOCK missing"

# Prints "name url branch commit" for every pin, or for one name.
pins() {
  grep -vE '^\s*(#|$)' "$LOCK" | awk -v n="${1:-}" 'n == "" || $1 == n { print $1, $2, $3, $4 }'
}

pin_of() {
  local line
  line="$(pins "$1")"
  [[ -n "$line" ]] || die "unknown source '$1' (known: $(pins | awk '{print $1}' | xargs))"
  echo "$line"
}

retry() {
  local delay=2
  for attempt in 1 2 3 4 5; do
    "$@" && return 0
    [[ $attempt -eq 5 ]] && return 1
    echo "  retrying in ${delay}s..." >&2
    sleep "$delay"; delay=$((delay * 2))
  done
}

fetch_one() {
  local full="$1" name url branch commit dir
  read -r name url branch commit <<<"$(pin_of "$2")"
  dir="$CACHE/$name"
  echo "==> $name  $url @ ${commit:0:12} ($branch)"

  if [[ ! -d "$dir/.git" ]]; then
    mkdir -p "$CACHE"
    retry git clone --quiet --filter=blob:none --no-checkout "$url" "$dir"
  fi

  if [[ "$full" == 1 ]]; then
    git -C "$dir" sparse-checkout disable
  else
    git -C "$dir" sparse-checkout set --cone "${SPARSE_PATHS[@]}"
  fi

  if ! git -C "$dir" cat-file -e "$commit^{commit}" 2>/dev/null; then
    retry git -C "$dir" fetch --quiet --filter=blob:none origin "$commit" \
      || retry git -C "$dir" fetch --quiet --filter=blob:none origin "$branch"
  fi
  git -C "$dir" -c advice.detachedHead=false checkout --quiet --detach "$commit"

  local head
  head="$(git -C "$dir" rev-parse HEAD)"
  [[ "$head" == "$commit" ]] || die "$name: checked out $head, expected $commit"
  echo "    $dir ($([[ "$full" == 1 ]] && echo full || echo sparse))"
}

status() {
  printf '%-16s %-14s %-14s %s\n' NAME PINNED CHECKED-OUT STATE
  while read -r name url branch commit; do
    local dir="$CACHE/$name" head="-" state="not fetched"
    if [[ -d "$dir/.git" ]]; then
      head="$(git -C "$dir" rev-parse HEAD 2>/dev/null || echo '?')"
      if [[ "$head" == "$commit" ]]; then state="ok"; else state="DRIFT (run fetch)"; fi
      git -C "$dir" sparse-checkout list >/dev/null 2>&1 && state="$state, sparse"
    fi
    printf '%-16s %-14s %-14s %s\n' "$name" "${commit:0:12}" "${head:0:12}" "$state"
  done < <(pins)
}

bump() {
  local name url branch commit tip
  read -r name url branch commit <<<"$(pin_of "$1")"
  tip="$(retry git ls-remote "$url" "refs/heads/$branch" | awk '{print $1}')"
  [[ -n "$tip" ]] || die "$name: branch $branch not found at $url"
  if [[ "$tip" == "$commit" ]]; then echo "$name is already at the tip of $branch"; return; fi
  sed -i.bak "s/$commit/$tip/" "$LOCK" && rm -f "$LOCK.bak"
  echo "$name: ${commit:0:12} -> ${tip:0:12}. Review the upstream changes, then commit $LOCK."
}

cmd="${1:-}"; shift || true
case "$cmd" in
  fetch)
    full=0
    if [[ "${1:-}" == "--full" ]]; then full=1; shift; fi
    if [[ $# -eq 0 ]]; then set -- $(pins | awk '{print $1}'); fi
    for n in "$@"; do fetch_one "$full" "$n"; done
    ;;
  status) status ;;
  path)
    [[ $# -eq 1 ]] || die "usage: $0 path <name>"
    pin_of "$1" >/dev/null; echo "$CACHE/$1" ;;
  bump)
    [[ $# -eq 1 ]] || die "usage: $0 bump <name>"
    bump "$1" ;;
  *)
    sed -n '2,19p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//' >&2; exit 2 ;;
esac
