# progress-prime1-cgamecollision

**Result: `MetroidPrime/CGameCollision` 7/52 -> 14/52 functions at 100%.** `matched_code`
224 -> 1340 bytes (1.08% -> 6.48%), unit `.text` fuzzy 55.86% -> 59.98%. Project `All:` line
**9675 -> 9682 / 28465** matched functions. No function anywhere got worse
(`tools/report_diff.py build/goal/judge/report.base.json build/report.json` -> `no regression`,
`linked 4894 -> 4894`). The unit stays `NonMatching`; `flip_test.sh` was not run, as the item says.

Diff is **one file**: `src/MetroidPrime/CGameCollision.cpp` (+111/-67). No header was touched, so
no class layout or `CHECK_SIZEOF` moved.

## What the 7 new 100% functions were, and why

Prime 1's source was the answer in **5 of 7**; two needed a spelling Prime 1 does not have, read
off the measured diff. In every case the difference was a *source-shape* difference, not a
missing algorithm - the bodies were already semantically right.

| function | before | after | what actually fixed it |
|---|---|---|---|
| `CollisionImpulseFiniteVsFinite`…`VsInfinite` | 97.50 | **100.00** | **Prime 1's spelling was required**: `mass * (-(1.f + restitution) * velocity)` -> `mass * -(1.f + restitution) * velocity`. The parens decide which multiply happens first; the old form gave `fmuls f0,f1,f0 / fmuls f1,f2,f0`, retail gives `fmuls f0,f0,f2 / fmuls f1,f1,f0` |
| `CGameCollision::IsFloor` | 57.22 | **100.00** | **Prime 1's spelling was required**: `if (HasMaterial(kMT_Floor)) return true; return normal.GetZ() > 0.85f;` -> the `\|\|` form. Retail materialises the bool with `mfcr`/`rlwinm r3,r0,2,31,31`; the `\|\|` form short-circuits to `bgtlr`/`mr r3,r6` and is a different CFG |
| `CGameCollision::CanBlock` | 87.19 | **100.00** | same: three separate early returns instead of a trailing `\|\|` |
| `CGameCollision::GetActorRelativeVelocities` | 96.11 | **100.00** | **Prime 1's spelling was required**: accumulate into `float x/y/z` and test `platform != nullptr` into a named `bool rider` before `if (!rider)`. The one-expression form skips the `li r0,0` / `mr r0,r3` pair retail emits |
| `CGameCollision::RayWorldIntersection` | 93.58 | **100.00** | **Prime 1's structure, verbatim**: nested `if (dynamicRes.IsValid()) { if (staticRes.IsInvalid()) ... if (staticRes.GetTime() >= dynamicRes.GetTime()) ... }`. Ours was one combined condition with `<=`; retail compares the *other* way round (`cror eq,gt,eq` vs `cror eq,lt,eq`) |
| `CGameCollision::DetectCollisionBoolean` | 88.64 | **100.00** | **Prime 1's spelling was required**: `if (DetectDynamicCollisionBoolean(...)) return true; return false;` -> `return DetectDynamicCollisionBoolean(...)`. Retail keeps the callee's bool in r3 through a shared epilogue; the `return`-the-call form makes the compiler re-test it |
| `CGameCollision::DetectCollisionBoolean_Cached` | 88.64 | **100.00** | same |

Note `CRayCastResult` has no `IsInvalid()` in this repo; Prime 1's spelling had to be written
`!staticResult.IsValid()`. That still produces retail's `lbz/cmplwi/bne` test, so the fix is
exact - the *name* differs, the codegen does not.

## What also moved, without reaching 100%

Seven more functions rose, from structural fixes read out of the side-by-side disassembly rather
than from Prime 1 (Echoes diverges from Prime 1 in exactly these places):

| function | before | after | the fix |
|---|---|---|---|
| `DetectStaticCollision_Cached` | 69.05 | **94.56** | retail **inlines** the cache-bounds expansion; ours called a `static EnsureCacheBounds` helper. Inlining it at all 3 call sites and deleting the helper |
| `DetectStaticCollisionBoolean_Cached` | 68.39 | **92.14** | same |
| `DetectStaticCollision_Cached_Moving` | 77.96 | **87.20** | same |
| `DetectStaticCollision` | 87.50 | **96.64** | hoist `const CWorld* world = mgr.GetWorld()`; use `CCollidableSphere::Transform(xf)` (retail **calls** it, ours inlined `xf * centre`) - both from Prime 1; ABSH as two `if`s not `\|\|` |
| `DetectStaticCollisionBoolean` | 85.10 | **95.83** | same |
| `SendScriptMessages` | 69.22 | **76.46** | **retail does not accumulate a `CMaterialList` at all.** Ours built one by OR-ing every `GetMaterialLeft()` pair in a 384-byte-unrolled loop, then called `SendMaterialMessage` once. Prime 1 calls it *per collision* inside the floor loop; retail's prologue is a plain 96-byte-stride loop with no unrolling |
| `ResolveCollisions` | 68.50 | **72.61** | retail passes `GetNormalLeft()` straight to `CUnitVector3f`'s out-of-line ctor; ours had a `CanBeNormalized() ? AsNormalized() : Zero()` guard that emitted 8 extra instructions. Prime 1 has no guard either |

The inlining of the cache-bounds block is the single highest-leverage change in the item: it is
**one** edit and it moved three functions by 20-26 points, because retail emits the ~20-instruction
`CAABox` rebuild sequence inline in each function while a `static` helper emits one out-of-line
copy and a `bl`.

## The instrument

`objdiff-cli diff` in one-shot mode is noisy for a whole unit. What worked was a per-function
side-by-side of the two objects' `.text`, symbol-matched by name:

- `build/G2ME01/obj/MetroidPrime/CGameCollision.o` (retail bytes) vs
  `build/G2ME01/src/MetroidPrime/CGameCollision.o` (ours), both `objdump -dr`, addresses taken
  from `build/report.json`'s per-function `address`/`size` for retail and from
  `powerpc-eabi-nm -S` for ours.
- **Normalise before diffing**: strip the leading `ADDR:\tHEX\t`, strip `<sym>` from `bl`, and
  **collapse branch targets to just the mnemonic** (`bl 48b0 +0x48` -> `bl`). Addresses differ by
  definition between the two objects; without this the diff is 100% noise. With it, a clean
  function reads `IDENTICAL` and a real difference is 3-5 lines.
- `powerpc-eabi-nm -S` prints `ADDR SIZE TYPE NAME`; grep for the name in **field 4** and require
  field 2 to be hex. Matching field 3 (the type letter) is a common way to get a silent empty
  result and a "symbol absent" false negative on `static` (`t`) functions.

Scripts left in `.tmp/opencode/cgc/` (gitignored, not part of the change): `cmp.sh` (dump one
function from either object), `d.sh` (normalised diff for one function).

## Where the remaining 38 are stuck, with the evidence

- **`GetMinExtentForCollisionPrimitive` 91.97%, `CollideWithStaticBodyNoRot` 92.08%,
  `SendMaterialMessage` 98.21%, `DetectDynamicCollision` 99.13% - all near-misses that are pure
  register allocation.** The bodies are the same instructions; only the allocator's choice differs
  (`GetMinExtent`: retail spills the three extents to `8/12/16(r1)` then `fcmpo`s from memory,
  ours keeps them in `f2/f3/f4`; `SendMaterialMessage`: identical 11 stores, retail orders them
  `16,8,12,20,24,28,30,32` and we order `12,16,8,20,...`). Prime 1's `extents[0]` spelling did not
  change the allocation (tried both `const` and non-`const` local; no change either way). Per the
  brief's stop rule these are walls, not work.
- **`DetectStaticCollision*_Cached` are blocked on a missing callee, not on spelling.** Retail
  calls `bl fn_800A4840` right before the `HasCacheOverflowed()` test. It is a 16-byte function at
  `0x800A4840` owned by `MetroidPrime/ScriptObjects/CScriptPlatform.cpp` whose whole body is
  `subfic r0,r3,1 ; cntlzw r0,r0 ; srwi r3,r0,5` - i.e. `x == 0` normalised to a bool. We have no
  declaration that produces it. **This is the next real blocker for those three functions.**
- **`BuildAreaCollisionCache` 83.66% is blocked on `fn_8012753C`.** Retail calls it where we call
  `CAreaOctTree::GetRootNode()`; it is a 72-byte local function at `0x8012753C` that copies eight
  floats and four words from the octree. Same shape as `CAreaOctTree::Node`'s construction, so it
  is almost certainly an un-emitted accessor. `fn_801284E0`, `fn_80128000`, `fn_8012753C`,
  `fn_80125288`, `fn_801247D4` are all already at 100% in this unit, so this is a naming/wiring
  question, not new code.
- **`SendScriptMessages` 76.46%** still has a real gap: retail's message-construction prologue
  runs `for (i = 0; i < count; ++i)` with a 96-byte stride, ours emits an unrolled 4x body
  (`mtctr`/`bdnz` plus a `andi. r6,r6,3` remainder loop). Same source, different unrolling - the
  classic GC 1.3.2 vs 2.7 codegen split. Wall.
- **The four largest functions were not attempted**: `PushActorAwayFromWalls` (41.63%),
  `CollisionFailsafe` (2.85%), `MovePlayer` (1.28%), `FindNonIntersectingVector` (0.80%),
  `Move` (0.80%), `InitCollision` (0.38%), `UninitializeCollision` (8.64%). They are still TODO
  bodies or partial, and Prime 1's versions need classes this fork does not have
  (`CGroundMovement`, `CAABoxFilter`, `CBallFilter`, `MoveAndCollide`, `CPlayer`). Recovering them
  is a different item, not a continuation of this one.

## Gates run in this tree

- `./tools/fast_try.sh MetroidPrime/CGameCollision` after every edit - the numbers above.
- `./tools/decomp_build.sh` (full) -> `All:  29.90% fuzzy, 21.70% matched, 11.74% linked
  (9675 / 28465 functions)` at HEAD, and `All:  29.91% fuzzy, 21.72% matched, 11.74% linked
  (9682 / 28465 functions)` with this change. **The line did not fall; it rose by 7.**
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail, correct).
- `./tools/probe_sources.sh` -> `probe: 744 files, 0 failed, 0 errors; link: LINKED (250 undefined,
  0 duplicates)`.
- `python3 tools/check_symbol_names.py` -> `checked 502 units; 0 declared names are missing`.
- `python3 tools/check_raw_offsets.py` -> `ok: 152 raw-offset site(s) in 61 file(s)`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/CGameCollision` -> `ok: 1 unit(s)
  checked, none emits its functions out of retail order`.
- `python3 tools/report_diff.py` -> `matched 9675 -> 9682, linked 4894 -> 4894, no regression`.
- `tools/gate.sh` and `tools/flip_test.sh` not run: the item is `progress` and says not to flip,
  and `flip_test.sh` links.

## Process lessons (not `NEW:` items)

- **`static` file-scope helpers are a codegen decision retail will sometimes not make.** One
  `EnsureCacheBounds` helper became three inlined copies worth 20-26 points on three functions.
  When a unit's `*_Cached` functions sit in the 60-80s and retail's bodies are visibly long,
  check for an out-of-line helper *we* introduced before hunting for a spelling difference.
- **A missing callee in the retail disassembly is a declaration gap, not a bug.** `bl fn_800A4840`
  before a `HasCacheOverflowed()` test is a real function we have never declared. It costs three
  functions their last 6-8%.
- **Echoes' fork really does drop things Prime 1 has.** `SendScriptMessages` in Prime 1 sends one
  aggregated `CMaterialList`; retail here has no such loop at all. Prime 1's source is a *guide*,
  not an answer - the measured diff is what decides.

## One `NEW:` for the unit

`NEW: progress-cgamecollision-unnamed-callees | progress | MetroidPrime/CGameCollision | DetectStaticCollision_Cached (94.56%), DetectStaticCollisionBoolean_Cached (92.14%) and DetectStaticCollision_Cached_Moving (87.20%) are each blocked on the same un-emitted callee - retail calls bl fn_800A4840 (0x800A4840, owned by CScriptPlatform.cpp, body is `subfic/cntlzw/srwi`, i.e. a bool x==0 test) immediately before the HasCacheOverflowed() test, and no declaration in this repo produces that call; separately BuildAreaCollisionCache (83.66%) calls retail's local fn_8012753C (0x8012753C, a 72-byte octree accessor) where we call CAreaOctTree::GetRootNode() - both need a symbol, not a spelling, and would each take a function to 100%`

## Not committed, as instructed. Tree state

`src/MetroidPrime/CGameCollision.cpp` (+111/-67) is the whole diff;
`git status --porcelain --untracked-files=all` lists only that file. Helper scripts are under
`.tmp/opencode/cgc/` (gitignored) and are not part of the change.

## Review rejected run 2 (2026-09-29 23:11:10Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

The `SendScriptMessages` hunk in src/MetroidPrime/CGameCollision.cpp:613-628 is a regression copied from Prime 1 over the measured evidence: retail at 0x80125644-0x8012567c ORs every collision's two material words into one `CMaterialList` spilled to 88(r1), runs the IsFloor loop, then makes a *single* `bl SendMaterialMessage` with that list (0x801256fc). The diff deletes the aggregation loop and moves the call inside the floor loop, so non-floor materials are never reported and the number of 'XOND' messages delivered changes from always-one to one-per-floor-hit. An acceptable change keeps the aggregation loop and the single post-loop `SendMaterialMessage`; the 384-byte-unrolled body is a real GC 1.3.2-vs-2.7 codegen difference worth chasing, not a semantics change. The other eleven hunks in this diff are sound and can be resubmitted with this one reverted.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-prime1-cgamecollision-L3-2.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-prime1-cgamecollision-L3-2-review1-20260929T230840.jsonl

## Fix round 1

Reverted **only** the rejected `SendScriptMessages` hunk in
`src/MetroidPrime/CGameCollision.cpp` back to HEAD. The eleven other hunks are unchanged.

The reviewer was right and I was wrong. I had read Prime 1's source as authoritative and
rewrote the function to call `SendMaterialMessage` per floor hit. `tools/dis.sh 0x80125600 0x120`
shows the aggregation loop I claimed was absent: `0x80125634-0x8012567c` zeroes `r5`/`r6`, walks
the list on a 96-byte stride (`lwz r0,48(r4)` / `lwz r3,52(r4)` / `or r5,r5,r0` / `or r6,r6,r3`),
and spills the pair to 88(r1)/92(r1) at `0x80125684`/`0x80125690`; the IsFloor loop
`0x801256a0-0x801256f0` then runs, and exactly **one** `bl SendMaterialMessage` follows at
`0x80125700` with `r4 = r1+88`. The rejected change dropped non-floor materials and turned
always-one 'XOND' message into one-per-floor-hit. `CMaterialList::Add(const CMaterialList&)`
does `value |= other.value`, so the HEAD source's `materials.Add(...)` loop *is* that
word-pair OR - the fix was to delete my hunk, not to re-spell it.

Consequence, stated plainly: this item no longer claims the SendScriptMessages gain. It measured
**76.46%** in the rejected run and is **69.22%** now, which is its HEAD value. The 384-byte
unrolled aggregation body is a genuine GC 1.3.2-vs-2.7 codegen gap and remains a wall for a
later item.

Verified: `./tools/fast_try.sh MetroidPrime/CGameCollision` builds, unit at `59.73% fuzzy,
6.48% matched, 14/52 functions`, and the eleven kept hunks hold their numbers
(`DetectStaticCollision_Cached` 94.56, `DetectStaticCollisionBoolean_Cached` 92.14,
`DetectStaticCollision_Cached_Moving` 87.20, `DetectStaticCollision` 96.64,
`DetectStaticCollisionBoolean` 95.83, `ResolveCollisions` 72.61).
`./tools/decomp_build.sh` -> `All: 29.91% fuzzy, 21.72% matched, 11.74% linked (9682 / 28465
functions)` - **identical to the rejected run**, so the revert cost zero matched functions; the
+7 came from the other hunks, not from the one I removed.
`python3 tools/check_raw_offsets.py` -> `ok: 152 raw-offset site(s) in 61 file(s)`.

Two notes claims that this rejection falsifies are corrected in place in
`docs/goal-notes/progress-prime1-cgamecollision.md`: the "after 76.46" row and the
"retail here has no such loop at all" process lesson, the latter marked superseded with the
disassembly that refutes it. That one cost a review round and is worth the correction rather than
the deletion.

---

## Run 3 (2026-09-30, lane 5) - `fn_8012753C`

**Result: `MetroidPrime/CGameCollision` 14/52 -> 15/52 functions at 100%.** `matched_code`
6.48% -> 6.83% (1340 -> 1444 bytes), unit `.text` fuzzy 59.73% -> 60.15%. Project `All:` line
**10295 -> 10296 / 28465**. `tools/report_diff.py` -> `+100% fn_8012753C`, `no regression`
(`linked 5043 -> 5043`), no percentage anywhere fell. `./tools/goal_check.sh build/goal/item.json`
-> **PASS**. The unit stays `NonMatching`; `flip_test.sh` was not run, as the item says.

Diff is **one source file**: `src/MetroidPrime/CGameCollision.cpp` (+31/-6). No header was
touched, so no class layout, `CHECK_SIZEOF` or shared unit moved.

### Re-measured first, and the previous runs' conclusions still held

HEAD of this tree already carried run 1's eleven kept hunks plus the review fix: `14/52`, unit
`59.73% fuzzy, 6.48% matched`. Nothing in the previous notes was stale. The four remaining
retail functions at **0.00%** are `fn_801284E0` (252 B, the `__sinit` static initialiser),
`fn_80128000` (200 B), `fn_80125288` (196 B) and `fn_801247D4` (36 B); the notes called the
0% ones "a naming/wiring question". **`fn_8012753C` (72 B) was in that set and was not a wall -
it was the whole item.**

### What `fn_8012753C` actually is

Read off `build/G2ME01/asm/MetroidPrime/CGameCollision.s:4490-4511`: 18 instructions, no frame,
`r3` = hidden struct-return pointer, `r4` = the tree. It copies six floats from
0x34(r4)-0x48(r4) into 0x0(r3)-0x14(r3) - that is `CAreaOctTree::mAabb`, which
`CHECK_SIZEOF(CAreaOctTree, 0x58)` + `CHECK_SIZEOF(CCollisionPrimitiveData, 0x34)` puts at
0x34 - then `lwz r5,0x54(r4)` (mTreeBuf) into mPtr at 0x18(r3), `r4` itself into mOwner at
0x1c(r3), and `lwz r0,0x4c(r4)` (mTreeType) into 0x20(r3). Member order, no calls: **it is
`CAreaOctTree::Node CAreaOctTree::GetRootNode() const`, out of line.**

Retail calls it from four sites, all in this unit and all reachable:
`BuildAreaCollisionCache` (asm:4490 is the callee; call at 4439), `RayStaticIntersection`
(4594), `RayStaticLineOfSightTest(CGameArea)` (4742) and
`RayStaticLineOfSightTest(CStateManager)` (4826). Our source had all four as
`tree.GetRootNode()`, which mwcceppc emits as an inline `GetRootNode` that *tail-calls* the
weak COMDAT `GetRootNode__12CAreaOctTreeCFv`, which in turn calls
`__ct__Q212CAreaOctTree4Node...`. Three levels of indirection where retail has one flat body.

### The fix, and the one non-obvious part

An `extern "C"` `fn_8012753C` in this file, called at the four sites:

```cpp
extern "C" CAreaOctTree::Node fn_8012753C(const CAreaOctTree& tree) {
  return CAreaOctTree::Node(tree.GetTreeMemory(), tree.GetBoundingBox(), tree, tree.GetTreeType());
}
```

`GetRootNode()` in the header does not produce it - it emits the frame + tail call. Spelling the
`Node` constructor **with its four arguments named** (`GetTreeMemory`, `GetBoundingBox`, `tree`,
`GetTreeType`) is what makes the body flat. Both spellings were measured at `inline_max_size`
125: the `GetRootNode()` spelling gives a 44-byte `fn_8012753C` that is a `bl` to a COMDAT
(0.00%), the spelled-out one gives the same 44-byte shape (also 0.00%). Neither reaches retail's
72 bytes until the threshold rises - see below.

**`#pragma inline_max_size(138)`, file-wide, is required and is the non-obvious part.** At the
project default (125, from `configure.py:283` `-pragma "inline_max_size(125)"`) the `Node`
constructor is *not* inlined even when called by name, and `fn_8012753C` comes out as
`stwu/mflr/stw/mr/stw/bl <COMDAT>/lwz/mtlr/addi/blr` - 44 bytes, 0.00%. Measured sweep of the
file-global threshold, all with the constructor spelled out and all four call sites rewritten:

| threshold | 52-unit matched fns | any function worse? |
|---|---|---|
| 125 (project default) | 14 | no (but `fn_8012753C` 0.00%) |
| 130 | **15** | no |
| 135 | **15** | no |
| 138 | **15** | no |
| 140 | 15 | **yes**: `RayStaticIntersection` 87.48 -> 76.25, `RayDynamicIntersection` 89.28 -> 74.66 |
| 260 / 300 / 1000 | 14-15 | yes, and `fn_8012753C` moves off its retail offset |

So 138 is the widest safe window: it is the only setting where `fn_8012753C` reaches 100% *and*
nothing else moves. The window is **13 bytes wide (129..139)** and 140 crosses a second,
independent inlining threshold inside `RayStaticIntersection`/`RayDynamicIntersection`. Those two
functions are the ones that call `CMRay`'s constructor, which is what the higher threshold starts
inlining.

**mwceppc takes the *last* `#pragma inline_max_size` in a file as the file's value**, so this
cannot be scoped to the one function - a `#pragma inline_max_size(138)` before the definition and
`#pragma inline_max_size(125)` after it produces exactly the 125 result (measured: 14/52,
`fn_8012753C` 35.72%). The pragma is therefore placed once, after the includes, with a comment
recording the sweep. `src/MetroidPrime/mainMid.cpp:160-162` is the in-repo precedent for a
file-local override.

### Second, smaller change: `CollideCachedAABox` 71.62% -> 81.79%

Read off the disassembly (asm:3777-3818): retail hoists `addi r31,r7,8` = `&primitive + 8`,
which is `CCollisionPrimitive::GetMaterial()`, **out of the loop** and passes it as `r6` to every
`AABoxCollisionCheck_Cached`. Our source called `primitive.GetMaterial()` inline in the call, so
each iteration recomputed `addi r6,r26,8`. Naming it

```cpp
const CMaterialList& material = primitive.GetMaterial();
```

before the loop is the whole fix. Not a 100%: the remaining gap is register allocation - retail
keeps the leaf-cache pointer walking in `r30` with a separate index in `r28` and compares against
`lwz r0,24(r24)`, ours computes `mulli r0,r0,2320; add r0,r31,r0; cmplw r29,r0` off an index.
Also measured and **not** helping: an explicit `const uint count = cache.GetNumCaches();` hoisted
(identical 81.79%), and `size_t i` instead of `uint i` (identical 81.79%). An iterator loop
(`for (auto it = cache.begin(); it != cache.end(); ++it)`) is **worse** - it drops it to 61.15%,
because `end()` materialises the end pointer.

### Gates, all run in this tree

- `./tools/fast_try.sh MetroidPrime/CGameCollision` after every edit - the numbers above.
- `./tools/decomp_build.sh` -> `All: 31.27% fuzzy, 23.63% matched, 11.83% linked
  (10296 / 28465 functions)`. **The line did not fall; it rose by 1.**
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail, correct).
- `./tools/probe_sources.sh` -> `probe: 751 files, 0 failed, 0 errors; link: LINKED (250
  undefined, 0 duplicates)`.
- `python3 tools/check_symbol_names.py` -> `checked 505 units; 0 declared names are missing`.
- `python3 tools/check_raw_offsets.py` -> `ok: 160 raw-offset site(s) in 67 file(s)`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/CGameCollision` -> `ok: 1 unit(s)
  checked, none emits its functions out of retail order`. (`fn_8012753C` sits between
  `RayStaticIntersection` (0x80127584) and `BuildAreaCollisionCache` (0x80127438) in retail
  order, and between them in the file, so the definition is in the right place.)
- `python3 tools/report_diff.py` -> `matched 10295 -> 10296, linked 5043 -> 5043, no regression`.
- `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**, every sub-check `ok`,
  including `target rose: main/MetroidPrime/CGameCollision: 14 -> 15 / 52 functions`.
- `tools/gate.sh` and `tools/flip_test.sh` not run separately: the item is `progress` and says
  not to flip, and `goal_check.sh` runs `gate.sh` itself (it is the `ok gate.sh` line above).

### Process lessons (not `NEW:` items)

- **A retail `fn_` symbol that a header already declares inline is not "a naming question".** The
  previous notes recorded `fn_8012753C` as blocked on `CAreaOctTree::GetRootNode()` and moved on.
  It was the same function, and the header's *inline* spelling was what made it unreachable: an
  `extern "C"` copy with the constructor spelled out reaches it in one edit.
- **`#pragma inline_max_size` is a whole-file knob with a narrow safe window, and the window has
  to be swept, not guessed.** 129..139 is safe here and 140 is not; the two thresholds are for
  two different callees in two different functions. A change that only *helps* at 300 is
  indistinguishable from one that helps at 138 if you only test the high value.
- **A first measurement of an inlining threshold needs a baseline taken from the same report.**
  My first sweep compared against `build/report.json` (the live file, i.e. the previous
  iteration's build) and reported `RayStaticIntersection` "worse" at every threshold. Re-running
  against a saved copy of the clean-tree report showed the damage was only at >= 140. If the
  baseline is not a file on disk, the sweep measures itself.
- **`tools/report_diff.py` is the regression gate and it is not symmetric with the brief.** The
  brief says "no function anywhere gets worse"; the script only *fails* a percentage drop inside a
  unit that was `Matching` in the baseline and prints NonMatching drops loudly. Here it printed
  the two `RayDynamic`/`RayStatic` drops at 140 as `WORSE` and still exited 0. Those two functions
  are why 138 is the chosen value and not 140 - the brief's rule is the stricter one and it is
  the one to obey.

### What I did **not** do, re-measured this run

- `fn_80128000` (200 B, 0.00%) is `CToken::operator=` (asm:5273-5333: `Unlock`/`RemoveRef`/
  `Lock`/`~CToken` on `mObjRef`+0x8, `mLockHeld`+0xc). It is called 4x from
  `UninitializeCollision`, whose body is still the 8.64% TODO. Defining it here would need
  `CToken`'s `mObjRef`/`mLockHeld` writes spelled out of `src/Kyoto/CToken.cpp`'s accessors, and
  `UninitializeCollision` needs its four debug-token blocks plus `SetPrebuiltTree` x4,
  `SetDuplicatePrimitiveBuffers` and `Free` to move at all. Still a naming/wiring item, not a
  spelling one.
- `fn_80125288` (196 B, 0.00%) is `CCollisionInfo`'s **copy constructor** - 24 float/int copies
  of the whole 0x60-byte struct, member order, no frame. Our object already emits the identical
  bytes as the weak COMDAT `__ct__14CCollisionInfoFRC14CCollisionInfo` (verified instruction for
  instruction against asm:2006-2059). It is the *same* function under a different symbol: retail's
  copy-constructor call in `CollideWithDynamicBodyNoRot` (asm:1963) is a `bl fn_80125288`, ours is
  a `bl` to the COMDAT. Retail put it in this unit because it is a local COMDAT there. Worth a
  separate look at how COMDAT-local copy constructors get named per unit; **not** a spelling wall.
- `fn_801247D4` (36 B) is `CMotionState`'s copy constructor, called once from `CollisionFailsafe`
  (2.85%, a TODO body). `fn_801284E0` (252 B) is `__sinit_CGameCollision_cpp`: `__shl2i` guard
  bits plus four `__register_global_object` calls with `fn_80070CF4` destructors. Both need their
  callers implemented first.
- Not retried, because they are still where the notes left them and this run measured no reason
  to think the spelling set changed: `SendMaterialMessage` 98.21%, `DetectDynamicCollision`
  99.13%, `DetectDynamicCollisionMoving` 98.48%, `GetMinExtentForCollisionPrimitive` 91.97%,
  `CollideWithStaticBodyNoRot` 92.08% (all pure register allocation - e.g. `SendMaterialMessage`
  is 11 identical `sth`s whose *order* differs, retail `16,8,12,20,24,28,30,32` vs ours
  `12,16,8,20,24,28,30,32`; `DetectDynamicCollision` differs only in `addi r31,r26,4` vs
  `addi r30,r26,4`, i.e. the loop cursor lands in r30 vs r31). `DetectStaticCollision*_Cached`
  still show the same `bl fn_802896F0` (retail's out-of-line `CMaterialFilter::WithImplicitMaterials`,
  0x24C bytes at 0x802896F0, which no object in this tree defines) where we call the inline
  header version - **that** is the missing callee for the three `*_Cached` functions, not
  `fn_800A4840` as the earlier notes said.

### No `NEW:` filed

The one new thing this run learned - that `fn_80125288` is `CCollisionInfo`'s copy constructor
already present as a COMDAT - is a measurement on the current item's unit, not a separate unit or
symbol whose success would raise a count on its own. Filing it would re-queue work this item has
already characterised.

## Not committed, as instructed. Tree state

`src/MetroidPrime/CGameCollision.cpp` (+31/-6) is the whole hand-made diff. `docs/HANDOFF.md`'s
state block was rewritten by `tools/sync_state_block.py` when the gate ran - machine-made, and the
driver rewrites it anyway. Helper scripts are under `.tmp/opencode/cgc/` (gitignored):
`cgc.sh` (rebuild + scores), `cmpfn.sh`/`cmp2.sh` (normalised side-by-side diff of one function,
the instrument the previous run's notes describe), `sweep.py` (inline_max_size sweep vs the saved
clean-tree baseline).

---

## Run 4 (2026-09-30, lane 1) - the two remaining 0% unnamed callees

**Result: `MetroidPrime/CGameCollision` 15/52 -> 17/52 functions at 100%.** `matched_code`
6.83% -> 7.95% (1444 -> 1596 bytes), unit `.text` fuzzy 60.37% -> 61.49%. Project `All:` line
**10553 -> 10555 / 28465** (`All:  31.85% fuzzy, 24.54% matched, 11.84% linked`). No function
anywhere got worse - `python3 tools/report_diff.py build/goal/judge/report.base.json
build/report.json` -> `+100% fn_801247D4`, `+100% fn_80125288`, `no regression`, `linked 5051 ->
5051`. `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**, every sub-check `ok`,
including `target rose: main/MetroidPrime/CGameCollision: 15 -> 17 / 52 functions`. The unit stays
`NonMatching`; `flip_test.sh` was not run, as the item says.

Hand-made diff is **two files**: `src/MetroidPrime/CGameCollision.cpp` (+52/-0) and one accessor in
`include/MetroidPrime/CPhysicsActor.hpp` (+3/-0). No class layout or `CHECK_SIZEOF` moved; the only
`CHECK_SIZEOF` added is on a file-local mirror struct. (`docs/HANDOFF.md`'s state block was
rewritten by `tools/sync_state_block.py` when the gate ran - machine-made.)

### Re-measured first

HEAD of this tree already carried runs 1-3 (the eleven hunks, the review fix, `#pragma
inline_max_size(138)` + `fn_8012753C` + the `CollideCachedAABox` hoist): **15/52**, unit
`60.37% fuzzy, 6.83% matched`. Nothing in the previous notes was stale. The four retail functions
still at **0.00%** are `fn_801284E0` (252 B), `fn_80128000` (200 B), `fn_80125288` (196 B) and
`fn_801247D4` (36 B) - and **two of those four are this item.**

### 1. `fn_80125288` 0.00% -> 100.00% (196 B) - `CCollisionInfo`'s copy constructor

Retail's body (`build/G2ME01/asm/MetroidPrime/CGameCollision.s:2006-2055`) is 49 instructions, no
frame: `lfs`/`stfs` for eighteen floats at 0x00..0x2F and 0x40..0x57, `lwz`/`stw` for four words at
0x30..0x3F, `lhz`/`sth` at 0x58, `lbz`/`stb` at 0x5A. So it is the **implicit copy constructor**,
member by member. **Run 3 was right that the bytes already exist** - our object emits them as the
weak COMDAT `__ct__14CCollisionInfoFRC14CCollisionInfo` - and wrong that this is "not a spelling
item": the only thing missing was a declaration whose *symbol* is retail's.

`CCollisionInfo`'s members are private, so the body is written through a file-local field-for-field
mirror `SCCollisionInfoFields` (`src/MetroidPrime/CGameCollision.cpp:751`), the same idiom as
`union SCCollisionInfoBlock` in `src/MetroidPrime/Player/CMorphBall.cpp:110`. Three spellings
measured, all with the same signature `(CCollisionInfo*, const CCollisionInfo&)`:

| spelling | size | score |
|---|---|---|
| `*self = other;` (implicit copy **assignment**) | 0xB4 | 0.00% - mwceppc copies a class member by word, so all 24 moves are `lwz`/`stw` |
| mirror, fields in offset order | 0xC4 | **99.84%** - identical except each material pair's two `lwz` load low-half-first |
| mirror, each material pair written high half first | 0xC4 | **100.00%**, byte-identical |

The last one is the load-order detail and nothing else: retail loads 0x34 before 0x30 and 0x3C
before 0x38 (asm:2030-2039), and writing each `CMaterialList`'s two words high-first reproduces
that. All three spellings produce the same 49 instructions; only the two word pairs move.

**This is a hand-mirror, so it is worth saying why nothing cheaper works:** an implicit copy
constructor is only ever emitted as a weak COMDAT and called, so it cannot be renamed from outside
the class; declaring it out-of-line gives `__ct__14CCollisionInfoFRC14CCollisionInfo`, which
objdiff will not match to `fn_80125288`; and the header's own `rstl::construct_impl` route is
`fn_800D042C` (0x800D042C), a different function in a different unit which happens to be the
`double[12]` block-move shape, not this one.

**Call site left alone, deliberately.** Wiring `ResolveCollisions` to call it -
`CCollisionInfo collision; fn_80125288(&collision, collisions[i]);` - **removes** the 196-byte
COMDAT from the object (`unit_fit.sh` extra list drops to 14) but costs
`ResolveCollisions` 72.61% -> **70.48%**, because the default constructor's stores appear before
the call where retail has none. Measured, then reverted: "no function anywhere gets worse" is the
brief's rule and it outranks the tidier unit fit on a `NonMatching` unit that is 3.7 kB short of
its split anyway.

### 2. `fn_801247D4` 0.00% -> 100.00% (36 B) - `CPhysicsActor::GetLastNonCollidingState()`

Nine instructions (asm:1254-1263): a 0x10 frame, `addi r4,r4,0x264`, `bl
__ct__12CMotionStateFRC12CMotionState`, restore. It copies out the `CMotionState` at 0x264 and
nothing else, so it is a getter by value whose symbol retail's table does not carry - the
`extern "C"` pattern of `fn_80143CD4` in `src/MetroidPrime/Player/CGameState.cpp:874`.

The offset was read off the two ends of the class rather than guessed: retail's only caller,
`CollisionFailsafe`, increments a counter at 0x2BC and 0x2C0 (`asm:1922-1926`), which are
`mNumTicksStuck`/`mNumTicksPartialUpdate`; working back from `CHECK_SIZEOF(CPhysicsActor, 0x2d0)`
puts `mLastNonCollidingState` - the last `CMotionState` before
`rstl::optional_object<CVector3f> mLastFloorPlaneNormal` - at exactly 0x264.

One header line, `const CMotionState& GetLastNonCollidingState() const`, added next to the existing
`SetLastNonCollidingState`. **The by-reference return type is load-bearing, not a style choice:**

| `fn_801247D4` body | size | score |
|---|---|---|
| `return actor.GetLastNonCollidingState();` with a **by-value** getter | 0x30 | 0.00% - adds `stw r31,12(r1); mr r31,r3; ...; lwz r31,12(r1)` |
| `return actor.GetLastNonCollidingState();` with a **by-reference** getter | 0x24 | **100.00%**, byte-identical |

mwceppc has to keep the hidden struct-return pointer alive across the call in the by-value form;
returning a reference lets it construct the return object straight from the member.

**Neither function is called from this unit yet**, because both callers (`CollisionFailsafe` at
2.85% and `ResolveCollisions` at 72.61%, whose copy-construction already goes through the COMDAT)
are not retail's shape yet. Both definitions are whole - no early return, no skipped call, no
placeholder - and both are byte-exact against retail, so they are not stubs; they are simply the
callees the next run's caller work needs.

### Also tried this run, none of it helping (measured, do not repeat)

- `!= 0` on the three single-callee wrappers `DetectDynamicCollisionBoolean` /
  `DetectDynamicCollision` / `DetectDynamicCollisionMoving` (`RC13CPhysicsActor` forms). Retail
  **does** have `clrlwi r3,r3,24; neg r0,r3; or r0,r0,r3; srwi r3,r0,31` where we had only the
  `clrlwi` (asm:4016-4022), so `!= 0` is the right *source*, and it does produce both - but it
  scores **worse**: 91.17 -> 87.38, 91.85 -> 88.23, 92.00 -> 84.00. Retail interleaves the three
  epilogue `lwz`s between the `neg`/`or`/`srwi` chain, mwceppc here schedules all three after it.
  Scheduling, not source. Correct in principle, rejected by the metric, reverted.
- `MakeCollisionCallbacks` 99.69%, the only remaining sub-100% function that is one register away
  (retail's second loop counter is in r31, ours in r29). Four spellings, all worse:
  `uint i` 96.35; `const int count = swapped.GetCount()` hoisted, 96.54 (and the counter moves to
  r30); loop over `collisions.GetCount()` instead of `swapped.GetCount()` 96.54; splitting the
  loops onto one shared `int i` with a countdown 83.00. The file-global `#pragma
  inline_max_size` is **not** the lever either: swept 125 / 130 / 138 and every one of
  `MakeCollisionCallbacks`, `SendMaterialMessage`, `BuildAreaCollisionCache`, `CollideCachedAABox`
  and all five `DetectDynamicCollision*` scores was **identical**, so these are not inlining
  decisions at all.
- `SendMaterialMessage` 98.21%, `DetectDynamicCollision` 99.13%,
  `DetectDynamicCollisionMoving` 98.48%, `GetMinExtentForCollisionPrimitive` 91.97% - all still
  exactly where the previous runs left them: identical instruction sequences, different register
  choice or store order. Not retried, this run measured no reason to think the spelling set moved.

### Not attempted, with the reason measured

- `fn_80128000` (200 B, 0.00%) is `CToken::operator=` (asm:5272-5325): it reads `mLockHeld` at
  0xC and `mObjRef` at 0x8, which are **private** in `include/Kyoto/CToken.hpp`, and `operator=`
  is declared-not-defined there. A `SCCollisionInfoFields`-style mirror is not enough - the body
  also needs `__as__6CTokenFRC6CToken`, `__ct__6CTokenFRC6CToken`, `__dt__6CTokenFv` and
  `Lock__6CTokenFv` to line up by name, and its four call sites are inside `UninitializeCollision`
  (8.64%), which is a TODO. Different item.
- `fn_801284E0` (252 B, 0.00%) is `__sinit_CGameCollision_cpp`; ours is 120 B, so it is not a
  spelling problem but a different amount of static initialisation.

### Gates, all run in this tree

- `./tools/fast_try.sh MetroidPrime/CGameCollision` after every edit - the numbers above.
- `./tools/decomp_build.sh` -> `All:  31.85% fuzzy, 24.54% matched, 11.84% linked (10555 / 28465
  functions)`. **The line did not fall; it rose by 2.**
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail, correct).
- `./tools/probe_sources.sh` -> `probe: 754 files, 0 failed, 0 errors; link: LINKED (250
  undefined, 0 duplicates)` - unchanged, so nothing new went undefined.
- `python3 tools/check_symbol_names.py` -> `checked 505 units; 0 declared names are missing`.
- `python3 tools/check_raw_offsets.py` -> `ok: 162 raw-offset site(s) in 69 file(s)`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/CGameCollision` -> `ok: 1 unit(s)
  checked, none emits its functions out of retail order`. Both definitions sit between their
  retail neighbours in the file: `fn_80125288` (0x80125288) between
  `GetMinExtentForCollisionPrimitive` (0x8012534C) and `ResolveCollisions` (0x801251B0), and
  `fn_801247D4` (0x801247D4) between `MovePlayer` (0x801247F8) and `CollisionFailsafe`
  (0x801241F4). The file is descending by retail offset because mwcceppc emits in reverse.
- `python3 tools/report_diff.py` -> `matched 10553 -> 10555, linked 5051 -> 5051, no regression`.
- `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**, every sub-check `ok`.
- `tools/flip_test.sh` not run: the item is `progress` and says not to flip.
- `./tools/unit_fit.sh MetroidPrime/CGameCollision.cpp` -> `.text` still `SHORT by 3940` (the TODO
  bodies), 15 extra functions / 1892 bytes, **the same as HEAD**: neither new definition is an
  extra, because retail defines both in this unit.

### Process lessons (not `NEW:` items)

- **An unnamed retail function that exists as a weak COMDAT in your object is a naming item, and
  it is worth one edit.** Two of the four 0% functions here were that, and a mirror struct
  reproduces the implicit copy constructor's bytes exactly. The tell is a retail body that is a
  plain memberwise move: no frame, no calls, load/store pairs in declaration order.
- **mwceppc copies a class-typed member by word and a scalar member in its own width.** That one
  sentence separates `*self = other` (0xB4 bytes, all `lwz`/`stw`, 0.00%) from the mirror's
  member-wise `float` copies (0xC4 bytes, `lfs`/`stfs`, 100%) for the same semantics.
- **mwceppc schedules what the IR order gives it; for an 8-byte pair it wants the high half
  first.** In offset order the same 49 instructions measured 99.84% - four `lwz`/`stw` the other
  way round and nothing else. Before calling a byte-exact function "register allocation", try
  reversing one pair's order; it is a source edit and it is checkable.
- **A by-value getter for a class member needs the hidden return pointer spilled across the
  callee; a by-reference getter does not.** Same function, same body, 0x30 vs 0x24 bytes. If a
  small wrapper comes out four instructions too long with a `stw r31,...; mr r31,r3` sandwich,
  return the reference.
- **`#pragma inline_max_size` was re-swept and is not the lever for the remaining functions.** All
  nine watched scores were byte-identical at 125/130/138, which rules the whole knob out for them
  and stops the next run sweeping it again.

### No `NEW:` filed

The one genuinely separate symbol left is `fn_80128000` (`CToken::operator=`), and it is not
independent work: it is private-member access to `CToken` **plus** four call sites inside
`UninitializeCollision`, which is a TODO body. Filing it would re-queue the same
`UninitializeCollision` recovery under a new name.

## Not committed, as instructed. Tree state

`src/MetroidPrime/CGameCollision.cpp` (+52/-0) and `include/MetroidPrime/CPhysicsActor.hpp` (+3/-0)
are the whole hand-made diff; `docs/HANDOFF.md`'s state block was rewritten by the gate. Helper
scripts are under `.tmp/opencode/cgc/` (gitignored, not part of the change): `d.sh` (normalised
side-by-side diff of one function), `m.sh` (rebuild one object + print the unit's scores),
`sweep.sh` (inline_max_size sweep).

---

## Run 5 (2026-09-30, lane 4) - the two unnamed callees, and the `hit = callee()` shape

**Result: `MetroidPrime/CGameCollision` 17/52 -> 19/52 functions at 100%.** `matched_code`
7.95% -> 9.84% (1644 -> 2036 bytes), unit `.text` fuzzy 61.49% -> 62.91%. Project
`All: 32.41% fuzzy, 24.98% matched, 11.94% linked`, **`11254 -> 11256 / 28465`**.
`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json` ->
`+100% DetectCollision_Cached`, `+100% DetectCollision_Cached_Moving`, `no regression`
(`linked 5507 -> 5507`). **`./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS`**,
every sub-check `ok`. The unit stays `NonMatching`; `flip_test.sh` was not run, as the item says.

Hand-made diff is **one source file**: `src/MetroidPrime/CGameCollision.cpp` (+74/-22). No header
was touched, so no class layout, `CHECK_SIZEOF` or shared unit moved. (`docs/HANDOFF.md`'s state
block was rewritten by `tools/check_docs_claims.py --write` when the gate ran - machine-made.)

### Re-measured first

HEAD of this tree carried runs 1-4: **17/52**, unit `61.48792% fuzzy, 7.95% matched_code`. Nothing
in the previous notes was stale, and the 0% functions are still the same four (`fn_801284E0`,
`InitCollision`, `fn_80128000`, `UninitializeCollision`).

### 1. The 100% pair: `hit = callee()` is a different CFG from `if (callee()) hit = true;`

`DetectCollision_Cached` 81.04 -> **100.00%** and `DetectCollision_Cached_Moving` 74.20 ->
**100.00%**, byte-identical, from one change each:

```cpp
-    hit = DetectStaticCollision_Cached(mgr, cache, primitive, transform, filter, collisions);
+    if (DetectStaticCollision_Cached(mgr, cache, primitive, transform, filter, collisions)) {
+      hit = true;
+    }
```

That is **Prime 1's spelling verbatim** (`prime-ref` lines 144-150 and 505-509), and it is what
retail does: with `hit = callee(...)` mwceppc emits `bl; mr r30,r3` and keeps the callee's bool in
the result register; with `if (callee(...)) { hit = true; }` it emits
`clrlwi. r0,r3,24 ; beq <skip> ; li r3,1` and never moves the bool. **The previous runs never
tried Prime 1's source for these two functions** - they read Prime 1 for the `DetectStaticCollision*`
family and stopped there. Both functions had been sitting at 74-81% with an *unexplored* diff.

This is the same lesson run 1 recorded for `DetectCollisionBoolean`, and it cuts both ways: there,
`return DetectDynamicCollisionBoolean(...)` was the fix; here, `if (...) { hit = true; }` is. **Which
form retail wants is a property of the call site, not of the callee** - read the disassembly.

### 2. `fn_800A4840` is Prime 1's `IsUser`, and it is 8 call sites

Run 1 and run 3 both recorded a missing callee called `bl fn_800A4840` and left it as a
"naming/wiring question". It is not: **Prime 1 has `bool IsUser(int name) { return name == 1; }`**
(`prime-ref/src/MetroidPrime/UserNames.cpp:5`, declared in `include/MetroidPrime/UserNames.hpp`) and
calls it as a bare `IsUser(0);`. Retail's `fn_800A4840` (0x800A4840, 0x10 bytes) is
`subfic r0,r3,1 ; cntlzw r0,r0 ; srwi r3,r0,5`, which is exactly `name == 1`, and Echoes calls it
with a literal 0 and discards the result at **eight** sites in this unit.

It is the last function of `MetroidPrime/ScriptObjects/CScriptPlatform.cpp`'s retail range
(`.text 0x800A0200..0x800A4850`) and no object in this tree defines it, so it is declared here as
`extern "C" bool fn_800A4840(int name);` and dtk fills the body from retail (the region is
unclaimed, so the DOL sha1 is unaffected - measured). The eight sites, from
`build/G2ME01/asm/MetroidPrime/CGameCollision.s`:

| function | asm line | where |
|---|---|---|
| `CollideWithDynamicBodyNoRot` | 1824, 1844, 1886, 1913 | inside the `normalVelocity < 0.1f` branch, and after each `SetVelocityWR` |
| `DetectStaticCollision_Cached_Moving` | 2707 | after `BuildAreaCollisionCache(mgr, cache)` |
| `DetectStaticCollisionBoolean_Cached` | 3250 | same |
| `DetectStaticCollision_Cached` | 3663 | same |
| `BuildAreaCollisionCache` | 4479 | after the area loop |

`BuildAreaCollisionCache` 83.66 -> 86.74, `CollideWithDynamicBodyNoRot` 77.05 -> 80.77,
`DetectStaticCollision_Cached` +1.0, `DetectStaticCollisionBoolean_Cached` +1.0,
`DetectStaticCollision_Cached_Moving` +0.5. `CollideWithDynamicBodyNoRot`'s four sites are exactly
where Prime 1's has `if (IsUser(0)) { gDebugPrintCount++; }`; **Echoes has no `gDebugPrintCount`**
(its only file-scope static is `skStaticGeometryMaterials`), so the bare `fn_800A4840(0);` is the
whole statement. The two `SetVelocityWR` sites match Prime 1's trailing `IsUser(0);` exactly.

### 3. `fn_802896F0` is `CMaterialFilter::WithImplicitMaterials`, and it is score-neutral

Run 3 recorded `fn_802896F0` (0x802896F0, 0x24C bytes, an unclaimed gap after
`Collision/CCollidableSphere.cpp`) as the missing callee for the three `*_Cached` functions. It is
`WithImplicitMaterials` out of line - `lwz r9,16(r4)` reads `type` off the filter, the body is this
tree's inline header version instruction for instruction. Declared here as
`extern "C" CMaterialFilter fn_802896F0(const CMaterialFilter&, const CMaterialList&);` and called
at all eight sites.

**Measured: every one of the eight functions scored exactly the same before and after** (e.g.
`RayStaticIntersection` 87.48 both ways). The reason is worth recording: **objdiff does not compare
the callee symbol of a `bl`.** At HEAD this unit already emitted
`bl WithImplicitMaterials__15CMaterialFilterCFRC13CMaterialList` - a real out-of-line weak COMDAT in
our own object, not an inlined body - so the instruction stream was already retail's. The change is
kept because it is more faithful and because `tools/unit_fit.sh` drops from 15 extra functions /
1892 bytes to **14 / 1328**: the 0x24C COMDAT is gone, since dtk now resolves the call to
0x802896F0. **So run 3's claim that the `*_Cached` functions are "blocked on this callee" is wrong,
and the real blocker for them is register rotation (see below).**

### 4. `A || B: return false;` is a different CFG from two `if`s - 5 functions

Splitting the short-circuit into two statements, in the five `DetectStaticCollision*` functions:

```cpp
-  if (staticFilter.GetType() == CMaterialFilter::kFT_Never || primitive.GetPrimType() == 'OBTG') {
-    return false;
-  }
+  if (staticFilter.GetType() == CMaterialFilter::kFT_Never) {
+    return false;
+  }
+  if (primitive.GetPrimType() == 'OBTG') {
+    return false;
+  }
```

`DetectStaticCollision` 96.64 -> **97.87**, `DetectStaticCollision_Cached` 95.57 -> **96.93**,
`DetectStaticCollisionBoolean` 95.83 -> **97.04**, `DetectStaticCollisionBoolean_Cached` 93.13 ->
**94.46**, `DetectStaticCollision_Cached_Moving` 87.71 -> **88.79**. Run 1 had already found this
trick for `DetectStaticCollision` ("ABSH as two `if`s not `||`") but it had not been carried across
to the other four. The disassembly shows why it is not cosmetic: with `||` mwceppc emits
`beq <near>`, with two `if`s it emits `bne <far> ; li r3,0 ; b <end>` - the two `return false`s
stop sharing one block.

### 5. `CUnitVector3f(x, y, z)` instead of `CUnitVector3f(vec, kN_No)` - the ray family

`CUnitVector3f(const CVector3f&, kN_No)` **materialises a 12-byte temporary** in this compiler and
then copies it into the `CLine`, which pushed every frame offset in the three ray functions up by
0x10. The three-argument spelling, which has no branch and no copy, removes the temp:

```cpp
-  const CLine line(position, CUnitVector3f(direction, CUnitVector3f::kN_No));
+  const CLine line(position, CUnitVector3f(direction.GetX(), direction.GetY(), direction.GetZ()));
```

`RayStaticLineOfSightTest(CStateManager)` 89.48 -> **94.14**, `RayStaticIntersection` 87.48 ->
**89.94**, `RayStaticLineOfSightTest(CGameArea)` 69.76 -> 72.51. Retail's frame layout then matches
ours exactly (filter temp at 8(r1), `CLine` at 0x20/0x30, filter copy at 0x38/0x48, `Node` at
0x50/0x60).

### 6. `return f();` vs `if (f()) { return true; } return false;` - 10.9 points

`RayStaticLineOfSightTest(CGameArea)` only: rewriting the tail as
`if (fn_8012753C(tree).LineTest(...)) { return true; } return false;` took it 72.51 -> **80.66** and
made the function **280 bytes, retail's exact size**, with the `clrlwi. / bne / li r3,0 / b / li r3,1`
tail retail has. This is the mirror image of the `hit = callee()` case in §1 and worth pairing with
it: retail re-tests the callee's bool at *this* site and not at the other.

### 7. `CollideCachedAABox` 81.79 -> 92.21: `== true` and an `int` index

`for (int i = 0; i < int(cache.GetNumCaches()); ++i)` with
`AABoxCollisionCheck_Cached(...) == true` instead of `uint i` and a bare `if (...)`. The `== true`
is what produces retail's `clrlwi r0,r3,24 ; cmplwi r0,1 ; bne` (no record bit, full compare)
instead of mwceppc's `clrlwi. r0,r3,24 ; beq`. Four spellings measured:

| spelling | score |
|---|---|
| `uint i`, bare `if`, index form (HEAD) | 81.79 |
| `uint i`, `== true`, index form | 85.03 |
| `uint i`, `== true`, **cursor** (`const COctreeLeafCache* leaf = &cache.GetOctreeLeafCache(0); ... ++leaf`) | 91.41 |
| cursor + `const uint count = cache.GetNumCaches();` hoisted | 91.71 |
| `int i`, `int(cache.GetNumCaches())`, index form, `== true` | **92.21** (kept) |
| `hit = hit || f()` instead of `if (f()) hit = true;` | 66.53 |

The remaining 8% is register numbering only - the body is instruction-for-instruction identical to
retail (136 vs 132 bytes, same 33 instructions). Retail keeps `cache` in r24 and re-loads
`mLeafCaches.mSize` from `24(r24)` every iteration; mwceppc hoists the load out of the loop into
r30, which costs it a register and shifts every other one. Run 3 measured the same hoist and called
it register allocation; that still holds. **WALL: `CollideCachedAABox` 92.21% - body is
instruction-identical; mwceppc hoists `GetNumCaches()` out of the loop and retail does not, and
nothing in the source stops the hoist.**

### Full per-function ledger for this run (base -> after)

| function | before | after |
|---|---|---|
| `DetectCollision_Cached` | 81.04 | **100.00** |
| `DetectCollision_Cached_Moving` | 74.20 | **100.00** |
| `DetectStaticCollision` | 96.64 | 97.87 |
| `DetectStaticCollision_Cached` | 94.56 | 96.93 |
| `DetectStaticCollisionBoolean` | 95.83 | 97.04 |
| `DetectStaticCollisionBoolean_Cached` | 92.14 | 94.46 |
| `DetectStaticCollision_Cached_Moving` | 87.20 | 88.79 |
| `CollideWithDynamicBodyNoRot` | 77.05 | 80.77 |
| `CollideCachedAABox` | 81.79 | 92.21 |
| `BuildAreaCollisionCache` | 83.66 | 86.74 |
| `RayStaticLineOfSightTest(CGameArea)` | 69.76 | 80.66 |
| `RayStaticLineOfSightTest(CStateManager)` | 89.48 | 94.14 |
| `RayStaticIntersection` | 87.48 | 89.94 |

Nothing fell. `unit matched 17 -> 19 / 52`, `project matched 11254 -> 11256`.

### Also tried this run, none of it helping (measured, do not repeat)

- **`TAreaId` is why `BuildAreaCollisionCache` cannot finish.** Retail
  `lwz r5,4(r31)` passes the area index straight into `__ct__COctreeLeafCache`; we emit
  `lwz r0,4(r30) ; addi r5,r1,12 ; stw r0,8(r1) ; stw r0,12(r1)`, i.e. a materialised `TAreaId`.
  `TAreaId` (`include/MetroidPrime/TGameTypes.hpp:19`) has a user-provided
  `TAreaId() : value(-1) {}`, which makes it a non-POD, and mwceppc then needs an addressable
  temporary for the by-value `CGameArea::GetId()`. Retail's `TAreaId` is a POD. **Not done on
  purpose:** removing that default constructor changes default-initialisation of every `TAreaId`
  member in the game (CEntityInfo, IGameArea::Dock, CAutoMapper, ...), which is exactly the
  "one shared header moves an unrelated function" hazard. It is a one-line experiment someone with
  a clean full-build diff should try, not a change to fold into a decomp item.
- **`SendScriptMessages`, `SendMaterialMessage`, `MakeCollisionCallbacks`, `DetectDynamicCollision*`,
  `GetMinExtentForCollisionPrimitive`, `CollideWithStaticBodyNoRot` are all instruction-for-
  instruction identical to retail at their current scores** and differ only in register *numbering*.
  `SendMaterialMessage` (98.21%, 96 B) is the clearest case in the unit and I re-derived it: retail
  `lhz r6,8(r5)` / `lhz r7,kInvalidUniqueId`, we `lhz r7,8(r5)` / `lhz r6,kInvalidUniqueId`; the
  value-to-offset map and the store order are identical, so the whole 1.79% is `r6` vs `r7`.
  Four spellings measured, all identical or worse: a named `const CScriptMsg msg` local (98.21),
  swapping `m_originator`/`m_id` (98.21), swapping `m_unk`/`m_originator` (98.21), and naming the
  three ids as locals (98.21). `MakeCollisionCallbacks` (99.69%, 260 B, 65 instructions) differs in
  exactly three: retail `li r31,0 / addi r31,r31,1 / cmpw r31,r0` where we use r29, because retail
  recycles the vector copy-ctor's countdown register for the swap loop's counter and mwceppc
  allocates a fresh one. Run 4 measured four spellings for this; the body is identical, so this is
  the register allocator and not the source.
- `CollideWithStaticBodyNoRot`: Prime 1's positive `if (CanBeNormalized()) { ... }` instead of our
  early `if (!CanBeNormalized()) return;` - **identical 92.08%**. Reverted; the early return is
  kept because it is the smaller diff. What is left is three extra `stfs` of the normal to
  20/24/28(r1) that retail does not emit, plus the `fmadds` chain in different registers.
- `DetectCollision_Cached_Moving`'s `kMT_NoStaticCollision` prologue is **not** a difference:
  retail and we both compute `(exclude.lo & 0) | (exclude.hi & 0x40)` and differ only in which
  register holds which half. Run 3's suspicion that `kMT_NoStaticCollision = 38` might be wrong is
  refuted by the masks.
- `RayStaticLineOfSightTest(CGameArea)`, further: `? true : false` instead of
  `if (...) { return true; } return false;` -> 76.61 (worse); `direction[0..2]` instead of
  `GetX/GetY/GetZ` -> 76.61 (worse); hoisting `const float maxDistance` -> 80.66 (identical);
  a named `const CUnitVector3f unitDir(direction, kN_No)` local -> 76.80 (worse, the temp returns).

### Walls, written from THIS run's measurements

- `WALL: CollideCachedAABox 92.21% - body instruction-identical to retail; mwceppc hoists GetNumCaches() out of the loop, retail re-loads it, and the one extra live register renames every other one.`
- `WALL: MakeCollisionCallbacks 99.69% - 65 of 68 instructions identical; the swap loop's counter is r29 instead of r31 because retail recycles the vector copy-ctor's countdown register and mwceppc allocates a fresh one.`
- `WALL: SendMaterialMessage 98.21% - 8 stores at identical offsets in identical order; the two loaded values land in r6/r7 the other way round. Four source spellings measured, none changed it.`
- `WALL: DetectStaticCollision 97.87% and DetectStaticCollisionBoolean 97.04% - identical instruction stream, register block rotated by one (retail r28-r31, ours r29-r28 for the cache pointer). Retail allocates the cache pointer last, mwceppc first.`
- `WALL: RayStaticLineOfSightTest(CGameArea) 80.66% - 280 bytes, retail's exact size, frame offsets all match; the direction components load in reverse (mwceppf evaluates ctor arguments right-to-left) and the bool tail is laid out with the opposite branch polarity.`

### What is still unclaimed work, characterised (no NEW: filed)

- **`fn_80128000` (200 B, 0.00%) is `CToken::operator=`** and its body is now fully readable
  (asm:5260-5315): a self-check, then on `other.mLockHeld` either
  `{ CToken tmp(other); self.mObjRef = other.mObjRef; self.Lock(); self.mLockHeld = true; }` or
  `bl __as__6CTokenFRC6CToken` (a self-call - retail really does it), and otherwise
  `{ if (self->mLockHeld) self->~CToken(false); self->mLockHeld = false; }`. All four callees
  already exist by name in `src/Kyoto/CToken.cpp` (`__as__6CTokenFRC6CToken` is a weak COMDAT at
  offset 0, plus `__ct__6CTokenFRC6CToken`, `Lock__6CTokenFv`, `__dt__6CTokenFv`). The only thing
  in the way is that `mObjRef` and `mLockHeld` are **private** in `include/Kyoto/CToken.hpp`, and
  the four call sites are inside `UninitializeCollision`, which is still the TODO body. A `friend`
  declaration generates no code and moves nothing, so the accessor half is cheap - but
  `UninitializeCollision` (364 B) has to be written first, and that is the larger job.
- **`InitCollision` (1048 B, 0.38%) is fully mapped and is the largest remaining reachable
  function.** Prime 1's source is a near-exact template for the first 30 instructions
  (`InitBeginTypes` / `GetType` / `InitAddType` / `InitEndTypes` / `InitBeginColliders` / 9
  `InitAdd*Collider` / `InitEndColliders` - every callee is already declared in
  `include/Collision/CCollisionPrimitive.hpp` and
  `include/WorldFormat/CCollidableOBBTreeGroup.hpp`). The rest is: a mode-dependent
  `CMemory::Alloc` + `SetDuplicatePrimitiveBuffers` (0xC800/0x2800/0x6000/0x4000 when the state
  manager is live, 0x42A0/0xDC0/0x20E0/0x1400 when it is not), then four copies of a debug-model
  token block over the string table at `lbl_803A8D68` - the names are **"UnitCube"** (+0x42),
  **"UnitSphere_Low"** (+0x4B), **"UnitSphere_Med"** (+0x5A), **"UnitSphere_High"** (+0x69), with
  `CCallStack(-1, "??(??")` at +0x3B - then four `COBBTree::SetPrebuiltTree` calls on the members
  at `lbl_803DAB70`+0x0C/+0x28/+0x44/+0x60. What blocks it is **not** the strings or the
  constants: it is the 0x70-byte file-scope object at `lbl_803DAB70` (four 0x1C-byte sub-objects,
  unclaimed `.bss`) and the signature of `fn_80070AD8` (0x80070AD8, called as
  `fn_80070AD8(lbl_803DAB70 + 0x0C + k*0x1C, CToken&)`). Both are absent from this tree, and
  guessing a class layout to hold them is not something a decomp item should do. Straight-line code
  with no loops is the most likely thing in this unit to reach 100%, so this is the right next item
  once `fn_80070AD8` and the `lbl_803DAB70` owner are identified.
- **`fn_801284E0` (252 B, 0.00%) is `__sinit_CGameCollision_cpp`** and it is **not reachable from
  this unit**, which I now think is worth recording so nobody re-derives it: retail registers four
  file-scope objects (`__register_global_object` at +0x0C/+0x28/+0x44/+0x60 of a 0x70-byte
  `lbl_803DAB70`, each 0x1C bytes, dtor `fn_80070CF4`) and we register one. Adding them means
  emitting 0x70 bytes of unclaimed `.bss`, which dtk can only place correctly if the object lands
  exactly on `lbl_803DAB70` with exactly retail's initialised content. Not a spelling problem and
  not worth an item.

### Gates, all run in this tree

- `./tools/fast_try.sh MetroidPrime/CGameCollision` after every edit - the numbers above.
- `./tools/decomp_build.sh` -> `All:  32.41% fuzzy, 24.98% matched, 11.94% linked (11256 / 28465
  functions)`, identical to the baseline's `All:` line (32.41 / 24.98 / 11.94 at 11254). The line
  did not fall; the matched count rose by 2.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail, correct) -
  **the two new `extern "C"` callees resolve into unclaimed regions, so nothing moved.**
- `./tools/probe_sources.sh` -> `probe: 751 files, 0 failed, 0 errors; link: LINKED (244 undefined,
  0 duplicates)`. The port does not compile `src/MetroidPrime/CGameCollision.cpp` (it has its own
  `CGameCollisionRayWorldIntersection.cpp`), so neither declaration reaches the port link.
- `python3 tools/check_symbol_names.py` -> `checked 514 units; 0 declared names are missing`.
- `python3 tools/check_raw_offsets.py` -> `ok: 162 raw-offset site(s) in 69 file(s)`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/CGameCollision` -> `ok: 1 unit(s) checked,
  none emits its functions out of retail order`.
- `python3 tools/report_diff.py` -> `matched 11254 -> 11256, linked 5507 -> 5507, no regression`,
  `+100% DetectCollision_Cached`, `+100% DetectCollision_Cached_Moving`.
- `./tools/unit_fit.sh MetroidPrime/CGameCollision.cpp` -> `.text` `SHORT by 4224` (the TODO bodies),
  **14 extra functions / 1328 bytes, down from 15 / 1892 at HEAD** - §3 removed the 0x24C
  `WithImplicitMaterials` COMDAT.
- `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**, every sub-check `ok`,
  including `target rose: main/MetroidPrime/CGameCollision: 17 -> 19 / 52 functions`.
- `tools/flip_test.sh` not run: the item is `progress` and says not to flip.

### Process lessons (not `NEW:` items)

- **Read Prime 1's source for *every* function in the unit, not the ones that look hard.** Two
  functions went to 100% from Prime 1's spelling verbatim this run, and the previous four runs had
  read Prime 1 for the neighbouring functions and never for these. The 0.4% and 0.8% functions in a
  list are the ones nobody has opened.
- **objdiff does not compare the callee symbol of a `bl`.** A call to a local weak COMDAT and a call
  to a retail-named external function score identically. This invalidates a class of "missing
  callee" hypotheses and it is cheap to test: name the call `fn_XXXXXXX` and read the score.
- **dtk fills unclaimed regions from retail, so a call to a function no unit defines still links
  and still hashes.** The DOL sha1 gate is about *claimed* bytes, not about completeness. This is
  what makes `extern "C" bool fn_800A4840(int);` a one-line fix rather than a five-file job.
- **`bool x = f();` and `if (f()) { x = true; }` are different code.** `hit = f()` lets the callee's
  bool ride in the result register; `if (f()) { hit = true; }` re-tests it with `clrlwi./beq/li`.
  Which one retail used is visible in three instructions, and it is not consistent across call
  sites in the same file. Check the disassembly before assuming either form.
- **A 64-bit `HasMaterial` mask that looks wrong is usually not.** `kMT_NoStaticCollision = 38`
  produces `(lo & 0) | (hi & 0x40)`, which reads like a bug in the disassembly and is exactly what
  retail emits.
- **Non-POD-by-constructor is a codegen decision with a footprint.** `TAreaId`'s
  `TAreaId() : value(-1) {}` is what makes `CGameArea::GetId()` need an addressable temporary.
  Small, self-contained value types with a default constructor are a recurring source of
  4-instruction differences in this tree.

### No `NEW:` filed

`fn_80128000` and `InitCollision` are both real, but neither is independent work: each needs
`UninitializeCollision` written first, and `InitCollision` additionally needs `fn_80070AD8` and the
`lbl_803DAB70` owner identified, which is a naming question about symbols outside this unit rather
than a unit whose count would rise on its own. Filing either would re-queue the same
`UninitializeCollision` recovery under a new name.

## Not committed, as instructed. Tree state

`src/MetroidPrime/CGameCollision.cpp` (+74/-22) is the whole hand-made diff;
`docs/HANDOFF.md`'s state block was rewritten by the gate (`check_docs_claims.py --write`),
machine-made. Helper scripts are under `.tmp/opencode/cgc/` (gitignored): `d.sh` (normalised
side-by-side diff of one function - note the retail side is addressed by `report.json`'s
`functions[].address`, the **unit-relative offset**, not `metadata.virtual_address`, or objdump
returns nothing), `m.sh` (rebuild + per-function diff against a saved clean-tree baseline),
`try.sh` (score of one function), `setbody.sh` + `/tmp/cca_head.txt`/`/tmp/cca_tail.txt` (swap a
whole function body while trying spellings).

---

## Run 6 (2026-09-30, lane 5) - the ray family, the leaf-cache cursor, and `CUnitVector3f`

**Result: `MetroidPrime/CGameCollision` 19/52 -> 20/52 functions at 100%.** `matched_code`
9.84% -> 10.89% (2036 -> 2250 bytes), unit `.text` fuzzy 62.91% -> 63.74%. Project `All:`
**11298 -> 11299 / 28465**. `python3 tools/report_diff.py build/goal/judge/report.base.json
build/report.json` -> `+100% ResolveCollisions`, `no regression` (`linked 5507 -> 5507`).
**`./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS`**, every sub-check `ok`,
including `target rose: main/MetroidPrime/CGameCollision: 19 -> 20 / 52 functions`. The unit
stays `NonMatching`; `flip_test.sh` was not run, as the item says.

Hand-made diff is **one source file**: `src/MetroidPrime/CGameCollision.cpp` (+33/-25). No header
was touched, so no class layout, `CHECK_SIZEOF` or shared unit moved. (`docs/HANDOFF.md`'s state
block was rewritten by the gate - machine-made.)

### Re-measured first

HEAD of this tree carried runs 1-5: **19/52**, unit `62.91% fuzzy, 9.84% matched_code`, and the
same 33 sub-100% functions the previous notes list. Nothing in them was stale. `fn_801284E0`,
`fn_80128000`, `InitCollision`, `UninitializeCollision`, `Move`, `MovePlayer`, `CollisionFailsafe`
and `FindNonIntersectingVector` are still the TODO/unrecovered bodies; the item's target rose from
the middle of the list instead.

### 1. `ResolveCollisions` 72.61% -> **100.00%** - one ctor, and it is the out-of-line one

The whole item. One line:

```cpp
- CUnitVector3f(collision.GetNormalLeft(), CUnitVector3f::kN_No),
+ CUnitVector3f(collision.GetNormalLeft()),
```

`include/Kyoto/Math/CUnitVector3f.hpp` declares **two** constructors and they are not
interchangeable:

- `CUnitVector3f(const CVector3f& vec, ENormalize)` - inline, and with `kN_No` it degenerates to a
  `CVector3f` copy, so mwceppc expands it in place;
- `CUnitVector3f(const CVector3f& vec)` - **out of line**, defined in
  `src/Kyoto/Math/CUnitVector3f.cpp` as `CVector3f(vec.IsNonZero() ? vec.AsNormalized() :
  CVector3f::Zero())`.

Retail calls the second one: `build/G2ME01/asm/MetroidPrime/CGameCollision.s:1982` is
`bl __ct__13CUnitVector3fFRC9CVector3f`, and it is **the only such call in the whole unit**
(`grep 'bl __ct__13CUnitVector3f'` returns exactly that line). The side-by-side made the shape
unmistakable: retail `mr r4, r29 / addi r3, r1, 0x8 / bl / fmr f1, f31 / mr r6, r3`, ours six
inline instructions `lfs f3, 88(r1) / lfs f2, 92(r1) / lfs f0, 96(r1) / stfs f3, 8(r1) / stfs f2,
12(r1) / stfs f0, 16(r1)` building the same object by hand, and retail additionally keeps
`addi r29, r1, 88` (the source `CVector3f`) live across the branch where we did not.

**This is a fork difference from Prime 1, not a Prime 1 mistake:** Prime 1's `ResolveCollisions`
passes `CUnitVector3f(infoCopy.GetNormalLeft(), CUnitVector3f::kN_No)`, i.e. **no** normalisation,
while Echoes retail normalises (with the zero-vector guard) here. Every earlier run of this item copied Prime 1's spelling, which is
correct for Prime 1 and 27 points wrong for this binary. **The lesson generalises: when an inline
header ctor and an out-of-line ctor of the same class differ only in their bodies, retail's `bl`
tells you which one, and the source has to name the other.**

### 2. `RayDynamicIntersection` 89.28 -> 98.05, `RayDynamicLineOfSightTest` 85.17 -> 97.76

Both build a `CInternalRayCastStructure` in the loop and then virtual-call
`CCollisionPrimitive::CastRayInternal`. Retail's instruction order is

```
[lwz/lwz 0x80/mtctr/bctrl]   <- actor->GetPrimitiveTransform()  (sret to a temp)
[lwz/lwz 0x7c/mtctr/bctrl]   <- actor->GetCollisionPrimitive()
[CMRay ctor][stfs mMaxTime][CTransform4f copy][stw mFilter]
[lwz/lwz 0x1c/mtctr/bctrl]   <- prim->CastRayInternal(ray)
[CRayCastResult copy ctor][lbz mValid]
```

and ours at HEAD had `GetPrimitiveTransform`, then the whole ray built, then
`GetCollisionPrimitive` - mwceppc evaluates the callee's object expression **after** the argument
list, and the aggregate construction is part of the argument list. Naming both callees as locals
gives the order back:

```cpp
const CTransform4f& xf = actor->GetPrimitiveTransform();
const CCollisionPrimitive* prim = actor->GetCollisionPrimitive();
const CInternalRayCastStructure ray(position, direction, closest, xf, filter);
const CRayCastResult candidate = prim->CastRayInternal(ray);
```

Nine spellings measured on `RayDynamicIntersection`, all with the four callees present:

| spelling | score |
|---|---|
| HEAD: transform inline, `actor->GetCollisionPrimitive()->CastRayInternal(ray)` | 89.28 |
| `const CTransform4f xf` (value) + hoisted `prim` | 96.67 |
| comma operator: `(prim = actor->GetCollisionPrimitive(), filter)` as the 5th ctor arg | 94.69 |
| hoisted `prim`, transform inline | 94.48 |
| `const CTransform4f& xf` + hoisted `prim`, transform inline | 94.48 |
| `prim` first, then `const CTransform4f& xf` (swap the two declarations) | 92.45 |
| **`const CTransform4f& xf` then `prim`, `CRayCastResult candidate` named (kept)** | **98.05** |
| same but `prim` as `const CCollisionPrimitive&` | 98.05 |
| same but `if (prim->CastRayInternal(ray).IsValid())` with no named `candidate` | 98.05 on `RayDynamicIntersection`, 88.86 on the LoS overload |

**The binding is load-bearing and it is a *reference*, not a value.** A by-value `const CTransform4f
xf` needs its own 0x30-byte slot and then copies twice; binding a `const&` to the virtual call's
sret temporary gives lifetime extension, so the temporary is built once and copied once into
`mTransform`, exactly as retail does.

The last 2% of both is **one instruction and one stack-slot order**, measured: ours emits
`addi r21, r1, 64` to materialise the reference's address into a register (retail just writes
`addi r4, r1, 12` at the point of use), and mwceppc gives the `CastRayInternal` sret buffer the
slot right after the `TUniqueId` temp (`0x10`) where retail puts the transform temp (`0x0c`) and
gives the transform temp `0x40` where retail puts the sret buffer. Both frames are the same total
size. Ten spellings did not move it; **that part is the allocator.**

### 3. `GetOctreeLeafCache(i)` -> a cursor: four more functions, +0.5 to +1.7

Retail's leaf loop keeps **two** induction variables - `addi r27, r28, 0x1c` hoisted, `li r29, 0`
for the count, then `mr r3, r27` for the argument, `addi r27, r27, 0x910` / `addi r29, r29, 1` in
the body, `lwz r0, 0x18(r28)` / `cmpw r29, r0` re-loading the size every iteration. The index form
gives `addi r26, base, 0x1c` plus `li r27, 0` plus `add r3, r26, r27` - one extra register and a
multiply-shaped address computation. Naming a cursor reproduces both induction variables:

```cpp
const CMetroidAreaCollider::COctreeLeafCache* leaf = &cache.GetOctreeLeafCache(0);
for (uint i = 0; i < cache.GetNumCaches(); ++i, ++leaf) { ... *leaf ... }
```

| function | before | after |
|---|---|---|
| `DetectStaticCollisionBoolean_Cached` (AABX loop) | 94.46 | 95.27 |
| `DetectStaticCollisionBoolean_Cached` (AABX **and** SPHR loops) | 94.46 | **96.10** |
| `DetectStaticCollision_Cached` (SPHR loop) | 96.93 | **97.89** |
| `DetectStaticCollision_Cached_Moving` (AABX and SPHR loops) | 88.79 | **89.31** |

`COctreeLeafCache` is 0x910 bytes and `mLeafCaches` is an inline array at `CAreaCollisionCache`
+0x1c (`CHECK_SIZEOF(CAreaCollisionCache, 0x1b50)` = `0x18 + 3*0x910`), which is why the cursor is
`base + 0x1c` and the stride is `addi ..., 0x910`. This is run 5's `CollideCachedAABox` cursor
finding carried across to the four functions run 5 did not try; `CollideCachedAABox` itself keeps
run 5's index form, which measured better there (92.21).

### 4. `RayStaticIntersection` 89.94 -> 89.97 - `length >= mT`, operand order only

Retail compares `fcmpo cr0, f29, f0` where f29 is `length` and f0 is `candidate.mT`, i.e. the
length test is written with `length` on the **left**; ours wrote `candidate.mT <= length`. Swapping
the operands is worth +0.03. Two other shapes were measured and are worse or neutral: splitting
the three-part `if` into three `if (...) continue;` (89.23) and nesting it as three nested `if`s
(89.97, identical to the flat form). Kept the flat form because it is the smaller diff.

### Full per-function ledger for this run (base -> after)

| function | before | after |
|---|---|---|
| **`ResolveCollisions`** | **72.61** | **100.00** |
| `RayDynamicIntersection` | 89.28 | 98.05 |
| `RayDynamicLineOfSightTest(CStateManager, ...)` | 85.17 | 97.76 |
| `DetectStaticCollisionBoolean_Cached` | 94.46 | 96.10 |
| `DetectStaticCollision_Cached` | 96.93 | 97.89 |
| `DetectStaticCollision_Cached_Moving` | 88.79 | 89.31 |
| `RayStaticIntersection` | 89.94 | 89.97 |

Nothing fell. `unit matched 19 -> 20 / 52`, `project matched 11298 -> 11299`.

### Also tried this run, none of it helping (measured, do not repeat)

- `RayStaticIntersection` as three `if (...) continue;` (89.23) or three nested `if`s (89.97,
  identical). Retail emits `beq / fcmpu / beq / fcmpo / bge / b / fcmpo / bge`; mwceppc folds the
  middle `||` into one `cror eq,gt,eq` whatever the source shape. The remaining 10% is FPR
  numbering: retail keeps `length` in `f29` and `closest` in `f30` for the whole function, we keep
  `closest` in `f30` and copy it into `f29`.
- `CollideWithStaticBodyNoRot` 92.08%: naming the product like Prime 1
  (`const CVector3f impulseVec = impulse * collisionNormal; actor.ApplyImpulseWR(impulseVec, ...)`)
  scores **exactly the same** (92.08). Run 5's remaining 7.9% is the store order of two `CVector3f`
  copies - retail `stfs 36 / stfs 32 / stfs 40` then `stfs 20 / stfs 24 / stfs 28`, ours
  `32 / 36 / 40` then `24 / 20 / 28` - and the `addi r4` / `addi r3` that follow are swapped to
  match.
- `DetectDynamicCollision(primitive, transform, actor, collisions)` 91.85%: `return Collide(...) != 0`
  re-confirms run 4's number exactly (**88.23**). The side-by-side shows why and it is scheduling,
  not source: retail interleaves `clrlwi. r3,r3,24 / neg r0,r3 / lwz r31,188(r1) / or r0,r0,r3 /
  lwz r30,184(r1) / srwi r3,r0,31 / ...` and mwceppc emits the whole four-instruction chain first
  and every epilogue `lwz` after it.
- The `RayStaticIntersection` middle test spelled `candidate.mT > length` in the split form is what
  made the three-`continue` variant 89.23; with `length < candidate.mT` it is the same code.

### Gates, all run in this tree

- `./tools/fast_try.sh MetroidPrime/CGameCollision` after every edit - the numbers above.
- `./tools/decomp_build.sh` -> `All:  32.53% fuzzy, 25.20% matched, 11.94% linked (11299 / 28465
  functions)`. The line did not fall; the matched count rose by 1.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail, correct).
- `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**, every sub-check `ok`.
  Its `gate.sh` line covers configure, ninja + build.sha1, the 86 REL hashes vs `config.yml`, the
  report, the per-function diff, module wiring, `dol_read`, docs claims, gs offsets, raw offsets,
  decl order, `files.cmake`, module order, the port probe and the port link gap -
  `build/goal/check-gate.log` ends `GATE PASS 76d3d116+2 changed`.
- `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json` -> `matched
  11298 -> 11299, linked 5507 -> 5507, +100% ResolveCollisions, no regression`.
- `python3 tools/check_symbol_names.py` -> `checked 514 units; 0 declared names are missing`.
- `python3 tools/check_raw_offsets.py` -> `ok: 162 raw-offset site(s) in 69 file(s)`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/CGameCollision` -> `ok: 1 unit(s) checked,
  none emits its functions out of retail order`.
- `tools/flip_test.sh` not run: the item is `progress` and says not to flip.

### Walls, written from THIS run's measurements

- `WALL: RayDynamicIntersection 98.05% and RayDynamicLineOfSightTest(CStateManager,...) 97.76% - identical instruction set to retail; mwceppc materialises the const CTransform4f& into a register (one extra addi) and allocates the CastRayInternal sret slot where retail allocates the transform temp. Ten spellings of the two callee hoist measured, none changed it.`
- `WALL: CollideWithStaticBodyNoRot 92.08% - 123 of 123 instructions, the same two CVector3f built and stored twice; only the order of the six stfs and the addi that follow differs. Prime 1's named product local scores identically.`
- `WALL: DetectDynamicCollision(primitive, transform, actor, collisions) 91.85% - retail interleaves the != 0 bool normalisation (clrlwi./neg/or/srwi) with the three epilogue lwz's and mwceppc schedules all four together; the `!= 0` source is right and scores 88.23. (Run 4 measured the same on all three wrappers; this run re-measured one of them and got the same number to two decimals.)`
- `WALL: RayStaticIntersection 89.97% - 162 of 162 instructions; retail holds `length` in f29 and `closest` in f30 for the whole function and mwceppc holds `closest` in f30 and copies, and the middle || is a cror instead of retail's beq/bge/b. Flat, nested and three-continue spellings all measured.`

### What is still unclaimed work, characterised (no NEW: filed)

- **`MovePlayer` (312 B, 1.28%) is now fully mapped and is the best next item.** Retail
  (`asm:1268-1352`, `0x801247F8`) is `SetApplyRotationWhenInCollision(true)` as
  `li r3,1 / lbz r0,0x168(r4) / rlwimi r0,r3,6,25,25 / stb r0,0x168(r4)` (bit 31 of the byte at
  +0x168), then `addi r3,r1,0x18 / bl PredictAngularMotion / mr r3,r30 / addi r4,r1,0x18 / bl
  AddMotionState`, then `lbz r0,0x169(r30)` as a branch, then the two 8-byte filter objects at
  `lbl_803B4E88` + the actor passed to `bl fn_80218E30` in both arms, with
  `CGroundMovement::MoveGroundCollider_New` between them under
  `lwz r3,0x68(r30) / and r0,r3,0x20` (which is `HasMaterial(kMT_GroundCollider)` - the enum is 37,
  so the mask is `1<<5` in the high word), and `SetApplyRotationWhenInCollision(false)` in the
  epilogue. Prime 1's version is the same shape with `CBallFilter` and the removed
  `gkUseNewPlayerMovement` branch. **What blocks it is not the control flow: it is that
  `CPhysicsActor` in this tree has no member at +0x168 or +0x169** (`mStandardCollider` is at 0x11
  here) and no `SetApplyRotationWhenInCollision`, and the 8-byte object at `lbl_803B4E88` is
  `{vptr, actor}` with no class in this tree. Recovering it means adding two members to a shared
  class, which is exactly the "one shared header moves an unrelated function" hazard, so it is a
  different item with a clean full-build diff.
- `UninitializeCollision` (364 B, 8.64%) and `fn_80128000` (`CToken::operator=`, 200 B, 0.00%) are
  unchanged from run 5's characterisation: `fn_80128000` needs private `CToken` access plus four
  call sites inside a TODO body. `InitCollision` (1048 B, 0.38%) still needs `fn_80070AD8` and the
  `lbl_803DAB70` owner. `fn_801284E0` (`__sinit`, 252 B, 0.00%) still needs 0x70 bytes of unclaimed
  `.bss`.
- `GetMinExtentForCollisionPrimitive` (288 B, 91.97%) is Prime 1's source verbatim already, and
  the only difference is that retail **spills** the three extents to `8/12/16(r1)` and compares
  from memory while mwceppc keeps them in `f3/f4/f1` - 3 stores that retail has and we do not.
  Run 1 tried `const` and non-`const` locals and `extents[0]` vs `.GetX()`; nothing moved it.

### Process lessons (not `NEW:` items)

- **Check whether an inline header constructor has an out-of-line sibling before trusting the
  disassembly's shape.** `CUnitVector3f` has both, they differ only in their bodies, and picking
  the wrong one cost this item four previous runs and 27 points on one function.
- **Echoes forks Prime 1 in ways that look like decompilation errors and are not.** Prime 1 does
  not normalise the normal in `ResolveCollisions`; Echoes does. Reading Prime 1 as an answer rather
  than a guide is what run 1 was rejected for once already, in a different function.
- **mwceppc evaluates a call's object expression after its arguments, and an inlined aggregate
  constructor counts as part of the argument list.** When retail's order interleaves a callee with
  the construction of a temporary, hoist the callee into a named local rather than reordering the
  expression - and bind the other callee's result as a `const&`, so the sret temporary is built
  once.
- **A cursor loop and an index loop are different code even when they do the same thing**, and
  retail often keeps *both* induction variables (one for the bound, one for the address). This
  generalised run 5's `CollideCachedAABox` finding to four more functions it had not been tried on.
- **When retail keeps `x` in one callee-saved register and `y` in another for a whole function and
  mwceppc keeps `y` and copies it into `x`, no source spelling has fixed it in this unit.** Three
  separate walls this run are exactly that shape.

### No `NEW:` filed

The one genuinely separate body this run mapped - `MovePlayer` - needs two new members in a shared
class plus an unidentified filter type, so filing it would re-queue a `CPhysicsActor` layout
recovery rather than a unit whose count would rise on its own.

## Not committed, as instructed. Tree state

`src/MetroidPrime/CGameCollision.cpp` (+33/-25) is the whole hand-made diff;
`docs/HANDOFF.md`'s state block was rewritten by the gate (machine-made). Helper scripts are under
`.tmp/opencode/cgc/` (gitignored, not part of the change): `d.sh` / `d2.sh` (normalised
side-by-side of one function, retail from the split `.s` when it has a `.fn` block and from
`main.elf` by address otherwise - **the retail address is `report.json`'s unit-relative `address`
field for objdiff but `metadata.virtual_address` for objdump**), `m.sh` (rebuild + per-function
diff against a saved clean-tree report), `v.sh` (the four ray/leaf functions' scores only).
