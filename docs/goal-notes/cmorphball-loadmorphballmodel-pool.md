# cmorphball-loadmorphballmodel-pool

Fixed the `.rodata` **string-pool** order this item's `reason` named. Three functions reached
**100.00%** and the unit rose **101 -> 104 / 158**. `./tools/goal_check.sh build/goal/item.json`:

```
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11429 -> 11432   linked 5565 -> 5565
  ok    check_symbol_names.py
  ok    All:  32.82% fuzzy, 25.64% matched, 12.06% linked (11432 / 28465 functions)
  flip  flip_test: FAIL - judged below as partial progress
        undefined: 'CElementGen::GetEmitterTime() const'
        undefined: 'fn_800CD4B8'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 101 -> 104 / 158 functions
  ok    no asm added
goal_check: PARTIAL ... - flip_test FAIL, but the target rose
```

Per-function, measured against `build/report.base.json` (**no function anywhere got worse**):

| function | before | after |
|---|---|---|
| `CreateBallShadow` | 99.96825% | **100.00000%** |
| `UpdateIceBreakEffect` | 99.99083% | **100.00000%** |
| `UpdateMorphBallTransitionFlash` | 99.99083% | **100.00000%** |
| `GetMorphBallModel` | 84.10000% | 84.13750% |
| `InitializeWakeEffects` | 99.72932% | 99.73684% |
| `__ct__10CMorphBall` | 96.26492% | 96.66734% |

## The cause, measured

Every one of the four near-100% functions differed from retail by **one instruction**: the
`addi r4,rX,<offset>` that computes the address of `"??"(??)"`, the `operator new` placement
string every `rs_new` site passes. Retail loads pool offset **378**; we emitted **682**.
That single displacement was the whole of `CreateBallShadow`'s 0.03%, both `Update*Effect`'s
0.01%, and most of `InitializeWakeEffects`' 0.27%.

`"??"(??)"` reaches offset 378 only if every literal ahead of it is at retail's offset. Two
things were wrong, and both are the same rule:

**`@stringBase0` is ordered by first use, in mwcceppc's emission order, which is reverse
source order.** Probe files (`tools/probe_cc.sh`, this unit's own flags) confirm it three ways:
a string first used in a function declared *last* lands *first* in the pool; file-scope
declarations are interned ahead of all function bodies; and two functions' literals always
appear as one contiguous run in reverse declaration order.

### 1. The two ctor-only literals were interned too late

Retail has `""` at 194 and `"SamusMultiBallANCS"` at 195 - right after the nine model tables and
**before** `InitializeWakeEffects`' twelve wake names (214..377). Inlined in the constructor they
interned after the wake names, which pushed everything down. Hoisting them to file scope as
`static const char* const` (lines 60-61) puts them at 194/195.

`const char* const`, **not `char[]`**: with `char[]` mwcceppc emits them as `.data` objects
(`nm` shows `d kNoModelName` / `r kMultiplayerBallModelName`) and they leave `@stringBase0`
entirely - measured, and the pool reverted to the wrong shape.

### 2. `CreateBallShadow`/`DeleteBallShadow` declared at the end of the file

They are retail's *first two* functions, but the pair's `TXTR_BallFade` has to intern at 385 -
after the wake names, before the ctor's `SlowBlueTailSwoosh*`. Where a function sits in this file
decides where its literals land, so the pair is now declared last (lines 1962+). This is the one
thing in the diff that looks like a gratuitous move; the comment there says why.

Net: the pool now matches retail **byte-for-byte from offset 0 through 385**, verified by
comparing our `.rodata` against `python3 tools/dol_read.py 0x803A86F0 0x2F0`.

## What is still wrong, and is NOT the pool

`InitializeWakeEffects` is 99.74% and its remaining diff is **register allocation only**: retail
keeps the pool base in `r18` and zero in `r19`, we keep them the other way round (retail's
`stw r19,32(r18)` vs our `stw r18,32(r19)`, plus the matching `lis`/`addi` pairs). Same
instructions, swapped registers - not reachable by reordering declarations.

`GetMorphBallModel` is 84.14% and is **not** a pool problem at all any more. Retail's frame is
96 bytes against our 80, and retail tests `__eq__`'s result with `clrlwi. r0,r3,24` / `beq` where
we emit `cmpwi r3,0` / `bne`. The frame gap is the whole story. One spelling tried and
**rejected by measurement**: rewriting the empty-name test as `if (name != "") { ... } return
nullptr;` scores **81.14%**, worse than the 84.14% above - do not retry it.

## Two literals still absent

Retail's pool has `Locomotion` (399) and `BallLight` (410) between `TXTR_BallFade` and
`SlowBlueTailSwoosh_MP`, and we emit neither, so every literal from 399 on sits 21 bytes early.
I could not place them: **a bare declaration cannot be interned between two function bodies.**
Declaring them after the ctor gives 378/389 (too early), declaring them among the file-scope data
gives 214/225 (far too early). Their only users in retail are `UpdateScrewAttackRecovery`
(0x800C7A88) and `AcceptScriptMsg` (0x800CA808), both still scaffolds here, so they cannot be
interned from real code until those two bodies exist. This is a dependency, not a spelling.

`NEW: cmorphball-loadmorphballmodel-pool-still | match | MetroidPrime/Player/CMorphBall | the pool's last 21 bytes cannot be closed until UpdateScrewAttackRecovery (0x800C7A88) and AcceptScriptMsg (0x800CA808) have bodies - they are the only users of the "Locomotion" and "BallLight" literals, and a bare declaration cannot intern between two function bodies`

## Gates

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; full
`./tools/decomp_build.sh` clean; `python3 tools/check_symbol_names.py` = 0 missing;
`goal_check.sh` gate green including all 86 RELs, wiring and docs claims.

`docs/HANDOFF.md` was reverted: `./tools/decomp_build.sh` rewrites the state block as a side
effect, and the brief says not to edit it.