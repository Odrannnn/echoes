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
