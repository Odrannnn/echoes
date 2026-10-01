# progress-unit-cfluidplanecpu — `MetroidPrime/CFluidPlaneCPU`

`kind: progress`. The unit stays `NonMatching`; the bar is `matched_functions` rising strictly on
`build/report.json`, the whole gate clean, no function anywhere worse, and no asm added.

## Measured

Unit `main/MetroidPrime/CFluidPlaneCPU`, `tools/fast_try.sh MetroidPrime/CFluidPlaneCPU`:

| | before | after |
|---|---|---|
| matched_functions | **8 / 17** | **9 / 17** |
| matched_code_percent | 2.63 | 5.94 |
| fuzzy_match_percent | 12.78 | 16.05 |

`./tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS progress-unit-cfluidplanecpu`**
(`target rose: 8 -> 9 / 17`, `counts: matched 11758 -> 11759   linked 5727 -> 5727`, gate.sh ok,
`check_symbol_names.py` ok, `All: 33.41% fuzzy, 26.43% matched, 12.64% linked (11759 / 28465)`,
no asm added). The whole gate includes the DOL sha1, all 86 REL hashes, the per-function report diff
and the port probe (`744 files, 0 failed; link: LINKED (324 undefined, 0 duplicates)`).

## What landed

**`RenderCleanup__14CFluidPlaneCPUCFv` (328 B): 1.22% → 100%.** It is the only function in the unit
that is nothing but library calls, and retail's body is fully readable at `0x80131ff8..0x80132140`:
seven `CGX::SetTexCoordGen(GX_TEXCOORD0..6, GX_TG_MTX3x4, GX_TG_TEX0..6, GX_IDENTITY, GX_FALSE,
GX_PTIDENTITY)`, `CGX::SetTevDirect(GX_TEVSTAGE0/1/2)`, `CGX::SetNumIndStages(0)`,
`CGX::ResetVtxDescv()`, `CGX::SetChanCtrl(...)`, `CGX::SetNumChans(1)`,
`CGraphics::SetLightState(CGraphics::GetLightMask())`, `GXSetCullMode(GX_CULL_FRONT)`, all behind
`if (!gkWaterEnable) return;`.

Two things had to be fixed before the last 4 bytes closed, and both are recorded because they are
general:

1. **`li r3,1` vs `li r3,0` for the `SetChanCtrl` channel.** 91.45% → 99.99% → 100%. Retail passes
   `GX_COLOR1` (`li r3,1` at `0x801320f8`), not `GX_COLOR0`; Prime 1's donor gets this wrong too
   (`prime-ref/src/MetroidPrime/CFluidPlaneCPU.cpp:1165` says `CGX::Channel1` there — but a port
   that guessed `Channel0` would land on 99.99% and look done). `RenderCleanup` also has **three**
   `SetTevDirect` calls (stages 0,1,2), not Echoes' two (stages 3,6), and ends in
   `GXSetCullMode(GX_CULL_FRONT)` that Echoes does not have at all.

2. **`gkWaterEnable` cannot be defined in the translation unit that reads it.** Retail's guard is
   `lbz r0,-27632(r2); cmplwi r0,0; beq <end>` at `0x80132004` — an SDA2 read of the byte at
   `.sdata2:0x8041B7D0`, whose value is 1:
   `objdump -s -j .sdata2 --start-address=0x8041B7D0 --stop-address=0x8041B7D4 build/G2ME01/main.elf`
   → `8041b7d0 01010000 00000000 40800000 3ecccccd`. mwcceppc **constant-folds** a `const` whose
   definition it can see: with `const bool gkWaterEnable = true;` in `CFluidPlaneCPU.cpp` the whole
   guard vanishes and the function sits at 91.45%. Retail defines the flag in `CFluidPlaneManager.cpp`,
   a different unit, which is why retail does not fold it. This repo has no `CFluidPlaneManager`
   unit, so the definition went into a new port-only TU, `src/MetroidPrime/CFluidPlaneManagerGlobals.cpp`
   (in `files.cmake`, **not** in `configure.py`, so the DOL objects are unchanged), and the flag is
   declared `extern const bool gkWaterEnable;` in `include/MetroidPrime/CFluidPlaneCPU.hpp`.

## The port-side definition the decompilation forced

With the body in place the judge's `gate.sh` failed on **one** step:

```
link_check: STRICT FAIL - regression gate: 325 undefined against a baseline of 324 (GREW)
GATE FAIL: probe link-gap
gap grew: _ZN9CGraphics13SetLightStateEh is not in port_link_gap_list.md
```

`CFluidPlaneCPU::RenderCleanup` is the first thing in this tree that calls
`CGraphics::SetLightState`, and the port has no definition: `src/Kyoto/Graphics/DolphinCGraphics.cpp:574`
has retail's body but that unit is **excluded** from `files.cmake` (4 non-local compile errors; see
`tools/check_files_cmake.py`'s `EXCLUDED`). The count must not grow — `docs/research/port_link_baseline.txt`
is judge-owned and may not be re-recorded — so the symbol had to become *defined*, and it is defined
from retail's own body in `src/Kyoto/Graphics/CGraphicsStreamState.cpp`, the port-only file that
already owns the vertex descriptor it reads (`sVtxDescr` = retail `lbl_80416EE0`; `DolphinCGraphics.cpp`'s
file-local `vtxDescr` is the same state). `CGuiFrame.cpp:148` and `CElementGen.cpp:2512` call
`SetLightState` too and are likewise outside the port build.

Re-measured after: `probe: 744 files, 0 failed, 0 errors; link: LINKED (324 undefined, 0 duplicates)`.

**The general lesson: on a `progress` item in this repo, decompiling a function that calls a game
method the port does not build fails the gate even when the function itself matches 100%.** Measure
the unit's retail relocations first (`build/binutils/powerpc-eabi-objdump -r build/G2ME01/obj/<unit>.o`)
and diff the new symbols against `build/goal/judge/undef.base.txt` before choosing a target.

## What is left, and what each one costs

Unchanged by this run (all measured, none regressed):

| function | bytes | % | why not |
|---|---|---|---|
| `fn_8013446C` | 60 | 0.00 | already emitted by our object, byte-identical, at `.text+0xbf4` — as the **local** symbol `uninitialized_copy<rstl::pointer_iterator<CVector3f,...>, CVector3f*>`. objdiff matches functions by symbol name, so retail's `fn_8013446C` scores 0.00% and no source change can rename a template instantiation. Nothing to do here. |
| `__ct__` | 1272 | 77.20 | retail resolves `gpSimplePool->GetObj` through the **vtable** (`lwz r12,0(r4); lwz r12,12(r12); mtctr r12; bctrl` at `0x80134094`), i.e. `CToken::GetObj` is virtual in retail; this tree calls it non-virtually (`GetObj__6CTokenFv`), and the surrounding `TLockedToken` lock/unlock sequence differs too. |
| `CalculateLightmapMtx` | 832 | 0.48 | needs `lbl_8041C050`, `lbl_8041C058` (retail `.sdata2` constants) and `fmod`. |
| `ClipPolygonToPlane` | 440 | 0.00 | needs `lbl_8041C02C` (the `0.0f` it compares against). Structurally the source is already right; see below. |
| `Render` | 420 | 0.95 | needs `lbl_8041C02C`, `GXSetVtxAttrFmt`, `TCastToPtr<12CScriptWater>__FP7CEntity`, `GetObjectById__13CStateManagerCF9TUniqueId`, and the EFB-mirror writes at `0xCC008000`. |
| `RenderDistortion` | 1948 | 0.21 | ~20 `lbl_8041C0xx` constants plus `GetUseVideoFilter`, `GXCopyTex`, `kSpareBufferTexMapID`, … |
| `RenderSetup` | 3868 | 0.10 | the largest; `sRenderer__13CCubeRenderer`, `GetNextConnectedWater__12CScriptWaterCFRC13CStateManager`, `FastCosR__5CMathFf`, `CalculateFluidTextureOffset__14CFluidUVMotionCFfPA2_f`, `GXSetIndTexOrder`, … |
| `UpdateGridDisplayList` | 476 | 0.84 | needs `ScheduleDeletion__19CFrameDelayedKiller…`, `__ct__10CCallStack…`, `Alloc__7CMemoryF…`, `lbl_803A9170`, `kUnknownType__10CCallStack`. |

**None of `lbl_8041C02C / 028 / 030 / 034 / 038 / 03C / 040 / 044 / 048 / 04C / 050 / 058`,
`GXSetVtxAttrFmt`, `TCastToPtr<12CScriptWater>__FP7CEntity` or
`GetObjectById__13CStateManagerCF9TUniqueId` appears in `build/goal/judge/undef.base.txt`** (grep,
0 hits each), so every one of those functions fails the gate today for exactly the reason
`SetLightState` did — until each is *defined* port-side. They are retail `.sdata2` bytes, so the
arrangement `src/Kyoto/Graphics/CGraphicsHostScene.cpp` already uses for `lbl_80419928` &c. is the
one to copy: `extern "C" const float lbl_8041C02C;` in the reader, the value port-side. Read the
value out of the DOL (`objdump -s -j .sdata2`), never from memory. **This is the first thing to do
before any further attempt on this unit**; until then every remaining function is blocked on the port,
not on the decompilation.

### `ClipPolygonToPlane` — the one worth attacking next, and why it is not done

The body was read instruction by instruction (`0x8013399c..0x80133b54`) and the *current source is
already the right algorithm*: `da = -plane.GetHeight(a)`, `db = -plane.GetHeight(b)`, push `b` when
both are `>= 0`, else push `a + (da/(da-db))*(b-a)` and then `b` only when `da < 0 && db >= 0`.
Three concrete codegen gaps remain, all measured:

* **The `%`.** Retail computes `polygon[(i+1) % n]` branchlessly as
  `xoris r10,r0,0x8000; subf r4,r0,r8; addc r4,r4,r10; subfe r4,r4,r4; andc r4,r8,r4; mulli r4,r4,12`
  (`0x801339c0..0x80133a0c`) — the signed-modulo-by-variable lowering. Our source, with the same
  `(i + 1) % polygon.size()`, gets `divw`/`mullw`/`subf` instead, which is why our function is
  728 bytes against retail's 440. Retail's loop is also a `mtctr`/`bdnz` countdown; ours is a
  bottom-tested `for`. **Spellings not yet tried:** hoisting `const int n = polygon.size();` and
  writing the index as `(i + 1) % n`; writing it as `(i + 1 == n) ? 0 : i + 1`; and iterating with
  an explicit `int n` countdown. This is the wall to push on.
* **`CPlane::GetHeight`.** Retail's `da` is `fneg(dot); fadds f31,f1,pw` (`0x80133a2c/0x80133a34`),
  ours is `fsubs f0,dot,pw; fneg f26,f0`. Same value, different association — retail's
  `GetHeight` is not this header's. Changing `CPlane` is a shared-header edit and was deliberately
  left alone.
* **`lbl_8041C02C`**, above.

## Files touched

- `src/MetroidPrime/CFluidPlaneCPU.cpp` — includes `CGX.hpp`/`CGraphics.hpp`/`<dolphin/gx.h>`;
  `RenderCleanup` written out in full (was a TODO stub).
- `src/MetroidPrime/CFluidPlaneManagerGlobals.cpp` — **new**, the `gkWaterEnable` definition.
- `include/MetroidPrime/CFluidPlaneCPU.hpp` — `extern const bool gkWaterEnable;`.
- `files.cmake` — one line for the new TU.
- `src/Kyoto/Graphics/CGraphicsStreamState.cpp` — `CGraphics::SetLightState` from retail, plus the
  `CGX.hpp` include and the file's header comment corrected from "three" to "four".

Side effect worth knowing: including `Kyoto/Graphics/CGX.hpp` costs this object **4 bytes of
`.sdata2`** it did not have before — `CGX::apply_fog`'s function-local
`static const GXColor black = {0,0,0,0}` (`include/Kyoto/Graphics/CGX.hpp:191`) is emitted as a weak
symbol in every TU that includes the header. `tools/unit_fit.sh` reports it
(`.sdata2 claimed - ours 8`); it costs nothing while the unit is `NonMatching` and the object is not
in the link, but a future flip of this unit has to account for it.

## NEW:

None filed. Every remaining function is blocked on the *port* growing no new undefined symbols, not
on a fresh decompilation idea, and `NEW:` items must name a symbol that is undefined *now* — none of
`lbl_8041C0xx` are. Spelling them here is the cheaper record.