#!/bin/bash
# tools/goal_check.sh <item.json> - the ONLY judge. Exit 0 means the item passed; exit 3 (match
# items only) means PARTIAL - the flip failed but the target unit rose, so the change is committed
# and the item stays queued. See the verdict at the end.
#
# The agent does not decide whether its own work counted; this does, from the tree. Nothing here
# is relaxed, and a *new* failure anywhere fails the item even if the target improved - a change
# that fixes its target while regressing something else is a regression.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${MP_GOAL_TREE:-$REPO_ROOT}" || exit 2   # MP_GOAL_TREE: run against the worktree, not this repo
export MP_TOOLCHAIN_DIR="${MP_TOOLCHAIN_DIR:-$REPO_ROOT/../MetroidPrimePort}"
export MP_TOOLCHAIN="${MP_TOOLCHAIN:-$MP_TOOLCHAIN_DIR/build/review-tools}"
export PATH="$MP_TOOLCHAIN/bin:$PATH"
LOGDIR="${MP_GOAL_LOGDIR:-${MP_GOAL_TREE:-$REPO_ROOT}/build/goal}"
mkdir -p "$LOGDIR"

ITEM="${1:-}"
[ -n "$ITEM" ] || { echo "goal_check: usage: goal_check.sh <item.json>" >&2; exit 2; }
[ -f "$ITEM" ] || { echo "goal_check: no such item file: $ITEM" >&2; exit 2; }
[ "$ITEM" -ef "$LOGDIR/item.json" ] || cp "$ITEM" "$LOGDIR/item.json"

ID=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["id"])' "$ITEM")
KIND=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["kind"])' "$ITEM")
TARGET=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("target",""))' "$ITEM")
echo "goal_check: item $ID ($KIND) target=$TARGET"

fail=()
note() { echo "  FAIL  $*"; fail+=("$1"); }
ok()   { echo "  ok    $*"; }

# ---------------------------------------------------------------- baseline
# The baseline is a *path* the caller owns, because the tree being checked is the worktree
# ../wt-mp2-goal while this script lives in the repo. Default to the worktree's copy so a bare
# `goal_check.sh item.json` works; run_goal.sh exports MP_GOAL_BASE explicitly.
# The judge's baselines live in build/goal/judge/, untracked: the report, the port's undefined
# count and its undefined-symbol list, all recorded by run_goal.sh on the branch head.
JUDGE="${MP_GOAL_JUDGE:-${MP_GOAL_TREE:-$REPO_ROOT}/build/goal/judge}"
if [ -z "${MP_GOAL_BASE:-}" ]; then
  MP_GOAL_BASE="$JUDGE/report.base.json"
fi
BASE="$MP_GOAL_BASE"
if [ ! -f "$BASE" ]; then
  echo "goal_check: no baseline at $BASE - the brief's default is to stop the item, record a" >&2
  echo "             baseline on a clean HEAD once with tools/gate.sh --baseline, and retry." >&2
  exit 2
fi
echo "goal_check: baseline $BASE"

# ------------------------------------------------- 0. what the change touched
# **The agent must not be able to move the goalposts.** It runs with --auto in this tree, so
# nothing stops it re-recording a baseline (`link_check.sh --record` rewrites
# docs/research/port_link_baseline.txt) or editing the judge's own tools. Either would turn a
# regression into a pass, so a change that touches them fails outright, whatever else it did.
CHANGED=$(git status --porcelain --untracked-files=all | cut -c4- | sed 's/.* -> //')
BAD=$(printf '%s\n' "$CHANGED" | grep -E '^(tools/|docs/research/port_link_baseline\.txt$|build/goal/)' || true)
if [ -n "$BAD" ]; then
  note "the change touches paths the agent may not edit"
  printf '%s\n' "$BAD" | head -6 | sed 's/^/        /'
else
  ok "no judge-owned path touched"
fi
CODE_CHANGED=$(printf '%s\n' "$CHANGED" | grep -cE '^(src|include)/' || true)

# ---------------------------------------------------------------- 1. the gate
if ./tools/gate.sh "$BASE" >"$LOGDIR/check-gate.log" 2>&1; then
  ok "gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)"
else
  note "gate.sh"
  grep -E "FAIL|stale:|missing:|GONE|WORSE|UNLINKED|FELL" "$LOGDIR/check-gate.log" | head -12 | sed 's/^/        /'
fi

# ------------------------------------------------- 2. matched/linked vs baseline
# Read with the same definition the docs use: `linked` counts functions in units that are
# `complete`, which is objdiff's word for "Matching and really in the link".
python3 - "$BASE" <<'PY' >"$LOGDIR/check-counts.log" 2>&1
import json, sys
b = json.load(open(sys.argv[1])); c = json.load(open("build/report.json"))
def linked(r):
    return sum(u["measures"].get("matched_functions", 0) for u in r["units"]
               if u.get("metadata", {}).get("complete"))
bm, cm = b["measures"]["matched_functions"], c["measures"]["matched_functions"]
bl, cl = linked(b), linked(c)
print(f"matched {bm} -> {cm}   linked {bl} -> {cl}")
sys.exit(0 if (cm >= bm and cl >= bl) else 1)
PY
if [ $? -eq 0 ]; then
  ok "counts: $(cat "$LOGDIR/check-counts.log")"
else
  note "matched/linked fell: $(cat "$LOGDIR/check-counts.log")"
fi

# ---------------------------------------------------------------- 3. symbol names
if python3 tools/check_symbol_names.py >"$LOGDIR/check-names.log" 2>&1; then
  ok "check_symbol_names.py"
else
  note "check_symbol_names.py"; tail -4 "$LOGDIR/check-names.log" | sed 's/^/        /'
fi

# ---------------------------------------------------------------- 4. decomp_build All:
# The `All:` line must not fall. Quoted, not parsed out of a progress bar.
./tools/decomp_build.sh >"$LOGDIR/check-build.log" 2>&1
ALL=$(grep -oE "^All: .*" "$LOGDIR/check-build.log" | tail -1)
BASE_ALL=$(grep -oE "^All: .*" "$LOGDIR/check-gate.log" | tail -1)
if [ -z "$ALL" ]; then
  note "decomp_build.sh printed no All: line"
else
  ok "$ALL"
  [ -n "$BASE_ALL" ] && echo "        (gate's run: $BASE_ALL)"
fi

# ---------------------------------------------------------------- 5. per-kind extra
# target_rose - the `progress` test, shared with a `match` item whose flip failed: the target
# unit's matched_functions rose strictly, and no asm was added. Notes its own failures.
target_rose() {
  python3 - "$BASE" "$TARGET" >"$LOGDIR/check-progress.log" 2>&1 <<'PY'
import json, re, sys
t = re.sub(r'\.(cpp|cp|c)$', '', sys.argv[2])
def unit(path):
    units = json.load(open(path))["units"]
    if t.startswith("module:"):
        # A REL module: carving it renames its units (auto_* -> ours), so the count is summed
        # over every unit under `<Module>/` rather than read from one unit that may not survive.
        m = t[len("module:"):] + "/"
        hits = [u for u in units if u["name"].startswith(m)]
        if not hits:
            print(f"target {t} names no units in {path}"); sys.exit(2)
        return t, sum(u["measures"].get("matched_functions", 0) for u in hits), \
            sum(u["measures"].get("total_functions", 0) for u in hits)
    hits = [u for u in units if u["name"] == t or u["name"].endswith("/" + t)]
    if len(hits) != 1:
        print(f"target {t} names {len(hits)} units in {path}, need exactly 1"); sys.exit(2)
    return hits[0]["name"], hits[0]["measures"].get("matched_functions", 0), hits[0]["measures"].get("total_functions", 0)
name, b, total = unit(sys.argv[1])
_, c, _ = unit("build/report.json")
print(f"{name}: {b} -> {c} / {total} functions")
sys.exit(0 if c > b else 1)
PY
  if [ $? -eq 0 ]; then ok "target rose: $(cat "$LOGDIR/check-progress.log")"
  else note "target did not rise: $(cat "$LOGDIR/check-progress.log")"; fi
  # A score bought with hand-written assembly is not decompilation. Checked on the added lines
  # only, so existing asm (the SDK's, the port's label stubs) is untouched. Comments are stripped
  # first: a note citing `build/G2ME01/asm/...` is not assembly, and failed Blogg's head on it.
  ASM=$(git diff -U0 HEAD -- src include | grep -E '^\+' | grep -vE '^\+\+\+' \
    | sed -E 's#^\+##; s#/\*.*\*/##g; s#//.*$##; s#/\*.*$##' | grep -vE '^[[:space:]]*\*' \
    | grep -nE '\basm\b|__asm' || true)
  if [ -n "$ASM" ]; then
    note "change adds asm"; printf '%s\n' "$ASM" | head -4 | sed 's/^/        /'
  else ok "no asm added"; fi
}

FLIP_FAIL=""   # a match item's flip failure, held back from `fail` until the verdict
case "$KIND" in
  match)
    if [ -z "$TARGET" ]; then
      note "match item with no target unit"
    else
      # **`flip_test`'s EXIT STATUS IS NOT THE VERDICT.** Measured: it prints `SKIP <unit> - not
      # listed in configure.py`, then `kept: 0/1 failed: 0 skipped: 1`, and **exits 1.** So an
      # already-`Matching` unit - the most common possible state for a `match` item - looks exactly
      # like a failure if you trust `$?`. The verdict is the word on its own line: `PASS` means it
      # was kept as Matching (or was already Matching and verified in place). Judging on the exit
      # code would make every already-Matching unit un-promotable. (Superseded: this used to say
      # `SKIP` meant "nothing to promote"; flip_test prints SKIP only when the unit is absent.)
      #
      # **SKIP IS NOT A PASS (2026-09-28).** Queue targets are written without the extension
      # (`Kyoto/Audio/CStaticAudioPlayer`), configure.py lists `...CStaticAudioPlayer.cpp`, so
      # flip_test found no entry, printed SKIP, and SKIP was taken as "already Matching". Two items
      # were marked done that way with the unit still NonMatching - one with nothing changed at all,
      # one with a docs-only diff. So: resolve the target to its configure.py entry first (absent
      # is a failure), and after the flip the entry itself must read `Matching`.
      UNIT=$(python3 - "$TARGET" <<'PY'
import re, sys
t = sys.argv[1]
s = open('configure.py').read()
for u in ([t] if re.search(r'\.(cpp|cp|c)$', t) else [t + '.cpp', t + '.cp', t + '.c']):
    if re.search(r'Object\(\s*(Matching|NonMatching|MatchingFor)\b[^,]*,\s*"' + re.escape(u) + '"', s):
        print(u); break
PY
)
      if [ -z "$UNIT" ]; then
        note "match target $TARGET has no Object(...) entry in configure.py"
      else
        ./tools/flip_test.sh "$UNIT" >"$LOGDIR/check-flip.log" 2>&1
        VERDICT=$(grep -oE '^[[:space:]]*(PASS|SKIP|FAIL)' "$LOGDIR/check-flip.log" | tail -1 | tr -d '[:space:]')
        STATE=$(grep -oE "Object\(\s*(Matching|NonMatching|MatchingFor)[^,]*,\s*\"$(printf '%s' "$UNIT" | sed 's/[.[\*^$/]/\\&/g')\"" configure.py \
                | grep -oE '(Matching|NonMatching|MatchingFor)' | head -1)
        case "$VERDICT" in
          PASS) if [ "$STATE" = Matching ]; then ok "flip_test $UNIT: PASS, Object(Matching) in configure.py"
                else note "flip_test $UNIT: PASS but configure.py has it as ${STATE:-?}, not Matching"; fi ;;
          "")   note "flip_test $UNIT: no verdict line"
                 tail -4 "$LOGDIR/check-flip.log" | sed 's/^/        /' ;;
          FAIL) FLIP_FAIL="flip_test $UNIT: FAIL"
                 echo "  flip  $FLIP_FAIL - judged below as partial progress"
                 grep -iE "FAIL|undefined|reverted|error" "$LOGDIR/check-flip.log" | head -6 | sed 's/^/        /' ;;
          *)    note "flip_test $UNIT: $VERDICT"
                 grep -iE "FAIL|SKIP|undefined|reverted|error" "$LOGDIR/check-flip.log" | head -6 | sed 's/^/        /' ;;
        esac
      fi
    fi
    ;;
  progress)
    # **Partial progress on a unit that cannot flip yet (2026-09-28).** A `match` item on a big
    # unit can only reset: CStateManager's +34 functions and CGameState's +5 were each done right
    # 2-4 times and thrown away because the unit was 69/239 and 70/116. This kind passes on the
    # target unit's matched_functions rising strictly, on top of the gate - whose per-function diff
    # (report_diff.py) already fails any function anywhere that got worse, and whose DOL/REL
    # hashes keep every Matching unit exact. It is not "the unit is done"; that is still `match`.
    if [ -z "$TARGET" ]; then
      note "progress item with no target unit"
    elif [ "$CODE_CHANGED" = 0 ]; then
      note "progress item changed nothing under src/ or include/"
    else
      target_rose
    fi
    ;;
  port)
    # **What "done" means for a port item has to be something that can fail.**
    #
    # The old check was "the target is no longer in the undefined list". For an inline target
    # (`CInputStream::ReadInt32`) that is true before any work at all - the linker never asks for
    # an inline - and it read `build-port-link/build.log` *before* `link_check.sh` rebuilt it, so
    # it judged the previous tree's link. Both are fixed here:
    #
    #   - the link runs first, so the list is this tree's;
    #   - a target counts as resolved only if it WAS in the baseline list (recorded on the
    #     branch head) and is gone now - otherwise "gone" proves nothing;
    #   - a target the linker never asked for needs the item's `verify` script, a real test
    #     under tools/goal_verify/ that the orchestrator wrote and the agent cannot edit;
    #   - either way, the change must touch src/ or include/.
    VERIFY=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("verify",""))' "$ITEM")
    [ "$CODE_CHANGED" -gt 0 ] && ok "$CODE_CHANGED path(s) changed under src/ or include/" \
                              || note "port item with no change under src/ or include/"

    ./tools/link_check.sh >"$LOGDIR/check-link.log" 2>&1
    # A port that does not compile reports "unique undefined symbols 0": the linker never ran,
    # and link_check.sh says so. Read that line, or a broken build scores as a perfect link.
    if grep -qE "LINKER NEVER RAN|compile errors [1-9]" "$LOGDIR/check-link.log"; then
      note "the port did not build - link_check's counts are vacuous"
      grep -m3 -E "error:" "$LOGDIR/check-link.log" | sed 's/^/        /'
      UNDEF_NOW=""
    else
      UNDEF_NOW=$(sed -n 's/.*unique undefined symbols \([0-9]*\).*/\1/p' "$LOGDIR/check-link.log" | head -1)
    fi
    UNDEF_LIST="$LOGDIR/undef_by_obj.txt"
    rm -f "$UNDEF_LIST"
    [ -n "$UNDEF_NOW" ] && MP_UNDEF_LIST="$UNDEF_LIST" python3 tools/link_undef_refs.py >"$LOGDIR/check-undef.log" 2>&1
    UNDEF_BASE_LIST="$JUDGE/undef.base.txt"
    UNDEF_BASE=$(cat "$JUDGE/undef.base.count" 2>/dev/null)
    judged=0; lists=0
    if [ -z "$TARGET" ]; then
      note "port item with no target symbol"
    elif [ ! -s "$UNDEF_LIST" ] || [ ! -s "$UNDEF_BASE_LIST" ]; then
      note "could not read the port's undefined-symbol list (now: $UNDEF_LIST, base: $UNDEF_BASE_LIST)"
    elif lists=1 && cut -f1 "$UNDEF_BASE_LIST" | grep -qF -- "$TARGET"; then
      judged=1
      if cut -f1 "$UNDEF_LIST" | grep -qF -- "$TARGET"; then
        note "$TARGET is still undefined"
        cut -f1 "$UNDEF_LIST" | grep -F -- "$TARGET" | head -2 | sed 's/^/        /'
      else
        ok "$TARGET was undefined at the branch head and is not now"
      fi
    fi
    if [ -n "$VERIFY" ]; then
      judged=1
      if [ ! -x "$REPO_ROOT/tools/goal_verify/$VERIFY" ]; then
        note "verify script tools/goal_verify/$VERIFY is missing"
      elif timeout -k 10 600 "$REPO_ROOT/tools/goal_verify/$VERIFY" >"$LOGDIR/check-verify.log" 2>&1; then
        ok "verify $VERIFY: $(tail -1 "$LOGDIR/check-verify.log")"
      else
        note "verify $VERIFY failed"
        tail -6 "$LOGDIR/check-verify.log" | sed 's/^/        /'
      fi
    fi
    if [ "$judged" -eq 0 ] && [ "$lists" -eq 1 ]; then
      note "unjudgeable: $TARGET was never undefined and the item has no verify script"
    fi
    if [ -n "$UNDEF_NOW" ] && [ -n "$UNDEF_BASE" ]; then
      if [ "$UNDEF_NOW" -le "$UNDEF_BASE" ]; then
        ok "port undefined $UNDEF_BASE -> $UNDEF_NOW"
      else
        note "port undefined rose $UNDEF_BASE -> $UNDEF_NOW"
      fi
    else
      [ -z "$UNDEF_NOW" ] && [ -n "$UNDEF_BASE" ] \
        || note "could not read the port's undefined count (now '$UNDEF_NOW', base '$UNDEF_BASE')"
    fi
    PROBE=$(./tools/probe_sources.sh 2>&1 | tail -1)
    case "$PROBE" in
      *"0 failed"*) ok "probe: ${PROBE%% (logs*}" ;;
      *) note "probe: $PROBE" ;;
    esac
    ;;
  *)
    note "unknown kind: $KIND"
    ;;
esac

# ---------------------------------------------------------------- verdict
# **A match item whose flip fails can still be progress (2026-09-29).** `match-ccharlayoutinfo`
# took the unit 27/28 -> 28/28 with every other check green and was thrown away twice, because
# the unit cannot flip for an object-layout reason no C++ edit reaches. So a failed flip is judged
# as a `progress` item would be - the target's matched_functions rose, no asm, the change touched
# src/ or include/ - and passes as PARTIAL (exit 3): run_goal.sh commits it and requeues the item
# for the rest. Any other failure, including a flip that printed no verdict, still fails it.
if [ -n "$FLIP_FAIL" ] && [ ${#fail[@]} -eq 0 ]; then
  if [ "$CODE_CHANGED" = 0 ]; then
    note "match item changed nothing under src/ or include/"
  else
    target_rose
  fi
  if [ ${#fail[@]} -eq 0 ]; then
    echo "goal_check: PARTIAL $ID - $FLIP_FAIL, but the target rose; commit it and keep the item"
    exit 3
  fi
fi
[ -n "$FLIP_FAIL" ] && fail+=("$FLIP_FAIL")
if [ ${#fail[@]} -eq 0 ]; then
  echo "goal_check: PASS $ID"
  exit 0
fi
echo "goal_check: FAIL $ID - ${#fail[@]} failing check(s): ${fail[*]}"
exit 1
