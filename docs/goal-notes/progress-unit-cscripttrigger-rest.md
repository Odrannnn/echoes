# progress-unit-cscripttrigger-rest

`kind: progress`, target `MetroidPrime/ScriptObjects/CScriptTrigger`. The unit stays `NonMatching`;
`flip_test.sh` is not the acceptance test here. The judge is `build/report.json`'s per-unit
`matched_functions`.

## Result

**16 -> 18 of 44 matched functions.** `./tools/goal_check.sh build/goal/item.json` prints
`PASS progress-unit-cscripttrigger-rest`; the whole-project matched count went 11893 -> 11895 with
`linked` unchanged at 5727 and the `All:` line unmoved at 33.64% fuzzy / 26.80% matched.

Two functions reached 100%, which is what raises the count:

| function | before | after |
| --- | --- | --- |
| `RemoveInhabitant__14CScriptTriggerF9TUniqueIdR13CStateManager` | 96.67% | **100%** |
| `RemoveInhabitantIfOutside__...F9TUniqueIdR13CStateManager` | 99.38% | **100%** |

`ReplaceInhabitant` rose 97.52% -> 98.39% but did not reach 100%, so it does not count yet.

## The change: retail's return type is `uchar`, not `bool`

Six lines, three declarations and three definitions, all the same edit:

- `include/MetroidPrime/ScriptObjects/CScriptTrigger.hpp:62-64` - `bool` -> `uchar` on
  `RemoveInhabitant`, `RemoveInhabitantIfOutside`, `ReplaceInhabitant`.
- `src/MetroidPrime/ScriptObjects/CScriptTrigger.cpp:170, 198, 219` - the matching definitions.
- `src/MetroidPrime/PortGlobals.cpp:1461, 1466` - the two port stand-ins for these methods, which
  were declared `bool` and would no longer match the header. Required: the build fails without it.

**Why `uchar` and not `bool`.** Both functions' only remaining difference from retail was the
final instruction: retail's `clrlwi r3,rX,24`, ours `mr r3,r3`. `clrlwi rD,rS,24` is a
sign-extend of the low byte - what mwceppc emits when a **1-byte** value is returned, not when a
`bool` is. Changing the return type to `uchar` reproduces it byte-for-byte, and nothing else in
either function moves.

This was not a guess. `src/MetroidPrime/BodyState/CBSJump.cpp:267` already had the exact shape:
`uchar CBSJump::CheckForWallJump(...)` with an `int ret` local, returning
`clrlwi r3,r31,24` at `0x2f4`. Its object is the same compiler. Retail uses `uchar` for
several C++-`bool`-looking predicates (`CBSJump::CheckForWallJump`, `CBSJump::CheckForLand`), so
the trigger's three are consistent with that.

The flag *locals* stay `bool` in the source; that is what produces retail's `li r30,0` /
`li r30,1` / `clrlwi r3,r30,24`. Widening the *local* instead of the return type is not the same
edit and does not work - see the measured list below.

## Files touched

- `include/MetroidPrime/ScriptObjects/CScriptTrigger.hpp` (3 lines)
- `src/MetroidPrime/ScriptObjects/CScriptTrigger.cpp` (3 lines)
- `src/MetroidPrime/PortGlobals.cpp` (2 lines, forced by the header)

No `configure.py`, `splits.txt` or `files.cmake` change: nothing was carved and no symbol was
claimed, so the four-file carve rule does not apply. `python3 tools/check_decl_order.py --unit
MetroidPrime/ScriptObjects/CScriptTrigger` says `ok: 1 unit(s) checked, none emits its functions
out of retail order`.

## Things measured, so nobody repeats them

Every number below is `./tools/fast_try.sh MetroidPrime/ScriptObjects/CScriptTrigger` after a
rebuild of just that object, editing **one function at a time**.

Return type / flag type on `RemoveInhabitantIfOutside` (retail is one instruction away; the point
is which spelling produces `clrlwi`):

- return `uchar`, local `bool removed` -> **100%** (what landed).
- return `uchar`, local `uchar` -> 97.52%. return `uchar`, local `int` -> 98.39%.
- return `bool`, local `bool` -> 99.38% (the pre-existing state; `mr r3,r30`, one instruction
  short).
- return `bool`, local `int`/`uint` -> 97.76%, tail becomes `neg r0,r30 / or r0,r0,r30 /
  srwi r3,r0,31` - the compiler's full nonzero test, three instructions where retail has one.
- return `bool`, local `char` -> 96.72%. return `bool`, local `short` -> 96.72%.

So the conversion is produced by the **return** type's width, not the local's. `uchar` local +
`uchar` return is wrong (97.52%) because the local then also needs widening.

Other spellings tried on `ReplaceInhabitant`, all measured, none reaching 100%:

- `it->mId = newId` instead of `it->SetObjectId(newId)` (the setter is a one-line inline, so this
  should be identical - it is) -> 98.39%, no change.
- `int replaced = false` -> 98.39%. `uchar replaced` -> 97.52%.
- `const bool inside = HasInhabitant(newId);` hoisted, then `if (!inside)` -> 98.39%.
- `this->HasInhabitant(newId)` -> 98.39%. `static_cast<TUniqueId>(newId)` at the call -> 98.39%.
- Setting `replaced = true;` *before* `it->SetObjectId(newId)` -> 98.39%.
- Dropping the two `const CActor*` actor locals and testing the casts inline -> **74.41%**
  (retail saves `oldActor` in `r31` across the second call, so the locals are load-bearing).
- `auto` instead of `rstl::list<CObjectTracker>::iterator` -> 93.10%. Same change with the flag
  untouched is a control, so `auto` really is worse.

## A dead end worth recording: renaming retail's unnamed functions does not pair them

`fn_8007334C`, `fn_800733E0` and `fn_8007298C` are **byte-identical** to three COMDAT template
functions our object already emits - `do_erase__Q24rstl67list<Q214CScriptTrigger14CObjectTracker...>4node`,
`__dt__Q24rstl67list<...>Fv` and `push_back__Q24rstl43list<9TUniqueId,...>FRC9TUniqueId` - verified
instruction by instruction (`/tmp` script comparing objdump output, 0 differing words, same three
relocation targets). They stay at 0.00% because objdiff pairs by name and retail's `symbols.txt`
gives them `fn_*` names. Renaming the three entries in `config/G2ME01/symbols.txt` to the mangled
names was tried and **did not raise the count** (still 16/44, both still `None` in the report), so
objdiff is not reading the names from the file the build points at. Reverted - `symbols.txt` is
untouched. Worth knowing before a lane spends an hour on it; the three functions are reachable
some other way, if at all.

## Still open in this unit (26 functions unmatched, measured)

`UpdateInhabitants` 1520 B at 0.26%, `Touch` 1044 B at 0.38%, `AddInhabitant` 944 B at 0.42%,
`UpdateCameraInhabitant` 664 B at 0.60%, `ClearInhabitants` 224 B at 1.79%, `SetPlayerInside`
240 B at 1.67%, `HasInhabitant` 148 B at 39.4%, `AcceptScriptMsg` 228 B at 73.2%, the constructor
at 93.8%, and 11 unnamed `fn_*` at 0.00%.

`HasInhabitant` at 39.4% is the cheapest of these and is now the best next target. Retail's body is
148 B and spills a copy of the tracker node to the stack (`stw r9,8(r1) / stw r0,16(r1) /
stw r0,20(r1) / stw r9,12(r1)`) before walking `mTriggers` - i.e. retail iterates a **copy** of the
tracker's trigger list, not the tracker's own list. The current source reads
`it->GetTriggers()` on the live element. Whoever takes it should try copying the tracker first;
that is the whole 60% gap.

WALL: ReplaceInhabitant__14CScriptTriggerF9TUniqueId9TUniqueIdR13CStateManager 98.39% - the
remaining difference is one extra spill of the stack-copied `TUniqueId` and a 4-byte stack-slot
shift (retail `r1+16`/`r1+12`/`r1+8`, ours `r1+20`/`r1+16`/`r1+12`); the ten spellings above all
shift it without closing it, and the remaining difference is register allocation, not logic.

NEW: progress-unit-cscripttrigger-rest2 | progress | MetroidPrime/ScriptObjects/CScriptTrigger |
`HasInhabitant` at 39.4% is the cheapest remaining function and retail's shape is now known - it
copies the tracker's trigger list onto the stack before walking it, the current source walks the
live list; `AcceptScriptMsg` (73.2%) and the constructor (93.8%) are next.