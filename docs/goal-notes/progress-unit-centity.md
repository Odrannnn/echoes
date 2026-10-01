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
