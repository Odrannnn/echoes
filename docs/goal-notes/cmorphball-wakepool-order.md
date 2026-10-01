# cmorphball-wakepool-order (match, `MetroidPrime/Player/CMorphBall`)

Lane 6, 2026-10-01. **Result: `goal_check` PARTIAL** - the unit's matched count rose
**105 -> 107 of 158**, every other check green. The flip still fails, on the unit's
**unwritten** bodies, unchanged by this diff.

## The item as filed is mostly stale, and re-measuring it is what found the work

The `NEW:` line that queued this said the unit's `.rodata` string pool is out of order and
pins four functions at 99.73-99.99%. **At HEAD that is no longer true**, and the fix is
already in the tree: `src/MetroidPrime/Player/CMorphBall.cpp:61-68` hoists
`kNoModelName` / `kMultiplayerBallModelName` to the top of the TU precisely to get the pool
offsets right. Measured, per function, by disassembling both objects and extracting every
`lis rX,0` / `addi rX,rX,0` / `addi rY,rX,<off>` string materialisation (throwaway script,
`tools/dol_fd.py`-style, deleted; the command is in "Reproduce" below):

| pool offset | string | referenced by (retail) | referenced by (ours) |
|---|---|---|---|
| 0x0C2 | `""` | `fn_800D0584` (188 B, **no body in ours**) | - |
| 0x18F | `Locomotion` | `UpdateScrewAttackRecovery` (1328 B, scaffold in ours) | - |

**That is the whole list.** Retail's 158 functions contain exactly two string references
between them, both in functions this tree has not written, and our object contains **zero**
string references. So the pool order is now correct for every function that exists in our
object, and `CreateBallShadow`, `UpdateIceBreakEffect` and
`UpdateMorphBallTransitionFlash` (three of the four the item named) are already at 100% at
HEAD. `BallLight` at 0x19A and `TXTR_BallFade` at 0x181 are in the pool but referenced by no
function in this unit at all, so they are not this unit's to account for.

What was left was two functions a hair under 100% for reasons that had nothing to do with the
pool. Both are fixed below.

## 1. `GetMorphBallModel` 99.9375% -> 100.00% - and it was returning null for every real name

`src/MetroidPrime/Player/CMorphBall.cpp:1002-1014`. The function was spelled
`if (!rstl_string_eq_c(name, "")) { return nullptr; }`. Retail's `beq` at 0x800C12E0 is taken
when `rstl_string_eq_c` returns **false**, i.e. retail branches *into* the body and the
**empty** name is what returns null. Ours had `bne` - the one differing instruction out of 80 -
and the inverted semantics with it: the constructor asks for `kNoModelName` when it wants no
model (`mSpiderBallGlassModel`, 0x800C0758) and passes "SamusBallCMDL",
"SamusBallLowPolyCMDL", "SamusBallFrozenCMDL" and the eleven table entries when it wants a
model, so as written the function returned null for all of those and built a model from `""`.

The spelling is the same codegen rule three other bodies in this file already established
(`fn_800C084C`, `GetGravityAcceleration`, `IsMovementAllowed`): **mwcceppc branches to the
continuation when the `if` condition is false**, so to get retail's `beq` over the
`return nullptr` the source condition has to be the equality, not its negation. Writing the
negation back measures 99.9375% with a `bne`; the equality measures 100.00% with a `beq`,
every other instruction and the frame unchanged.

**This also corrects a wrong claim in the tree.** The comment above `rstl_string_eq_c` said
the last 0.0625% was "one relocation out of 160" - the call site's `R_PPC_REL24` naming
`rstl_string_eq_c` where retail names `__eq__4rstlF...` - and that it was unreachable from
C++. It was the branch: **objdiff normalises a `R_PPC_REL24` target**, so the renamed symbol
costs nothing and the function is 100.00% with the reloc still named `rstl_string_eq_c`. The
comment is corrected in place (`:120-138`) so nobody spends a run trying to rename the symbol.
The rename is still not expressible (MWCC rejects both `asm("...")` labels on a function and
namespace-scope `__asm__`, measured), it is just not needed.

## 2. `InitializeWakeEffects` 99.7368% -> 100.00% - one register swap

`src/MetroidPrime/Player/CMorphBall.cpp:849-861`. The whole 532-byte function differed from
retail in **seven instructions, all the same r18/r19 swap**, and nothing else: the frame, both
local arrays, all six pool-relative loads and every `bl` matched.

```
retail   addi r18,r5,0  ; li r19,0 ; stw r19,32(r18) ; stw r20,36(r18) ; ...
ours     addi r19,r5,0  ; li r18,0 ; stw r18,32(r19) ; stw r20,36(r19) ; ...
```

Retail copies the vector's address into its own register once and puts the value 0 in a
second one. Four separate `operator[]` calls give the constant the lower register instead.
Hoisting one pointer - `EWakeEffectIndex* const indices = sWakeEffectForMaterial.data();` and
four `indices[kMT_*] = ...` - reverses the assignment and the function becomes **0 differing
lines** (`tools/dol_fd.py MetroidPrime/Player/CMorphBall InitializeWakeEffects`: "133 retail
insns, 133 ours, 0 differing lines"). One spelling, no search: the two candidates were
`operator[]` and a hoisted `data()`, and the hoisted one is the one that reproduces the bytes.

## Measured

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11456 -> 11458   linked 5587 -> 5587
  ok    check_symbol_names.py
  ok    All:  32.85% fuzzy, 25.72% matched, 12.17% linked (11458 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'fn_800CD4B8'
                                         'CAnimRes::kDefaultCharIdx'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 105 -> 107 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-wakepool-order - flip_test ...: FAIL, but the target rose;
commit it and keep the item

$ python3 tools/report_diff.py <report at HEAD> build/report.json
  matched 11456 -> 11458  linked 5587 -> 5587  (+2 functions at 100%, 0 units newly linked)
    +100%  main/MetroidPrime/Player/CMorphBall :: GetMorphBallModel__10CMorphBallFRCQ24rstl...
    +100%  main/MetroidPrime/Player/CMorphBall :: InitializeWakeEffects__10CMorphBallFv
  no regression
```

Unit `matched_code` **13436 -> 14288** of 66600, `.text` fuzzy **28.954535 -> 28.956938**.
Per function, `build/report.json`: `GetMorphBallModel` (320 B) **99.9375 -> 100.0**,
`InitializeWakeEffects` (532 B) **99.73684 -> 100.0**, every other function in the unit and
every other unit unchanged (`report_diff.py` over all 2066 units: +2, 0 worse, 0 changed).

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; all 86 RELs
(`hashes vs config.yml ok`); `build/gate-probe.log`: `probe: 752 files, 0 failed, 0 errors;
link: LINKED (250 undefined, 0 duplicates)` - 250, unchanged from the judge baseline, so
this diff adds no undefined symbol. `check_symbol_names.py`: 514 units, 0 missing.
`unit_fit.sh`: **51** functions present in ours but not in the retail unit object, 5272
bytes - identical before and after this diff, since both hunks touch existing functions and
add no symbol. `check_docs_claims.py`: ok (via gate.sh). `docs/HANDOFF.md` shows as modified
after `goal_check.sh`; that is `MP_GATE_DOCS_WRITE=1` rewriting the derived counts, and it
was reverted.

### Reproduce

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
python3 tools/dol_fd.py MetroidPrime/Player/CMorphBall GetMorphBallModel InitializeWakeEffects
python3 tools/dol_read.py 0x803A86F0 0x2F0        # retail's pool, pool offset 0x18F = Locomotion
./tools/decomp_build.sh >/dev/null && python3 tools/report_diff.py <report at HEAD> build/report.json
```

## Still open in this unit, measured not guessed

- **The flip is stopped by unwritten bodies, not by anything in this diff.** `flip_test.sh`
  fails to link with `undefined: 'fn_800CD4B8'` and `'CAnimRes::kDefaultCharIdx'`. Note the
  pair is **not** the one the previous run recorded (`CElementGen::GetEmitterTime() const` and
  `fn_800CD4B8`): `GetEmitterTime` is defined at `:118` now, so `CAnimRes::kDefaultCharIdx` -
  a static data member, not a function - is the new one in the list.
- **10 of the unit's 158 functions still have no body** (`fuzzy_match_percent: None`):
  `fn_800D0584` (188), `fn_800CD4B8` (152), `fn_800CD460` (88), `fn_800CD35C` (260),
  `fn_800CD244` (280), `fn_800C5420` (444), `fn_800C9380` (48), `fn_800C93B0` (72),
  `fn_800C88C0` (92), `fn_800C33DC` (92). A carve is the only route and that is four files.
- `check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` still reports the unit as
  out of retail order, identical at HEAD and already listed in
  `docs/research/decl_order.md:101`. That is the next wall after the unwritten bytes.
- Walls carried forward from earlier runs, **not retried here** (they belong to other items
  and are recorded there): `ComputeMaxSpeed` 96.84%, `GetRenderBounds` 96.48%,
  `fn_800C8CE0` 74.05%, `GetSpiderBallControllerMovement` 94.51%,
  `DampLinearAndAngularVelocities` 57.27%.

No `NEW:` line from this run: the pool order it was queued for is already correct, and what
is left in this unit (the unwritten bodies, then the declaration order) is already covered by
the queued `cmorphball-wakeeffects-carve-74-unwritten` and `docs/research/decl_order.md`.
Filing a third item for the same target would cost a lane an hour to re-derive this.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp`
  - `:120-138` - the `rstl_string_eq_c` comment: the residual 0.0625% was the branch, not the
    relocation, and objdiff normalises a `R_PPC_REL24` target
  - `:849-861` - `InitializeWakeEffects`' four writes through one hoisted `data()`
  - `:1002-1014` - `GetMorphBallModel`'s empty-name test, and the measurement behind it

Not committed, per the brief.
