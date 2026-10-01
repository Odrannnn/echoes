# match-cscriptspindlecamera — MetroidPrime/ScriptObjects/CScriptSpindleCamera

## Result: PARTIAL — the unit went 1/3 → 2/3 matched functions

`./tools/goal_check.sh build/goal/item.json` (run in `wt-mp2-goal-L4`, 2026-10-01):

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11419 -> 11420   linked 5537 -> 5537
  ok    check_symbol_names.py
  ok    All:  32.81% fuzzy, 25.57% matched, 12.03% linked (11420 / 28465 functions)
  flip  flip_test MetroidPrime/ScriptObjects/CScriptSpindleCamera.cpp: FAIL - judged below as partial progress
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptSpindleCamera: 1 -> 2 / 3 functions
  ok    no asm added
goal_check: PARTIAL match-cscriptspindlecamera - flip_test ... FAIL, but the target rose; commit it and keep the item
```

## What I changed

One file, one hunk: `src/MetroidPrime/ScriptObjects/CScriptSpindleCamera.cpp`,
`CScriptSpindleCamera::AcceptScriptMsg` (lines 39-71). The `if (GetActive() && message == kSM_XALD)`
became `if (GetActive()) { switch (message) { case kSM_XALD: … break; default: break; } }`.
Nothing else in the tree changed; `configure.py` still says `NonMatching`.

## Measured before / after (`build/report.json`, this tree)

| | before | after |
| --- | --- | --- |
| `fuzzy_match_percent` | 96.48679 | **97.73208** |
| `matched_code` | 148 / 1060 (13.96%) | **584 / 1060 (55.09%)** |
| `matched_functions` | 1 / 3 | **2 / 3** |
| `AcceptScriptMsg` | 96.97248% , 428 B in our object | **100.0%, 436 B** |
| `__dt__` | 100% | 100% |
| `__ct__` | 94.94958% | 94.94958% (unchanged) |
| `.data` | 128 B, 100% | 128 B, 100% |

## The codegen rule this item found (worth more than the 8 bytes)

Retail tests the message with **`lis r3,hi` / `addi r0,r3,lo` / `cmpw r29,r0` / `beq body` /
`b end`** — materialise the 32-bit constant, compare signed, and branch both ways. Our build
emits **`addis r0,r29,hi` / `cmplwi r0,lo` / `bne end`** for the *same* source line: MW's
compare-with-zero idiom, two instructions instead of five, so the function is 8 bytes short of
retail's 436 and the `bl`-relative offsets after it are all wrong.

I tried nine spellings of the test before the one that works. All of these produce the
two-instruction `addis`/`cmplwi` form (measured, `%` on the function is 96.97248 in every case):

| spelling | result |
| --- | --- |
| `GetActive() && message == kSM_XALD` (baseline) | cmplwi, 428 B |
| `message == kSM_XALD && GetActive()` (tests swapped) | cmplwi, 428 B |
| `static_cast<int>(message) == static_cast<int>(kSM_XALD)` | cmplwi, 428 B |
| `kSM_XALD == message` | cmplwi, 428 B |
| `message != kSM_XALD` negated (`!(message != kSM_XALD)`) | cmplwi, 428 B |
| `static_cast<uint>(message) == kSM_XALD` | cmplwi, 428 B |
| early `if (!GetActive()) return;` then `if (message == kSM_XALD)` | cmplwi, 428 B |
| nested `if (GetActive()) { if (message == kSM_XALD) {…} }` | cmplwi, 428 B |
| a `bool` local holding the `&&` | cmplwi, 428 B |
| `switch (message) { case kSM_XALD: … }` (with or without `default:`) | **lis/addi/cmpw, 436 B, 100%** |

So: **MW only takes the `lis`/`addi`/`cmpw` route for a `switch` case label.** A `case` compares
and branches both ways; an `if`-condition comparison gets fused with the branch. Since retail
carries the compare-and-branch-pair shape, retail's source here was almost certainly a `switch`
on the message, not an `&&` — which is also how `CAi::AcceptScriptMsg` is already written in
this repo (`src/MetroidPrime/Enemies/CAi.cpp:66`).

This is not specific to this unit: `CScriptPathCamera.cpp:134` has the identical
`if (GetActive() && message == kSM_XALD)` line and retail's object at
`build/G2ME01/obj/MetroidPrime/ScriptObjects/CScriptPathCamera.o` has the same `lis`/`addi`/
`cmpw`. Every `msg.GetMessage() == kSM_…` / `message == kSM_…` site in a `NonMatching` unit
should be re-spelled as a `switch` before anyone tries to flip one — that is a free 8 bytes and
it un-breaks every branch offset after it. **No NEW: filed**: the fix is mechanical and local
to whichever unit is being worked, not an hour of separate work.

## What still blocks the flip: the constructor (94.94958%)

The constructor is the *only* thing left, and its whole residual is one 5-instruction window
inside the base-class call. Both objects are 476 bytes, 119 instructions, the same frame
(`stwu r1,-320(r1)`, save area at `248(r1)`), the same registers (`this` in r15, uid r16, name
r17, info r18, xf r19, flags r20, …), the same frame slots (kInvalidUniqueId 48(r1), uid 52,
material list 56/60, `CActorParameters` temp 64, `CModelData` temp 160) and the same reloc
sequence. The deltas, from a slot-by-slot diff of
`build/G2ME01/obj/…` vs `build/G2ME01/src/…`:

```
retail                                  ours
34  lhz  r0,sda   (kInvalidUniqueId)    34  addi r3,r1,64
35  addi r3,r1,64                       35  bl   __ct__16CActorParametersFv
36  sth  r0,48(r1)                      36  lhz  r4,sda   (kInvalidUniqueId)
37  bl   __ct__16CActorParametersFv     37  li   r0,0
38  li   r0,0                           38  lwz  r5,sda
39  lwz  r5,sda                         39  li   r3,0
40  stw  r0,60(r1)                      40  sth  r4,48(r1)   <-- copy store lands in here
41  li   r3,0                           41  li   r4,1
42  li   r4,1                           42  stw  r0,60(r1)
43  stw  r0,56(r1)                      43  stw  r0,56(r1)
44  bl   __shl2i                        44  bl   __shl2i
```

Retail emits the `kInvalidUniqueId` **copy as one group** immediately before the
`CActorParameters` temporary's constructor call. Our build emits the `lhz` immediately *after*
that call and lets the `sth` sink into a hole in the middle of the `CMaterialList` computation
(it has to: all callee-saved registers r14-r31 are already live with the 16 interpolant
pointers, so nothing can be held across the call, whereas retail never holds anything across it
because its whole copy precedes the call). Everything from instruction 45 on is identical.
Retail's order is exactly "scoped `CModelData` temp, then argument 9, then argument 8, then
argument 7, then argument 1" — reverse argument order with the scoped temporary hoisted; ours is
the same list with arguments 8 and 9 the other way round.

### Spellings tried on the base-class init (all 94.94958%, 476 B)

| spelling | result |
| --- | --- |
| `CActor(uid, name, info, 0, xf, CModelData::CModelDataNull(), CMaterialList(kMT_NoStepLogic), CActorParameters(), kInvalidUniqueId)` (baseline) | 5-insn window differs |
| `0u` / `false` for the `inGrave` argument | identical, no change |
| `CMaterialList{ kMT_NoStepLogic }` | build fails (no such ctor) |
| `CActorParameters{}` | build fails |
| `TUniqueId(kInvalidUniqueId)` (extra copy) | worse: 480 B, new `sth r6,52(r1)` |
| `TUniqueId(kInvalidUniqueId.value)` | worse: 480 B, same |
| `CMaterialList(static_cast<EMaterialType>(kMT_NoStepLogic))` | build fails |
| `TUniqueId nextDrawNode = kInvalidUniqueId` defaulted in `include/MetroidPrime/CActor.hpp:69`, arg dropped at the call site | byte-identical to baseline — the default argument is filled in at the same place |
| `const CActorParameters& params = CActorParameters()` defaulted, arg dropped | **build fails**: `Error: illegal default argument(s)` (MW rejects a non-trivial default) |

The frame is already tight in both builds, so this is not the 12-byte gap
`docs/goal-notes/match-cgamelight.md` hit; it is a pure scheduling decision I could not steer
from the source. I am not going to guess further at it blind.

### A second, independent flip blocker (not scheduling)

`tools/unit_fit.sh MetroidPrime/ScriptObjects/CScriptSpindleCamera.cpp`:

```
   .text      claimed   1060   ours   2164   retail   1060   over by 1104
   .data      claimed    128   ours    124   retail    128   SHORT by 4
   .sdata     claimed      -   ours     52   <- NOT CLAIMED BY splits.txt
   .sdata2    claimed      -   ours      4   <- NOT CLAIMED BY splits.txt
   extra:    +  464  __ct__Q24rstl52vector<15CMayaSplineKnot,…>F…   (W)
   extra:    +  172  __ct__11CMayaSplineFRC11CMayaSpline              (W)
   extra:    +   88  __dt__11CMayaSplineFv                           (W)
   extra:    +   84  __dt__Q24rstl45vector<9CVector3f,…>Fv            (W)
   extra:    +   84  __dt__Q24rstl48vector<11CQuaternion,…>Fv         (W)
   extra:    +   84  __dt__Q24rstl52vector<15CMayaSplineKnot,…>Fv    (W)
   extra:    +   60  __dt__16CActorParametersFv                      (W)
   extra:    +   44  GetHealthInfo__6CActorCFv                       (W)
   extra:    +   32  CModelDataNull__10CModelDataFv                  (W)
```

The 9 extra functions are all weak/COMDAT (`nm` shows `W` for every one), which is the
"harmless causes first" case `unit_fit.sh` describes (CAi carries 224 bytes of these and flips),
so I did not chase them. The 56 bytes of `.sdata`/`.sdata2` are not harmless: our object makes
**private copies of two constants that retail references as external labels** —

```
ours   0x548 R_PPC_EMB_SDA21 @616        retail  0x3c8 R_PPC_EMB_SDA21 lbl_8041D3D0  (.sdata2, float = 1.0f)
ours   0x45c R_PPC_EMB_SDA21 @391        retail  0x2e0 R_PPC_EMB_SDA21 lbl_804186F0 (.sdata, 8 bytes = the material list pair)
```

i.e. `-str reuse,pool,readonly` pooled a private copy of the `1.0f` and of the
`CMaterialList` mask word instead of relocating to the DOL's globals, and `splits.txt` claims no
`.sdata`/`.sdata2` for this unit. Same blocker `docs/goal-notes/match-cgamelight.md` recorded
(44 + 4 bytes there, 52 + 4 here): the constants would have to become extern globals, which is a
header change shared with other units, not something to do inside this item.

## Gates (all re-run on the final tree)

```
sha1sum build/G2ME01/main.dol                 -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh                      -> probe: 751 files, 0 failed, 0 errors;
                                                  link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py           -> checked 514 units; 0 declared names are missing
./tools/decomp_build.sh                       -> All:  32.81% fuzzy, 25.57% matched, 12.03% linked (11420 / 28465)
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptSpindleCamera
                                              -> ok: 1 unit(s) checked, none emits its functions out of retail order
```

(The `All:` fuzzy/matched rose because `AcceptScriptMsg` is now an exact match; `linked` stayed
at 5537, as it must — the unit is still `NonMatching`, so the DOL is byte-identical.)

`docs/HANDOFF.md`'s state block was rewritten by `goal_check.sh` itself (11419 → 11420); I did
not edit it.

WALL: CScriptSpindleCamera ctor 94.94958% - the 5 remaining wrong instructions are one
scheduling window (retail emits the kInvalidUniqueId copy as a group before the
CActorParameters temp's ctor call, we emit the lhz after it and sink the sth into the
CMaterialList code); 9 spellings tried, plus a private-constant-pool blocker on top

SUPERSEDED by the second run below: that WALL was a list-scheduling permutation, not register
allocation, and a comma operator reaches 100%. Do not re-try the spellings listed above.

---

# Second run (lane L7, 2026-10-01) — the ctor WALL is BROKEN, unit now 3/3 functions

## Result: PARTIAL — `matched_functions` 2 → 3, unit 100% fuzzy, flip still blocked by 16 bytes

`./tools/goal_check.sh build/goal/item.json` (run in `wt-mp2-goal-L7`):

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11425 -> 11426   linked 5537 -> 5537
  ok    check_symbol_names.py
  ok    All:  32.81% fuzzy, 25.63% matched, 12.03% linked (11426 / 28465 functions)
  flip  flip_test MetroidPrime/ScriptObjects/CScriptSpindleCamera.cpp: FAIL - judged as partial progress
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptSpindleCamera: 2 -> 3 / 3 functions
  ok    no asm added
goal_check: PARTIAL match-cscriptspindlecamera - flip_test ... FAIL, but the target rose
```

## What I changed

One hunk, `src/MetroidPrime/ScriptObjects/CScriptSpindleCamera.cpp` (lines 26-34). The
eighth argument of the base-class initialiser:

```cpp
// baseline, 94.94958%
: CActor(uid, name, info, 0, xf, CModelData::CModelDataNull(), CMaterialList(kMT_NoStepLogic),
         CActorParameters(), kInvalidUniqueId)

// now, 100.000%
: CActor(uid, name, info, 0, xf, CModelData::CModelDataNull(), CMaterialList(kMT_NoStepLogic),
         (static_cast< void >(kInvalidUniqueId), CActorParameters()), kInvalidUniqueId)
```

Nothing else in the tree changed by my hand; `configure.py` still says `NonMatching`.
(`docs/HANDOFF.md`'s state block was rewritten by `goal_check.sh` itself, as before.)

## The previous run's WALL was wrong: it was not the register allocation

The old note claims the five wrong instructions are a scheduling window MW would not budge from,
and that "all callee-saved registers r14-r31 are already live with the 16 interpolant pointers,
so nothing can be held across the call". That is not what is happening. I re-measured the window
instruction by instruction (`build/G2ME01/obj/…` vs `build/G2ME01/src/…`, both 476 bytes / 119
instructions) and it is a **permutation of the same 11 instructions**:

```
retail  lhz r0,kInv | addi r3,r1+64 | sth r0,48(r1) | bl __ct__CActorParameters | li r0,0 | lwz r5,LBL | stw r0,60 | li r3,0 | li r4,1 | stw r0,56 | bl __shl2i
ours    addi r3,r1+64 | bl __ct__CActorParameters | lhz r4,kInv | li r0,0 | lwz r5,LBL | li r3,0 | sth r4,48(r1) | li r4,1 | stw r0,60 | stw r0,56 | bl __shl2i
```

Both objects hold the value in a **volatile** register (r0 / r4) and neither holds anything
across a call, so nothing had to give. The only real difference is the order in which MW
materialises the two base-class arguments: retail does the TUniqueId copy (argument 9) before
the `CActorParameters` temporary's constructor call (argument 8); the plain spelling does them
the other way round and the `sth` sinks into the `CMaterialList(kMT_NoStepLogic)` code.

**The fix is a comma operator.** `(static_cast<void>(kInvalidUniqueId), CActorParameters())`
is exactly `CActorParameters()` — reading a `const TUniqueId` has no side effect and the value
is discarded — but it sequences the copy first, which is retail's order, and the window comes out
byte for byte. `(kInvalidUniqueId, CActorParameters())` (bare comma, no cast) works identically.
This is a codegen lever, not a semantic change, and the comment in the file says so.

## Measured before / after (`build/report.json`, this tree)

| | before (L7 baseline) | after |
| --- | --- | --- |
| `fuzzy_match_percent` | 97.73208 | **100.0** |
| `matched_code` | 584 / 1060 (55.09%) | **1060 / 1060 (100.0%)** |
| `matched_data` | 128 / 128 (100%) | **128 / 128 (100%)** |
| `matched_functions` | 2 / 3 | **3 / 3** |
| `__ct__` | 94.94958%, 476 B | **100.000%, 476 B** |
| `__dt__`, `AcceptScriptMsg` | 100% | 100% |
| `.text` / `.data` sections | 97.73% / 100% | **100.0% / 100.0%** |
| `All: matched` | 11425 | **11426** (`linked` 5537 unchanged, as it must) |

## Spellings tried THIS run (measured on this tree; do not repeat)

| spelling | ctor |
| --- | --- |
| `CModelData()` instead of `CModelData::CModelDataNull()` | 94.95% |
| `static_cast<TUniqueId>(kInvalidUniqueId)` as arg 9 | 94.95% |
| `false` instead of `0` for `inGrave` | 94.95% |
| `CMaterialList(static_cast<EMaterialTypes>(kMT_NoStepLogic))` | 94.95% |
| `(CMaterialList(kMT_NoStepLogic), CActorParameters())` (comma on arg 7) | 94.95% |
| `DefaultParams(kInvalidUniqueId)` helper returning `CActorParameters()` | **build fails** |
| `(kInvalidUniqueId, CActorParameters())` | **100.000%** |
| `(static_cast<void>(kInvalidUniqueId), CActorParameters())` | **100.000%** — shipped |

## What still stops the flip — narrowed to 16 bytes; the old note over-reports this blocker

`flip_test.sh` still fails, and I now have the exact cause instead of a guess. The flip builds
and links cleanly; only `build.sha1` fails, because our object contributes **two referenced
words** that retail references as DOL globals:

```
ours   0x45c R_PPC_EMB_SDA21 @391                retail 0x2e0 R_PPC_EMB_SDA21 lbl_804186F0  (.sdata,  0x804186F0, value 0)
ours   0x548 / 0x570 R_PPC_EMB_SDA21 @616         retail 0x3c8 / 0x3f0 R_PPC_EMB_SDA21
                                                                     lbl_8041D3D0 (.sdata2, 0x8041D3D0, value 1.0f)
```

* `@616` (4 bytes, `3f800000`) is the pooled copy of the `1.f` literal this file passes to
  `mTargetSpline(...)` and `mPlayerSpline(...)`.
* `@391` (4 bytes, `00000000`) is the pooled copy of the u64 zero that `CMaterialList`'s mem-init
  list reads — `CMaterialList(EMaterialTypes) : value(0)` in
  `include/Collision/CMaterialList.hpp` — pulled in by `CActor`'s seventh argument, the
  `CMaterialList(kMT_NoStepLogic)` temporary at 56(r1). **That one is a header change, not a
  change in this file**: it is emitted by every unit that constructs a `CMaterialList`, so it
  cannot be fixed inside this item.

Measured effect on the linked ELF (flipped vs unflipped `main.elf` section headers):

```
 9 .sdata    00001104 -> 0000110c  (+8)   first byte to move: 0x80418950 = our @391
11 .sdata2   000054c0 -> 000054c8  (+8)   first byte to move: 0x8041D3F8 = our @616
       DOL file 3969024 -> 3969056 bytes (+32; the rest is section alignment)
```

Both addresses already exist as defined symbols in the link, supplied by the dtk auto units
`auto_09_80418448_sdata.o` and `auto_11_8041D340_sdata2.o`. So the only way to flip is for the
source to reference those globals by name instead of pooling private copies — i.e. `extern "C"`
declarations using the **dtk labels**, which is not something to do inside this item (and
`@391`'s site is a shared header).

### Correction to the previous run's second blocker

The old note says the unclaimed `.sdata`/`.sdata2` are "not harmless" because `splits.txt` claims
none for this unit. **That over-reports it.** I measured it: 27 units that are
`MatchingFor("G2ME01")` today carry unclaimed `.sdata`/`.sdata2` in their object and still flip
(`MetroidPrime/CParticleGenInfo.cpp` 40 B, `Kyoto/Animation/CPOINode.cpp` 36 B,
`Dolphin/pad/PadClamp.c` `.sdata2` 32 B, …). The reason is that **mwldeppc drops the unreferenced
local pool entries**: this object's 52 bytes of `.sdata` hold `SolidMaterial` and `@290`–`@334`
(all dead) plus the one live `@391`, and only those 4 bytes reach the output. So "make
`.sdata`/`.sdata2` empty" is the wrong target — **remove the two *references***. Chasing the
section size (e.g. adding a `splits.txt` claim for them) would be wrong.

Likewise `.data` claimed 128 vs ours 124 is **not** a blocker: `.data` is `ALIGN(0x20)`, so 124
pads to the same 128 and every following address is unmoved; 16 other `Matching` units carry the
same -32. And the 9 "extra" functions `unit_fit.sh` lists are the COMDAT-weak copies (CAi carries
224 B of them and flips); they are discarded — the flipped `.text` at 0x801DFBE0..0x801E0004 is
byte-identical to retail.

## Gates, all re-run on the final tree

```
sha1sum build/G2ME01/main.dol                 -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh                      -> probe: 752 files, 0 failed, 0 errors;
                                                  link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py           -> checked 514 units; 0 declared names are missing
./tools/decomp_build.sh                       -> All: 32.81% fuzzy, 25.63% matched, 12.03% linked (11426 / 28465)
all 86 RELs cmp                                -> 0 mismatches
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptSpindleCamera
                                              -> ok: 1 unit(s) checked, none emits its functions
                                                 out of retail order
```

## Do not spend a third run on the ctor

It is at 100.000% with 476 bytes and the whole unit is 100% fuzzy on `.text` and `.data`.
The only thing between this unit and `Matching` is the 16 bytes of private constant pool above,
and the `CMaterialList` half of it needs `include/Collision/CMaterialList.hpp` to stop pooling its
`value(0)` zero and reference `lbl_804186F0` instead. That header is shared by every unit that
constructs a `CMaterialList`, so it wants its own item (and a check that no other unit's
`fuzzy_match_percent` falls), not a one-line edit smuggled in here.

---

# Third run (lane L2, 2026-10-01) — the unit is MATCHING; the "16 bytes" blocker is gone

## Result: PASS — `flip_test.sh` holds, `configure.py` says `Matching`

`./tools/goal_check.sh build/goal/item.json` (run in `wt-mp2-goal-L2`):

```
goal_check: item match-cscriptspindlecamera (match) target=MetroidPrime/ScriptObjects/CScriptSpindleCamera
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11434 -> 11434   linked 5572 -> 5575
  ok    check_symbol_names.py
  ok    All:  32.82% fuzzy, 25.64% matched, 12.13% linked (11434 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/CScriptSpindleCamera.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS match-cscriptspindlecamera
```

`matched` is unchanged because objdiff already counted these three functions as exact matches
while the unit was `NonMatching`; `linked` **5572 -> 5575** is the number that moves, because the
unit now contributes its own object to the link. Report for the unit: `fuzzy_match_percent 100.0`,
`matched_functions 3 / 3`, `complete_units 1`, `metadata.complete true`.

## What I changed

1. `src/MetroidPrime/ScriptObjects/CScriptSpindleCamera.cpp:9-30` — two `extern "C"` declarations
   with a comment, and the two `CMotionSpline` duration arguments changed from `1.f` to
   `lbl_8041D3D0`, and `CMaterialList(kMT_NoStepLogic)` to `CMaterialList(lbl_804186F0)`.
2. `configure.py:561` — `NonMatching` -> `Matching` (left in place by `flip_test.sh`).

Nothing else. `include/Collision/CMaterialList.hpp` was **not** touched.

## The rule: a `Matching` unit may not own data, so declare the constant and read it in place

This is the whole blocker, and it is the tree's existing idiom, not a special case
(`CWorldStateCtor.cpp:123`, `GlowbugAccessors.cpp:26-27`, `EmperorIngStage2TentacleAccessors.cpp:26`,
`CAtomicAlphaRel.cpp:97`, `CIngSnatchingSwarmGenAccessors.cpp:79`).

MW's `-str reuse,pool,readonly` pools a literal into the *using* object. Retail's compiler had
already put both constants in the DOL, so retail references them as external labels:

```
ours   0x460 R_PPC_EMB_SDA21 @392         retail 0x2e0 R_PPC_EMB_SDA21 lbl_804186F0  (.sdata, 8 bytes of 0)
ours   0x548 / 0x570 R_PPC_EMB_SDA21 @622  retail 0x3c8 / 0x3f0 R_PPC_EMB_SDA21
                                                          lbl_8041D3D0 (.sdata2, 4 bytes = 1.0f)
```

Both pool copies were live, so the link grew `.sdata` and `.sdata2` by 8 bytes each and every
address after them moved — 100% fuzzy and a broken DOL at the same time, which is the exact shape
`docs/PROCESS_LESSONS.md` warns about. `splits.txt` claims no `.sdata`/`.sdata2` for this unit, so
owning them is not an option. Declaring the two dtk labels and passing them where retail passes
them removes the copies: our `.sdata2` disappears entirely and `.sdata` drops to 48 bytes with no
live relocation pointing into it.

* `lbl_8041D3D0` is `.sdata2:0x8041D3D0`, `3f800000` = 1.0f — the two `CMotionSpline` durations.
  Trivially equivalent to `1.f`.
* `lbl_804186F0` is `.sdata:0x804186F0`, eight zero bytes (read from `build/G2ME01/main.elf`).
  Passing its value as the `CMaterialList` material keeps the material `kMT_NoStepLogic` and the
  list value 1; what changes is only that the word is read through a symbol. The load MW emits
  for the runtime material lands on the same instruction (`lwz r5`, ctor+0x98) retail uses, so the
  bytes and the relocation now match exactly.

Result: ctor/dtor/`AcceptScriptMsg` are **byte-identical to retail** with the relocation
displacement fields masked, and **zero** relocation type/target mismatches (measured by a
word-by-word diff of `build/G2ME01/obj/...o` vs `build/G2ME01/src/...o`). Sections: retail
`.text 0x424` / ours `0x87c` and retail `.data 0x80` / ours `0x7c` are the discarded COMDAT extras
`unit_fit.sh` lists; they are gone from the link, which is why the flip holds.

## Spellings tried THIS run (measured on this tree; do not repeat)

| spelling | result |
| --- | --- |
| `1.f` (baseline) -> `lbl_8041D3D0` as the duration | **works**: `.sdata2` gone, `lfs f1,lbl_8041D3D0` twice, `.text` unchanged |
| `CMaterialList(kMT_Stone)` | diagnostic: same 7 instructions, but the pool word becomes 1 — so the pooled word is **not** purely the mem-init zero; it tracks the material |
| `CMaterialList(static_cast< u64 >(0))` (the `CMaterialList(u64)` ctor) | no `__shl2i` at all, `.text` 0x87c -> 0x854, dead `lwz` still points at `@392` — wrong shape |
| `(lbl_804186F0, CMaterialList(kMT_NoStepLogic))` — comma operator | **does not work**: the read is dead and MW drops it, still `@392` |
| `CMaterialList(static_cast< EMaterialTypes >(lbl_804186F0))` | **works** |
| `CMaterialList(lbl_804186F0)` with `extern "C" const EMaterialTypes` | **works** — shipped (no cast needed) |

Measurement trap that cost two bogus readings: a scripted "replace the first
`CMaterialList(kMT_NoStepLogic)`" edits the *comment* at line 53, not the argument, and the build
then looks unchanged. Anchor on `CModelDataNull(), CMaterialList(...)`.

## Corrections to the second run's notes

* "**That one is a header change, not a change in this file**: it is emitted by every unit that
  constructs a `CMaterialList`" — **wrong.** `include/Collision/CMaterialList.hpp` needs no change
  at all, and `lbl_804186F0` is not a shared constant: exactly **one** object in the whole DOL
  references it (`find build/G2ME01/obj -name '*.o' | xargs objdump -r | grep -c lbl_804186F0` ->
  1), while 39 reference `lbl_8041C398` and 2 reference `lbl_8041D3D0`.
* "the only way to flip is ... which is not something to do inside this item (and `@391`'s site is a
  shared header)" — **wrong on both counts.** It is two `extern "C"` declarations in this file and
  a comment; the flip took one item.
* The old "Do not spend a third run on the ctor" advice was right about the ctor and wrong about
  the 16 bytes: those were the only thing left, and they are not a wall.

The old WALL line stands superseded; this run flips the unit, so nothing is parked.

## Gates, all re-run on the final tree

```
sha1sum build/G2ME01/main.dol                 -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh                      -> probe: 752 files, 0 failed, 0 errors;
                                                  link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py           -> checked 514 units; 0 declared names are missing
./tools/decomp_build.sh                       -> All: 32.82% fuzzy, 25.64% matched, 12.13% linked
                                                  (11434 / 28465 functions)
all 86 RELs cmp                                -> 0 mismatches
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptSpindleCamera
                                              -> ok: 1 unit(s) checked, none emits its functions
                                                 out of retail order
./tools/flip_test.sh MetroidPrime/ScriptObjects/CScriptSpindleCamera.cpp
                                              -> PASS  -> kept as Matching
```

(`docs/HANDOFF.md`'s state block was rewritten by `goal_check.sh` itself; I did not edit it.)

## No NEW: filed

Scanned for siblings before stopping: **zero** other `NonMatching` units in
`build/report.json` are at >= 99% fuzzy today (`state == NonMatching and fuzzy >= 99.0`), so this
was the last unit of its kind and there is no next unit to point a `NEW:` at. The general rule is
in the notes above rather than in the queue, since it is a codegen rule and not an hour of work.
