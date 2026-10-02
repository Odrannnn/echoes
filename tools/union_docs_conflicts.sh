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
# for lines both sides rewrote; `tools/sync_state_block.py --dedupe` re-derives the counted ones, and the
# caller re-judges the result, so a union that leaves a false claim fails check_docs_claims.py
# like any other change.
#
# **A plain `git merge-file --union` is not used, because it doubles a line both sides rewrote.**
# Two lanes that each moved the probe file count left two copies of that line, check_docs_claims.py
# then wrote the same number into both, and every later carry doubled them again: on 2026-10-02
# docs/HANDOFF.md went from 39 KB to 935 MB in four hours, two lines repeated 2,155,590 times
# each, and nothing failed. So a hunk is resolved by what its base says. Base empty - both sides
# only added - keeps both. Base not empty - both sides rewrote the same lines - keeps the tip's
# (ours) and, of the carried side's, only the lines that are new: not in the base, not in ours,
# and not resembling a line of either - same first 40 characters with the digits masked, or a
# difflib ratio of 0.8 and up (a rewritten row stays close to what it was, an appended one does
# not). Last, a line repeated more often than in either input is cut
# back to the larger of the two counts, so no carry can grow a repeat whatever the hunks did.
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
  cp "$tmp/ours" "$tmp/ours.in"
  git merge-file --diff3 "$tmp/ours" "$tmp/base" "$tmp/theirs" >/dev/null 2>&1
  [ $? -lt 128 ] || exit 1
  python3 - "$tmp" <<'PY' || exit 1
import collections, difflib, pathlib, re, sys
tmp = pathlib.Path(sys.argv[1])
read = lambda n: (tmp / n).read_text(errors="surrogateescape").splitlines(keepends=True)
key = lambda l: re.sub(r"\d+", "#", l)[:40]
def rewrite_of(line, others):
    return any(difflib.SequenceMatcher(None, line, o, autojunk=False).ratio() >= 0.8 for o in others)
out, state, ours, base, theirs = [], None, [], [], []
for line in read("ours"):
    if state is None:
        if line.startswith("<<<<<<< "):
            state, ours, base, theirs = "ours", [], [], []
        else:
            out.append(line)
    elif state == "ours" and line.startswith("||||||| "):
        state = "base"
    elif state in ("ours", "base") and line.rstrip("\n") == "=======":
        state = "theirs"
    elif state == "theirs" and line.startswith(">>>>>>> "):
        if base:
            known = set(ours) | set(base)
            keys = {key(l) for l in known if l.strip()}
            theirs = [l for l in theirs if l.strip() and l not in known and key(l) not in keys
                      and not rewrite_of(l, [o for o in known if o.strip()])]
        out += ours + theirs
        state = None
    else:
        {"ours": ours, "base": base, "theirs": theirs}[state].append(line)
if state is not None:
    sys.exit("union_docs_conflicts: unterminated conflict hunk")
cap = collections.Counter(read("ours.in"))
for l, n in collections.Counter(read("theirs")).items():
    cap[l] = max(cap[l], n)
seen, kept = collections.Counter(), []
for l in out:
    seen[l] += 1
    if l.strip() and seen[l] > max(cap[l], 1):
        continue
    kept.append(l)
(tmp / "ours").write_text("".join(kept), errors="surrogateescape")
PY
  cp "$tmp/ours" "$f" && git add -- "$f" || exit 1
  echo "$f"
done <<<"$conf"
exit 0
