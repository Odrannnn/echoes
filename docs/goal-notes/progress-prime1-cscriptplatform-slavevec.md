# progress-prime1-cscriptplatform-slavevec

`MetroidPrime/ScriptObjects/CScriptPlatform` — `progress` item, unit stays `NonMatching`.

The item was `fn_800A14DC` (100 B), undefined in our object and called from `AddSlave+0x144`,
`BuildSlaveList+0xfc` and `AddRider(vector)+0x22c`. It is now defined, and so is the reserve it
needs, and `AddSlave` is written for real.

## Result

| | before | after |
|---|---|---|
| unit `matched_functions` | **20** / 60 | **23** / 60 |
| unit `matched_code` | 1916 B | **2352 B** (of 18000) |
| unit `fuzzy_match_percent` | 21.83% | **26.58%** |
| tree `matched_functions` | 10011 / 28465 | **10014** / 28465 |
| `AddSlave` | 0.93% | **98.83%** (432 B of retail's 428) |

**+3 functions at 100%, 0 worse, 0 asm added** — `tools/gate.sh`'s own per-function diff line,
quoted:

```
per-function diff   matched  10011 -> 10014   linked 4896 -> 4896   (+3 functions at 100%, 0 units newly linked)
```

Gained, all three new: `fn_800A14DC`, `fn_800A46F0`, `fn_800A47A8`. Nothing in the unit, or
anywhere in the tree, went below a score it had.

## What the three symbols are

Read out of `build/G2ME01/obj/MetroidPrime/ScriptObjects/CScriptPlatform.o` (retail) with
`objdump -d -r`; the relocation at each `bl` names the callee, which is what turns a guess into
a measurement. The chain the previous item's notes could not see is two deep, not one:

| symbol | DOL address | size | what it is | called from |
|---|---|---|---|---|
| `fn_800A14DC` | 0x800A14DC | 100 B | `rstl::vector<SRiders>::push_back_unsafe`, out-of-line | `AddSlave+0x144`, `BuildSlaveList+0xfc`, `AddRider(vector)+0x22c` |
| `fn_800A46F0` | 0x800A46F0 | 184 B | `rstl::vector<SRiders>::reserve`, out-of-line | `AddSlave+0x104`, `AddRider(vector)+0x220` |
| `fn_800A47A8` | 0x800A47A8 | 152 B | `rstl::uninitialized_copy<pointer_iterator<SRiders>, SRiders*>` | `fn_800A46F0+0x68` |

So `AddSlave` cannot be written without `fn_800A46F0` either, and `fn_800A46F0` cannot be
written without `fn_800A47A8`; that is the whole reason the previous item stopped at a note.

## The two codegen rules this needed (both measured, both worth keeping)

- **A retail symbol with no name in the map is matched by writing it `extern "C"` with that
  name.** `fn_800A14DC` has no parameter suffix in retail's object, i.e. the original had no
  mangleable signature there, so the only way objdiff can pair it with a function of ours is a
  symbol of exactly that name. `extern "C" void fn_800A14DC(...)` emits it verbatim; the repo
  already does this in `src/MetroidPrime/CAnimData.cpp:368` (`fn_800A27B44`, 100% matched).
  A member or template name never matches — `rstl::vector<SRiders>::push_back_unsafe` is
  `push_back_unsafe__Q24rstl43vectorI7SRiders,Q24rstl17rmemory_allocator>FRC7SRiders`, and
  calling it instead of writing `fn_800A14DC` gives a 32-byte forwarder (`bl`, prologue, blr)
  and 0%. The repo already does this in `src/MetroidPrime/CAnimData.cpp:368`
  (`fn_80027B44`, 100% matched, verified in `build/report.json`).
- **Our `rstl` already generates both of the other two bodies instruction for instruction.**
  Measured by forcing the out-of-line instantiations into the object
  (`tmp_reserve(v,n){ v.reserve(n); }`, 184 B weak `reserve__Q24rstl43vectorI7SRiders...Fi`):
  identical to retail 0x800A46F0 except the one `bl` target name, and the weak
  `uninitialized_copy<...>` is identical to retail 0x800A47A8. Only the names were missing, so
  both are written out by hand in `CScriptPlatform.cpp` and the templates are left alone.
- **`fn_800A46F0` must not take the fourth argument retail passes.** Retail calls
  `fn_800A47A8(r3=begin, r4=end, r5=out, r6=oldEnd)` (0x800A4740-0x800A4758: the two iterator
  temporaries at `r1+8..r1+23`, then the call). No instruction of the body reads `r6`, and
  spelling the argument (`slaves.mItems + slaves.mCount`) costs four instructions — a reload
  of `mCount`, of `mItems` and a second `mulli`/`add` — because the value is already in a
  register. Measured: 3 parameters → 184 B, byte-exact; 4 parameters → 200 B.
- `fn_800A14DC`'s `add. r5,r6,r0` + `beq` null test is CodeWarrior's placement-new guard, not a
  hand-written `if (p)`: `rstl::construct(&slaves.mItems[slaves.mCount++], slave)` reproduces it,
  and the implicit `SRiders` copy constructor is what produces the `sth`/`cmplwi`/`stb`/`beq`/
  `lfs`/`stfs` optional field copy plus the `bl __ct__12CTransform4fFRC12CTransform4f`.

## AddSlave: written, 98.83%, one instruction of register allocation away

Retail's body (0x800A1330, 428 B) is the Prime 1 `AddSlave`
(`prime-ref/src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp:480`) with Echoes' decay timer
and the "already a slave" update branch:

```cpp
rstl::vector< SRiders >::iterator slave =
    rstl::find(mDynamicSlaves.begin(), mDynamicSlaves.end(),
               SRiders(id, CTransform4f::Identity(), rstl::optional_object< float >()));
if (slave == mDynamicSlaves.end()) {
  if (CActor* actor = TCastToPtr< CActor >(mgr.ObjectById(id))) {
    actor->AddMaterial(kMT_PlatformSlave, mgr);
    CTransform4f xf = GetTransform().GetQuickInverse() * actor->GetTransform();
    fn_800A46F0(mDynamicSlaves, mDynamicSlaves.mCount + 1);
    fn_800A14DC(mDynamicSlaves, SRiders(id, xf, rstl::optional_object< float >(decayTimer)));
  }
} else {
  slave->mDecayTimer = decayTimer;
}
```

Everything matches except, at 0x800A11C8, retail loads the found iterator into `r3`
(`lwz r3,32(r1); cmplw r3,r5`) and keeps it into the update branch, while ours loads it into
`r0` and reloads `r3` in the branch target — one extra instruction, 432 B against 428. The
update branch itself (0x800A147C: `addi r0,r3,4; cmplw r0,r31` for the self-check, then the
valid test, then the value/`stb` stores) is the repo's `optional_object::operator=` un-inlined
and already matches; the `else` branch's `b 12c8` in retail is our `b` to the epilogue.

Spellings measured with `tools/bytescmp.py` against 0x800A1330 (retail 428 B), so nobody repeats
them:

| spelling | ours | note |
|---|---|---|
| inline `SRiders` probe via `rstl::find`, timer forwarded by reference | 408 B | frame 336 vs 352; retail **copies** the timer to the stack first, so this cannot be right |
| + `rstl::optional_object< float >(decayTimer)` (the one kept) | **432 B** | frame 352, every stack slot and every call site identical; only the `r0`/`r3` allocation left |
| named `end` local compared against `find`'s result | 444 B | whole prologue reordered, worse |
| hand-written `while` loop instead of `rstl::find` | 352 B | drops the `SRiders` probe entirely, frame 256 |
| `if (slave != end) { update } else if (actor) { ... }` | 432 B | same single instruction, opposite branch polarity |
| `SRiders probe` as a named local | 432 B | probe lands at `r1+256` instead of `r1+208` |

## Verified

```
sha1sum build/G2ME01/main.dol       -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh             -> All: 30.84% fuzzy, 23.11% matched, 11.74% linked (10014 / 28465 functions)
./tools/probe_sources.sh            -> probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py -> checked 503 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform -> ok, none out of retail order
./tools/gate.sh build/goal/judge/report.base.json
```

`tools/bytescmp.py` per function (only the `bl` relocation field differs in each, which objdiff
resolves by symbol name and scored 100%):

```
fn_800A14DC  0x800A14DC  100 B   1 of  25 instructions differ (the bl)
fn_800A46F0  0x800A46F0  184 B   3 of  46 instructions differ (three bls)
fn_800A47A8  0x800A47A8  152 B   1 of  38 instructions differ (the bl)
```

`gate.sh` is ok on every step except `docs claims`, which reports only

```
missing: 'matched    10014 / 28465 functions'  (HANDOFF state block: total matched)
missing: 'DOL units  8603 / 16726 functions'  (HANDOFF state block: DOL matched)
```

Those are the derived state-block numbers this change moved. The judge rewrites them itself —
`tools/gate.sh:115` runs `check_docs_claims.py ${MP_GATE_DOCS_WRITE:+--write}` and
`goal_check.sh` invokes the gate with `MP_GATE_DOCS_WRITE=1` — so per the brief I did not
hand-edit `docs/HANDOFF.md`.

Diff is one file, `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp`; no `.s`, no `tools/`,
no `docs/`, no `build/goal/`. Not committed.

## For whoever takes the rest of this unit

- **DOL address = 0x800A0200 + object offset** for this unit. Getting that wrong makes
  `tools/bytescmp.py` compare against the previous function and print a long, plausible,
  completely bogus diff — it cost me one round here. `build/report.json`'s per-function
  `metadata.virtual_address` gives the address directly.
- `BuildSlaveList` (0x800A2934, 468 B) and `AddRider(vector)` (0x800A38D0, 660 B) are the other
  two callers of `fn_800A14DC` and are still the `// TODO` bodies. Both now have their callees
  (`fn_800A14DC`, `fn_800A46F0`) defined, so they are the next real work in this unit.
- `tools/unit_fit.sh` reports 28 functions (3048 B) in our object that retail's does not have —
  all of them COMDAT template instantiations (`erase<...>`, the `vector` copy constructors,
  `__ct__11CMayaSplineFRC11CMayaSpline`, the `optional_object`/`single_ptr` destructors). None
  of the three new functions is among them. The unit is at 23/60 and cannot be flipped in this
  state; this item does not try.
- Still blocked, unchanged from the previous item's notes: `__dt__` (77.78%) needs the member
  order above 0x424 re-derived (`progress-prime1-cscriptplatform-dtor`), and the erase chain
  `fn_800A1004 -> fn_800A1050 -> fn_800A1148 -> fn_800A1180` is still missing
  (`progress-prime1-cscriptplatform-ridervec`).

## NEW:

NEW: progress-prime1-cscriptplatform-callers | progress | MetroidPrime/ScriptObjects/CScriptPlatform | fn_800A14DC and the reserve chain (fn_800A46F0/fn_800A47A8) now match at 100% and AddSlave is written at 98.83% (one instruction: retail keeps the found iterator in r3 across the branch at 0x800A11C8, ours reloads it), but the other two callers - BuildSlaveList (0x800A2934, 468 B) and AddRider(vector) (0x800A38D0, 660 B) - are still the TODO bodies, so five more functions in this unit are one written body each from counting

---

# Run 2 (lane 2, `goal/lane-2` at `e3a03358`)

The item's stated blocker (`fn_800A14DC` undefined) was already landed by run 1, so I re-measured
and went after the ten functions the unit still had at **0.00%** - retail's own out-of-line symbols
that `dtk` could only name after their addresses. Six reached 100%; the unit is now 40 / 60.

## Result

| | before this run | after |
|---|---|---|
| unit `matched_functions` | 34 / 60 | **40 / 60** |
| unit `matched_code` | 4788 B | **5208 B** (of 18000) |
| unit `fuzzy_match_percent` | 36.53% | **39.55%** |
| tree `matched_functions` | 11317 / 28465 | **11323** / 28465 |
| `linked` | 5507 | **5507** (unchanged) |

`tools/gate.sh`'s own per-function diff line, quoted:

```
matched  11317 -> 11323   linked 5507 -> 5507   (+6 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/ScriptObjects/CScriptPlatform :: fn_800A1CA0
  +100%    main/MetroidPrime/ScriptObjects/CScriptPlatform :: fn_800A4038
  +100%    main/MetroidPrime/ScriptObjects/CScriptPlatform :: fn_800A4090
  +100%    main/MetroidPrime/ScriptObjects/CScriptPlatform :: fn_800A4654
  +100%    main/MetroidPrime/ScriptObjects/CScriptPlatform :: fn_800A469C
  +100%    main/MetroidPrime/ScriptObjects/CScriptPlatform :: fn_800A4840
no regression
```

Nothing in the unit, or anywhere in the tree, went below a score it had.

## What the six are

All are read out of `build/G2ME01/obj/MetroidPrime/ScriptObjects/CScriptPlatform.o` with
`objdump -d -r`; the relocation at each `bl` names the callee, which is what turns a guess into a
measurement. `python3 tools/who_calls.py <addr>` finds the call sites and so fixes each one's role.

| symbol | size | what it is | who calls it (retail) |
|---|---|---|---|
| `fn_800A4840` | 16 B | `arg == 1`, the leaf `CGameCollision`/`CGroundMovement` use after `clrlwi. rX,24` | 12 sites in `CGameCollision`, `CGroundMovement`, `fn_80218E30`, `fn_8021A47C` |
| `fn_800A469C` | 84 B | `CGameSplineDesc::operator=` | nothing in the DOL |
| `fn_800A4654` | 72 B | `rstl::single_ptr<CMayaSpline>::operator=(T* const)` | ctor, 3x: 0x800A43A8 / 0x800A43E8 / 0x800A4428 |
| `fn_800A4090` | 88 B | `rstl::single_ptr<CMayaSpline>::~single_ptr(short)` | `__dt__`, 3x: 0x800A3F04 / 0x800A3F10 / 0x800A3F1C |
| `fn_800A4038` | 88 B | `rstl::single_ptr<SPlatformMotionSpline>::~single_ptr(short)` | `__dt__`, 1x: 0x800A3F78 |
| `fn_800A1CA0` | 72 B | `rstl::single_ptr<SPlatformMotionSpline>::operator=(T* const)` | `AcceptScriptMsg`, 1x: 0x800A196C with `li r4,0` (a release) |

`fn_800A1CA0` and `fn_800A4654` are the same function for the two pointee types, and `fn_800A4038`
and `fn_800A4090` are the same for the two pointee types, which is the shape of a template that
CodeWarrior emitted out of line once per instantiation. **The `bl` inside each is what fixes the
pointee's type**: `__dt__15CGameSplineDescFv` in the first pair, `__dt__11CMayaSplineFv` in the
second. That is the whole reason there are four symbols and not two.

## The three codegen rules this needed (all measured, all reusable)

- **A deleting destructor is a free `extern "C"` function taking `(T* self, int flag)` and
  returning `T*`, with `if (self != nullptr)` and `static_cast<short>(flag) > 0`.** All three
  details are the ones `src/MetroidPrime/Player/CGameStateBlockDtor.cpp` and
  `src/MetroidPrime/ScriptObjects/CFogOverlayRel.cpp` already record for this exact shape, and they
  are all confirmed here on two new instances: the flag must be a **`short`** or mwcceppc emits
  `cmpwi r31,0` where retail has `extsh. r0,r31`; the return type must be a **pointer** or the
  trailing `mr r3,r30` is lost; and the block must be freed with `CMemory::Free(self)`, not through
  `operator delete`. Measured: 88 bytes and 100% for each, from a 16-byte frame.
- **`delete p`, not `p->~T()`.** Retail's `li r4,1` before the pointee's destructor is the deleting
  flag that only a `delete` expression materialises. Spelling the destructor out by hand leaves r4 at
  -1: `fn_800A4038`/`fn_800A4090` were at **99.95%** with `p->~T()` and reached **100%** on the same
  code with `delete`. The pointee's own `operator delete` is *not* emitted either way, because
  `__dt__11CMayaSplineFv` is the out-of-line destructor and the free that follows is on `self`.
- **`rstl::single_ptr<T>::operator=(T* const)` is the three-instruction body, unchanged.** It
  already existed in `include/rstl/single_ptr.hpp`; what could not exist is the *name*. This is run
  1's rule (`extern "C"` for an unmappable retail symbol) applied to a template member, and it is
  why both `operator=` bodies are `delete self->mPtr; self->mPtr = ptr; return self;` verbatim.

## The one that needed a mirror struct: `fn_800A469C`

`CGameSplineDesc::operator=` copy-**constructs** its `SLdrSpline` member (the call at 0x800A45B8 is
`__ct__11CMayaSplineFRC11CMayaSpline`, with `this` = the object itself, so it is a placement-new over
a live member) and then copies `mType` (int, 0x44), `mDuration` (float, 0x48) and `mClosedLoop`
(bool, 0x4c) one field at a time. The last three are private, so the body reaches them through a
local `SMirror` - the same trick `fn_800D042C` in `src/MetroidPrime/Player/CMorphBall.cpp` uses, and
`rstl::construct` for the member, which is what emits the copy-constructor call rather than an
assignment. **Adding `#include "Kyoto/Math/CGameSplineDesc.hpp"` is required** (`CScriptPlatform.hpp`
does not reach it) and it is harmless: the include pulls in `CMayaSpline.hpp` and `CMotionSpline.hpp`,
which this unit already had.

## `fn_800A31A0` at 93.45% - what is left, and the four spellings that did not reach it

`rstl::vector<SRiders>::~vector(short)`, 132 bytes. Its body is the same deleting-destructor shape
(so all three rules above apply and the frame, the `mr. r30,r3 ; beq` guard, the `extsh.` test and
both `Free` calls all match), and the element destroy goes through the already-matched
`fn_800A1148`. **The remaining 3 instructions are the four frame stores of the two range
endpoints.** Retail stores each endpoint twice, at r1+8/r1+12 and r1+16/r1+20, and passes
r3 = r1+20, r4 = r1+12; the duplication is the signature of an inlined `destroy(begin, end)` that
forwards its two by-value parameters *by address* to the out-of-line `destroy_impl`. Measured with
`tools/bytescmp.py` against 0x800A2FA0 (retail 132 B):

| spelling | ours | note |
|---|---|---|
| two `SRiders*` locals passed by address to `fn_800A1148` directly | 75.36% | two stores, not four |
| two `vector<SRiders>::iterator` locals (`begin()`/`end()`), forwarded by address | 90.67% | four stores, but the *values* land in the wrong pairs: `first` gets r1+8/r1+20 |
| a `static` forwarder taking the two iterators **by value** and passing their addresses on | **93.45%** | the kept spelling; `last` declared before `first` |
| the same, `first` declared before `last` | 75.36% | mwcceppc gives the *second* local the lower slot, so the declaration order is load-bearing |
| a `static` forwarder taking two `SRiders*` **by value** | 78.09% | the frame collapses to 16 bytes - the by-value copy is elided and the two stores go too |

The 93.45% version is one frame-slot pair away. I stopped here rather than spend the rest of the run
on register allocation: the shape is known, the four stores are known, and what is missing is which
of the two endpoints each pair belongs to.

## `fn_800A1CE8` and `fn_800A1D4C` are not writable from this tree (measured, not guessed)

- **`fn_800A1CE8`** (100 B) is a deleting destructor that stores a vtable pointer by hand
  (`lis r4,lbl_803B32B0@ha` / `addi r0,r4,lbl_803B32B0@l` / `stw r0,0(r30)`, then
  `addi r3,r30,4` and `bl fn_800A1D4C`). It needs the **relocation-retargeted data label
  `lbl_803B32B0`**, which no `Rel(...)` block in this repo defines, so there is nothing to point the
  store at. Same blocker as the vptr stores in `CFogOverlayRel.cpp`, which solve it with a stand-in
  class - but that stand-in has to be a real class with its own vtable, which is a header change to
  a type this unit does not own. `fn_800A1D4C` (172 B) is that class's `vector` member destructor,
  whose elements are destroyed **virtually** (`lwz r12,0(r30)` / `lwz r12,8(r12)` / `mtctr` /
  `bctrl`, stride 0x50) - so it is reachable only once the class exists.
- **`fn_800A359C`** (228 B) is a straight 28-float copy (112 bytes, offsets 0x00..0x6c), called from
  `CMorphBall::TransformSpiderBallState` and `fn_8012900C` with `r4 = lwz 3200(r3)` off a
  `GetPhysicsState` result - i.e. a member of `CPhysicsState` at offset 0x3200. Our
  `include/MetroidPrime/CPhysicsState.hpp` declares eight members totalling 100 bytes (0x64), not
  112, and has no `CHECK_SIZEOF`, so **the struct's real layout is not recovered** and a 28-float
  copy written against it would be 28 arbitrary offsets. This one is blocked on the layout, not on
  the codegen.

## Declaration order: the one thing that cost the most time here

`tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform` went from **0
divergences to 3 and back** while I moved functions around, and the reason is not the "declare
descending by retail offset" rule on its own:

- **mwcceppc emits free functions in reverse source order, but a class member in the order its class
  declares it.** So retail's order in this region - `fn_800A469C` (0x4454+... = 0x449c),
  `fn_800A4654` (0x4454), **the constructor (0x3ee8)**, `fn_800A4090` (0x3e90), `fn_800A4038`
  (0x3e38) - **has the constructor in the middle of the free functions**, and no single "descending"
  source order can produce it. The block has to be **split around the constructor's definition**:
  `fn_800A469C` and `fn_800A4654` before it, `fn_800A4090` and `fn_800A4038` after it. With the
  block unsplit the constructor is emitted at 0x1bfc and all four land after it.
- **`fn_800A31A0` (0x2fa0) goes immediately *before* `PreThink` (0x2908) in the source**, i.e.
  between `MoveRiders` and `PreThink` - not after `PreThink`, which reads naturally but is wrong
  because of the reverse emission. Same for `fn_800A1CA0` (0x1aa0), which belongs immediately
  before `AcceptScriptMsg` (0x1458).

`check_decl_order.py` and `tools/gate.sh`'s `decl order` step are the only things that catch this;
objdiff pairs by name and would have reported 40/60 with the object permuted.

## Verified

```
sha1sum build/G2ME01/main.dol       -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh             -> All: 32.58% fuzzy, 25.24% matched, 11.94% linked (11323 / 28465 functions)
python3 tools/check_symbol_names.py -> checked 514 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py   -> ok: 977 unit(s) checked, 31 permuted, all 31 accounted for in decl_order.md
./tools/goal_check.sh build/goal/item.json
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11317 -> 11323   linked 5507 -> 5507
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptPlatform: 34 -> 40 / 60 functions
  ok    no asm added
goal_check: PASS progress-prime1-cscriptplatform-slavevec
```

`gate.sh` is **GATE PASS** on every step, `docs claims` included - the judge rewrites the state block
itself (`MP_GATE_DOCS_WRITE=1`), so the `docs/HANDOFF.md` diff in my tree is its doing, not mine.
Diff is one source file, `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp` (+143/-1); no `.s`, no
`tools/`, no `build/goal/`. Not committed.

## For whoever takes the rest of this unit

- **DOL address = 0x800A0200 + object offset**, still. `build/report.json`'s per-function
  `metadata.virtual_address` gives it directly, and `python3 tools/who_calls.py <addr>` gives the
  callers - that is how each of the six above was identified, and it is much faster than reading the
  disassembly cold.
- **The four functions still at 0.00% are blocked, and each for a named reason** (above): `fn_800A1CE8`
  and `fn_800A1D4C` on a vtable label this tree does not define plus a header change to a type the
  unit does not own; `fn_800A359C` on `CPhysicsState`'s real 112-byte layout; and `fn_800A31A0` is at
  93.45% with one frame-slot pair to go. The genuinely writable remainder is the `// TODO` bodies:
  `UpdateSlaveTransforms` (224 B), `DecayRiders` (300 B), `MoveRiders` (888 B), `DragSlaves` (484 B),
  `DragSlave` (736 B), `Think` (536 B), `PreThink` (1688 B), `TeleportToWaypoint` (160 B),
  `SetMotionTime` (304 B), `AdvanceMotionTime` (436 B), `AcceptScriptMsg` (1608 B), `Move` (2088 B)
  and the constructor (1388 B, 42.52% - the largest single win in the unit).
- `__dt__15CScriptPlatformFv` is at **100%** and stayed there. The `bl fn_800A4038/4090/31A0` diffs
  the earlier runs measured on it are gone: those symbols now exist, so the calls resolve to them.
  (objdiff resolves a `bl` by symbol name, so this only counts once the names are right.)
- `tools/unit_fit.sh` still reports extra COMDAT instantiations in this object - now including the
  `static` forwarder `fn_800A31A0_destroy__FPCP7SRidersPCP7SRiders`, which mwcceppc emitted as a
  local rather than inlining despite `static`. It is `t` (local), so it does not reach the linker,
  but it is one more thing in that list and the reason `fn_800A31A0` cannot flip the unit anyway.
- `SPlatformMotionSpline` and `CGameSplineDesc` are both 0x50 bytes with the same shape
  (`CMayaSpline`/0x44, then an int, a float and a bitfield). The two `single_ptr` pairs above are
  the same functions for the two types, so if either struct's real base class is recovered, **both
  symbols change name and both drop out of 100%**. Worth knowing before building on them.

## NEW:

NEW: progress-prime1-cscriptplatform-bodies | progress | MetroidPrime/ScriptObjects/CScriptPlatform | six more of the ten 0.00% functions are written and at 100% (unit 34 -> 40 / 60), the four remaining are blocked for named reasons (fn_800A1CE8/fn_800A1D4C need the lbl_803B32B0 vtable label this tree does not define plus a header change to a type the unit does not own; fn_800A359C is a 28-float copy of a CPhysicsState member whose real 112-byte layout our header does not have, it declares 100 bytes), and fn_800A31A0 is at 93.45% with one frame-slot pair left, so the rest of this unit is its // TODO bodies - the constructor at 42.52% / 1388 B is the largest single function left in it
