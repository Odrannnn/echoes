# progress-prime1-cscriptplatform-teleport

`MetroidPrime/ScriptObjects/CScriptPlatform` — `progress` item, unit stays `NonMatching`.

## Result

| | before | after |
|---|---|---|
| unit `matched_functions` | **44** / 60 | **46** / 60 |
| unit `matched_code` | 6320 B | **6784 B** (of 18000) |
| unit `fuzzy_match_percent` | 44.94% | **47.46%** |
| tree `matched_functions` | 11386 / 28465 | **11388** / 28465 |
| `linked` | 5507 | 5507 (unchanged) |
| port undefined | 250 | 250 (unchanged) |

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11386 -> 11388   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.72% fuzzy, 25.47% matched, 11.94% linked (11388 / 28465 functions)
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptPlatform: 44 -> 46 / 60 functions
  ok    no asm added
goal_check: PASS progress-prime1-cscriptplatform-teleport
```

Two new functions at **100.00%**: **`TeleportToWaypoint__15CScriptPlatformF9TUniqueIdR13CStateManager`**
(0x800A2030, 160 B) and **`SetMotionTime__15CScriptPlatformFfR13CStateManager`** (0x800A20D0, 304 B).
Nothing in the unit or the tree went below a score it had.

## `reason` was wrong about the blocker, and that is the finding

`reason` said the two `bl`s into `fn_801FAC1C` / `fn_801FAC14` "add a new undefined symbol to the host
port link and fail the gate's `port probe` and `port link gap` steps", and that the symbols therefore
had to be **defined in the port first**.

**They do not.** `tools/probe_sources.sh` compiles only the sources `files.cmake` lists, and
**`src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp` is not one of them** — `tools/check_files_cmake.py:445`
records the measured reason for excluding it (listing it takes the port's undefined count 318 -> 321
and closes nothing). So no host object ever references `fn_801FAC1C`, whatever this file calls.
Measured, after the change: `build/probe-logs/files.txt` has 0 hits for `CScriptPlatform`, the link
reports `LINKED (250 undefined, 0 duplicates)`, and `tools/link_undef_refs.py` produces a 246-line
list whose symbols are **identical to `build/goal/judge/undef.base.txt`** (`diff` of the sorted
symbol column is empty).

The declaration is therefore all that is needed, and the DOL side is free for a second reason:
`fn_801FAC14` and `fn_801FAC1C` are defined by `build/G2ME01/obj/auto_03_801FA3CC_text.o`, which the
DOL link already pulls in, so the two `bl`s resolve without this file defining anything. Verified by
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and by all 86 RELs
(the gate's `hashes vs config.yml ok` step).

**Generalisable, and it cost the previous run:** a new undefined symbol can only reach the port link
through a translation unit the port actually compiles. Before treating "the callee is not defined in
`src/`" as a blocker, check `grep <file> files.cmake` — most units are *excluded* from the host build
on purpose, and the "port link gap" list only ever counts what the host link asks for. The previous
note asserted the failure without having run the probe.

## `TeleportToWaypoint` — 0.00% -> 100.00%

```c
if (TCastToConstPtr< CScriptWaypoint >(mgr.GetObjectById(id)) != nullptr &&
    !mWaypointTracker.null()) {
  float time = fn_801FAC1C(mWaypointTracker.get(), id, mgr);
  if (time >= 0.f) {
    SetMotionTime(time, mgr);
  }
}
```

Three things decide it:

- **the two guards are one `&&`, not two `if`s with early returns.** Retail branches to the *same*
  epilogue address from both null tests (0x800A2070 and 0x800A207c), which is what `&&` emits; two
  nested `if`s with `return` produce the same bytes only because nothing follows, and `if (a) { if (b) }`
  is what keeps the branch targets identical without spelling the returns twice.
- **`TCastToConstPtr`, not `TCastToPtr`.** `CStateManager::GetObjectById` returns `const CEntity*`
  and `TCastTo.hpp`'s `TCastToPtr` overloads take a non-const pointer, so the direct spelling does not
  compile (mwcceppc: "function call 'TCastToPtr(const CEntity *)' does not match"). `TCastToConstPtr`
  is the repo's own `const_cast` wrapper and emits the identical `li r4,9` + `bl TCastToPtr__FP7CEntity`.
- **`fn_801FAC1C` takes three arguments even though it reads two.** Retail sets r5 (`mr r5,r31`, the
  `mgr` argument) before the call; declaring the third parameter `CStateManager& mgr` and passing it
  is what reproduces that `mr`. Dropping it loses an instruction.

## `SetMotionTime` — 0.00% -> 100.00%

```c
mMotionTime = time;
if (!mWaypointTracker.null()) { fn_801FAC14(mWaypointTracker.get(), time); }
mMotionForward = true;
mPreviousMotionForward = true;
mPassedMotionEnd = false;
mPassedMotionStart = false;
Stop();
CVector3f pos = GetTranslation();
if (!mSplineController.null() && mSplineController->GetPositionKnotCount() != 0) {
  pos = mSplineController->GetPositionByTime(time);
}
SetTranslation(pos);
if (!mStaticSlaves.empty() || !mDynamicSlaves.empty()) {
  TMovedList moved;
  DragSlaves(mgr, moved);
}
```

- **the four flag writes are in that exact order**, each its own read-modify-write, because retail
  stores back to 0x48c, 0x48d, 0x48c, 0x48c. Verified against the bitfield encoding already
  confirmed by three bodies in this unit at 100% (`StopMotion`, `fn_800a1df8`):
  `mDead` is byte 0x48c with `MB=24`, `mMotionActive` byte 0x48c `MB=28`, `x48d_25_` byte 0x48d `MB=25`.
  Retail's four here are `MB=31` (0x48c), `MB=24` (0x48d), `MB=29` (0x48c), `MB=30` (0x48c), which is
  `mMotionForward`, `mPreviousMotionForward`, `mPassedMotionEnd`, `mPassedMotionStart` in declaration
  order. Note the byte alternation: the second write is 0x48**d** and the rest 0x48**c**, so the order
  cannot be rearranged.
- **`pos` is a named local, not a second `SetTranslation` call.** `GetTranslation()` (0x54) is copied
  into `r1+20` up front and the spline result overwrites that slot; `SetTranslation(pos)` then takes
  `r1+20`. Writing `SetTranslation(GetTranslation())` first and a second `SetTranslation(...)` in the
  spline arm emits an extra copy.
- **the `&&` over the two spline conditions is what produces retail's two tests** (`cmplwi r3,0; beq`
  on the controller at 0x800A20B4, then `cmplwi r3,0; beq` again on `GetPositionKnotCount()`).
- **the slave guard is `||` over two `empty()` calls**, at 0x400 (`mStaticSlaves.mCount`) and 0x410
  (`mDynamicSlaves.mCount`); retail's shape is `bne` after the first and `beq` after the second.
- `TMovedList moved;` is the only construction: `rstl::reserved_vector`'s `mCount(0)` is the single
  `stw r0,32(r1)`, and its 2048-byte inline buffer is left uninitialised.

## One new weak COMDAT, which is the expected cost

`tools/unit_fit.sh` goes from 32 extras / 3128 B to **33 / 3188 B**. The new one is
`__dt__Q24rstl24reserved_vector<Us,1024>Fv`, 60 bytes, emitted because `TMovedList moved;` is a real
object with a destructor. It is **`W` (weak COMDAT)** in the object
(`powerpc-eabi-nm`: `000012fc W __dt__Q24rstl24reserved_vector<Us,1024>Fv`), the same family as the 32
that were already there, and the DOL still hashes to `6ef9b491...`, so mwldeppc discards it. It is not a
new *name* in the link.

## Things the next run on this unit should know

**`AdvanceMotionTime` (0x800A3B64, 436 B) is still 0.92%** and `docs/goal-notes/progress-prime1-cscriptplatform-bodies.md`
decodes all but the `fmod`-shaped expression at 0x800A3C48. Every callee it needs already exists, so it
adds no undefined symbol. The remaining expression is CodeWarrior's inline `fmodf`:
`f5 = 1.0f / duration`, `fctiwz` of `mMotionTime * f5` into a double, `xoris` 32768 into a second
double, `f0 = <double> - 0x4330000080000000`, then `fnmsubs f0,f0,f4,f3` — i.e.
`mMotionTime - (<double> - 0x4330000080000000) * duration`. A plain `fmodf(mMotionTime, duration)` is
the obvious first spelling and nobody has measured it.

Still unwritten and still blocked or unwritten: the constructor (42.52%, 1388 B), `PreThink` (0.24%,
1688 B), `Move` (1.58%, 2088 B), `AcceptScriptMsg` (1.98%, 1608 B), `MoveRiders` (0.45%, 888 B),
`DragSlave` (0.54%, 736 B), `DragSlaves` (0.83%, 484 B), `Think` (0.75%, 536 B), and
`fn_800A1CE8` / `fn_800A1D4C` (0.00%), the last two still blocked on `lbl_803B32B0`.
`DragSlaves` is now the blocker for `SetMotionTime`-adjacent work: it calls `fn_800B8038` (0x800B8038),
which no `src/` defines, and **that** one *would* be a real undefined-symbol problem if the unit were
ever listed in `files.cmake` — unlike `fn_801FAC1C`, `CScriptPlatform.cpp` is excluded today but a
future lane that lists it needs `fn_800B8038` in `src/` first.

`Prime 1's own decomp is on disk at `../../prime-ref`** (relative to the worktree) and is the fastest
route into `PreThink` / `Move` / `MoveRiders` / `DragSlaves` / `AcceptScriptMsg` / the constructor.

## A build gotcha that cost time in this run

Deleting `build/G2ME01/obj/<unit>.o` (or `build/G2ME01/config.json`) and running `decomp_build.sh` fails
with `ninja: error: '<obj>', needed by 'build/G2ME01/main.elf', missing and no known rule to make it` —
the `obj/` copies are the output of the `split` rule (`dtk dol split config/G2ME01/config.yml
build/G2ME01`), and nothing re-runs it unless `build/G2ME01/config.json` is itself out of date. Recovery:

```sh
rm -f build/G2ME01/config.json
$MP_TOOLCHAIN_DIR/build/review-tools/bin/ninja build/G2ME01/config.json   # re-splits, ~4 s
./tools/decomp_build.sh
```

While recovering, `tools/fast_try.sh` printed a **stale report** (the previous build's `44/60`) after a
failed ninja, and `unit_fit.sh` then compared against a missing retail object and invented a list of
**91** extras instead of 33. Both are silent-wrong, not loud-wrong: check that the object exists and
that the report's function count moved before believing either.

## Verified

```
sha1sum build/G2ME01/main.dol                        -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh                              -> All: 32.72% fuzzy, 25.47% matched, 11.94% linked (11388 / 28465 functions)
python3 tools/check_symbol_names.py                  -> checked 514 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform
                                                     -> ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CScriptPlatform.cpp
                                                     -> 33 extras / 3188 B (was 32 / 3128); the new one is a weak COMDAT destructor
./tools/goal_check.sh build/goal/item.json           -> PASS (block quoted at the top)
```

`gate.sh` is GATE PASS on every step, `docs claims` included — the judge rewrites the state block itself
(`tools/gate.sh` runs `check_docs_claims.py ${MP_GATE_DOCS_WRITE:+--write}` and `goal_check.sh` invokes
the gate with `MP_GATE_DOCS_WRITE=1`), so the `docs/HANDOFF.md` hunk in this tree is its doing, not mine.
All 86 RELs `cmp`-equal with sha1s matching `config/G2ME01/config.yml` is the gate's own
`hashes vs config.yml ok` step.

Diff is **one source file**, `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp`: two bodies, one
include, two `extern "C"` declarations and the comment above them. No `.s`, no `asm`, no `tools/`, no
`config/`, no hand edit to `build/goal/` or `docs/`. Not committed.

## NEW:

None. The item's own reason was the blocker and it did not hold; nothing new was found that raises a
count.