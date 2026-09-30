# progress-prime1-cmodeldata-l8 — `MetroidPrime/CModelData`

`kind: progress`. The unit stays `NonMatching`; the flip is out of reach (49 functions, 15 of
them the `Render*`/`DisintegrateDraw`/`AdvanceAnimation` pool characterised in the previous
runs). Every number below was measured in this worktree; none is recalled.

Re-measured first, as the brief requires: on the clean tree the unit was **32 / 49** matched
(fuzzy 60.99%, matched code 47.49%), global `matched_functions` 10428, `linked` 5048,
`All: 31.66% fuzzy, 24.23% matched (10428 / 28465)`. Nothing here was `STALE:`.

## Result

* `main/MetroidPrime/CModelData` **32 -> 34** of 49 matched (fuzzy 60.99% -> 64.22%, matched
  code 47.49% -> 50.84%).
* New unit `main/Kyoto/Graphics/Carve80310E8C`, **3 / 3** matched and **`Matching`** — the
  unclaimed gap this item named is now our own object, in the link.
* Global `matched_functions` **10428 -> 10433**; `linked` **5048 -> 5051** (the carve).
* `All: 31.67% fuzzy, 24.24% matched, 11.84% linked (10433 / 28465 functions)`.
* `total_functions` still **28465** — the carve moves three functions out of dtk's
  `auto_03_80310E8C_text` placeholder into a named unit and adds none.
* `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`** (gate.sh green: DOL
  sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs, per-function report diff,
  module wiring, docs claims, port probe, raw offsets, decl order, files.cmake; "counts:
  matched 10428 -> 10433   linked 5048 -> 5051"; "target rose: 32 -> 34 / 49";
  "no asm added").
* `tools/probe_sources.sh` -> `753 files, 0 failed, 0 errors`. `tools/link_check.sh` ->
  `compile errors 0`, `250 undefined`, `0 duplicates`, **unchanged from baseline** — the two new
  calls add nothing undefined because the carve defines both symbols for the port too.
* `tools/unit_fit.sh Kyoto/Graphics/Carve80310E8C.c` -> `claimed 172, ours 172, retail 172,
  fits`, no extra functions. `MetroidPrime/CModelData` still reports the same 4 pre-existing
  extra template destructor copies the baseline had.
* `python3 tools/check_symbol_names.py` -> `505 units; 0 declared names are missing`.
* `python3 tools/check_decl_order.py` -> `973 unit(s) checked, 31 permuted, all 31 accounted
  for`. `python3 tools/check_raw_offsets.py` -> `161 raw-offset site(s) in 68 file(s), all
  documented` — **the carve adds zero raw-offset sites** (see below).

Diff, five files, one carve's worth of wiring plus the two functions:

    config/G2ME01/splits.txt                       +3   the new claim
    configure.py                                   +1   Object(Matching, "Kyoto/Graphics/Carve80310E8C.c")
    files.cmake                                    +1   listed for the port build
    src/Kyoto/Graphics/Carve80310E8C.c            new   3 functions
    src/MetroidPrime/CModelData.cpp              +36   two `extern "C"` declarations, two bodies

## The carve: `Kyoto/Graphics/Carve80310E8C.c`, 0x80310E8C..0x80310F38

All four files moved together, as a carve must. `configure.py` says `Matching` **because
`tools/flip_test.sh Kyoto/Graphics/Carve80310E8C.c` printed `PASS -> kept as Matching`**, not
because it looked right; the DOL sha1 is the check that backs it.

```
fn_80310E8C  0x80310E8C  0x7C
fn_80310F08  0x80310F08  0xC
fn_80310F14  0x80310F14  0x24
```

* **`fn_80310E8C`** — raise `CModel`'s `x30_16_` flag, then `UnlockTextures` every material set:
  `lbz r3,0x32(r3) / rlwinm. r0,r3,25,31,31 / bne` early-out, `rlwimi r3,r0,7,24,24` sets it,
  then a loop bounded by `mMatSets.mCount` (0x1C) walking `mMatSets.mItems` (0x24) in 0x20
  strides calling retail's own `UnlockTextures__Q26CModel7SShaderFv`.
* **`fn_80310F08`** — `lwz r3,0x28(r3) / addi r3,r3,0x20 / blr`: `&mModelInstance->mBounds`.
* **`fn_80310F14`** — this is `CModel::IsDefinitelyOpaque`: `li r3,0` is hoisted, `beqlr` when
  there is no cube model, then the word at **`CCubeModel`+0x3C**, which by
  `CHECK_SIZEOF(CCubeModel, 0x50)` is `mFirstSorted.mData` — the *alpha* surface list. "No alpha
  surfaces" is why the name means what it says.

### Facts worth keeping (all measured here)

* **The 0x32 bitfield byte is `x30_16_` (index 0) and `mHasSkinMatrices` (index 1),** pinned by
  retail's own readers rather than by guesswork: `CModel::VerifyCurrentShader` (0x803115F8) and
  `CModel::SetupSkinMatrices` (0x80311954) test *that same byte* with
  `rlwinm. r0,r0,25,31,31` and `rlwinm. r0,r0,26,31,31`. That is the `SH = 25 + i` ladder the
  earlier runs recorded for `CModelData`'s flag byte, and it is why the header's
  `uint mCurrentMatxIdx : 16; uint x30_16_ : 1; uint mHasSkinMatrices : 1;` layout is right.
* **mwldeppc did not keep the object's `.text` verbatim here, contrary to the standing
  warning.** The three bodies were declared ascending first time; the object came out
  permuted (`fn_80310F14` at `.text+0x0`) and `check_decl_order.py --unit` flagged it — yet
  `tools/dol_read.py 80310E8C 32` on the relinked `main.dol` returned retail's bytes and the
  sha1 never moved. Re-declared descending, the object emits `fn_80310E8C`, `fn_80310F08`,
  `fn_80310F14` at `+0x0`, `+0x7C`, `+0x88`, the checker is clean, and the hash still holds.
  **Descending is still the rule** — one link is not a proof about a linker, and objdiff and
  `unit_fit.sh` are both blind to a permutation — but the failure mode is worth knowing: for
  this unit a permutation would not have broken the hash.
* **MWCC's C mode (`-lang=c`) has no `bool`.** `<stdbool.h>` does not fix it either
  (`-i libc -gccinc`, still `declaration syntax error` at the `bool`). So the carve returns
  `int` and `MetroidPrime/CModelData.cpp` declares `extern "C" bool fn_80310F14(CModel*)`.
  That is forced, not lazy: retail's `IsDefinitelyOpaque` does `bl fn_80310F14 / b epilogue`
  and hands r3 straight back, so any `int` on the C++ side would add a `cmpwi`/`srwi`
  `bool` normalisation that retail does not have. `extern "C"` mangles neither, so the linker
  does not care. `TLockedToken::operator*() const` returns `T*`, so passing
  `*PickStaticModel(which)` to a `CModel*` parameter needs no `const_cast`.
* **`for (EWhichModel which = ...; which <= kWM_Echo; which++)` does not compile** —
  `illegal operand 'CModelData::EWhichModel'`. The loop variable has to be `int` with
  `static_cast< EWhichModel >(which)` at the call, which is a no-op under `-enum int`.
* **The carve has no raw-offset sites at all**, so `docs/research/raw_offsets.md` did not need
  a section and still reads `161 raw-offset site(s) in 68 file(s)`. That is by construction,
  not luck: the offsets live in *array extents* (`char x00[0x1C]`) and in named members of two
  local mirror structs (`SCarveModel`, `SCarveCubeModel`), which is the shape
  `check_raw_offsets.py` does not key on. Worth copying into the next carve.
* **`UnlockTextures__Q26CModel7SShaderFv` is a literal symbol name, not a mangled one.** MWCC
  emits GameCube-style mangling (`Draw__6CModelCFRC11CModelFlags`), which is what
  `nm build/G2ME01/main.elf` prints, so a `.c` file declaring that identifier verbatim is the
  correct way to call it.

## The two `CModelData` functions: 1.92% and 6.36% -> 100.00%, first spelling

| function | before | after |
|---|---|---|
| `IsDefinitelyOpaque__10CModelDataCFQ210CModelData11EWhichModel` | 6.36 | **100.00** |
| `LockTextures__10CModelDataFv` | 1.92 | **100.00** |

Both took 100% on the first spelling; no `tools/try_edit.py` run was needed and none is
recorded, so nothing was spent that a later lane would have to repeat.

* **`IsDefinitelyOpaque`** is the two-arm selector: `mAnimData.null()` ->
  `*PickAnimatedModel(which).GetModel()`, else `mNormalModel` ->
  `*PickStaticModel(which)`, else `false`. The second test is `lbz r0,0x28(r3)`, i.e.
  `optional_object::m_valid` of `mNormalModel`, which is the same two predicates
  `GetNumShaders` uses (`src/MetroidPrime/CModelDataModelSlots.cpp` already records that
  `0x28` is that member's flag).
* **`LockTextures`** early-returns on `mTexturesLocked` (the `rlwinm. r0,r4,26,31,31` /
  `rlwimi r4,r0,6,25,25` pair on the 0x14 flag byte), sets it, calls `GetNumShaders` **once**
  before choosing a selector, then runs `which` 0..2 and, inside that, `shader` 0..n-1 calling
  `fn_80310E8C`. `cmpwi r,2 / ble` is `<=`, i.e. `<= kWM_Echo`, not `< 3` written differently.
* `*PickAnimatedModel(which).GetModel()` — the `.GetModel()` is free (it returns a reference)
  and is what turns `CSkinnedModel&` into the `TLockedToken<CModel>&` the selectors' callers
  actually mean; without it the `lwz r3,8(r3)` lands on the wrong member.

## Host arms, and why they are stand-ins rather than the same code

`TARGET_PC` is not defined by `configure.py` (grepped), so the carve's bodies are the DOL's and
the `#else` arm is what the port compiles. On a 64-bit host `mMatSets`/`mModelInstance` are not
at 0x18/0x28, and both `CModelData` methods **are** reachable from port code
(`src/MetroidPrime/Player/CGrappleArm.cpp:127,129` and `src/MetroidPrime/CActor.cpp:610`), so
the retail offsets would dereference a wild pointer. The host arms therefore return 0 / do
nothing — which is exactly what those two `CModelData` methods already did, so port behaviour
is unchanged, and the comment in the file says so rather than pretending.

This is the same hazard that got run 2 of the parent item rejected, so it is worth being
explicit: nothing in `src/MetroidPrime/CModelData.cpp` casts a host object to a 32-bit-offset
struct any more. `GetNumShaders` and `Touch` keep their existing `TARGET_PC` accessor arms; the
new code only calls the two carved entry points, so it is layout-agnostic on the host and
byte-exact on the DOL.

## Not attempted, and why

* **`fn_80310F08`** is now ours and matching but has no caller in this tree yet, so nothing
  names it. That is worth a note rather than an item: it is `&mModelInstance->mBounds`, i.e.
  retail has a `CModel` accessor returning the bounds pointer that no `CModelData` path uses.
* **`DisintegrateDraw` (1.28%), `RenderSolid` (1.37%), `RenderNoise` (1.37%),
  `RenderModelMultipleTimesWithFlags` (1.02%), `MultipassDrawCallback` (2.22%),
  `SetupWorldSpacePortalPlane` (3.03%), `SetEchoModel`/`SetDarkModel` (0.74%),
  `__ct__FRC8CAnimRes` (29.03%), `AdvanceAnimation` x2 (33.89 / 79.81)** — untouched. Their
  blockers are the ones the previous two runs measured (`IRenderer::DrawModelDisintegrate`'s
  `const CModel&` vs retail's 16-byte context struct; `CAnimData`'s different `Advance`
  signature; four missing symbols for the `CAnimRes` constructor) and this item did not move any
  of them. Re-asserted here without re-measuring: `IsDefinitelyOpaque` and `LockTextures` were
  the only two on this unit that a carve could unblock.
* `Render` (98.43%), `Touch__Fv` (98.21%), `GetIsLoop` (62.50%), `RenderParticles` (90.91%) —
  measured walls from the previous runs, untouched and un-re-tried. No `WALL:` line is written
  for them: they were not measured in this run, and copying an old wall is what the brief
  forbids.

## NEW: items

None. The carve that was this item's own deliverable is the only thing that raised a count
here, and it is landed. `fn_80310F08` has no caller in this tree, so naming an item on it would
be a guess about what would consume it.

`NEW: progress-prime1-cmodeldata-l9` from run 3 (`IRenderer::DrawModelDisintegrate`'s
declaration) is untouched and still valid — this run measured nothing about it.
