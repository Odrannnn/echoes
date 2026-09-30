# progress-prime1-ccubemodel — `Kyoto/Graphics/CCubeModel`

`kind: progress`, target `main/Kyoto/Graphics/CCubeModel`. Stayed `NonMatching`; no `flip_test.sh`
was run and `configure.py` / `config/` were not touched. The change is 2 files: one new inline
accessor in a header, two call sites switched to it.

## Result

`tools/goal_check.sh build/goal/item.json` → **PASS**

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10394 -> 10395   linked 5048 -> 5048
  ok    check_symbol_names.py
  ok    All:  31.56% fuzzy, 24.11% matched, 11.83% linked (10395 / 28465 functions)
  ok    target rose: main/Kyoto/Graphics/CCubeModel: 23 -> 24 / 26 functions
  ok    no asm added
```

`build/report.json` for `main/Kyoto/Graphics/CCubeModel`, before → after:

| function | bytes | before | after |
|---|---|---|---|
| `DrawFlat__10CCubeModelCFi` | 352 | 96.93182 % | **100.0 %** |
| `__ct__10CCubeModelFPQ24rstl37vector<Pv,...>RC6CAABoxUcbUi` | 416 | 96.68269 % | 96.68269 % (unchanged, see wall) |
| `DrawSurfaceWireframe__10CCubeModelCFRC12CCubeSurface` | 1404 | 99.91453 % | 99.91453 % (unchanged, see wall) |
| unit `matched_functions` | | 23 / 26 | **24 / 26** |
| unit `fuzzy_match_percent` | | 99.58144 % | 99.75665 % |
| unit `matched_code_percent` | | 64.76314 % | 70.47372 % |

Global: matched 10394 → 10395, fuzzy 31.564077 % → 31.564245 %, linked flat at 5048. No function
anywhere got worse (diffed every function in `build/report.json` against the pre-change report).

## The one change: Prime 1 has two vertex-desc accessors, Echoes needs both

Prime 1's `include/Kyoto/Graphics/CCubeMaterial.hpp` carries **two** accessors for the same value:

```cpp
uint GetVertexDesc() const {          /* byte arithmetic: add, lwz with displacement */
  const uchar* data = GetData();
  data += (GetTextureCount() * sizeof(uint));
  data += sizeof(uint) + sizeof(uint);
  return CBasics::SwapBytes(*reinterpret_cast< const uint* >(data));
}
uint GetVertexDescLwzx() const {     /* indexed load: slwi, addi, lwzx */
  return CBasics::SwapBytes(static_cast< const uint* >(mData)[GetTextureCount() + 2]);
}
```

Our tree only had the first one, and every call site used it. Echoes' retail object needs **both**
forms, in the same translation unit, so the split is not cosmetic:

* `DrawSurfaceFlat` and `DrawSurfaceWireframe` → `add rX, rBase, rTmp; lwz rX, 0x8(rX)` (form 1)
* `DrawFlat` → `slwi r3, r0, 2; addi r0, r3, 0x8; lwzx r3, r4, r0` (form 2)

Prime 1 draws the same split (`DrawFlat` uses `GetVertexDescLwzx`, `DrawSurfaceWireframe` uses
`GetVertexDesc`; `src/MetaRender/CCubeRenderer.cpp` in Prime 1 uses the Lwzx one too). So I added
`GetVertexDescLwzx()` to `include/Kyoto/Graphics/CCubeMaterial.hpp` and used it at the two
`DrawFlat` call sites. The two return the same value by construction, so behaviour is unchanged.

`DrawFlat`: 5 differing instructions → 0, in both loop bodies. `DrawSurfaceFlat` and
`DrawSurfaceWireframe` are byte-identical to before, so the split costs them nothing.

## Spellings tried, with their measured scores

Everything below is this run. `tools/decomp_build.sh Kyoto/Graphics/CCubeModel` after each (~10 s).

| spelling | result |
|---|---|
| **baseline** (`GetVertexDesc()` everywhere) | DrawFlat 96.93, ctor 96.68, wireframe 99.91, unit 23/26 |
| **`GetVertexDescLwzx()` in DrawFlat (kept)** | DrawFlat **100.0**, unit **24/26**, global 10395 |
| change the *header's* `GetVertexDesc()` to the indexed form (no second accessor) | DrawFlat 100.0 but **DrawSurfaceFlat 100 → 96.88** and wireframe 99.91 → 99.60; **global 10393**, one function worse. Rejected: one header cannot serve both call sites. |
| `const uint vertexDesc = material.GetVertexDesc();` temp in DrawFlat | no change (96.93) |
| `CGX::SetVtxDescv_Compressed(GetMaterial(surface).GetVertexDesc());` (no local) | no change (96.93) |
| Prime 1's `GetTextureCount()` body (`const uchar* data = GetData(); data += sizeof(uint); const int ret = ...`) | byte-identical everywhere; no effect on either wall |
| `const uint vertexAttributes = material.GetVertexDesc();` merged decl in wireframe | no change (99.91) |
| function-statics declared before `material` in wireframe | wireframe **99.91 → 90.92**. Reverted. |

Also checked, as a possible free win and **not** done because it is a different unit: Prime 1's
`src/MetroidPrime/CDecalManager.cpp` and Echoes' `src/Kyoto/Animation/DolphinCSkinnedModel.cpp:292`
call the same accessor (`AddToRenderer` is at 99.29 %, the two `DolphinDrawInternal` at 98.59 /
98.57 %). If one of those call sites really wants the Lwzx form, switching it is a one-line change
in a lane item of its own. Not touched here — out of scope for this unit, and the reviewer rejects
unrelated fixes.

## Walls

WALL: `__ct__10CCubeModelFPQ24rstl37vector<Pv,...>RC6CAABoxUcbUi` 96.68% - retail re-reads `surf[i-1]` after every store; mwcceppc keeps one register copy, and no spelling tried here flips the tie.

WALL: `DrawSurfaceWireframe__10CCubeModelCFRC12CCubeSurface` 99.91% - only the two scratch registers of the `GetVertexDesc()` load differ (ours r0/r5, retail r3/r0); the other 346 of 351 instructions match.

### The ctor, in detail (for the next run)

Everything outside the second loop matches byte for byte. The loop that links the surface list
into the two chains is the only difference, and it is a pure register/CSE choice — **identical
instruction count either way**:

ours (105 entries, 3 loads of the element + 2 stack spills):

```
70  lwzx r9, r7, r6        ; data = surf[i-1]     -> register copy kept
71  lwz  r0, 0xc(r9)       ; mMaterialIndex
72  stw  r9, 0x8(r1)
74  stw  r9, 0xc(r1)
87  stw  r0, 0x18(r9)      ; uses the copy
88  lwzx r0, r7, r6        ; reloads after the store
```

retail (1 stack spill, 5 loads, no register copy across the branch at 75):

```
70  lwzx r3, r7, r6
71  lwz  r0, 0xc(r3)
72  stw  r3, 0x8(r1)
86  lwzx r3, r7, r6
87  stw  r0, 0x18(r3)
88  lwzx r0, r7, r6
```

The stores are `stw …, 0x18(r3)` (writes `mNextSurface` through a `const void*`) and
`stw …, 0x3c/0x38(r31)` (writes `this->mFirstSorted/mFirstUnsorted.mData`), so mwcceppc cannot
prove the element is unchanged and must reload — retail reloads every time, ours caches one copy.
The source has **three** reads of the element (material check, `->mNextSurface`, head insert), so
this is a tie in the allocator, not a missed optimisation.

Spellings tried for this function, all measured (differing-instruction counts come from
`objdiff-cli diff -1 build/G2ME01/src/…o -2 build/G2ME01/obj/…o <symbol>`):

| spelling | ctor % |
|---|---|
| `void*& data = surf[i-1];` (baseline) | 96.68 |
| repeat the subscript: `surf[i-1]` at all four use sites, no reference | 94.33 (stops hoisting `surf.data()` out of the loop) |
| Prime 1's `uint materialIndex = …->mMaterialIndex;` local first | 94.20 |
| `void* data = surf[i-1];` (by value, not a reference) | 94.23 |
| `mFirstSorted = CCubeSurface(data);` instead of `mFirstSorted.mData = …` | 96.68 (byte-identical) |
| `const CCubeMaterial material = GetMaterial(CCubeSurface(data));` as its own statement | 96.68 (byte-identical) |

The `void*&` reference matters and must stay: every non-reference spelling lost 8–15 points because
the vector's data pointer stopped being hoisted out of the loop. What is still missing is a way to
stop mwcceppc from promoting the element to a register for the `->mNextSurface` store; that is the
only thing between 96.68 % and 100 %.
