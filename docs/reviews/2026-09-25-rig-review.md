# Rig review, 2026-09-25 (Claude Opus 5.5)

A full review of the measurement rig, the tools, and the lane procedure, run at the user's request
against `5ecc955` with the tools of the day. It is kept verbatim below except that the pieces which
were acted on say so; the point of keeping it is that its measurements are falsifiable and several of
its recommendations are still open.

**What was adopted in the same session** (commits `50f8e94`, `0efc05e`, `5ecc955`):

- `tools/gate.sh` - the whole acceptance test in one command, ninja's exit status as the hash gate,
  non-zero exit, ~1.9 s. Prototype came from this review.
- `tools/report_diff.py` - `WORSE` / `GONE` / `UNLINKED` / fell-linked-total. Prototype from this review.
- `tools/flip_test.sh` hardened: refuses the two vacuous passes, non-zero exit, `mktemp` backup.
- `tools/range_bounds.py` and `range_owner.py` restored from stubs; `range_bounds`' always-False
  boundary test fixed; `unit_fit.sh`'s REL object paths fixed.
- `tools/check_module_wiring.py` now means what it says (a `NonMatching` entry with no source is the
  legal reserved-range pattern, not BROKEN).
- `check_docs_claims.py` gained a deny-list of the phrases this review showed to be false.
- The docs corrections listed in its section 6, except those noted as still open below.
- `Puffer` promoted (+9 linked functions), the review's W1.

**Still open** (its section 2 "still to do" and section 5's remaining items):

1. **Per-module config files.** Give each module its own object list (e.g.
   `config/G2ME01/rels/<Module>/objects.py`) so REL lanes never share a hunk in `configure.py`; this
   is the structural fix for the clobbers, where the checker is only detection afterwards.
2. **`collect.sh <lane>`** - three-way apply a lane's diff in a fresh HEAD worktree, run the gate,
   print the diff stat and `report_diff`. Collection is the recognised bottleneck now that a build is
   3.8 s; it is also the step where this session's judgement was demonstrably fallible.
3. **A "renamed consistently" check** - every externally referenced `fn_` inside a `Matching` range is
   renamed. Duplicate addresses are already 0, so that part is done.
4. **Ranking by linked yield** - order the DOL tail by units close to 100% *whose `unit_fit` is clean*,
   not by fuzzy percentage.
5. **Remove the remaining lane worktrees** (61 of them, 11 GB in a tmpfs) once their work is collected
   or acknowledged.

---

## Review, verbatim

The worst problems are in the verification chain, and several of them are what produced today's false
"verified" commits. I confirmed each flaw below by experiment in my own worktree
(`/tmp/opencode/w23-review`). The master tree is untouched: `git status` is clean at `5ecc955`, and
`main.dol` is `6ef9b491…`.

The measurement that changes the picture: **a cold lane build takes 3.8 s**, not minutes. Configure
plus ninja is 473 edges and 3.83 s wall; the objdiff report takes 0.03 s. The machine is idle almost
all the time, and the procedure is the bottleneck.

### 1. Flaws, ranked by cost

#### Silently produces a wrong result

**S1. The documented gates go green on a broken build.** When the link fails, ninja leaves the
previous `main.dol` and RELs in place.
- **Experiment:** I renamed one `CAi` symbol with a spurious `const`. ninja returned 1
  (`undefined: 'CAi::HealthInfo(CStateManager&) const'`), yet `sha1sum build/G2ME01/main.dol` still
  printed `6ef9b491…`.
- **It happened today.** `4d81aba` and `e6344d5` both say "DOL sha1 6ef9…, 86/86 RELs byte-identical".
  The fix commit `0bc522c` says that at that point every REL failed to link.
- **Where it's documented:** `AGENTS.md` lists `sha1sum` as the first gate, and the HANDOFF snippet
  hashes `build/` without building.
- **Fix:** the hash gate should be **ninja's exit code**. Its `CHECK` step runs
  `dtk shasum -c config/G2ME01/build.sha1`, which lists the DOL plus all 86 RELs, and I checked those
  hashes are identical to `config.yml`'s. Never hash files after a failed ninja.
- **Corollary:** "87 files OK is not evidence" is an overcorrection. That was only true for
  `build-clone/`. In a real `build/`, it is exactly the `config.yml` check, and the hand-typed
  replacements are what read stale files.

**S2. The headline number is the signal, not the result.** From `build/report.json`: only **1626 of
the 3020** "matched" functions are in units whose own object is linked (`metadata.complete`). DOL: 1334
linked, of which 880 are SDK, leaving **454 game functions**. REL: 292 linked, of which 202 are
`REL_Setup`/`global_destructor_chain` boilerplate (one source credited once per module), leaving
**90**. The gate "the All: line must not fall" gates on the signal. `LANE_BRIEFING.md` actively
encourages scoring `NonMatching` 100%s. **Fix:** make linked functions the headline and the gate, and
keep fuzzy/matched as secondary numbers.

**S3. `flip_test.sh` can pass vacuously.**
- **No split block:** a unit that no `splits.txt` declares is compiled and never linked, and a flip
  reports `PASS -> kept as Matching`. I ran this on `CScriptIngSwarm.cpp` and it passed in 0.6 s.
- **No source:** `configure.py` does **not** refuse a `Matching` unit without source.
  `tools/project.py:1151-1153` prints "Missing source file" and links the retail object. I flipped
  `ScriptGuiPrefix.cpp`: configure returned 0 and the ScriptGui hash still matched `2b58f6d3…`.
  `flip_test` hides that line with `>/dev/null`. Its own guard checks `src/<unit>`, which is the wrong
  path for `source=` entries. The claim that configure refuses is false in
  `RUNNING_THE_DECOMP.md:128`, `PORT_NOTES.md:431` and `check_module_wiring.py:83`.
- **Already present:** five `Matching` `Dolphin/card/CARD*.c` units are in no split. The 14:04 commit
  that "wires IngSwarm and WallCrawlerSwarm" wired them into nothing.
- **Fix:** after configuring, assert that the unit's `build/G2ME01/src/…o` appears in a `link` edge of
  `build.ninja`, and fail if "Missing source file" is printed. Ideally also do a mutation check:
  change our object and the hash must break. I used that to verify Puffer.

**S4. `flip_test.sh` backs up `configure.py` to one shared path** (`/tmp/opencode/cfg.before`, lines
79 and 112). Two lanes flipping at the same time will restore each other's `configure.py`. It is a
*possible* cause of the clobbers; I have no direct proof. **Fix:** use `mktemp` or
`git show HEAD:configure.py`.

**S5. `check_symbol_names.py` cannot fail.** It compares `symbols.txt` against `obj/*.o`, and dtk names
those objects *from* `symbols.txt`. On the `CAi` rename above it reported "0 missing" while the link
was broken. It also covers only the 100 DOL `.cpp` units' `.text`, and no RELs. **Fix:** drop it from
the gates; ninja's exit code covers link breakage, and a per-function report diff covers lost pairings.

**S6. Losing a function is invisible to every gate.** Removing a `Rel` block and worsening one
`CPlayerState` return value left the DOL and RELs retail; the wiring warning only shows if someone
runs it, and `All:` dropped by 10, which any gain elsewhere would hide. A per-function diff flags all
10.

**S7. The root cause of the config clobbers is structural.** Every REL lane appends its `Rel(...)`
block just before `config.libs`' closing `]`. `git apply --3way` of real lanes' diffs (`l1`, `m1`,
`n1`) conflicts in `configure.py` at that same hunk for all three, while their `symbols.txt` files
applied cleanly. `33b73a3` is exactly this. `check_module_wiring.py` only detects the damage
afterwards.

**S8. Reusing a lane name silently runs the lane on a stale tree.** `git worktree add -f
/tmp/opencode/m1 -b mod-m1 HEAD` fails with `fatal: a branch named 'mod-m1' already exists` (rc 255);
the documented sequence has no `set -e`, so the next lines prepare the *old* worktree. The doc's own
example name, `m7`, already exists.

**S9. Several tools report success on nothing.** At the time of the review:
`range_bounds.py`/`range_owner.py` were docstring-only and exited 0; `unit_fit.sh` looked for REL
objects at `build/G2ME01/<Module>/src/`, so every REL unit read "build it first"; `fnmap.py`/
`autorename.py` are DOL-only and silently empty for RELs; `flip_test.sh` always exited 0, including on
SKIP and FAIL, and skipped 158 of 305 `configure.py` entries (all `MatchingFor(...)`, multi-line
entries, and entries with irregular spacing); `check_module_wiring.py` exited 1 by flagging legal
`NonMatching` placeholders as BROKEN. *(All of these were fixed the same day.)*

**S10. `check_docs_claims.py` only checks one direction.** It asserts that current strings appear,
never that stale ones are absent. *(A deny-list was added the same day.)*

#### Wastes time

- **W1. Puffer can be promoted right now, for +9 linked functions.** Nine `NonMatching` units have
  every function at 100%. Flipping all nine in 15.8 s: `CScriptPuffer` and `CScriptPufferRel` **pass**;
  the other seven fail genuinely (`CEntity` and `CScanTreeInventory` break many RELs; `CQuaternion`,
  `DolphinCColor`, `DolphinCDvdFile` and `AIMannedTurret` break the DOL or their REL; `CPowerBeam`
  fails). *(Puffer was promoted the same day, with a mutation check.)*
- **W2. The probe gate can't fail on today's work.** `files.cmake` omits 40 of the 144 game sources the
  matching build compiles, including `CAi`, `CPatterned` and `TypesMatch`.
- **W3. Raw-offset "decompilation" slips past the no-assembly rule** (judgement, not a measured
  failure). Code like `extern "C" fn_42_324(void* self) { *(float*)((char*)self + 0x448) = …; }` has
  26 raw-offset sites in `ScriptFrontEndDataNetwork` and more in Metaree, WallCrawler and Puffer. It
  matches bytes, but upstream would not accept it, and it is wrong on a 64-bit PC port.

### 2. Hardening

- `tools/gate.sh` - one command, no partial mode; ninja's exit code as the hash gate; an independent
  re-hash only after a green build; the per-function diff against a baseline recorded with
  `--baseline`; wiring, docs claims and the probe; one verdict line.
- `tools/report_diff.py` - flags any function that got worse or disappeared, any unit that stopped
  being linked, and any fall in the linked total.
- **Still to do:** `flip_test.sh`'s version of these (done the same day); a "renamed consistently"
  check; per-module config files; `collect.sh`.

### 3. Speed

| Step | Measured |
| --- | --- |
| Cold lane build (split, 291 compiles, 173 links, check) | 3.83 s wall, 380 MB RSS |
| No-op `decomp_build.sh` | 0.17 s |
| objdiff report | 0.03 s |
| Probe | 1.27 s |
| Full gate | 1.85 s |
| One flip attempt | about 1.7 s |

dtk caching, tmpfs tuning and parallelism limits are not worth pursuing. Wall-clock goes into metered
orchestrator turns: 76 commits that day, each re-measured by hand, one at a time.

### 4. The procedure

"One lane per unit, orchestrator verifies every one by hand" puts judgement where a script belongs and
scripting where judgement belongs. **Automate:** all measurement, gates, flip attempts, collection, the
state block, and a nightly flip sweep over every `NonMatching` unit that has a source (~2 s each).
**Keep:** reading the code diff for legitimacy (assembly, raw offsets, deleted initialisations),
tree-wide type decisions such as `rc_ptr`, which ranges a split claims, and blocker write-ups.
**For the 86 RELs:** the module should be the unit of ownership, with all of its config in its
directory. **For the DOL tail:** rank by linked yield, not fuzzy percentage.

### 5. Top 5 by value/effort

1. `gate.sh`, with ninja's exit code as the hash gate. *(done)*
2. `report_diff.py` in the gate, and linked functions as the headline. *(done)*
3. Harden `flip_test.sh`. *(done)*
4. Per-module config files plus `git apply --3way` collection. *(open)*
5. Promote Puffer, delete the stub tools, fix `unit_fit.sh`'s REL path. *(done)*

### 6. Doc statements to correct

*(All corrected the same day; kept here as the record of what was wrong.)*

- "configure.py refuses to run if a Matching object has no source" - false, it links the retail object.
- "`auto_*` units count as matched by default" - the report has 791 `auto_*` units with 0 of 24,456
  functions matched. The Puffer "2633→2629" explanation needs another cause.
- "`check_symbol_names.py` checks the whole tree" / "catches a rename that breaks every REL link" - it
  covers the 100 DOL `.cpp` units and largely compares the file with itself.
- "87 files OK is not evidence" - true only for `build-clone/`; in `build/` a green ninja *is* the
  `config.yml` check.
- The state block's DOL/REL counts are fuzzy, not the one rule's count.
- Stale in place: the AIMannedTurret rows, "`CPatterned`/`CAi` still do not exist", "75 need the
  missing Enemy hierarchy", Ripper's "no CPatterned", the `CScriptCannonBall` row, and HANDOFF's
  "6.75%".
- "a full build is minutes" (3.8 s cold); the lane model's "Luna workers" heading beside a Space Bunny
  practice; the 6-8 lanes per wave limit beside ~20 actual lanes.
- `LANE_BRIEFING.md`: "can never be Matching, however good its percentages look" contradicts
  `unit_fit.sh`'s "harmless causes first" (CAi flips with 224 extra bytes).
