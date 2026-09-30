# progress-buckets-insert — `MetaRender/CCubeRenderer`

`kind: progress`, target `main/MetaRender/CCubeRenderer`. **11 functions matched, 46 -> 57 of 217.**
`tools/goal_check.sh build/goal/item.json` -> **PASS** (`goal_check: PASS progress-buckets-insert`).
`linked` is unchanged at 4917: the unit is 57/217 and cannot flip, so nothing here is a unit result.

## Measured position

| | before | after |
|---|---|---|
| `main/MetaRender/CCubeRenderer` `matched_functions` | 46 / 217 | **57 / 217** |
| unit `fuzzy_match_percent` | 10.618709 | 10.942382 |
| `All:` matched | 10041 | **10052** |
| `All:` linked | 4917 | 4917 |

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10041 -> 10051   linked 4917 -> 4917
  ok    check_symbol_names.py
  ok    All:  30.97% fuzzy, 23.24% matched, 11.78% linked (10051 / 28465 functions)
  ok    target rose: main/MetaRender/CCubeRenderer: 46 -> 56 / 217 functions
  ok    no asm added
goal_check: PASS progress-buckets-insert
```

(That run was at 56; the final run reads `10041 -> 10052` and `46 -> 57`, same verdict. The number
quoted in the table is the one `build/report.json` holds now.)

`tools/unit_fit.sh MetaRender/CCubeRenderer.cpp` before and after, measured on the same worktree by
rebuilding `HEAD` and then this change:

| | HEAD | after |
|---|---|---|
| `.text` ours | 13500 | 13488 |
| functions in ours but not in retail's unit object | 57 (4976 bytes) | **57 (4976 bytes)** |
| `.data` over / `.sbss` over | 336 / 45 | 336 / 45 |

The 57 are pre-existing COMDAT weak template/destructor instantiations; the count and the byte total
are unchanged, so this change adds no function the retail object does not define. The unit is
`NonMatching` and stays so.

## The eleven functions that went to 100.0%, and what each needed

Every entry is a real source-level difference recovered from a retail-vs-ours instruction diff.
`build/G2ME01/main.elf` is retail for this NonMatching unit's `.text`, so `tools/dis.sh` is the
retail side; the diff tool is `.tmp/opencode/insdiff.py` (untracked, in this worktree's `.tmp`).

1. **`Buckets::Clear` 78.62 -> 100.0** (the item's third-named function). Retail's bucket loop is a
   pointer walk that recomputes `end()` every iteration, not an index loop:

   ```
   80271c58  addi  r5,r3,4            ; p = &(*sBuckets)[0]
   80271c68  lwz   r3,sBuckets ; lwz r0,0(r3) ; mulli r0,r0,516 ; add r3,r3,r0 ; addi r3,r3,4
   80271c7c  cmplw r5,r3 ; bne 80271c60    ; while (p != sBuckets->end())
   80271c60  stw   r4,0(r5) ; addi r5,r5,516
   ```

   `mulli r0,r0,516 ; add ; addi +4` is exactly `reserved_vector::end()` = `data() + mCount` with
   `data()` at `mCount+4` and `sizeof(Bucket) == 4 + 128*4 == 516`, so the loop is
   `for (Bucket* p = sBuckets->begin(); p != sBuckets->end(); ++p) p->clear();`. Ours was
   `for (int i = 0; i < sBuckets->size(); ++i) (*sBuckets)[i].clear();`, which MWCC turns into an
   index+byte-offset walk (8 instructions where retail has 12) and which also needs a second zero
   register (`li r0,0 ; li r7,0 ; ... ; mr r4,r0` against retail's single `li r4,0`).

2. **`Buckets::InsertPlaneObject` 79.30 -> 100.0**. Pure register allocation, and it is fixed by
   spelling the container pointer as a local. Retail holds `sPlaneObjectData` in callee-saved `r31`
   across the ctor call and `farDistance` in `r30`; ours rematerialised the pointer with a second
   `lwz` and never touched r30/r31. Writing `PlaneList* list = sPlaneObjectData;` and using
   `list->` throughout makes MWCC keep it live, and the object becomes instruction-identical
   (30/30, only relocation operands differ). Spelled out:

   | | retail | ours before | ours after |
   |---|---|---|---|
   | frame | `stwu r1,-80(r1)` + `stw r31,76` `stw r30,72` | same frame, no spills | same |
   | ptr | `lwz r10,-26140(r13) ; lwz r0,0(r10) ; mr r31,r10` | `lwz r9,0(0) ; lwz r0,0(r9) ; mr r9,r8` | matches retail |
   | push_back receiver | `mr r3,r31` | `lwz r3,0(0)` (reload) | matches retail |

3. **`Buckets::Insert` 81.40 -> 84.11** (not 100; see the wall below). The identical local-pointer
   change fixes the part the item's reason named — the 80-byte frame and `sData` in callee-saved
   `r31`:

   ```
   retail 8027249c  stwu r1,-80(r1) ; ... ; stw r31,60(r1) ; lwz r31,-26156(r13)
   ours before       stwu r1,-64(r1) ; ... ; (no r31)      ; lwz r10,0(0)
   ours after        identical to retail here
   ```

4. **`CCubeRenderer::GetRealReflection` 76.11 -> 100.0**. Retail has **two** `blr`s and falls
   through to the `&mBlackTex` return: `bne L ; addi r3,r3,184 ; blr ; L: mr r3,r0 ; blr`. The
   ternary `p ? p : &mBlackTex` gives the opposite branch polarity and a shared tail
   (`beq ; b ; addi r0,r3,184 ; mr r3,r0 ; blr`). `if (p == nullptr) return &mBlackTex; return p;`
   through a local reproduces retail exactly, and the single `lwz r0,288(r3)` confirms one `get()`.

5. **`CCubeRenderer::DrawSpaceWarp` 91.92 -> 100.0**. Retail spells the test the other way round and
   never needs the negation: `fcmpo cr0,f2,f0 ; cror eq,gt,eq ; beq end`. `if (z < 1.f) { call; }`
   gives `fcmpo ; bge` (MWCC folds the negation into the branch). `if (z >= 1.f) return; call;`
   builds `>=` from `gt|eq` with the `cror`, which is retail's instruction.

6. **`CCubeRenderer::FindOrAddLightSet` 85.36 -> 100.0**. Retail branches **over** the append with
   `cmpwi r6,96 ; bge <li r3,0 ; blr>` — the success path is the fall-through and the `return 0` is
   the branch target at the end. `if (size >= capacity()) return 0;` as a *guard* emits
   `bne`/`blt` over it and gets the block order backwards. Inverting the test and trailing the
   `return 0` gives retail's shape.

7. **`CCubeRenderer::SetPerspective(float,float,float,float,float)` 84.62 -> 100.0**. Retail has
   one instruction we did not: `lfs f0,-29320(r13)` then `fmuls f2,f2,f0`, and
   `python3 tools/sda.py -29320` resolves it to `mPixelAspectRatio__9CGraphics` (`.sdata:0x80418AF8`,
   the word is `0x3f800000` = 1.0f). So the aspect argument is
   `width / height * CGraphics::GetPixelAspectRatio()`, not `width / height`. The four-argument
   overload, which takes an aspect directly, was already 100% and must not gain the multiply.

8-12. **`CCubeRenderer::BeginLines` / `BeginLineStrip` / `BeginTriangles` / `BeginTriangleStrip` /
`BeginTriangleFan`, all 48.90 -> 100.0** (five functions for one finding). Retail calls
`BeginPrimitive` **directly** — `bl 8026ee0c`, and 0x8026ee0c is `BeginPrimitive`'s own entry — while
ours emitted the vtable dance `lwz r12,0(r3) ; lwz r12,156(r12) ; mtctr r12 ; bctrl`. The wrappers
therefore suppress virtual dispatch, which the qualified call `CCubeRenderer::BeginPrimitive(...)`
does. This is the only spelling tried that MWCC turns into a direct `bl`; the plain call cannot.
`BeginPrimitive` is genuinely virtual in retail — the vtable slot at `0x9C` is the one our header's
`override` fills, and `__ct__13CCubeRendererFR12IObjectStore...` stays 97.06% with no change — so
the qualifier, not a header edit, is the finding. Note the vtable *contents* are zero in both
`orig/G2ME01/sys/main.dol` and our `build/G2ME01/main.elf` at `0x803B0C1C` (`lbl_803B0C1C`,
`.data`, size `0x140`), so the vtable is never compared by objdiff; the `__ct__` is the only
evidence for the slot order.

## `Buckets::Insert` is a wall — measured, with the spellings

WALL: `Buckets::Insert` 84.11% - retail's 7-instruction `dcbtct` push-back tail has no C++ spelling
found, and the dot-product load order is per-callsite scheduling.

Two things remain, after the local-pointer change removed the frame/allocation delta:

- **The `dcbtct` tail (retail 0x80272558-0x80272574, 7 instructions we do not emit at all):**

  ```
  addi  r4,r31,4          ; sData->begin()
  stfs  f0,4(r3)          ; (the sMinMaxDistance.second store, interleaved)
  lwz   r3,0(r31) ; addi r0,r3,-1 ; mulli r0,r0,36 ; add r3,r4,r0 ; addi r0,r3,36
  dcbtct 0,r0             ; touch &(*sData)[mCount] == end()
  ```

  `addi r4,r31,4` sits *before* the interleaved `stfs`, so `begin()` is computed early and used only
  by this block. `dcbtct` occurs exactly twice in the whole DOL (`grep -c dcbtct` over
  `objdump -d build/G2ME01/main.elf` = 2): here, and at `0x8036F030` inside `__LCEnable`, which is
  the SDK's hand-written `DCStoreRange` loop (`dcbtct 0,r3 ; dcbst 0,r3 ; addi r3,r3,32 ; bdnz`).
  So `dcbtct` is a *cache-prefetch* instruction, and nothing in this repo's `src/` or `include/`
  mentions it (`grep -rn dcbtct src include` = no hits) and neither does the MWCC 2.7 include tree
  the toolchain ships. I did not find the source construct that makes MWCC emit it, and I did not
  write it as inline asm — a transcribed `dcbtct` would score the bytes while decompiling nothing.

- **Load and operand order inside the inlined `CPlane::GetHeight`.** Retail loads
  `lfs f0,4(r3)` (pos.y) *before* `lfs f1,4(r7)` (normal.y) and folds as
  `fmadds f1,f1,f2,f0`; ours loads normal.y first and folds `fmadds f1,f2,f1,f0`. The products and
  the final value are identical, and `xsmaddmsp` (retail) vs `xxsel` (ours) at the f31 spill is the
  same class of difference. `docs/goal-notes/progress-cvector3d.md` already measured that
  `CPlane::GetHeight`'s `fmuls` operand order is chosen per inlining site by GCC 2.7 and cannot be
  read off the source expression, so the header is not at fault and I did not touch it.

## Also measured, not taken (no count to gain, or a shared header)

- **`CCubeRenderer::GetScreenMipInfo` 91.43 -> 95.83 with `width = (width & ~1) >> 1`.** Retail emits
  `clrrwi r30,r30,1 ; srawi r30,r30,1` (and the same for height) where `width >>= 1` gives a bare
  `srawi`. Spelling it `(width & ~1) >> 1` reproduces both pairs and the instruction count becomes
  42/42. It does **not** reach 100% — what is left is register allocation (retail puts the shifted
  width/height in r30/r29 and the untouched parameters in r23-r26; ours keeps width/height in
  r23/r24), which I could not steer from the source, and it does not raise `matched_functions`, so I
  reverted it to keep this diff to what the item needs. The spelling is recorded here so the next
  run starts from it.
- **`rstl::list<CFogVolumeListItem>::push_back` 75.00%** (32 bytes). Retail is
  `bl do_insert_before` with no argument setup at all, i.e. it passes `r3 = this` as the node
  pointer; ours does `mr r5,r4 ; lwz r4,8(r3)` first, i.e. it loads the tail node from `this+8`.
  Retail's node looks **embedded at offset 0** and ours is a **pointer at offset 8**. Fixing that
  means changing `include/rstl/list.hpp`, which is shared with already-matching units, so it was
  not attempted here.
- **`CCubeRenderer::GetStaticWorldDataSize` 70.33%** (60 bytes). Retail has a null check ours does
  not: `lwz r6,32(r4) ; cmplwi r6,0 ; beq` before dereferencing. Same shared-`rstl::list` cause.
- **`CCubeRenderer::SetWorldViewpoint` 87.54%** (112 bytes). Two independent deltas: the operand
  order inside the inlined `CUnitVector3f`/`CPlane` construction
  (`fmuls f0,f3,f0` vs ours `fmuls f0,f0,f3`, and `fmadds f0,f2,f1,f0` vs `fmadds f0,f1,f2,f0`),
  and ours materialising a 3-float temporary on the stack (`stfs f2,8(r1) ; stfs f3,12(r1) ;
  stfs f4,16(r1)`, frame 32 vs retail's 16) where retail writes straight through to
  `160/164/168(r30)`. Not a spelling I found.
- **`CCubeRenderer::RenderFogVolume` 85.43%** (148 bytes). Retail runs three extra `beq` around the
  `optional_object` validity test (`addic. r3,r1,88 ; beq ; lbz r0,100(r1) ; cmplwi r0,0 ; beq ;
  cmplwi r3,0 ; beq ; beq`) and uses a 128-byte frame to our 112. Not chased.
- **`Buckets::Sort` 0.31%** (1304 bytes) and **`Buckets::Init` 2.38%** (168 bytes) are still
  `TODO` stubs; they are the bulk of what is left in this unit's `Buckets` namespace.

## NEW

Nothing filed. Every function I moved is in the item's own target unit, so the work this item was
queued for is done; the two walls above are inside the same unit and are recorded here rather than
requeued, and a `rstl::list` layout question would raise no count in this unit and touches a shared
header.

## Reproducing

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
./tools/decomp_build.sh main/MetaRender/CCubeRenderer   # prints the per-function list
./tools/goal_check.sh build/goal/item.json
python3 .tmp/opencode/insdiff.py build/G2ME01/src/MetaRender/CCubeRenderer.o \
    Clear__7BucketsFv 0x80271B98 0x108
```

No commit was made. `docs/HANDOFF.md` was restored with `git checkout --` after `goal_check.sh`
(which runs `gate.sh` with `MP_GATE_DOCS_WRITE=1`) rewrote its state block; the diff is
`src/MetaRender/CCubeRenderer.cpp` only.
