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
