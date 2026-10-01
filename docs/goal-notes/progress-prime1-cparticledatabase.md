# progress-prime1-cparticledatabase

`kind: progress`, target `main/MetroidPrime/CParticleDatabase`. The unit stays `NonMatching`;
`flip_test.sh` was not run (it is nowhere near a flip - 77 of its 100 functions are below 100%).

## Result

Unit **19 -> 23** matched functions of 100, fuzzy **18.33% -> 21.33%**.
Global `build/report.json` matched **11506 -> 11510**, linked 5590 -> 5590.
`./tools/goal_check.sh build/goal/item.json` -> **PASS** (gate ok, 36 checks).

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  11506 -> 11510   linked 5590 -> 5590   (+4 functions at 100%, 0 units newly linked)
  +100%    ... :: AddToRendererClippedParticleGenMap__17CParticleDatabaseCFRCQ24rstl89map<...>RC14CFrustumPlanes
  +100%    ... :: AddToRendererClippedParticleGenMapMasked__17CParticleDatabaseCFRCQ24rstl89map<...>RC14CFrustumPlanesUiUi
  +100%    ... :: ClearAllNonPersistentEffects__17CParticleDatabaseFP13CStateManager
  +100%    ... :: InsertParticleGen__17CParticleDatabaseFbiUiRCQ24rstl28auto_ptr<16CParticleGenInfo>
no regression
```

Changed: `src/MetroidPrime/CParticleDatabase.cpp` only (43 insertions / 17 deletions). No header,
no `configure.py`, no `splits.txt`, no `files.cmake`, no `build/goal/` file, no `asm`.
(`docs/HANDOFF.md`'s state block was rewritten by `gate.sh` itself, which `goal_check.sh` runs with
`MP_GATE_DOCS_WRITE=1`; the driver discards it.)

## The two functions this item was queued for are not done, and why

`item.json` names `CacheParticleDesc__...FRC10SObjectTag` (0x800A93A8, 212 B) and
`CacheParticleDesc__...FRCQ214CCharacterInfo16CParticleResData` (0x800A947C, 112 B). Both are
**five calls to helpers this unit does not define yet**, and objdiff only lets a caller reach 100%
when each `bl` target pairs with retail's:

```
CacheParticleDesc(CParticleResData)  0x800A947C, 112 B - five tail calls, no logic at all
  mr r3,r31 / mr r4,r30          ; &data.mPart  , this+0
  bl 0x800A9C50 (408 B)  addi r3,r31,16 / addi r4,r30,20  bl 0x800A9DE8 (408 B)
  addi r3,r31,32 / addi r4,r30,40  bl 0x800A9F80 (408 B)
  addi r3,r31,48 / addi r4,r30,60  bl 0x800AA118 (408 B)
  addi r3,r31,64 / addi r4,r30,80  bl 0x800AA2B0 (408 B)

CacheParticleDesc(SObjectTag)       0x800A93A8, 212 B - a binary-search switch on tag.GetType()
  'PART' 0x50415254 -> bl 0x800AA448 (384 B), r4 = this+0
  'SWHC' 0x53574843 -> bl 0x800AA5C8 (384 B), r4 = this+20
  'ELSC' 0x454C5343 -> bl 0x800AA748 (384 B), r4 = this+40
  'SPSC' 0x53505343 -> bl 0x800AA8C8 (384 B), r4 = this+60
  'SRSC' 0x53525343 -> bl 0x800AAA48 (384 B), r4 = this+80
  (r3 = tag.GetId(), i.e. tag.mId at +4, in all five)
```

**Useful, and it contradicts the file's TODO:** the offsets are *already right*. The five desc maps
sit at 0/20/40/60/80 (`rstl::map` is 0x14, `CHECK_SIZEOF(unk_map, 0x14)`) and `CParticleResData`'s
vectors at 0/16/32/48/64 (`rstl::vector` is 0x10; it holds six, and retail uses the first five:
`mPart, mSwhc, mElscA, mSpsc, mSrsc`). So the `// TODO: correct CParticleResData's five resource
lists before traversing them` comment on line 11 is stale - **no layout change is needed**. The
mapping is `mPart->mParticleDescs, mSwhc->mSwooshDescs, mElscA->mElectricDescs,
mSpsc->mSpscDescs, mSrsc->mSrscDescs`.

**Why the callers cannot be matched yet:** objdiff pairs a `bl` target by *function pair*, not by
address - proof from this same unit: `Update` measures **100%** while calling
`UpdateParticleGenDB`, which measures 0.21%. Retail's ten helpers are all `fn_*` (address-derived
names), so a pair only exists if our object defines symbols with **exactly** those names. The repo's
convention for that is `extern "C" void fn_800A9C50(...)`, as in `src/MetroidPrime/CScriptSound.cpp`.
Two obstacles to writing the five list-walkers (`0x800A9C50` etc.):

1. Each is a 408-byte body: `for (id in list) if (map.find(id) == map.end()) map.insert({id,
   rc_ptr<TLockedToken<TDesc>>(rs_new TLockedToken<TDesc>(gpSimplePool->GetObj(SObjectTag(4CC,
   id))))})` plus the iterator/`rc_ptr` destruction - five near-identical copies, because a
   template instantiation would not carry the `fn_*` symbol.
2. `CParticleDescriptionSPSC` and `CParticleDescriptionSRSC` are **only forward declarations** in
   `include/MetroidPrime/CParticleDatabase.hpp` (lines 27-28, marked "Guessed names"). Constructing a
   `TLockedToken<X>` needs `X` complete, and the repo has no such class: the SPSC/SRSC asset types
   are behind `fn_8032B5DC` / `fn_8032F0D4` in `CFactoryFunctionsPort.cpp`, and
   `CParticleSpawnSystem.hpp:63` only guesses that SRSC "uses the SPSC property layout". Whoever
   picks this up has to settle that pair of types first; the map layout does not depend on the
   choice, so a wrong guess costs nothing but the `__dt__Q24rstl1xxmap<...>` COMDAT symbol names.

Neither caller is one line away - they are five (resp. ten) missing functions.

## What did match, and how (per function, as the item asked)

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `AddToRendererClippedParticleGenMap` | 1.96% | **100%** | **verbatim** (`prime-ref` line 449) |
| `AddToRendererClippedParticleGenMapMasked` | 1.96% | **100%** | **verbatim** (`prime-ref` line 460) |
| `ClearAllNonPersistentEffects` | 41.87% | **100%** | no counterpart (Echoes-only); 3x `map.clear()` |
| `InsertParticleGen` | 74.79% | **100%** | **verbatim** (`prime-ref` line 92, the `if (oneShot) {...} else {...}`) |
| `DeleteAllLightsForParticleDB` | 83.22% | 99.75% | loop verbatim; one `cmplw` operand order left |
| `SetModulationColorAllActiveEffectsForParticleDB` | 89.74% | 99.74% | same |
| `SuspendAllActiveEffectsForParticleDB` | 82.34% | 99.74% | same |
| `DestroyParticlesForParticleDB` | 81.58% | 99.72% | same |
| `GetParticleEffect` | 99.01% | 99.01% | Prime 1 has no `GetParticleEffect` |
| `AccumulateBounds` | 86.94% | 86.94% | no counterpart |
| `SetParticleExternalParam` | 3.57% | 3.57% | not attempted (see `NEW:`) |
| `CacheParticleDesc` x2 | 3.57 / 1.89% | unchanged | not attempted (above) |
| `AddParticleEffect` x2, `UpdateParticleGenDB` | 0.12 / 0.39 / 0.21% | unchanged | TODO stubs, 1032-3220 B |

### The `== true` is load-bearing (this is the whole trick in the two 100% functions)

`AddToRendererClippedParticleGenMap` went 1.96% -> 94.02% on the first try and only reached 100%
with Prime 1's exact test. Retail's tail after the `BoxInFrustumPlanes` call is

```
clrlwi   r0,r3,24 ; subfic r0,r0,1 ; cntlzw r0,r0 ; rlwinm. r0,r0,27,24,31 ; beq skip
```

five instructions, against our two (`clrlwi. r0,r3,24 ; beq skip`). Measured, same body, only the
test varied:

| spelling | score |
| --- | --- |
| `if (frustum.BoxInFrustumPlanes(gen->GetBounds()))` | 94.02% |
| `if (!frustum.BoxInFrustumPlanes(gen->GetBounds()))` | 93.92% |
| `if (... == false)` | 98.04% |
| `if (!(... == true))` | 99.90% |
| `if (... == true)` (Prime 1) | **100.00%** |

### `InsertParticleGen`: one ternary chain -> two switches

Retail branches on `oneShot` *first* (`clrlwi. r0,r4,24; beq`) and then runs a `switch (flags & 0x60)`
inside each arm - six `addi r4,r29,<map>` sites. The tree had one `switch` with a `oneShot ?` ternary
in every case, which GCC evaluates the other way round. Prime 1's `if (oneShot) { switch } else {
switch }` reproduces it instruction for instruction (69 -> 73 instructions, 276 -> 292 B = retail).

### `ClearAllNonPersistentEffects`: the three TODO'd maps are `clear()`

Retail (208 B) is the three `DeleteAllLightsForParticleDB` calls the tree already had, then per map
`if (map.mRootNode) <164-byte eraser>(map); map.mLeftmost = map.mRightmost = map.mRootNode = 0;
map.mCount = 0;`. That is exactly `rstl::map::clear()` in `include/rstl/red_black_tree.hpp:227`, so
three `clear()` calls replace the TODO with no other edit.

## The four `ForParticleDB` loops: 83-90% -> 99.7%, and the wall

All four had the identical shape: every instruction matched except the loop-exit block, where retail
materialises the `end()` iterator in a stack slot and ours kept it in registers.

```
retail  cmplw r29,r31 ; stw r31,8(r1) ; li r0,0 ; stw r30,12(r1) ; bne ; cmplw r30,r30 ; beq
ours   cmplwi r30,0  ; li r0,0 ; bne ; cmplw r31,r31 ; beq
```

`map.end() != it` produces retail's stores, because the temporary is then the *implicit object* of
`operator!=` and so must have an address (spelling `it != map.end()` binds it to a const-reference
parameter and MWCC forwards the stores away). Measured on
`SetModulationColorAllActiveEffectsForParticleDB`:

| spelling | score |
| --- | --- |
| `it != map.end()` (the tree) | 89.74% |
| `while (it != map.end())` | 89.74% |
| `const_iterator end = map.end(); it != end` | 89.74% |
| `!(it == map.end())` | 89.36% |
| `CParticleGenInfo* gen = it->second.get()` hoisted | 89.74% |
| `(*it).second` instead of `it->second` | 89.74% |
| **`map.end() != it`** | **99.74%** |

What is left is one instruction, the operand order of the first compare: retail `cmplw r30,r0`
(node vs 0), ours `cmplw r0,r30` (0 vs node) - everything else in all four functions is now
byte-identical. Getting it needs `rstl::red_black_tree::const_iterator::operator!=` to compare
`other.mNode` first. **I tried it and it is not available:** flipping the operands in
`include/rstl/red_black_tree.hpp:86` and rebuilding gives

```
FAILED: [code=1] build/G2ME01/ok
build/G2ME01/main.dol: FAILED
86 files OK
WARNING: 1 computed checksum(s) did NOT match
```

i.e. it moves code in a `Matching` unit and the DOL stops reproducing retail. Reverted; the header
is untouched in the diff. (`operator==` is not the answer either: `!(map.end() == it)` puts the
temporary's 0 on the left again.)

## Other things measured, so the next run skips them

- **`GetParticleEffect` 99.01%, held there.** Six byte-identical lookup blocks; block 1 matches, and
  in blocks 2-6 retail puts the iterator's node in `r4` where we put it in `r5` (both then
  `cmplw rX,r3` against the 0 in `r3`). `map.end() != it` in all six blocks made it **worse**
  (99.01% -> 90.32%), so the operand reversal that helps the `ForParticleDB` family hurts here.
  Reverted. This is register allocation, not spelling.
- **`AccumulateBounds` 86.94%, three spellings tried, all worse or equal.** Retail copies the
  `optional_object<CAABox>` it gets back from `GetBounds()` into a *second* stack slot
  (6 `stw` + the flag byte) and then reads the box out of that copy; the tree reads the original
  temporary and re-tests its flag. | copy local `71.37%` | `if (bounds.valid())` first `90.03%` |
  copy local + `if (bounds)` `71.37%` | the tree's own spelling `86.94%` (kept). No Prime 1
  counterpart exists - Prime 1's `CParticleDatabase` has no `AccumulateBounds`/`GetTotalBounds` at
  all, bounds accumulation is an Echoes addition.
- **`SetParticleExternalParam`, and a signature bug worth knowing about.** Retail's symbol is
  `SetParticleExternalParam__17CParticleDatabaseFUiif` - **four** parameters `(uint, int, int, float)`,
  and the body reads the index from `r5`, not `r4`. The second `int` is never read. The tree declares
  three parameters, so even with the body written the index lands in the wrong register. The caller
  is `src/MetroidPrime/CAnimData.cpp:676` (`CAnimData.cpp` is `NonMatching`, so it can be changed).
  The body also needs two helpers this unit does not define: `fn_800A7A7C` (36 B, copies the
  `TLockedToken<CElementGen>` at `+120/+124` out of the effect and bumps the lock) and `fn_800A6444`
  (100 B, releases it), then `CElementGen::SetExternalParam(uint, float)` at 0x802D0D50.
- **The two `AddParticleEffect` (3220 B, 1032 B) and `UpdateParticleGenDB` (1900 B)** are still TODO
  stubs at 0.12% / 0.39% / 0.21% and are where this unit's real remaining mass is.

## Other gates, measured

- `sha1sum build/G2ME01/main.dol` and all 86 REL sha1s vs `config/G2ME01/config.yml`: unchanged
  (`gate.sh` -> `ninja + build.sha1 ok`, `hashes vs config.yml ok`).
- `./tools/decomp_build.sh` -> `All: 32.97% fuzzy, 25.83% matched, 12.18% linked (11510 / 28465)`.
- `python3 tools/check_symbol_names.py` -> `checked 515 units; 0 declared names are missing`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/CParticleDatabase` -> `ok: 1 unit(s)
  checked, none emits its functions out of retail order` (no function was added or moved).

WALL: DeleteAllLightsForParticleDB 99.75% - with `map.end() != it` every instruction matches except one `cmplw` operand order, and the fix needs `rstl::red_black_tree::const_iterator::operator!` comparing `other.mNode` first, which breaks the DOL sha1 (measured)

NEW: progress-cparticledb-setexternalparam | progress | MetroidPrime/CParticleDatabase | SetParticleExternalParam is retail's 4-parameter `(uint, int, int, float)` (index in r5) but the tree declares 3, and its body needs the two unnamed 36 B/100 B helpers fn_800A7A7C and fn_800A6444; CAnimData.cpp:676 is the only caller and is NonMatching
