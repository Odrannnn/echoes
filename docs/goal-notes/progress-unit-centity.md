# progress-unit-centity (`progress`, `MetroidPrime/CEntity`)

## Result

**PASS.** One function taken to 100%, the unit's matched count rose strictly, nothing anywhere got
worse. `./tools/goal_check.sh build/goal/item.json` (the driver's own judge, same baseline):

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11805 -> 11806   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.51% fuzzy, 26.54% matched, 12.64% linked (11806 / 28465 functions)
  ok    target rose: main/MetroidPrime/CEntity: 18 -> 19 / 24 functions
  ok    no asm added
goal_check: PASS progress-unit-centity
```

| | before | after |
|---|---|---|
| **matched functions** | **18 / 24** | **19 / 24** |
| unit fuzzy | 91.75% | 94.80% |
| unit matched code | 69.69% | 76.80% |
| `All:` matched functions | 11805 | 11806 |

Per function (`build/report.json`, `main/MetroidPrime/CEntity`), before % from the item's `reason`,
re-measured before acting and identical:

```
  100.00%  304 B  __ct__Q24rstl48vector<11SConnection,...>FRCQ24rstl48vector<11SConnection,...>   (was 55.96%)  <-- MATCHED
   99.87%  372 B  AcceptScriptMsg__7CEntityFR13CStateManagerRC10CScriptMsg                      (was 99.78%)
   99.29%  136 B  __ct__11CEntityInfoF7TAreaIdRCQ24rstl48vector<11SConnection,...>b9TEditorId    (unchanged)
   96.74%  272 B  SendScriptMsgs__7CEntityF18EScriptObjectStateR13CStateManager9TUniqueId20EScriptObjectMessage
                                                                            (was 98.04% - see the honest note below)
   0.00%   152 B  reserve__Q24rstl48vector<11SConnection,Q24rstl17rmemory_allocator>Fi           (unchanged)
   0.00%    60 B  uninitialized_copy<Q24rstl116pointer_iterator<11SConnection,...>,P11SConnection>__4rstlFQ24rstl116pointer_iterator<...>P11SConnection
                                                                            (unchanged)
```

## What I changed

Two files, no `configure.py`, no carve, no config, no asm.

### 1. `include/MetroidPrime/CEntityInfo.hpp` - the function that landed

`RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(SConnection)`, the repo's existing convention (same as
`CAABox.hpp`, `CPlane.hpp`, `TGameTypes.hpp`, ...), with a comment giving the measurement.

Retail's `rstl::vector<SConnection>` copy constructor is 0x130 = 304 bytes at 0x800483A4
(`config/G2ME01/symbols.txt`), 304 of them the 4x-unrolled `srwi. r0,rX,2` copy of 12-byte
`SConnection`s plus the one-element remainder loop. The generic `rstl::construct_impl` is
`new (dest) T(src)`; its null test puts a **branch in the loop body**, mwcceppc then guards the
store with the address arithmetic's flags and **does not unroll** (this is the finding already in
`docs/RUNNING_THE_DECOMP.md` for `CDependencyGroup::ReadFromStream`). Our object was 0xB8 = 184
bytes, one element per iteration, **55.96%**. With the assignment form the macro gives, the object
is 304 bytes and **byte-identical to retail** - I diffed all 76 instructions of
`__ct__Q24rstl48vector<11SConnection,...>FRCQ24rstl48vector<11SConnection,...>` against
`build/G2ME01/obj/MetroidPrime/CEntity.o` 0x800483A4: identical, including `stwu r1,-16(r1)`,
`srwi. r0,r3,2` / `mtctr` / 4x12 `lwz`-`stw` / `bdnz`, `andi. r3,r3,3` and the 3-word tail.
**55.96% -> 100.00%.**

`CEntityInfo`'s `conns` member and `CEntity`'s constructor both instantiate it, which is why it is
emitted as a real function in this unit and why the item's `reason` listed it at 55.96%.

A **TU-local** `construct_impl<SConnection>` specialisation (the `CActorLights.cpp` / `CLight`
shape) does *not* work: mwcceppc outlines it and the copy loop becomes a `bl` to it
(43.00%). It has to be the `inline` form in the header.

### 2. `src/MetroidPrime/CEntity.cpp` - two spellings, no match gained, both measured

Both are provably the same values; neither reached 100%, and I say so rather than dressing them up.

- **`AcceptScriptMsg`, 99.78% -> 99.87%.** The two `SendScriptMsgs` call sites now spell their
  trailing arguments through the **declared defaults** (`SendScriptMsgs(kSS_Active, mgr)`). That is
  what makes mwcceppc give the two sites **one** shared outgoing stack slot, as retail does: our
  `kSS_Active`/`kSS_Inactive` argument temporaries are both at `r1+32` and the toggle case's
  `CScriptMsg` at `r1+36`, exactly retail's three addresses. Spelled out, the two sites each got
  their own slot (`r1+36`, `r1+40`) and the frame layout was retail's plus 8. What is left is two
  independent `lhz` in the other order (below).
- **`SendScriptMsgs`, 98.04% -> 96.74%, and the percentage went *down* on purpose.** Retail's inner
  `CScriptMsg` gets its fifth field (`m_state`, the temp's `+12`) from **the register the `state`
  parameter came in** (`r27`, set by `mr r27,r4` in the prologue) with no reload, while the source
  said `it->state`, which costs a `lwz r8,0(r31)` + `stw r8,40(r1)` retail does not have. The
  function was 276 bytes against retail's 272. Passing `state` - provably equal, it *is* the test
  two lines above - makes the object **exactly 272 bytes** with retail's instruction count and the
  same two values. objdiff's fuzzy number is lower because its alignment heuristic punishes the two
  swapped loads; it is not a regression, and the gate agrees (no function anywhere got worse).
  Recorded because the number looks like one.

## Tried and rejected, with scores (so the next run does not repeat them)

All measured with `tools/fast_try.sh MetroidPrime/CEntity` (1.2 s a round).

| spelling tried | copy ctor | unit |
|---|---|---|
| `uninitialized_copy_n` loop changed to prime-ref's counting-up `for (int i = 0; i != n; ++it, ++i, ++cur)` (`include/rstl/construct.hpp`) | 55.96% (no unroll) | **17/24** - regressed `__ct__vector<TUniqueId>FRC` 100% -> 97.34% |
| copy ctor uses `uninitialized_copy(other.begin(), other.end(), mItems)` | 47.75% | **17/24** - `__ct__vector<TUniqueId>FRC` 100% -> 60.98% |
| copy ctor with a hand-written `for (int i = 0; i < mCount; ++i) mItems[i] = other.mItems[i];` | 55.33% | **17/24** |
| TU-local `construct_impl<SConnection>` in `CEntity.cpp` | 43.00% (loop becomes `bl`) | 18/24 |
| **`RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(SConnection)` in the header** | **100.00%** | **19/24** |

`AcceptScriptMsg`, 99.87% is the best of six: case order Deactivate/Activate swapped 99.53%;
`const CScriptMsg newMsg` 99.87% (no change); direct members `msg.m_unk, msg.m_originator, ...`
96.34%; `const TUniqueId originator = msg.GetOriginator();` hoisted 99.78%; `const next` 99.87% (no
change); the two default arguments 99.87% and the one that fixed the stack slots.

`SendScriptMsgs`, 96.74% is the best of three: `(*current).second` 96.74% (no change); the message
built into a named `const CScriptMsg out` then passed 96.57%.

`__ct__11CEntityInfo`, 99.29% with the flags in the member-initialiser list and 99.29% with them
assigned in the body - no change either way, so the diff keeps the original list.

## What is left, and why the unit still cannot flip

`tools/unit_fit.sh MetroidPrime/CEntity.cpp`: `.text` claimed 4276, ours 4396 (**over by 120**), and
three functions present in ours but not in the retail unit object, 332 bytes:
`reserve__Q24rstl45vector<9TUniqueId,Q24rstl17rmemory_allocator>Fi` (164),
`__dt__Q24rstl45vector<9TUniqueId,...>Fv` (84),
`__dt__Q24rstl48vector<11SConnection,...>Fv` (84). Retail's object *references*
`reserve<vector<TUniqueId>>` (relocs at 0x340 and 0x614) but never defines it - dtk put that
definition in a neighbouring unit, and mwcceppc emits a weak copy into every TU that calls it. The
two destructors are the same COMDAT-weak situation `unit_fit.sh` describes for CAi. Only
`flip_test.sh` decides, and I did not run it: the unit is `NonMatching` with five functions short, so
a `match` item on it is out of reach in one run and is not requeued here.

**The two 0% functions are not reachable from this unit's source.** Retail's object *defines*
`reserve<vector<SConnection>>` (152 B) and
`uninitialized_copy<pointer_iterator<SConnection,vector<SConnection>,rmemory_allocator>,SConnection*>`
(60 B) but **nothing in it calls either** - the copy constructor's only call is
`allocate__Q24rstl17rmemory_allocatorFi`, and the two `SendScriptMsgs`-side calls are
`reserve<vector<TUniqueId>>`. Ours emits neither because nothing instantiates them:
`rstl::uninitialized_copy` is `static inline` in `include/rstl/construct.hpp`, so no out-of-line
copy is ever emitted, and no code path in `CEntity.cpp` calls `reserve` on a `vector<SConnection>`.
Getting them needs a source change that alters behaviour, so I left them.

WALL: AcceptScriptMsg__7CEntityFR13CStateManagerRC10CScriptMsg 99.87% - the whole 372-byte body is
byte-identical to retail except two independent `lhz` the allocator issued in the other order
(retail `lhz r7,2(r5)` then `lhz r6,4(r5)`, ours `lhz r7,4(r5)` then `lhz r6,2(r5)`), with the r5/r6
assignment following; six spellings measured, none moves it.

`__ct__11CEntityInfoF7TAreaIdRC...b9TEditorId` is a different, already-known wall: retail's epilogue
reloads `r0` before `r31`/`r30`/`r29`, ours reloads it last, and `docs/RUNNING_THE_DECOMP.md`
already records for `fn_8014601C` that "no source shape moves that". Two spellings here
(flags in the init list / in the body) leave it at 99.29%.

---

# Attempt 2 (2026-10-02) — PASS. Both walls the previous attempt recorded are gone: the
# `CScriptMsg` constructor's *parameter order* was wrong. Unit 19 -> 21 / 24, global +8.

`./tools/goal_check.sh build/goal/item.json` in `../wt-mp2-goal-L8`, the driver's own judge on the
same baseline:

```
goal_check: item progress-unit-centity (progress) target=MetroidPrime/CEntity
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12301 -> 12309   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    All:  34.77% fuzzy, 28.33% matched, 12.90% linked (12309 / 28465 functions)
  ok    target rose: main/MetroidPrime/CEntity: 19 -> 21 / 24 functions
  ok    no asm added
goal_check: PASS progress-unit-centity
```

| | before | after |
|---|---|---|
| **`MetroidPrime/CEntity` matched functions** | **19 / 24** | **21 / 24** |
| unit fuzzy | 94.80075% | 95.019646% |
| unit matched code | 76.80074% (3284/4276 B) | 91.86155% (3928/4276 B) |
| **`All:` matched functions** | **12301** | **12309** |

Per function, `build/report.json` (`main/MetroidPrime/CEntity`), re-measured on this tree before
acting and identical to the previous attempt's:

```
  100.00%  304 B  __ct__Q24rstl48vector<11SConnection,...>FRCQ24rstl48vector<11SConnection,...>  (was 100.00%)
  100.00%  372 B  AcceptScriptMsg__7CEntityFR13CStateManagerRC10CScriptMsg                      (was  99.87%)  <-- MATCHED
  100.00%  272 B  SendScriptMsgs__7CEntityF18EScriptObjectStateR13CStateManager9TUniqueId20EScriptObjectMessage
                                                                                      (was  96.74%)  <-- MATCHED
   99.29%  136 B  __ct__11CEntityInfoF7TAreaIdRCQ24rstl48vector<11SConnection,...>b9TEditorId    (unchanged)
    0.00%  152 B  reserve__Q24rstl48vector<11SConnection,Q24rstl17rmemory_allocator>Fi           (unchanged)
    0.00%   60 B  uninitialized_copy<...pointer_iterator<SConnection>...,P11SConnection>        (unchanged)
```

`tools/bytescmp.py` on the two that landed — every differing instruction is a relocation field, so
objdiff's 100% is not a fuzzy-match artefact:

```
SendScriptMsgs  ours 272 B vs retail 272 B: 3 differing of 68, all `bl`
AcceptScriptMsg ours 372 B vs retail 372 B: 4 differing of 93, 2 `bl` + 2 `lhz r0,0(0)` (relocated)
```

## The cause: `CScriptMsg`'s constructor declared its two id parameters in the wrong order

`include/MetroidPrime/CEntityInfo.hpp`. The **members** were right all along (`m_unk` +0,
`m_originator` +2, `m_id` +4, `m_msg` +8, `m_state` +12 — retail's layout, confirmed by every
`AcceptScriptMsg` in the tree). The **parameters** were `(unk, originator, id, msg, state)`; retail's
were `(unk, id, originator, msg, state)`. Only the *order the arguments are written in at the call
site* changes the code, and it changes it because of a mwcceppc scheduling rule:

> **mwcceppc issues the load for the constructor's _third_ argument before its _second_, and gives
> the value it loads first the higher of the two registers.**

Measured, both directions, in this unit:

| | retail | ours before | ours after |
|---|---|---|---|
| `AcceptScriptMsg` toggle case | `lhz r7,2(r5)` then `lhz r6,4(r5)`, stored `r7`->+2, `r6`->+4 | `lhz r7,4(r5)` then `lhz r6,2(r5)`, stored `r6`->+2, `r7`->+4 | identical to retail |
| `SendScriptMsgs` inner loop | `lhz r6,0(r29)` (the `id` argument) then `lhz r5,20(r22)` (`current->second`) | the two the other way round | identical to retail |

So with `(unk, originator, id)` at the call site the third argument (`msg.GetId()`,
`current->second`) was loaded first and took the high register, and the *field* the value belongs to
was still written to the right offset — which is why the diff was only ever two `lhz` plus the two
`sth` that consume them, and why it looked like a register-allocation wall in both functions.
Declaring the parameters as retail has them makes the value that must land in `+2` the one loaded
first.

**The isolating experiment, which is the part worth keeping.** Swapping arguments 2 and 3 at the two
`CEntity.cpp` call sites *without* touching the header moves the loads to retail's order and leaves
only the two stores wrong (`AcceptScriptMsg` 99.87 -> 99.89, `SendScriptMsgs` 96.74 -> 99.85, the
object byte-identical otherwise). That is a semantics-violating probe and was thrown away; it is
what proved the cause is the *declaration*, not the call site's spelling. Six other call-site
spellings, all measured, all rejected, none of them moves the load order - see the table below.

### The change

- `include/MetroidPrime/CEntityInfo.hpp`: the five-argument constructor's parameters 2 and 3 swapped,
  with a comment saying why and not to "tidy" it back. The mangled name is unchanged (all three
  leading parameters are `TUniqueId`), and `tools/check_symbol_names.py` confirms it
  (`checked 525 units; 0 declared names are missing`).
- **32 five-argument `CScriptMsg(...)` construction sites in 16 `.cpp` files**, each with arguments 2
  and 3 swapped so every message is byte-for-byte the same message it was (32 = 31 found by
  re-scanning the tree for five-argument constructions, plus `CEntity.cpp:48`, whose
  `CScriptMsg newMsg(` form the scan's `CScriptMsg(` pattern misses). The net effect on behaviour is
  nil: the ctor body still assigns `m_originator`/`m_id` from the same two values, and the readers -
  22 `GetOriginator()` and 66 `GetId()` occurrences across `src/` and `include/` - are untouched.
  Files:
  `CEntity.cpp`, `CStateManager.cpp`, `CGameArea.cpp`, `CGameCollision.cpp`, `CCollisionActor.cpp`,
  `CGroundMovement.cpp`, `CGameHint.cpp`, `CBSJump.cpp`, `CBSHurled.cpp`, `CBSWallHang.cpp`,
  `CScriptEffect.cpp`, `CScriptColorModulate.cpp`, `CScriptActorKeyframe.cpp`, `CScriptPlatform.cpp`,
  `CScriptCannonBall.cpp`, `CScriptSequenceTimer.cpp`.

**It was wrong in six other functions too**, which is the honest measure of how much the tree was
carrying: `CCollisionActor::Touch` 99.04 -> 100.00, `CCollisionActor::AcceptScriptMsg` 99.67 ->
100.00, `CGameCollision::SendMaterialMessage` 98.21 -> 100.00, `CStateManager::SendScriptMsg(TUniqueId
dest, ...)` 99.52 -> 100.00, `CStateManager::SendScriptMsg(CEntity* dest, ...)` 99.58 -> 100.00,
`CScriptPlatform::DecayRiders` 99.51 -> 100.00. Plus five more that moved without reaching 100%
(`CBSHurled::Start` 98.95 -> 99.99, `CBSWallHang::UpdateBody` 99.55 -> 99.99, `CGameArea::UpdateDocks`
99.06 -> 99.98, `CScriptPlatform::AddRider` 99.61 -> 99.99, and `CGameHint::AcceptScriptMsg`
70.57 -> 77.55, `CGameCollision::SendScriptMessages` 69.22 -> 71.07, `CGroundMovement` x2,
`CScriptSequenceTimer::fn_801e1c1c` 95.88 -> 96.04, `CScriptCannonBall::Think` 77.08 -> 78.32,
`CScriptActorKeyframe::UpdateEntity` 63.43 -> 63.57, `CScriptColorModulate::End` 55.09 -> 55.12).

**Nothing anywhere got worse.** Per-function report diff, clean tree (stashed) rebuilt against this
tree: `matched 12301 -> 12309`, **0 functions worse**, 20 better, 0 new, 0 gone. `main.dol` is
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, exact.

### Supersedes the previous attempt's WALL lines

The `WALL: AcceptScriptMsg ... 99.87%` line above is **resolved** - six "spellings" had been tried
and none moved it, because the thing that had to move was not in the function at all but in the
constructor's declaration in another file. The same applies to the `SendScriptMsgs` residue. The
general lesson, for the next wall of this shape: *when a function differs from retail only in which
register two same-width values got, check the declaration of the thing that consumed them before
trying more spellings of the call.* A probe that swaps the arguments at the call site and nothing
else separates "the call site is spelled wrong" from "the callee's declaration is spelled wrong" in
one build.

## Still open, and why the unit still cannot flip

`tools/unit_fit.sh MetroidPrime/CEntity.cpp`: `.text` claimed 4276, ours 4396 (**over by 120**), and
332 bytes of functions ours emits that the retail unit object does not define -
`reserve<vector<TUniqueId>>` (164), `__dt__vector<TUniqueId>` (84), `__dt__vector<SConnection>` (84),
all COMDAT-weak carry-overs. Unchanged by this diff and re-measured. So a `match` item on this unit
is still out of reach; this was a `progress` item and it passes.

**`__ct__11CEntityInfo` (136 B, 99.29%) - the epilogue reload order.** Retail reloads `r0` (LR)
before `r31`/`r30`/`r29`; ours reloads it last. The 34-instruction body is otherwise byte-identical.
Five more spellings measured this run, **all 99.29%** (the previous attempt had two):

| spelling | `__ct__11CEntityInfo` |
|---|---|
| initialiser list (as committed) | 99.29% / 136 B |
| all six members assigned in the body | 99.29% / 136 B |
| `mUpdateWhileOccluded(1)` / `mUpdateDuringCinematicSkip(1)` | 99.29% / 136 B |
| the two `true` flags swapped in the initialiser list | 99.29% / 136 B |
| `mEditorId` written before `mConnections` in the list | 99.29% / 136 B |
| a trailing `mAreaId = aid;` in the body | 79.65% / 136 B (worse; the extra store moves) |

Six distinct spellings across the two attempts; the previous attempt had the first two.

I also tried to find a *rule* instead of more guesses, and did not find one - recording it so the
next run does not spend the time. Counting every function in `build/G2ME01/src` that reloads both LR
and at least one callee-saved register before its `mtlr`:

```
LR-FIRST  2865      LR-LAST  785        (LR-first is the majority, 78%)
```

so "LR-first is a rare spelling" (recorded in `progress-cgamestate-map-lowerbound.md`) is true of
that one function and false of the tree. Grouping the same 3650 functions by the mnemonic of the last
body instruction does **not** separate the two orders - `stb` splits 94/104, `stw` 577/328, `lfd`
37/177, while `bl`, `li`, `bne`, `bctrl`, `blt` are 100% LR-first. There is no cheap discriminator
there, and the two nearest LR-first shapes in our own build (`__ct__7CEntity` in this same file, and
`__ct__16CProjectedShadowFiiUci`) differ from `__ct__11CEntityInfo` in ways I could not turn into a
spelling. Note also that retail's own `__ct__11CEntityInfo` *ends on a store* and is still LR-first,
so whatever decides it is not a property of the last instruction and the table above cannot be read
as a rule.

**The two 0% functions are a dtk split artefact, re-confirmed this run** (the previous attempt's
conclusion, re-measured rather than inherited). Retail's object *defines*
`reserve<vector<SConnection>>` (152 B at 0xf3c) and
`uninitialized_copy<pointer_iterator<SConnection>,SConnection*>` (60 B at 0xfd4), and **nothing in
it calls either**: `objdump -r` on `build/G2ME01/obj/MetroidPrime/CEntity.o` shows exactly three
relocations mentioning them, two of them to `reserve<vector<TUniqueId>>` (from the
`FindConnectedObjects` side) and the third at **0xfa4, inside `reserve<vector<SConnection>>` itself**.
So the pair is a dead weak-COMDAT island that dtk's address-range split swept into this unit from
code that lives elsewhere. Nothing in `CEntity.cpp` instantiates them, and getting them would need a
source change that adds real behaviour, so they stay unreachable from this unit.

WALL: __ct__11CEntityInfoF7TAreaIdRCQ24rstl48vector<11SConnection,Q24rstl17rmemory_allocator>b9TEditorId 99.29% - the 34-instruction body is byte-identical to retail except that retail reloads r0 (LR) before r31/r30/r29 in the epilogue and ours reloads it last; six distinct spellings measured across two attempts (init list, body, int literals, both flag orders, mEditorId first, trailing store) leave it at 99.29%, and no grouping of our own 3650 comparable functions by the last body instruction separates LR-first from LR-last

## Call-site spellings measured for `SendScriptMsgs` that do NOT work

The loads land in the wrong order and the wrong registers. None of these moves them (they change the
object's frame layout, which is why the percentages move without the loads becoming right). Measured
with `tools/fast_try.sh MetroidPrime/CEntity`:

| spelling | `SendScriptMsgs` |
|---|---|
| as committed | 96.74% / 272 B |
| `const TUniqueId originator = id;` before the call | 97.74% |
| `const TUniqueId unq = GetUniqueId(); const TUniqueId originator = id;` | 97.91% |
| the two above plus `const EScriptObjectMessage m = it->msg;` | 97.91% |
| `const TUniqueId unq = GetUniqueId();` alone | 96.38% |
| `const TUniqueId target = current->second;` alone | 94.71% |
| `const EScriptObjectMessage m = it->msg;` alone | 96.74% (no change) |
| the message built into `const CScriptMsg out` then passed | 96.57% |
| `(*current).second` | 96.74% (no change) |

Each adds one halfword store to the dead scratch area at `r1+8` and pushes the `CScriptMsg` temp four
bytes up, so the frame stops being retail's. The two that score highest get the load order and
register assignment right and are still wrong, because the value that must land in `+2` is then the
one loaded second. None of them is worth revisiting.

## Verification

```
sha1sum build/G2ME01/main.dol          6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (retail)
./tools/decomp_build.sh                All:  34.77% fuzzy, 28.33% matched, 12.90% linked (12309 / 28465)
                                       main/MetroidPrime/CEntity: 95.02% fuzzy, 91.86% matched (21 / 24)
python3 tools/check_symbol_names.py    checked 525 units; 0 declared names are missing
./tools/probe_sources.sh               752 files, 0 failed, 0 errors; LINKED (287 undefined, 0 duplicates)
./tools/goal_check.sh build/goal/item.json   PASS progress-unit-centity
```

`python3 tools/check_docs_claims.py` on its own reports `missing: per-unit count for CStateManager
(103/239 in the report)` - that is the derived count the driver rewrites, and `gate.sh` inside
`goal_check` is clean. `python3 tools/check_decl_order.py --unit main/MetroidPrime/CEntity` says
`would break on a flip`; that is **pre-existing and not mine** - stashed, re-run, identical output on
the clean tree, and `gate.sh` does not flag it. Not run: `flip_test.sh` (the unit stays
`NonMatching`; `unit_fit.sh` above says why).

Files touched: 17 (one header, sixteen `.cpp`); no `configure.py`, no carve, no `config/`, no asm.
`docs/HANDOFF.md` also shows a diff, written by `tools/goal_check.sh` itself (the state block and
the per-unit counts it derives from the tree); I did not edit it and the driver discards edits to it
before judging.
