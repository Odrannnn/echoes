# progress-prime1-crainsplashgenerator

`kind: progress`, target `MetroidPrime/CRainSplashGenerator` (DOL unit, stays `NonMatching`).

## Result, measured

`build/report.json`, this worktree, `./tools/decomp_build.sh`:

| | before (branch head) | after |
|---|---|---|
| unit `fuzzy_match_percent` | 42.41 | **88.90** |
| unit `matched_functions` | 5 / 22 | **14 / 22** |
| repo `matched_functions` | 9678 | **9687** (+9) |
| repo `matched_code` | 1419172 | **1420940** |
| repo `fuzzy_match_percent` | 29.9031 | **29.9339** |

`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:
`matched 9678 -> 9687   linked 4894 -> 4894   (+9 functions at 100%, 0 units newly linked)` /
`no regression`. All nine are in this unit; nothing anywhere got worse.

Gates, all green:
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
- `./tools/probe_sources.sh` -> `744 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)` (baseline 250, no growth)
- `python3 tools/check_symbol_names.py` -> `checked 502 units; 0 declared names are missing from their object`
- `python3 tools/check_decl_order.py` -> `ok: 956 unit(s) checked, 31 permuted, all 31 accounted for`
- `./tools/decomp_build.sh` -> `All: 29.93% fuzzy, 21.74% matched, 11.74% linked (9687 / 28465 functions)`
- no `asm` added; the diff touches only `src/MetroidPrime/CRainSplashGenerator.cpp` and
  `include/MetroidPrime/CRainSplashGenerator.hpp`.

## Files

- `src/MetroidPrime/CRainSplashGenerator.cpp` - every TODO body replaced with the real one.
- `include/MetroidPrime/CRainSplashGenerator.hpp` - `SSplashLine`'s three starting values became
  named `static const` members (see `__ct__SRainSplash` below). No member moved: the struct is
  still 23 bytes used / 24 with padding, and `CHECK_SIZEOF(CRainSplashGenerator, 0x4c)` still holds.

## Per function: before % -> after %, and how Prime 1's source fared

Prime 1 (`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref/src/MetroidPrime/CRainSplashGenerator.cpp`)
is the right source for most of this unit - Echoes' engine is a fork of it. Per function:

| function | before | after | Prime 1's source |
|---|---|---|---|
| `SSplashLine::Update` | 100 | 100 | already matched, untouched |
| `AddPoint` | 100 | 100 | already matched, untouched |
| `UpdateRainSplashes` | 100 | 100 | already matched, untouched |
| `Draw(CTransform4f)` | 100 | 100 | already matched, untouched |
| `SSplashLine::SetActive` | 100 | 100 | already matched, untouched |
| `DoDraw` | 0.93 | **100** | **verbatim**, no edit |
| `Update(float, CStateManager&)` | 76.15 | **100** | needed the Echoes `mForceRaining` tail; Prime 1's body plus it |
| `UpdateRainSplashRange` | 95.34 | **100** | **verbatim** but for `const int` on the parameters |
| `SRainSplash::Update` | 74.32 | **100** | `AUTO(it, begin())` -> raw-pointer loop, see below |
| `SRainSplash::Draw` | 71.91 | **100** | same |
| `SRainSplash::SetPoint` | 74.16 | **100** | same |
| `SRainSplash::IsActive` | n/a (0) | 96.25 | same; one instruction short, see the wall |
| `SRainSplash::SRainSplash()` | 92.31 | **100** | needed the `static const` members, see below |
| `SSplashLine::Draw` | 0.87 | 99.13 | Echoes differs in three places, see below |
| `GetNextBestPt` | 0.81 | 99.56 | rewritten: Echoes samples the model, not arrays |
| `GeneratePoints` | 1.59 | **100** | Prime 1's body, Echoes' signature |
| `GeneratePoint` | 8.60 | **100** | Echoes-only function, derived from Prime 1's `GeneratePoints` |
| `CRainSplashGenerator::CRainSplashGenerator` | 90.84 | 90.84 | unchanged; see the wall |
| `fn_801819B4`, `fn_801819EC`, `fn_8018271C`, `fn_80182790` | 0 | 0 | compiler-emitted `rstl` helpers; see the wall |

## What Prime 1's source got wrong for Echoes, and what the measured bytes said

1. **The `AUTO(it, mLines.begin())` loops.** Prime 1's `rstl::reserved_vector` uses
   `pointer_iterator`; this repo's typedefs are commented out and `iterator` is a raw `T*`. An
   index loop (`for (int i = 0; i < mLines.size(); ++i)`) does *not* produce retail's loop.
   Retail's is `lwz r0,0(r29); mulli r0,24; add r3,r29,r0; addi r3,4; cmplw r31,r3` -
   `it != end()` with `it` a `SSplashLine*`. Spelling it as
   `for (SSplashLine* it = mLines.begin(); it != mLines.end(); ++it)` took all four
   `SRainSplash` loops to 100%. `IsActive` needs the same loop and then differs by one
   instruction (below).

2. **`SSplashLine`'s starting values are constants, not literals.** Retail's
   `__ct__Q220CRainSplashGenerator11SRainSplashFv` loads them from the constant pool:
   `lfs f1,-23200(r2)`, `lfs f0,-23196(r2)`, `lbz r7,-23192(r2)`. With the literals in the
   member-initialiser list, mwcceppc emits `li r6,3` where retail has a `lbz` - 95.38%, and the
   scheduling differs too. Moving the three into `static const` members of `SSplashLine` defined
   in the `.cpp` (upstream's own arrangement - Prime 1 has exactly these three) reproduced the
   bytes: 100%. The `uchar` one is the load that matters; the two floats were already pool loads.

3. **`SSplashLine::Draw` is not Prime 1's function.** Three measured differences:
   - the trail is `mTime - delta * mSpeed` (with `delta = dt * mSpeed`), **not** Prime 1's
     `delta * mLength`; an intermediate `const float trail` is needed or mwcceppc fuses it into
     one `fnmsubs` where retail has `fmuls` + `fsubs`;
   - the line width is `mLineWidth * 6`, not `mLineWidth * 6` after `mLength` - retail does
     `lbz r0,20(r26)`, which is `mLineWidth`; with `mLength` (offset 21) it is 97.34%;
   - the parabola constant is a pool load of `-4.0` multiplied as
     `-4.f * vt * (vt - 1.f) * mParabolaHeight`, which is Prime 1's expression unchanged.
   Result 99.13%; the only difference left is one **dead** `lq r0,-24560(r26)` (an 8-byte load
   of `mParabolaHeight`+`mLineWidth` whose result retail never uses). I could not find a
   spelling that emits it; every version of the body that is otherwise byte-identical omits it.

4. **`GetNextBestPt` / `GeneratePoints` / `GeneratePoint` are Echoes' own.** Prime 1's versions
   take `(const CVector3f* vertices, const CVector3f* normals, int count)`; Echoes' take
   `(const CSkinnedModel&, const SSkinningWorkspace&, int count, CRandom16&, float minZ)` and
   sample the model. Derived from Prime 1's algorithm against the disassembly. The vertex count
   is `model.GetSkinRules()->GetNumPoints()` - measured as `lwz r4,20(model)` then
   `lwz r6,32(r4)`, which is `CSkinnedModel+0x14` (the `mItem` of the `TLockedToken<CSkinRules>`
   at `+0x0C`) and `CSkinRules+0x20` (`mVertexCount`).
   Three source details were each worth several percent and are worth recording:
   - **order**: retail computes `(refVert - vert)` and its `MagSquared()` *before* calling
     `GetSkinnedNormal`, and materialises the difference into a stack `CVector3f`. Taking its
     address (`const CVector3f& delta = refVert - vert`) is what makes mwcceppc store it;
     a by-value `const CVector3f delta` keeps it in registers: 85.36% -> 90.87%.
   - **`goodZ` is `>`**, not `>=`: retail emits a bare
     `fcmpo cr0,vert.z,minZ; mfcr; rlwinm r4,r0,2,31,31` with no `cror`, so one CR bit, so `>`.
     `>=` adds `cror eq,gt,eq` and a different shift: 90.87% -> 90.87% but the wrong bytes.
   - **`float maxDist = 0.f;` must be declared before `refVert`**, so its `lfs 0.0` is the
     first thing the function loads. With `maxDist` second, the compiler CSEs the three `0.0`
     literals into one register and needs a ninth callee-saved FP register, shifting every
     FP register by one and the frame by 16 bytes: 90.87% -> **99.56%**.

5. **`Update(float, CStateManager&)` is Echoes'**: Prime 1 has no `mForceRaining` and no
   `mForceRaining` branch. Retail's tail is
   `clrlwi. r0,r6,24; beq -> else; bl UpdateRainSplashes; ...set mRaining...; b end; else: ...clear...`
   so the call is inside `if (raining)` and the flag is written on both paths. The
   `switch (neededFx) { case kEFX_Rain: ... }` spelling (Prime 1's) is what produces retail's
   `cmpwi r3,2; beq <body>; b <tail>` pair rather than an inverted `bne`; and
   `if (envFx.GetRainMagnitude())` rather than `!= 0.f` is what puts `0.0` in the second
   operand of the compare.

6. **`GeneratePoints` hoists the vertex count above the `!mRaining` test**:
   retail loads `lwz r3,20(model)` *before* `rlwinm. r0,r0,26,31,31`. Declaring
   `const int count = model.GetSkinRules()->GetNumPoints();` as the first statement reproduces
   it (93.48% -> 100%).

## The port-side finding, which is the one thing a next run must not rediscover

`GetNextBestPt`, `GeneratePoints` and `GeneratePoint` call
`CSkinnedModel::GetSkinnedPosition` / `GetSkinnedNormal`. Those are defined in
`src/Kyoto/Animation/DolphinCSkinnedModel.cpp`, which is a `configure.py` unit and is **not** in
`files.cmake`; the port has no definition of either. With the retail bodies unguarded:

```
probe: 744 files, 0 failed, 0 errors; link: NOT LINKED (252 undefined, 0 duplicates)
link_check: STRICT FAIL - regression gate: 252 undefined against a baseline of 250 (GREW)
link_check: 2 symbol(s) this change ADDED to the gap:
  NEW  CSkinnedModel::GetSkinnedNormal(SSkinningWorkspace const&, int) const
  NEW  CSkinnedModel::GetSkinnedPosition(SSkinningWorkspace const&, int) const
```

`gate.sh`'s `port probe` step runs `probe_sources.sh`, which exits 1, so `goal_check.sh` would
fail the item. Defining the two methods port-side in `src/Kyoto/Animation/CSkinnedModel.cpp`
does not work either: they need `CModel::GetPositions`/`GetNormals`, which need `CCubeModel`,
which is unported too - that route took the count to 254. The chain bottoms out at
`DolphinCModel.cpp` (also unlisted, for the same reason: it pulls `CCubeModel`, `CCubeMaterial`
and `CFrameDelayedKiller`).

So the three functions are behind `#ifndef TARGET_PC`, with a `TARGET_PC` branch that records
the gap in the same shape as `src/MetroidPrime/CMiscTableInit.cpp` and
`src/Kyoto/Graphics/CModelPortStub.cpp`. The guards are **two** separate blocks, not one: this
file's declaration order is reverse retail offset (mwcceppc emits definitions in reverse source
order, so the order here is what puts retail's `.text` order back) and the constructor and
`AddPoint` sit between the three sampling functions. With one block the port build loses the
constructor, `CPlayerGunBase`/`CGrappleArm`/`CMorphBall` cannot construct a generator, and the
gap grows by one instead.

mwcceppc does not define `TARGET_PC`, so the matching build compiles retail's bodies and the
DOL hash is unaffected - which the sha1 above confirms.

## Walls

- `WALL: IsActive__Q220CRainSplashGenerator11SRainSplashCFv 96.25% - retail re-masks the bool accumulator on return (clrlwi r3,r4,24) where MWCC's range analysis already knows it is 0/1; a bool accumulator gives 96.25% and an int one drops the loop's own clrlwi (80.00%), so the accumulator type is not the lever`
- `WALL: __ct__20CRainSplashGeneratorFRC9CVector3fiiff 90.84% - the ctor's tail is retail's inlined rstl::vector::push_back (mCount++ and the element address in the caller, then one call); this repo's push_back is a weak out-of-line function because its inline size passes -pragma "inline_max_size(125)", so our object is 0x1B4 bytes against retail's 0x1D4 and every register after it is shifted`

Two more near-misses, measured, no `WALL:` because I have not tried enough spellings:

- `GetNextBestPt` 99.56% - instruction-for-instruction identical to retail; only the GPR
  allocation differs (retail r30=nextPt, r29=loop counter, r28=idx; ours r29/r28/r30). Moving
  `maxDist`/`nextPt` past `refVert` changes nothing (99.56% either way). The FP allocation is
  right, so the lever is GPR pressure, not the source order.
- `SSplashLine::Draw` 99.13% - one dead `lq` in retail that no spelling of the body reproduced.

## The four unnamed functions, and why the unit will not flip

`fn_801819B4` (56 B) and `fn_801819EC` (96 B) are `rstl::reserved_vector`'s `(int, const T&)`
constructor and the `uninitialized_fill_n` it calls; `fn_80182790` (160 B) and `fn_8018271C`
(116 B) are `rstl::vector`'s `reserve` and the element construct behind the constructor's
`push_back`. **All four are byte-identical to ours** - verified by disassembling
`build/G2ME01/src/MetroidPrime/CRainSplashGenerator.o` at `__ct__Q24rstl56reserved_vector<...>FiRC...`
(0x2F0, 0x38 bytes) and `uninitialized_fill_n<...>` (0x328, 0x60 bytes) and diffing against
retail's 0x264/0x29c. They score 0 only because objdiff cannot name them: dtk gives retail's
copy no symbol and ours carries the template's mangled name.

`./tools/unit_fit.sh MetroidPrime/CRainSplashGenerator.cpp` reports 17 functions in ours that the
retail unit object does not define, 1624 bytes: the four above are not among them, but the
object is 1160 bytes over the claimed `.text` range, so a flip is out of reach on this unit as
it stands. The extras are `rstl::vector`/`reserved_vector` instantiations this repo's headers
pull in and retail's do not (`~reserved_vector` 140 B, `~SRainSplash` 144 B, `~vector` 132 B,
`reserve` 172 B, `push_back` 124 B, plus `destroy`/`destroy_impl`/`uninitialized_copy*`). Making
`rstl::vector` instantiate fewer of them is a shared-header change that can move other units, so
I did not attempt it.

---

# Run 2 (lane 5, branch head `9070b2af`) - one more function matched, 14/22 -> 15/22

Re-measured on this tree first: the branch head already carries the run above
(unit 88.90% fuzzy, 14/22, repo `matched_functions` 10313), so this run is not a repeat of it.

## Result, measured

`build/report.json`, this worktree, after the change:

| | before | after |
|---|---|---|
| unit `fuzzy_match_percent` | 88.90 | **88.96** |
| unit `matched_functions` | 14 / 22 | **15 / 22** |
| unit `matched_code` | 2404 / 4320 | **2468 / 4320** |
| repo `matched_functions` | 10313 | **10314** |
| repo `matched_code` | 1548212 | **1548276** |

`./tools/goal_check.sh build/goal/item.json` -> **PASS**:
`counts: matched 10313 -> 10314  linked 5048 -> 5048`,
`target rose: main/MetroidPrime/CRainSplashGenerator: 14 -> 15 / 22 functions`,
`no asm added`, `no judge-owned path touched`. Gates it ran, all green:

- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
- `./tools/probe_sources.sh` -> `752 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)` (baseline 250, no growth)
- `python3 tools/check_symbol_names.py` -> `checked 505 units; 0 declared names are missing from their object`
- `python3 tools/check_decl_order.py` -> `ok: 972 unit(s) checked, 31 permuted, all 31 accounted for`
- `./tools/decomp_build.sh` -> `All: 31.31% fuzzy, 23.69% matched, 11.83% linked (10314 / 28465 functions)`

## The change: `SRainSplash::IsActive` 96.25% -> 100.00% (64 B, byte-identical)

This **supersedes the run-1 `WALL:` for `IsActive`** - that wall was about the *accumulator*, and
the accumulator was never the lever. The lever is the **top-level `const` on the return type**.

```c++
-    bool IsActive() const;                                   // include/.../CRainSplashGenerator.hpp:56
+    const bool IsActive() const;
- bool CRainSplashGenerator::SRainSplash::IsActive() const {  // src/.../CRainSplashGenerator.cpp:322
+ const bool CRainSplashGenerator::SRainSplash::IsActive() const {
```

The body is unchanged (`bool ret = false; for (pointer loop) ret |= it->mActive; return ret;`).
Retail ends `clrlwi r3,r4,24; blr` at 0x80181808; with a plain `bool` mwcceppc knows the
accumulator is already 0/1 and ends `mr r3,r4; blr` - 15 of 16 instructions, 96.25%. With the
`const` on the return type the extra canonicalisation is emitted and the function is byte-identical.
**Prime 1 declares it `const bool IsActive() const;` and its definition is
`const bool CRainSplashGenerator::SRainSplash::IsActive() const`** - this is Prime 1's spelling
verbatim, and the mangled name is unchanged (a top-level cv on a return type is not mangled:
`IsActive__Q220CRainSplashGenerator11SRainSplashCFv` before and after, confirmed with
`powerpc-eabi-nm`). The body did not need any adaptation, so this is also the answer to the item's
"did Prime 1's source match unchanged" for this function: yes, unchanged.

### The mechanism, for the next one that hits this

mwcceppc emits two different things for a bool:
- the **loop's** `clrlwi r4,r0,24` is the canonicalisation of `ret |= it->mActive` (int `or` into a
  bool) - always emitted, whatever the return type;
- the **return's** `clrlwi r3,r4,24` is an int->bool conversion node that the optimiser only keeps
  if the return type is *const-qualified*, because then the value has to be materialised as a
  `const bool` and cannot be forwarded. mwcceppc rejects a top-level-cv mismatch between the
  declaration and the definition (all four one-sided variants - const on the header only, on the
  definition only, `const bool ret` accumulator, `const bool` on a different local - **fail to
  compile**), so this is a two-line change or nothing.

### Every IsActive spelling measured this run (all at 96.25% or worse except the winner)

| spelling | % |
|---|---|
| `bool ret; ret \|= it->mActive; return ret;` (run 1's) | 96.25 |
| **`const bool IsActive() const` (header + definition)** | **100.00** |
| `const bool result = ret; return result;` (equivalent, local instead of return type) | 100.00 |
| `return ret != false;` / `ret == true` / `!!ret` / `static_cast<bool>(ret)` / `ret ? true : false` / `ret != 0` | 81.25 (adds the `clrlwi` **and** a `neg/or/srwi` non-zero test) |
| `int ret; ret \|= it->mActive; return ret;` | 80.00 |
| `uint ret; ...; return ret;` | 80.00 |
| `ret = ret \| it->mActive;` | 66.56 |
| `return ret & 1;` | 99.69 |

`const bool result = ret;` is the same effect without touching the header, if a future unit needs
it where the declaration is shared. Any explicit conversion in the *return statement* is wrong: it
emits the canonicalisation MWCC wants **plus** the non-zero test it wanted to avoid.

## What else was tried this run, and what it measured

All of these were built and measured with `./tools/fast_try.sh MetroidPrime/CRainSplashGenerator`
(about 0.5 s a build, so the sweeps are cheap; scripts are in `.tmp/opencode/L5/`).

### `__ct__20CRainSplashGeneratorFRC9CVector3fiiff` (ctor) 90.84% - one step further, then stopped

The run-1 wall said the missing bytes are retail's **inlined** `rstl::vector::push_back` body
(`lwz mCount; mulli 116; mCount++; add dest; bl <copy>; lwz/stw src+112 -> dest+112`). Measuring the
two loops side by side shows retail's inline sequence has **no capacity check**, so the callee is
`push_back_unsafe`, not `push_back`. That part is now settled. What is *not* settled is the inline:

| spelling | % |
|---|---|
| `push_back(SRainSplash())` (run 1) | 90.84 |
| `push_back_unsafe(SRainSplash())` | 90.84 |
| named local `SRainSplash splash;` + `push_back_unsafe` / + `push_back` / `const SRainSplash splash;` | 90.84 |
| `#pragma inline_max_size(0/25/50/75/100/125/138/150/175/200/250/300/400/600/1000/10000)` with `push_back_unsafe` | 90.84 at every value |
| **`rstl::construct(mRainSplashes.data() + mRainSplashes.mCount++, SRainSplash());`** (body written straight into the loop, nothing to inline) | **96.87** |
| same via `mItems` instead of `data()`, and with a named temp | 96.87 |
| `new (dest) SRainSplash();` | 76.11 |

So `#pragma inline_max_size` is **not** the lever for `push_back_unsafe`: at 10000 the body is still
a call. The pragma is real, though - at 175..250 it drops one other function
(`__ct__Q220CRainSplashGenerator11SRainSplashFv` 100 -> 0), and at 200+ it changes the ctor's
neighbours, so the file's threshold really is 125 and 125 is right here.

Writing the body inline does produce retail's instruction sequence, and the last three instructions
it still misses are all one cause. Retail:

```
add  r29,r5,r0        # dest
mr   r3,r29
bl   8018271c         # a routine that copies offsets 0..111 and nothing else
lwz  r0,120(r1)       # the 4-byte tail, offset 112 = SRainSplash::x70_
stw  r0,112(r29)
```

Ours calls `rstl::construct` -> `construct_impl` -> `__ct__SRainSplash(const SRainSplash&)` ->
`uninitialized_copy_n`, i.e. **four** nested out-of-line calls, and the innermost one copies all
116 bytes. Retail has **one** call, and it copies 112: that is `mCount`(4) + `mData`(96) +
`mPosition`(12) merged into a single 8-aligned block copy, with the 4-byte `x70_` left to the
caller. The merge can only happen if the 96-byte `mData` copy is itself an inline block, i.e. if
`uninitialized_copy_n` is inlined into the copy constructor - and **that is the one thing a pragma
in the .cpp cannot do**: mwcceppc records `inline_max_size` per function *definition*, and
`uninitialized_copy_n` is defined in `include/rstl/construct.hpp:126`, which is parsed before the
pragma (this is the same reason `include/rstl/construct.hpp:9-11` says the `RSTL_PRECONDITION`
stubs "still count toward MWCC's inline size limit"). Making it inline needs
`#pragma inline_max_size` in a shared header, which moves every unit that uses
`uninitialized_copy*`. **That is the blocker, and it is a shared-header change, so I stopped.**

**I did not keep the 96.87% spelling in the diff.** It raises no matched count (the ctor is still
sub-100, and on a `progress` item only exact matches count), and it replaces a `push_back` with
`rstl::construct(...data() + ...mCount++, ...)` in a constructor, reaching into vector internals for
a percentage. It is recorded here so the next run does not have to re-derive it.

### `GetNextBestPt` 99.56% - the wall from run 1 holds; this run adds spellings, not a lever

Re-measured and confirmed instruction-for-instruction identical to retail apart from **which of
r28/r29/r30 holds which of three same-live-range values**:

| | nextPt | loop counter `i` | random `idx` |
|---|---|---|---|
| retail | r30 | r29 | r28 |
| ours | r29 | r28 | r30 |

Same 8 callee-saved GPRs, same live ranges, same FP allocation, same frame. Spellings tried:
`i` declared before `refVert` and hoisted out of the `for` header (99.56, 97.98), `i` between
`refVert` and the loop (99.56), `i != 3` instead of `i < 3` (99.52), `i++` instead of `++i` (99.56),
`const int first = pt` feeding both `nextPt` and `refVert` (99.56). Nothing moves it; the
allocation order of the three values is chosen by the backend, not by the source order.

### `SSplashLine::Draw` 99.13% - the dead `lq` is not reachable from source; here is the proof

The one remaining difference is retail's dead 8-byte load at 0x80181AD8, between `fmuls f31,f2,f0`
and `fmuls f0,f31,f0`. `r0` is overwritten by `lbz r0,20(r26)` eleven instructions later and never
read, so it is a compiler artefact, not a missing computation. I now know why the previous run
could not spell it: the base register really is `r26` (= `this`, set by `mr r26,r3` at 0x80181AC8
and never reassigned) with displacement **-24560**, so the address is `this - 24560`, i.e. garbage.
The earlier note in this file guessed it was "an 8-byte load of `mParabolaHeight`+`mLineWidth`",
which would be `lq r0,16(r26)`; that guess is **wrong** and superseded - the displacement is not 16.
The only self-consistent reading is that MWCC's SDA21 pass rewrote an SDA2 constant reference
(`lq rD,d(r2)`) into `lq rD,d-1372(r26)` after believing `r26 == r2+1372`, then a later pass killed
the result and left the load. No source spelling produces an 8-byte load from `this-24560`.

Spellings measured (all worse or equal): `const float speed = mSpeed;` local (99.13),
`delta = mSpeed*dt` (99.04), `vt` assigned from `trail` then subtracted (97.91), the fused
`mTime - delta * mSpeed` with no `trail` local (97.35), `const float mT = mTime` (99.13),
`vt = trail - mTime; vt = -vt;` (97.52), `vt = vt + 0.f;` (97.61). The run-1 body
(`delta = dt*mSpeed`, `trail = delta*mSpeed`, `vt = mTime - trail`) is the best of the seventeen and
stays.

## Where the unit stands, and what a next run should not redo

`IsActive` is now matched, so the reachable ceiling for this unit is **18/22**: the four
`fn_*` functions have no retail symbol, so objdiff can never pair them, and the remaining three
(ctor 90.84, `Draw` 99.13, `GetNextBestPt` 99.56) are each blocked on something measured above:
the ctor on a shared-header `uninitialized_copy_n` inline, `Draw` on a dead load, `GetNextBestPt` on
the backend's GPR choice. `IsActive` is the reason this run is worth +1: a 0.06% unit score change
(88.90 -> 88.96) and a whole extra function.

**WALL lines for this run:**

- `WALL: GetNextBestPt__20CRainSplashGeneratorFiRC13CSkinnedModelRC18SSkinningWorkspaceiR9CRandom16f 99.56% - identical instruction stream to retail; only the assignment of r28/r29/r30 to nextPt/loop-counter/idx differs, and 5 more source-order spellings (i hoisted, i != 3, i++, named first local, i between refVert and loop) all leave it at 99.5x: the register choice is the backend's, not the source order's`
- `WALL: Draw__Q220CRainSplashGenerator11SSplashLineCFffRC9CVector3f 99.13% - the single difference is a DEAD lq r0,-24560(r26) whose base is this and whose address is therefore garbage; 8 further body spellings cannot emit it, and the earlier guess in this file that it was an 8-byte load of mParabolaHeight+mLineWidth (which would be lq r0,16(r26)) is superseded`

I am **not** writing a `WALL:` for the ctor: the 96.87% spelling is a real step and the next thing to
try is stated above (make `uninitialized_copy_n` inline, which needs the shared header, and is the
one piece of this unit's remaining work that could still reach 100% on its own).
