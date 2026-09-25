#!/bin/bash
# Collect a lane: three-way apply its work to current HEAD and run the whole gate.
#
#   tools/collect.sh <lane> [<lane> ...]      e.g. tools/collect.sh a1 a3
#
# Why this exists. A lane is a git worktree at a fixed commit plus one agent, and its
# result arrives as uncommitted edits. Applying those by hand is where collection goes
# wrong: a lane's `config/` is its own view, so copying a file can silently revert a later
# rename or delete a split block - three modules lost their `Rel(...)` blocks that way and
# sat in `src/` compiled by nothing. This applies the lane's diff as a patch onto a fresh
# HEAD worktree, so a stale file becomes a visible conflict instead of a silent revert,
# and then measures the result instead of trusting the lane's report:
#
#   1. a worktree at current HEAD, with its own real build/ (never a symlink - see below)
#   2. a report baseline built from *unmodified* HEAD, so the per-function diff is honest
#   3. the lane's diff, three-way
#   4. tools/gate.sh: configure, ninja + dtk shasum, all 86 REL sha1s against config.yml,
#      the per-function report diff, module wiring, the docs' claims, the port probe
#   5. the diff stat, so the change is visible before it is trusted
#
# The collect worktree is left in place - that is where you read the result, run
# tools/decomp_build.sh, or fix something by hand and copy the files across. Remove it
# with `git worktree remove --force /tmp/opencode/collect-<lane>`.
#
# A lane's build/ must be a real directory, not a symlink to the master's: build.sha1 names
# files as build/G2ME01/... literally, so a shared build/ hashes the *master's* files and
# reports success while the lane's own output differs.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$REPO_ROOT"
WORKROOT="${COLLECT_ROOT:-/tmp/opencode}"
LANE_PATHS=(src include config configure.py libc tools docs)

# The toolchain lives in the Metroid Prime *port* tree, which is a sibling of the master
# checkout - not of the /tmp worktrees this script creates, so it has to be passed on.
TC="${MP_TOOLCHAIN_DIR:-$REPO_ROOT/../MetroidPrimePort}"
[ -x "$TC/build/review-tools/bin/ninja" ] || {
  echo "error: no toolchain at $TC - set MP_TOOLCHAIN_DIR to the MetroidPrimePort tree" >&2; exit 2; }
export MP_TOOLCHAIN_DIR="$TC"

[ "$#" -gt 0 ] || { sed -n '2,30p' "$0"; exit 2; }

status=0
for lane in "$@"; do
  echo "=============================================================="
  echo "collecting lane $lane"
  echo "=============================================================="
  lane_dir="$WORKROOT/$lane"
  [ -d "$lane_dir" ] || { echo "no such lane: $lane_dir" >&2; status=1; continue; }
  # Only collect a lane that has stopped. This script cannot tell - an agent's command line
  # does not name its worktree - so the caller has to know, and reading a lane's files while
  # it writes them gives a patch that applies to nothing.

  out="$WORKROOT/collect-$lane"
  git -C "$SRC" worktree remove --force "$out" >/dev/null 2>&1
  git -C "$SRC" branch -D "collect-$lane" >/dev/null 2>&1
  git -C "$SRC" worktree add -f "$out" -b "collect-$lane" HEAD >/dev/null || { echo "worktree add failed" >&2; status=1; continue; }
  mkdir -p "$out/build" "$out/orig"
  ln -sfn "$SRC/orig/G2ME01" "$out/orig/G2ME01"
  cp -r "$SRC/build/binutils" "$out/build/binutils"
  # No .gitignore edit here, unlike a lane's worktree: `tools/gate.sh --baseline` refuses a
  # dirty tree, and build/ and orig/ are untracked so they do not make it dirty.

  # 2. Baseline from unmodified HEAD, before anything is applied.
  echo "-- baseline (unmodified HEAD)"
  (cd "$out" && ./tools/gate.sh --baseline) || { echo "baseline failed; is HEAD itself sound?" >&2; status=1; continue; }

  # 3. The lane's diff, from the paths a lane legitimately edits.
  patch="$WORKROOT/$lane.patch"
  git -C "$lane_dir" add -A -- "${LANE_PATHS[@]}" 2>/dev/null
  git -C "$lane_dir" diff --cached --binary HEAD -- "${LANE_PATHS[@]}" >"$patch"
  if [ ! -s "$patch" ]; then echo "lane $lane changed nothing under ${LANE_PATHS[*]}"; status=1; continue; fi
  echo "-- lane diff: $(grep -c '^diff --git' "$patch") file(s)"
  (cd "$out" && git apply --3way --verbose "$patch") \
    || { echo "CONFLICT: the lane's diff does not apply to current HEAD - read it by hand" >&2; status=1; continue; }

  # 4. The gate, on the merged result.
  echo "-- gate on the merged result"
  (cd "$out" && ./tools/gate.sh)
  gate=$?

  # 5. What changed, for reading before it is trusted.
  echo "-- diff stat vs HEAD"
  git -C "$out" diff --stat HEAD
  [ "$gate" -eq 0 ] || status=1
  echo "-- worktree left at $out"
done
exit $status
