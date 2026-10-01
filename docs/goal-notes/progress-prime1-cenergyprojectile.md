# progress-prime1-cenergyprojectile

`kind: progress`, target `main/MetroidPrime/Weapons/CEnergyProjectile`, unit stays `NonMatching`.
No `flip_test.sh` was run: this is a `progress` item judged on `build/report.json`'s per-function
exact matches, and the unit is nowhere near a flip (`Explode` 11.9%, `AcceptScriptMsg` 51.8%,
sixteen unnamed `fn_*` at 0%).

## The item passed

`./tools/goal_check.sh build/goal/item.json` in the lane worktree:

```
goal_check: item progress-prime1-cenergyprojectile (progress) target=MetroidPrime/Weapons/CEnergyProjectile
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11497 -> 11498   linked 5587 -> 5587
  ok    check_symbol_names.py
  ok    All:  32.91% fuzzy, 25.77% matched, 12.17% linked (11498 / 28465 functions)
  ok    target rose: main/MetroidPrime/Weapons/CEnergyProjectile: 10 -> 11 / 34 functions
  ok    no asm added
goal_check: PASS progress-prime1-cenergyprojectile
```

## Measured result

`main/MetroidPrime/Weapons/CEnergyProjectile`: **41.08% -> 42.32% fuzzy**, matched functions
**10 -> 11 / 34**, matched code 9.35% -> 11.26%. Global `build/report.json` matched
**11497 -> 11498**, linked unchanged at 5587.

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  11497 -> 11498   linked 5587 -> 5587   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Weapons/CEnergyProjectile :: InitializeMuzzleOffset__17CEnergyProjectileFfR13CStateManager
no regression
```

Only `src/MetroidPrime/Weapons/CEnergyProjectile.cpp` was edited (7 insertions, 6 deletions, two
hunks). No header, no `configure.py`, no `config/`, no `files.cmake`, no `build/goal/`, no `.s`,
no asm. The `docs/HANDOFF.md` state block in `git diff` is machine-made: `gate.sh` ran with
`MP_GATE_DOCS_WRITE=1`, exactly as `goal_check.sh` invokes it.

## Per function, as the item asked: before%, after%, and how

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `InitializeMuzzleOffset` | 70.33% | **100.00%** | **no counterpart at all** - Prime 1 has no `InitializeMuzzleOffset` (0 hits in its .cpp and .hpp). Echoes-only; the fix is a named local. |
| `ResolveCollisionWithActor` | 83.44% | **97.07%** | **matched the shape, small edits**: Prime 1 already writes `if (!Explode(...)) { msgs } else { damage }`, which is the whole fix. Echoes sends `kSM_XHIT` + `kSM_XXDG` where Prime 1 sends one `kSM_Touched`, and hoists `*act.GetDamageVulnerability()` into the call where we keep a named `vulnerability`. |
| `Think` | 94.29% | 94.29% (not attempted) | not usable whole: Echoes' `Think` has the visor-volume test, the cooldown update, the projectile light and `mExplodePending`, which Prime 1 does not. Its lifetime test *is* the `if / else if` two-call-site form, which is L5's 94.29 -> 98.32 recipe. Left alone - see the wall below. |
| `ResolveCollisionWithWorld` | 81.76% | 81.76% (not attempted) | **wrong for Echoes**: Prime 1 wraps the body in `if ((GetAttribField() & (kPA_Wave \| kPA_ComboShot)) != (kPA_Wave \| kPA_ComboShot))` and `kPA_Wave` does not exist in this tree. L5's spelling work took it to 99.14%. |
| `Explode` | 11.87% | 11.87% (not attempted) | Prime 1's `Explode` is lines 69-212 (~143 lines) of a different generation of the class. Echoes' retail `Explode` is **4772 bytes / 1193 instructions** and its `.rodata` references 7 more literals than ours. |
| `PreRender` 89.36%, `AcceptScriptMsg` 51.81%, `__ct__` 99.99% | - | unchanged | not attempted; see the walls below. |

## What produced the +1: `InitializeMuzzleOffset` 70.33 -> 100.00

One edit, and it is the same class of fix as the `Render` and `PreRender` wins earlier in this
project: name the float temporary so `mwcceppc` schedules the bit-field store around the
arithmetic instead of after it.

```cpp
const CVector3f offset = muzzle.GetTranslation() - GetTranslation();
mHasMuzzleOffset = true;
mMuzzleOffset = offset;
```

The tree computed the subtraction inside the member assignment, so the `mHasMuzzleOffset` store
came first and the six `lfs`/`fsubs` followed. Retail interleaves them - `li r3,1` is hoisted to
`0x80168710`, the `lbz`/`rlwimi` pair sits in the middle of the float block, and `stb r0,1378`
lands between the second and third `fsubs` (`0x80168738`). Naming the value is what lets the
scheduler put the store there. With it every instruction and every offset matches: object frame
`stwu r1,-192`, objdiff 100.0%, 61 rows on both sides.

## What produced the other gain: `ResolveCollisionWithActor` 83.44 -> 97.07

Prime 1's spelling, and it is the whole change - the tree had the two arms the other way round:

```cpp
if (!Explode(result.GetPoint(), result.GetPlane().GetNormal(), type, mgr, vulnerability,
             actor.GetUniqueId())) {
  mgr.SendScriptMsg(&actor, GetUniqueId(), kSM_XHIT, kInvalidUniqueId);
  mgr.SendScriptMsg(&actor, GetUniqueId(), kSM_XXDG, kInvalidUniqueId);
  actor.SendScriptMsgs(kSS_ReflectedDamage, mgr, kInvalidUniqueId, kSM_None);
} else {
  CGameProjectile::ResolveCollisionWithActor(result, actor, mgr);
  ApplyDamageToActors(mgr, GetCurrentDamageInfo());
}
```

Retail tests the result as `clrlwi. r0,r3,24; bne 0x80168f54` and lets the **script-message arm be
the fall-through**, with the damage arm out of line. The tree emitted `beq` into the message arm
and had the damage arm inline, so the two blocks were permuted and every `r1` displacement after
them was wrong. Swapping the arms is a pure control-flow spelling - same semantics - and it fixed
the permutation. 83.44 -> 97.07.

## Spellings measured and rejected this run (do not repeat)

`ResolveCollisionWithActor`, all on top of the 97.07% state:

| spelling | score |
| --- | --- |
| the tree's original `if (Explode(..)) { damage } else { msgs }` | 83.44% |
| **`if (!Explode(..)) { msgs } else { damage }` (Prime 1's shape)** | **97.07%** (kept) |
| the kept shape plus `const TUniqueId actorId = actor.GetUniqueId();` for `mLastResolvedObj` | 97.07% (no change - GCC coalesces the two) |
| the kept shape, `actorId` for the member **and** passed to `Explode` | 97.03% |
| the kept shape plus `const TUniqueId hitActor = actor.GetUniqueId();` for the `Explode` argument | 97.08% (+0.01, not worth the extra line) |
| the kept shape plus named `const CVector3f forward` / `const CVector3f dir` for the two `GetTransform().GetForward()` sites | 92.82% |

## What is left, measured

### ResolveCollisionWithActor - 97.07%, and the gap is two dead stores

Every remaining difference is a **stack offset 4-16 bytes low** (retail `stwu r1,-192`, ours
`-176`), caused by exactly two instructions we do not emit. Both are dead stores of the actor's
uid, sandwiching the forward-vector build:

```
retail  80168dd4: lhz  r0,8(r5)      ; actor.GetUniqueId(), loaded once
        80168ddc: sth  r0,1010(r29)  ; mLastResolvedObj
        80168dec: sth  r0,56(r1)    ; <- 0x38, ours has no counterpart
        80168df0: stfs f0,100(r1)   ; the forward vector
        80168e00: sth  r0,60(r1)    ; <- 0x3c, ours has no counterpart
        80168e08: mtctr r12
```

Counting `sth r0,N(r1)` on both sides: **retail 13, ours 11**; the other eleven line up one-for-one
with retail's once the 16-byte frame shift is taken into account, and neither `0x38` nor `0x3c` is
ever read. So the source created two more `TUniqueId` objects that the allocator then spilled and
rematerialised. Four spellings (above) all leave the count at 11: mwcc coalesces every named local
into the single value it already has, so naming things cannot add a slot - the two extra slots are
a register-pressure artefact of the region between `mLastResolvedObj` and the first virtual call.

### Three functions cannot reach 100% at all, because of retail *symbol names* - measured

`objdiff` compares the **relocation target name**, not just the bytes, and dtk names a symbol
`fn_<addr>` whenever it cannot mangle it (a free/static function, or a local in the unit). Our
source can only ever emit a mangled C++ name, so a function that calls one can never be 100%.
Measured over the whole tree: of the **233 NonMatching units that have at least one function at
100%, zero** have a retail object that references a bare `fn_XXXXXX` symbol. The three walls:

| function | retail call | our call | blocks |
| --- | --- | --- | --- |
| `PreRender` 89.36% | `bl fn_800366E4` | `bl fn_800366e4__13CStateManagerFP6CActor` | the `!mgr.fn_800366e4(this)` row is `DIFF_ARG_MISMATCH` on the name alone |
| `ResolveCollisionWithWorld` 81.76% | `bl fn_80283750` | `bl BitPosition__13CMaterialListFUx` | same; this is the row L5 was one instruction from at 99.14% |
| `Think` 94.29% | `bl fn_8016B1C8` (local, this unit) | `bl Update__Q217CEnergyProjectile19CCollisionCooldownsFf` | same |

Their *other* differences are still real work (L5's `ResolveCollisionWithWorld` 99.14%, L8's
`PreRender` float block already instruction-identical), but no source spelling reaches 100% while
the name differs. Naming the three retail symbols properly is a `config/G2ME01/symbols.txt` job,
not a `CEnergyProjectile.cpp` one, and it would rename the symbols for every unit that calls them.

### `__ct__` 99.99% - the string literal, and where the missing ones are

The ctor's only differing instruction is the offset of its `string_l("GameProjectile")`: retail
`lis r11,0x803b; addi r11,r11,-26736; addi r4,r11,166` = `.rodata + 0xA6`, ours is `.rodata +
0x1F`. Decoded retail's whole 184-byte `.rodata` (`lbl_803A9790`) and the order is fixed:

```
+0x00 "ProjectileLight_GameProjectile"   +0x44 "HomingBlobSpread1"
+0x1f "??(??)"                            +0x56 "CHomingBlobImpact"
+0x26 "Projectile collision response"     +0x68 "DarkBlackHole"
+0x76 "HomingBlobSpreadRegularBeam"       +0xa6 "GameProjectile"  <- the ctor's
+0x92 "AnnihilatorImploder"
```

**Every one of the seven literals before `+0xA6` is referenced from `Explode` alone** (13
references to `+0x00` and one each to `+0x1f`, `+0x26`, `+0x44`, `+0x56`, `+0x68`, `+0x76`, `+0x92`;
first use in that order, which is the order mwceppc pools them in). So the ctor reaches 100% the
day `Explode` is written - adding the literals without the code that uses them would emit nothing,
and adding them in a stub purely to move an address is not a decompilation. L5's `NEW:` line for
this is the right characterisation; it is not a cheap win.

### Explode - 11.87%, unchanged

4772 bytes / 1193 instructions against a 38-line stub. Prime 1's version is a different generation
of the class. Unchanged this run.

## Other gates, measured

- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged).
- `./tools/probe_sources.sh` -> `752 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)`.
- `python3 tools/check_symbol_names.py` -> `checked 514 units; 0 declared names are missing`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/Weapons/CEnergyProjectile` ->
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `./tools/gate.sh` -> `GATE PASS  e4ac22e8+2 changed` (18 checks, all 86 RELs still match
  `config/G2ME01/config.yml`).

## Codegen rules worth keeping (not `NEW:` items)

- **`if (c) { A } else { B }` and `if (!c) { B } else { A }` are different code.** Retail lays out
  the arm that the *source* wrote first as the fall-through; the tree's inverted spelling put the
  arms in the wrong order and shifted every stack slot behind them. 83.44 -> 97.07 here, and it is
  the kind of change that looks like a no-op in review and is worth 14 points.
- **Naming a float temporary moves the bit-field stores around it.** The same edit pattern took
  `InitializeMuzzleOffset` 70.33 -> 100.00 here and `PreRender` 89.36 in an earlier run.
- **mwcc pools string literals in codegen order**, i.e. ascending retail address of the function
  that first uses them (which is descending source order, because definitions are emitted in
  reverse). That is why the ctor's literal is *last* in `.rodata` and why adding literals to one
  function moves a string reference in a different one.
- **A `TUniqueId` is passed by value as a pointer to a caller temp**, so every by-value `TUniqueId`
  argument creates a 4-byte stack slot. `ResolveCollisionWithActor` makes 13 of them; a source
  that names the value instead does not change the count, because mwcc coalesces the local with
  the argument temp.
- **dtk's `fn_<addr>` names are a hard ceiling on objdiff's per-function score.** Measured above;
  check the retail object's undefined symbols before spending a run on a function that calls one.

## NEW:

(new none - the only remaining blocker, `Explode`, is already filed as
`NEW: progress-ep-explode` by an earlier run of this item, and the three symbol-name ceilings are
a `config/G2ME01/symbols.txt` rename rather than work whose success raises a count in this unit.)

WALL: ResolveCollisionWithActor 97.07% - two dead `sth r0` spills of the actor's uid (0x38, 0x3c) that no source spelling reproduced; five spellings tried this run all leave the `sth r0,(r1)` count at 11 against retail's 13
