# progress-prime1-cscriptplatform-erase-pair

`MetroidPrime/ScriptObjects/CScriptPlatform` — `progress` item, unit stays `NonMatching`.

## Result

| | before | after |
|---|---|---|
| `fn_800A1050` (248 B) | 0.00% | **100.00%** |
| `fn_800A1004` (76 B) | 0.00% | **100.00%** |
| unit `matched_functions` | 26 / 60 | **28** / 60 |
| unit `fuzzy_match_percent` | 27.79% | 29.59% |
| tree `matched_functions` | 10041 / 28465 | **10043** / 28465 |

`tools/gate.sh build/goal/judge/report.base.json` prints
`per-function diff  matched 10041 -> 10043  linked 4917 -> 4917  (+2 functions at 100%, 0 units newly linked)`.
Independent full per-function diff over **every** unit in both reports:

```
better 2   worse 0   missing-now 0
  BETTER   0.00 -> 100.00 fn_800A1004
  BETTER   0.00 -> 100.00 fn_800A1050
```

## What I changed

One file, `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp`, +32/-1.

- **Added `fn_800A1050`** (`src/.../CScriptPlatform.cpp:315-336`) — `rstl::vector<SRiders>::erase(first, last)`,
  as a free function taking the vector by reference. The body is the template in
  `include/rstl/vector.hpp:251` with `this` spelled `riders`, so the signed-division-by-60 index
  (`first - riders.begin()`), the shift loop, `riders.mCount = newCount` and `return first` are the
  same statements the compiler already produced.
- **Added `fn_800A1004`** (`:338-341`) — `return fn_800A1050(riders, it, it + 1);`, the one-argument erase.
- **One call site changed** (`:355`): `mRiders.erase(it)` -> `fn_800A1004(mRiders, it)` in
  `RemoveRider`, the only `erase` call in the tree. Without it the templates would still be
  instantiated and the object would carry both spellings.

Both are `extern "C"`, which suppresses mangling, so they carry retail's `fn_XXXXXXXX` names and
objdiff pairs them with `config/G2ME01/symbols.txt`'s address-named entries. Declared **after**
`fn_800A1148` and **before** `RemoveRider` — descending by retail offset (0x1148, 0x1050, 0x1004,
0xd38) as the file already is.

## The first try was already the whole answer

`reason` says the pair "needs sr26 = (first - mItems)/60, a shift loop that assigns
mUid/optional timer/CTransform4f per element and ends with mCount = r26 and *out = first" — i.e. that
writing the bodies from the disassembly is the job. It is not: **the `rstl::vector` templates in
`include/rstl/vector.hpp` already emit these bytes.** The prior run's note said the pair was
"already byte-identical in our object", and the only thing missing was the name. Reusing the template
body verbatim as the `extern "C"` body, with `riders` for `this`, gave 100.00% on both functions on
the first build, with no iteration.

**The object did not grow.** `powerpc-eabi-size` reports `.text` **8615 B before and after**, and
`nm | grep -c erase__` is now **0** (was 2). The extern "C" definitions replaced the template
instantiations in place; nothing was added. That is the cheap check that the change is a rename and
not a second copy of the chain.

The calling convention came out right without any of it being spelled: `iterator` is a one-word class
with a non-trivial copy, so MWCC passes the two iterators as caller-built copies in `r5`/`r6` and
returns the sret'd iterator through `r3` — the same shape the template member function had, and the
same shape retail has.

## Notes for whoever takes `DecayRiders`

`DecayRiders`, `MoveRiders` and `DragSlaves` call this pair (relocations at 0x2418, 0x2fec, 0x3268,
0x3500 in the retail object), and now that the pair carries retail's names those `bl`s are named
correctly too — `DecayRiders` still reads 1.33%, `MoveRiders` 0.45%, `DragSlaves` 0.83%, because their
own bodies are stubs, not because of the erase. `fn_800A14DC` is already 100% in this tree (another
lane landed it), so `progress-prime1-cscriptplatform-slavevec`, which was the open one there, is
answered; `fn_800A14DC` is now called by name from `AddSlave`.

Walls, unchanged, not re-filed: `GetSortingBounds` 92.12%, `GetTouchBounds` 93.10%, `IsSlave` 95.29%,
`BuildNearListFromRiders` 98.01%, `__dt__` 77.78% / `__ct__` 42.52%
(`progress-prime1-cscriptplatform-dtor`).

`tools/unit_fit.sh MetroidPrime/ScriptObjects/CScriptPlatform.cpp` still lists 26 extra functions
(2724 B), all pre-existing template constructors/destructors and dtor instantiations; `fn_800A1004`
and `fn_800A1050` are not among them — they pair with retail. The unit cannot flip until the
constructors and `__ct__`/`__dt__` are done, which is the dtor item.

## Verified

```
sha1sum build/G2ME01/main.dol       -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh             -> All: 30.97% fuzzy, 23.24% matched, 11.78% linked (10043 / 28465 functions)
./tools/probe_sources.sh            -> probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py -> checked 503 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform
                                    -> ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/gate.sh build/goal/judge/report.base.json
```

`gate.sh` is ok on every step except `docs claims`, which reports only the two derived state-block
numbers this change moved (`matched 10043 / 28465`, `DOL units 8632 / 16726`). Per the brief I did not
hand-edit `docs/HANDOFF.md`: `goal_check.sh:105` runs the gate with `MP_GATE_DOCS_WRITE=1`, so the
judge rewrites them.

Diff is **one file**: no `.s`, no `asm`, no `tools/`, no `config/`, no `docs/`, no `build/goal/`.
Not committed. `objdiff.json` deliberately not touched.

## NEW:

(none — the item's stated work was real and is done; the remaining sub-100% functions in this unit
are walls already recorded above or belong to `progress-prime1-cscriptplatform-dtor`.)

---

# Run 2 (lane 2, worktree `wt-mp2-goal-L2`, branch `goal/lane-2`, 2026-10-01)

## The erase pair was already landed: this item's stated work is STALE

Re-measured on this tree's clean head, `build/goal/judge/report.base.json` and `build/report.json`
agree, and both already read **100.00%**:

| | retail offset | size | measured on the clean tree |
|---|---|---|---|
| `fn_800A1004` | 0x800A1004 | 76 B | **100.00%** |
| `fn_800A1050` | 0x800A1050 | 248 B | **100.00%** |
| `fn_800A1148` | 0x800A1148 | 56 B | 100.00% |
| `fn_800A1180` | 0x800A1180 | 28 B | 100.00% |

`git blame src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp` puts both definitions in **f444a38a0**
(2026-09-30 07:23), an ancestor of this branch's head `47a2f801`, so run 1's change is already in the
tree and already counted. The unit stood at **40 / 60** matched, not the 28 / 60 the first run
recorded - `bb60b24b` (`cscriptplatform-dtor`) and `47a2f801` (`cscriptplatform-slavevec`) have landed
on top since.

So `fn_800A359C` is what this run did instead: the same class of work (an unnamed retail function in
this unit whose bytes our object can already produce, or nearly), on a function that is still at 0.

## Result

| | before (`report.base.json`) | after (`build/report.json`) |
|---|---|---|
| `fn_800A359C` (228 B) | 0.00% (unpaired) | **100.00%** |
| unit `matched_functions` | 40 / 60 | **41** / 60 |
| unit `fuzzy_match_percent` | 39.55% | **40.82%** |
| unit `matched_code_percent` | 28.93% | **30.20%** |
| tree `matched_functions` | 11333 / 28465 | **11334** / 28465 |
| tree `linked` (report_diff) | 5507 | 5507 |

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  11333 -> 11334   linked 5507 -> 5507   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/ScriptObjects/CScriptPlatform :: fn_800A359C
no regression
```

`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**, every step ok, including
`gate.sh` (DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86 RELs, report diff, wiring, docs
claims, port probe), `check_symbol_names.py`, `no asm added`, and
`target rose: ... CScriptPlatform: 40 -> 41 / 60 functions`.

## What I changed

One file, `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp`, +58 lines, no deletions: **`fn_800A359C`
inserted between `DecayRiders` and `MoveRiders`** (the only place in source order that puts it between
them in the object - mwcceppc emits in reverse source order, and retail has it at 0x800A359C between
`MoveRiders` 0x800A3024 and `DecayRiders` 0x800A3480).

### What `fn_800A359C` is

`CPhysicsState`'s implicit copy constructor, out of line. Measured, not guessed:

- `CGroundMovement::MoveGroundCollider_New` calls this one copy twice (`bl` at 0x80129500 and
  0x80129544), each time with `r3 = r1+2196` and `r4 = r1+1332` where 0x801294F4 has just been
  `bl GetPhysicsState__13CPhysicsActorCFv` on 0x80129500's destination slot. So it copies a
  `CPhysicsState`.
- It copies **0x70 bytes**, 28 floats, with `lfs`/`stfs` and no frame.
- Retail's 9-argument `__ct__13CPhysicsStateFRC9CVector3fRC11CQuaternionRC9CVector3fRC10CAxisAngleRC9CVector3fRC9CVector3fRC9CVector3fRC10CAxisAngleRC10CAxisAngle`
  (0x800EBD40) is the same straight run of `lfs`/`stfs`, and its last store is `stfs f0,108(r3)` -
  the same 0x70, so retail's `CPhysicsState` is 0x70 and `include/MetroidPrime/CPhysicsState.hpp`
  already agrees (`CVector3f` 3 + `CQuaternion` 4 + `CVector3f` 3 + `CAxisAngle` 3 + five more 3s = 28;
  `CHECK_SIZEOF(CAxisAngle, 0xc)` in `CAxisAngle.hpp` is what makes it come out).
- Retail's symbol table names it only by address, hence `extern "C"`.

### Three spellings, measured, all in the comment in the source

| spelling | `fn_800A359C` | note |
|---|---|---|
| `*self = other;` | **41.05%** | the implicit `operator=` is a block copy; MWCC emits `lwz`/`stw` |
| `rstl::construct(self, other);` | **9.47%** | the copy ctor is too big to inline, so it outlines into a weak `__ct__13CPhysicsStateFRC13CPhysicsState` this unit would then carry as an extra symbol |
| `for (int i = 0; i < 28; ++i) dst[i] = src[i];` | **93.84%** | unrolls to `lfs`/`stfs`, but strength-reduces: it needs a third float register and an `lfsu` induction pointer (`addi r5,r4,28` / `lfsu f2,28(r5)`), which retail has nowhere - retail's copy has no induction pointer at all because retail's is a *constructor* |
| 28 straight-line `dst->mF[n] = src->mF[n];` on a `float[28]` mirror | **100.00%** | kept |

The last one is what is in the tree. The mirror is the same trick `fn_800A469C` in this file already
uses, and is sound rather than a fudge: `CPhysicsState` really is 0x70 bytes of nothing but floats,
which is why the 28-float copy reproduces retail byte for byte.

**The object grew by exactly the function**: `.text` 10752 B -> 10980 B (`unit_fit`), and
`unit_fit.sh` still reports the **same 32 extra functions / 3128 bytes** as before - no new weak
symbol, nothing else moved. `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform`
-> `ok: 1 unit(s) checked, none emits its functions out of retail order`.

## `fn_800A1CE8` (100 B) and `fn_800A1D4C` (172 B): both still 0.00%, and here is all of it

These are the only two functions left at 0 in this unit, and I did not write them - but everything
below is measured, so the next run does not have to redo it. **This is not a spelling wall**; the
first run tried no spellings and I tried none either. Do not read the 0.00% as "the templates
already emit these bytes".

**`fn_800A1D4C` is `rstl::vector<T>::~vector(int)` for a `T` of 0x50 bytes that is polymorphic.**

- Prologue `mr. r28,r3` / `beq`, `lwz r0,4(r28)` (`mCount`), `lwz r30,12(r28)` (`mItems`),
  `mulli r0,r0,80` (element size **0x50**), and **four** frame stores (items at r1+8/r1+20, end at
  r1+12/r1+16) - that four-store pair is the same `rstl::destroy` by-value-then-by-address shape
  `fn_800A31A0` in this file already documents, so this is the out-of-line `~vector` with the destroy
  loop not inlined.
- The loop at 0x800A1D9C-0x800A1DAC is `lwz r12,0(r30)` / `lwz r12,8(r12)` / `mtctr` / `bctrl` with
  `r4 = -1`: a **virtual** deleting destructor through vtable slot 2, at a 0x50 stride.
- Then `CMemory::Free(mItems)`, then `extsh. r0,r29` / `ble`, then `CMemory::Free(self)` - the
  `(short)flag > 0` idiom.

**`fn_800A1CE8` is the destructor of the class that owns it.** It stores a **data address into
offset 0** (`lis r4,0x803b` / `addi r0,r4,0x32b0` -> `stw r0,0(r30)`, i.e. a vptr reset, and the
pool word it loads is 0), calls `fn_800A1D4C(self+4, -1)`, then `extsh.`/`CMemory::Free(self)`, and
returns `self`. That is `__dt__13CMotionSplineFv`'s exact shape (0x80334B48, 124 B) minus two of the
three vector calls - and CMotionSpline is the one class here whose layout retail confirms is
vptr at +0, `mControlPoints` at +4, `mKnots` at +20, `mKnotDistances` at +36, which matches
`include/Kyoto/Math/CMotionSpline.hpp` exactly.

**The enclosing class is `CPlatformWaypointTracker`, and we have only a one-line stub for it.**
`include/MetroidPrime/ScriptObjects/CScriptPlatform.hpp:25` declares
`class CPlatformWaypointTracker { public: virtual ~CPlatformWaypointTracker(); };` - no members and
no definition anywhere in the tree. Retail's layout, recovered from its constructor:

- `AcceptScriptMsg` at 0x800A18C8 does `li r3,32` / `bl __nw__FUlPCcPCc` - **0x20 bytes**, no RTTI
  (the only two `bl` sites in the DOL that load 0x803B32B0 are this destructor and that constructor).
- The constructor `fn_801FADE8` (0x801FADE8, called only from `AcceptScriptMsg` at 0x800A18FC) is
  15 instructions: `stw r0,0(r3)` (vptr), then `mCount=0` at +8, `mCapacity=0` at +12, `mItems=0` at
  +16, `stfs` a float at +20 and +24, `sth` a `uint16` at +28. So: **vptr +0, `rstl::vector<T>` at
  +4 (count +8 / capacity +12 / items +16), `float` +0x14, `float` +0x18, `uint16` +0x1c = 0x20.**
  The vtable at 0x803B32B0 is `[0, 0, 0x800A1CE8]`: offset-to-top, no typeinfo, and exactly one
  virtual - the destructor.
- Its caller passes `CGameSpline::mFlags` (a `uint16`) and `CGameSpline::GetDuration()`.

**Why I stopped rather than writing them.** Two things are missing that I will not invent:

1. `T`, the vector's element type: 0x50 bytes and polymorphic. No class in `include/` is both.
   The only 0x50 candidates are `SPlatformMotionSpline`, `CGameSplineDesc`, `CMappableObject`,
   `CPFAreaOctree`, `CSaveWorldIntermediate`, `CMemoryCard`, `CLight`, `CCubeModel` and
   `COBBTree::CNode`, and retail proves the two spline-shaped ones are **not** polymorphic -
   `__ct__11CMayaSplineFRC11CMayaSpline` (0x80080B3C) stores `mPreInfinity` at +0 with no vptr, and
   `__dt__11CMayaSplineFv` (0x800327FC, 88 B) has no vptr store either.
2. `fn_800A1CE8`'s `stw <data address>,0(self)` is a **vptr reset**, and MWCC only emits that inside
   a compiler-generated destructor of a polymorphic class (that is how `__dt__13CMotionSplineFv` gets
   it). A free function cannot spell it, and a destructor cannot be given the name
   `fn_800A1CE8` without asm. Declaring a local class with a vtable and a `virtual ~` would emit the
   store, but only under a mangled name, and writing `self->mVptr = &someArbitraryObject` purely to
   get the `lis`/`addi`/`stw` triple is the "plausible-looking stand-in" the brief forbids.

Declaring both classes speculatively (a fake polymorphic 0x50 entry type plus a fake 0x20-byte
tracker) would very likely produce the bytes, and would also be a fabricated port. That is why the
pair is left at 0.00% with this much written down instead.

Cheap thing a next run should **not** repeat: the `CPhysicsState` copy has exactly three spellings
that do not reach 100% and they are all in the table above, and the erase pair / `fn_800A1148` /
`fn_800A1180` are already 100% and must not be touched.

Walls still standing, unchanged and not re-filed: `GetSortingBounds`, `GetTouchBounds`, `IsSlave`,
`BuildNearListFromRiders` are now 100% (run 1's list is stale); `__dt__` 100%, `__ct__` **42.52%**
(1388 B); `AddSlave` 99.86%, `AddRider`(vector) 99.61%, `fn_800A31A0` **93.45%**; the stubbed bodies
`Move` 1.58%, `PreThink` 0.24%, `DragSlave` 0.54%, `DragSlaves` 0.83%, `MoveRiders` 0.45%,
`DecayRiders` 1.33%, `AdvanceMotionTime` 0.92%, `SetMotionTime` 2.49%, `TeleportToWaypoint` 2.50%,
`Think` 0.75%, `UpdateSlaveTransforms` 1.79%, `AcceptScriptMsg` 1.98%, `fn_800a0200` 1.72%.

Diff is **one file**: no `.s`, no `asm`, no `tools/`, no `config/`, no `docs/`, no `build/goal/`.
Not committed. `objdiff.json` deliberately not touched.

## NEW:

`NEW: progress-prime1-cscriptplatform-waypointtracker-dtors | progress | MetroidPrime/ScriptObjects/CScriptPlatform | fn_800A1CE8 (100 B) and fn_800A1D4C (172 B) are still 0.00% and need the real CPlatformWaypointTracker (0x20 B: vptr, rstl::vector at +4, 2 floats, uint16) plus its 0x50-byte polymorphic vector element, neither of which is in include/; layout and constructor measured in this run's notes.`

---

# Run 3 (lane 4, worktree `wt-mp2-goal-L4`, branch `goal/lane-4`, 2026-10-02)

## The erase pair was still already done, so this run took `AdvanceMotionTime`

Re-measured on this tree's clean head `64b6b2f3`: `fn_800A1004`, `fn_800A1050`, `fn_800A1148`,
`fn_800A1180` and run 2's `fn_800A359C` all read **100.00%**, the unit stood at **49 / 60**, and
`AdvanceMotionTime` (retail 0x800A3B64, 436 B) was a **one-line TODO at 0.92%**. It is now
**100.00%**, so the unit is **50 / 60**.

## Result

| | before (`report.base.json`) | after (`build/report.json`) |
|---|---|---|
| `AdvanceMotionTime` (436 B) | 0.92% | **100.00%** |
| unit `matched_functions` | 49 / 60 | **50** / 60 |
| unit `fuzzy_match_percent` | 50.19% | 52.59% |
| unit `matched_code_percent` | 42.78% | 45.20% |
| tree `matched_functions` | 12422 / 28465 | **12423** / 28465 |
| tree `linked` (report_diff) | 5863 | 5863 |

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  12422 -> 12423   linked 5863 -> 5863   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/ScriptObjects/CScriptPlatform :: AdvanceMotionTime__15CScriptPlatformFf
no regression
```

`MP_GOAL_TREE=$PWD ./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**, every
step ok, including `target rose: ... CScriptPlatform: 49 -> 50 / 60 functions`.

## What I changed

**One file**, `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp`, +89/-1 (the comment above
`AdvanceMotionTime` and the body). No header change, no `config/`, no `tools/`, no `docs/`, no
`build/goal/`, no `.s`, no `asm`. Not committed. `.text` 12460 -> 12892 (`unit_fit`), i.e. exactly
the 436-byte function that used to be a 4-byte stub, and `unit_fit`'s extras are unchanged at
**29 functions / 2924 bytes**.

The eight non-obvious spellings are all in the comment in the file. The three that took the most
measuring, for whoever comes next:

1. **The `bool : 1` block at 0x48C: mwcceppc's read mask is not the inverse of its write mask.**
   A *write* to the field declared at index `b` is `rlwimi` with `MB = 24 + b`, `SH = 7 - b`,
   `stb` at the field's own byte. A *read* is `rlwinm.` with `SH = 25 - b`, i.e. it tests byte bit
   `6 - b`; index 7 degenerates to `clrlwi. r0,r0,31`, which tests bit 31 of a byte and is
   **always false** (that is retail's own direction test at 0x800A3BB4, and why
   `if (!mMotionForward)` always negates `dt`). I measured the whole table with a throwaway
   member function that returned all eleven flags as one `int` (declaration only, compiled,
   disassembled, then deleted - the header is byte-identical to HEAD):

   | field | index | byte | read emitted |
   |---|---|---|---|
   | `mDead` | 0 | 0x48C | `rlwinm. r0,r0,25,31,31` |
   | `mControlledAnimation` | 1 | 0x48C | `rlwinm. r0,r0,26,31,31` |
   | `mDetectCollision` | 2 | 0x48C | `rlwinm. r0,r0,27,31,31` |
   | `mSquishedRider` | 3 | 0x48C | `rlwinm. r0,r0,28,31,31` |
   | `mMotionActive` | 4 | 0x48C | `rlwinm. r0,r0,29,31,31` |
   | `mPassedMotionEnd` | 5 | 0x48C | `rlwinm. r0,r0,30,31,31` |
   | `mPassedMotionStart` | 6 | 0x48C | `rlwinm. r0,r0,31,31,31` |
   | `mMotionForward` | 7 | 0x48C | `clrlwi. r0,r0,31` |
   | `mPreviousMotionForward` | 8 | 0x48D | `rlwinm. r0,r0,25,31,31` |
   | `x48d_25_` | 9 | 0x48D | `rlwinm. r0,r0,26,31,31` |
   | `mMotionTransformed` | 10 | 0x48D | `rlwinm. r0,r0,27,31,31` |
   | `mMotionFlags & (1u << 9)` | - | - | `rlwinm. r0,r0,0,22,22` |

   The **gate** here is `mMotionActive` (index 4, SH = 29), not `mDetectCollision` (SH = 27) -
   guessing `mDetectCollision` was the one spelling that cost 4%.
2. **A named `const bool` local normalises the flag test; two raw uses of the `&` do not.**
   `const bool manual = mMotionFlags & (1u << 9);` gives `rlwinm. r3,r0,23,31,31` (MWCC
   normalises the value); writing `mMotionFlags & (1u << 9)` out in both the duration chain and the
   gate gives retail's `rlwinm. r3,r0,0,22,22`, with the common subexpression left in r3 and only
   ever tested (`cmplwi r3,0`, no normalisation needed). Same for `!= 0` and `> 0`: both normalise.
3. **`CMath::FastFmod(mMotionTime, duration)` is 99.72%, not 100.** Same instructions, but MWCC
   sinks the helper's `1.f / y` *inside* the loop arm, so the reciprocal lands in the register that
   held the constant (`fmuls f0,f3,f1` against retail's `fmuls f0,f3,f5`) and the whole float
   allocation shifts down one. Naming the reciprocal (`const float invDuration = 1.f / duration;`)
   above the arm and writing the modulo out with it reproduces retail exactly. That one change took
   99.72% -> 100.00% and is the last 0.28%.

The other five: the three duration overrides are unconditional overwrites, not an else-if chain;
`GetPositionSpline()` must be the **non-const** overload (it is the 8-byte symbol at 0x8032FC94);
the `>= duration` arm is the *fall-through*, so it is the first arm in the source; both clamp arms
set bit 7 (two separate copies, so the write is in both arms); and `mMotionTime = mMotionTime + step`
written out rather than `+=` is what makes MWCC keep the loaded value in f1 for the `fadds` *and*
reload the member into f3 for the compare.

## What is still open in the unit (re-measured, 10 functions)

`__ct__` 42.52% (1388 B), `AcceptScriptMsg` 1.98% (1608 B), `Move` 1.58% (2088 B),
`Think` 0.75% (536 B), `AdvanceMotionTime` **now 100%**, `DragSlave` 0.54% (736 B),
`MoveRiders` 0.45% (888 B), `PreThink` 0.24% (1688 B), `AddRider(vector)` 99.99% (660 B),
`fn_800A1D4C` 0.00% (172 B), `fn_800A1CE8` 0.00% (100 B).

`fn_800A1D4C`/`fn_800A1CE8` are unchanged and still blocked on a 0x50-byte polymorphic vector
element; `progress-prime1-cscriptplatform-waypointtracker-dtors` is already queued for them and is
not re-filed here.

### `AddRider(vector)` is still a two-byte store-order diff, and this run measured the shape

Retail `sth r7,12(r1)` / `mr r3,r30` / `addi r4,r1,88` / `sth r7,8(r1)`; ours `sth r7,8` /
`mr r3,r30` / `addi r4,r1,88` / `sth r7,12`. Both land in the **dead** frame temporary, and
`DecayRiders` (100%, our own source) shows the same construct compiling the *other* way round:
`sth r7,8` / `mr r3,r31` / `addi r4,r1,40` / `stw r8,36(r1)` / `sth r7,12`. So retail's ordering is
per-function scheduling noise, not a different source construct.

Three new spellings measured this run (added to the run-2 table; all still 99.99% or worse):

| spelling | `AddRider(vector)` |
|---|---|
| plain inline `CScriptMsg(...)` (kept) | **99.99%** |
| swap the two arms to `if (ridee == nullptr) { msg } else { ... }` | 77.54% |
| `CScriptMsg(kInvalidUniqueId, TUniqueId(id.value), kInvalidUniqueId, ...)` | 98.99% |
| hoist `const EScriptObjectMessage e = static_cast<EScriptObjectMessage>(0x584f4e50);` | 99.99% (byte-identical) |

Diagnostic that identifies the temporary: replacing the middle argument with `TUniqueId(0)` makes
the `ridee == nullptr` block emit **five** `sth` at 8/12/16/20/24 where the shipped source emits
four. So the temporary is a 20-byte, 4-byte-strided object, while the **live** `CScriptMsg` at
`r1+88` is 16 bytes at 2-byte stride (`sth r7,88 / sth r7,90 / sth r6,92 / stw r5,96 / stw r0,100`,
which already matches retail). The two are not the same type, and because every `rlwimi` in that
block inserts a value into a bit the following `stb` discards, **the source of the temporary's
values cannot change the bytes** - which is why six spellings of the call site could not reach
100%. The lever is register pressure elsewhere in the function, not this statement.
**Not a `WALL:`** - three spellings this run is not "several", and there is no evidence the
statement is the cause.

## Verified

```
sha1sum build/G2ME01/main.dol       -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh             -> All: 35.11% fuzzy, 28.82% matched, 12.90% linked (12423 / 28465 functions)
python3 tools/check_symbol_names.py -> checked 525 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform
                                     -> ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CScriptPlatform.cpp
                                     -> .text 12460 -> 12892; extras unchanged at 29 functions / 2924 bytes
MP_GOAL_TREE=$PWD ./tools/goal_check.sh build/goal/item.json
                                     -> PASS progress-prime1-cscriptplatform-erase-pair
      ok gate.sh | ok counts: matched 12422 -> 12423 linked 5863 -> 5863
      ok target rose: main/MetroidPrime/ScriptObjects/CScriptPlatform: 49 -> 50 / 60 functions
      ok no asm added
```

The gate's own `check_docs_claims.py --write` touched the two derived numbers in `docs/HANDOFF.md`
(`matched 12423 / 28465`, `DOL units 10875 / 16726`); that edit was reverted, as the brief
requires - the judge rewrites them. `objdiff.json` deliberately not touched.

## NEW:

`NEW: progress-prime1-cscriptplatform-think | progress | MetroidPrime/ScriptObjects/CScriptPlatform | Think (0x800A2258, 536 B) is 0.75% and is a flat sequence of independent flag tests, no calls; the bool:1 read table measured in this run's notes turns retail's SH values at 0x800A22C4/0x800A2304/0x800A2380/0x800A238C into mControlledAnimation / mDetectCollision / mMotionActive / mMotionTransformed, and the remaining unknowns are three callees (0x802C8A1C, 0x8004D640, 0x8014CF60), the float pool constants at -29564/-29592/-29604(r2), and CStateManager offsets 5636/5680/5684 and 180.`
