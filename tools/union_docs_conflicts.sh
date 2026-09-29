#!/bin/bash
# tools/union_docs_conflicts.sh - resolve a failed `git apply --3way` whose conflicts are all in docs.
#
#   tools/union_docs_conflicts.sh      (run in the worktree the apply left conflicted)
#
# The goal loop's rebase (tools/run_goal.sh rebase_onto_tip) carries a judged change onto a tip
# another lane moved. Measured over 2026-09-28/29: all 11 carries that failed conflicted only in
# docs/HANDOFF.md and docs/RUNNING_THE_DECOMP.md - two lanes appending rows to the same table and
# both moving the state block - never in code, and each one threw a judged change away.
#
# A union merge keeps both sides' lines, which is right for appended rows and sections and wrong
# only for lines both sides rewrote; `tools/sync_state_block.py --dedupe` re-derives the counted ones, and the
# caller re-judges the result, so a union that leaves a false claim fails check_docs_claims.py
# like any other change.
#
# Exit 0: every conflict was under docs/ and is now resolved and staged (the files are printed).
# Exit 1: nothing conflicted, or something outside docs/*.md did - the tree is left as it was.
set -uo pipefail
conf=$(git diff --name-only --diff-filter=U)
[ -n "$conf" ] || { echo "union_docs_conflicts: no conflicted paths"; exit 1; }
if printf '%s\n' "$conf" | grep -qvE '^docs/.*\.md$'; then
  echo "union_docs_conflicts: conflicts outside docs/*.md:" $conf
  exit 1
fi
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
while IFS= read -r f; do
  git show ":1:$f" >"$tmp/base" 2>/dev/null || : >"$tmp/base"   # add/add: no common base
  git show ":2:$f" >"$tmp/ours" 2>/dev/null || : >"$tmp/ours"
  git show ":3:$f" >"$tmp/theirs" 2>/dev/null || : >"$tmp/theirs"
  git merge-file --union "$tmp/ours" "$tmp/base" "$tmp/theirs"
  cp "$tmp/ours" "$f" && git add -- "$f" || exit 1
  echo "$f"
done <<<"$conf"
exit 0
