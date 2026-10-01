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
