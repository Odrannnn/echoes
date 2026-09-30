# progress-prime1-cmorphballshadow

`progress` on `MetroidPrime/Player/CMorphBallShadow`, from Prime 1's
`prime-ref/src/MetroidPrime/Player/CMorphBallShadow.cpp`. One file changed:
`src/MetroidPrime/Player/CMorphBallShadow.cpp`. `docs/HANDOFF.md`'s state block
was rewritten by the gate, not by hand.

## Measured, re-measured first

`build/report.json` for `main/MetroidPrime/Player/CMorphBallShadow` before the
change: `matched_functions 3`, `total_functions 19`, unit fuzzy 13.75%. Per
function: `AreasValid` 6.42%, `GatherAreas` 24.61%, `RenderIdBuffer` 10.65%,
`Render` 3.21% (the 3 matched were the ctor, dtor and the inlined
`CCubeRenderer::IsRGBA6Current`).

After: `matched_functions 5 / 19`, unit fuzzy 18.10%, `matched_code` 424 -> 692.
`All: 30.50% -> 30.52% fuzzy`, `matched 9908 -> 9910`, `linked 4895 -> 4895`
(gate's per-function diff: `+2 functions at 100%, 0 units newly linked`).

## Per function

| function | before | after | Prime 1's source |
|---|---|---|---|
| `GatherAreas` | 24.61% | **100.00%** | matched unchanged apart from two renames |
| `AreasValid` | 6.42% | **100.00%** | matched unchanged apart from two renames |
| `RenderIdBuffer` | 10.65% | 96.24% (reverted, see below) | needed 3 small edits, then a wall |
| `Render` | 3.21% | not attempted (see below) | - |

The two matches needed only: no `AUTO` macro in this tree, and
`CGameArea::GetAreaId()` is `GetId()` here. The class layout is identical to
Prime 1's (`CHECK_SIZEOF(CMorphBallShadow, 0xd4)` unchanged, members in the same
order), so nothing in the header needed touching. The retail bodies are a
literal transcription of Prime 1's, and the two functions are the smallest in
the unit, so they reproduce byte-for-byte under GC/2.7 with no tuning at all.

## RenderIdBuffer: what was measured, and why it is not in the diff

Prime 1's `RenderIdBuffer` adapted to Echoes reached **96.24%** and then hit a
wall. It is reverted, for a reason that has nothing to do with the percentage -
see "the port link gap" below. Spellings tried, all measured, newest first:

| change | fuzzy |
|---|---|
| Prime 1 verbatim (Echoes' `DrawOverlappingWorldModelShadows`, `CGX::SetDstAlpha`, no `renderer` local) | 87.90% |
| + `(aabb.GetMinPoint() + aabb.GetMaxPoint()) * 0.5f` instead of `aabb.GetCenterPoint()`, and `CGX::SetDstAlpha` for the first `GXSetDstAlpha` | 93.77% |
| + `CTransform4f::Scale(modelData.GetScale())` instead of `Scale(CVector3f(modelData.GetScale()))` | 95.36% |
| + `&& actor->TakesProjectedShadow()` in the actor loop | **96.24%** |
| `const CVector3f scale = modelData.GetScale();` as a named local | 94.47% |
| `CCubeRenderer& renderer = *gpRender;` and every `gpRender->` written `renderer.` | 93.01% |
| `const CModel& model` declared before `const CModelFlags flags` | 94.27% |
| `CCubeRenderer* const renderer = gpRender;` and every `gpRender->` written `renderer->` | 92.22% |
| inlining `0.99f * depth` instead of the `clearDepth` local | 95.36% (no change) |
| `const float clearDepth` moved above `BeginTriangleFan` | 94.33% |

Three facts a next run needs, all measured against retail's
`build/G2ME01/main.elf`:

1. **Echoes' `RenderIdBuffer` is Prime 1's with three changes**, not a rewrite:
   `renderer->DrawOverlappingWorldModelShadows` where Prime 1 has
   `DrawOverlappingWorldModelIDs` (the `FindOverlappingWorldModels` call is in
   both, and the retail `DrawOverlappingWorldModelShadows` takes three
   parameters - `r7`/`r8` are set to 0 immediately before the call but the
   mangled name has no fifth or sixth parameter, so they are dead);
   the box centre is computed inline, **not** through `CAABox::GetCenterPoint()`
   (retail `fmuls`es `0.5f*(min.x+max.x)` and `0.5f*(min.y+max.y)` in place and
   takes z from `aabb.GetMaxPoint()`), which is why
   `(aabb.GetMinPoint() + aabb.GetMaxPoint()) * 0.5f` is the spelling that
   matches; and the actor test has **one extra clause** (below).
2. **The extra actor clause is `CActor::mTakesProjectedShadow`.** Retail emits
   `lwz r3,12(SP); lbz r0,340(r3); rlwinm. r0,r0,28,31,31; beq` between
   `CanDrawStatic()` and `mActors.push_back`. Byte 0x154 bit 4 of `CActor`.
   Confirmed by compiling a standalone probe with this tree's exact
   `mwcceppc.exe` (GC/2.7) and flag set: mwcc allocates bitfields **LSB-first,
   contiguously across the 32-bit boundary** (no re-alignment), and this repo's
   `CActor` bitfield list puts `mTakesProjectedShadow` at 0x154 bit 4 -
   `mEchoEmitterEnabled` at bit 0, `mTakesProjectedShadow` at bit 4,
   `mAlphaSorted` at bit 7 of the same byte. The same probe reproduces retail's
   `IsRGBA6Current` (`lbz r0,1364(r3); rlwinm r3,r0,28,31,31` -> `mCurrentRGBA6`,
   the 4th of 8) exactly, so the rule is trustworthy. **There is no public
   accessor** for it; the body needs one added to `include/MetroidPrime/CActor.hpp`
   (an inline getter does not move any offset).
3. **The last ~4% is mwcc's stack-temp slot order and register allocation, not
   source.** Both compile to a 2672-byte frame, and the call sequence matches
   retail instruction-for-instruction. Retail allocates the `CModelFlags`
   temporary at `SP+16` (ahead of the four `PrimVertex` `CVector3f`s at
   40/52/64/76) and we allocate it at `SP+104`; retail keeps `gpRender` in `r25`
   for the whole function (`stmw r21,2516(SP)`, `mr r23,r6` for `player`) and we
   reload it from SDA at each use (`stmw r22,2520(SP)`, `mr r24,r6`). Four
   attempts to move the allocation by reordering declarations or hoisting
   `gpRender` into a local all made it *worse* (table above). This is where it
   stops.

## The port link gap - why RenderIdBuffer is not in the diff

Implementing `RenderIdBuffer` makes the port ask the linker for nine symbols
nothing in the tree defines, so `tools/probe_sources.sh`'s strict regression
gate fails and the whole gate fails with it:

```
port probe  ok -> probe: 749 files, 0 failed, 0 errors; link: NOT LINKED (259 undefined, 0 duplicates)
    link_check: STRICT FAIL - regression gate: 259 undefined against a baseline of 250 (GREW)
    link_check: 9 symbol(s) this change ADDED to the gap:
      NEW  CCubeModel::SetRenderModelBlack(bool)
      NEW  CCubeRenderer::DrawOverlappingWorldModelShadows(int, rstl::vector<unsigned int, rstl::rmemory_allocator>&, CAABox const&)
      NEW  CCubeRenderer::FindOverlappingWorldModels(rstl::vector<unsigned int, rstl::rmemory_allocator>&, CAABox const&)
      NEW  CGraphics::GetProjectionState()
      NEW  CGraphics::SetProjectionState(CGraphics::CProjectionState const&)
      NEW  CGraphics::mRenderModeObj
      NEW  CModel::DrawUnsortedParts(CModelFlags const&) const
      NEW  CTexture::GetBitMapData(int)
      NEW  CTexture::UnLock()
```

`docs/research/port_link_baseline.txt` is a file this lane may not edit, so the
count cannot be re-baselined. `python3 tools/link_gap.py --write-list` does add
the nine to `docs/research/port_link_gap_list.md` and `port link gap` then goes
`ok`, but `port probe` still fails on the 250 -> 259 regression. A `progress`
item therefore **cannot** land `RenderIdBuffer`'s body: it adds 0 to
`matched_functions` (96% is not 100%) and it costs the gate. It needs its
port-side callees defined first, in one change. Note the two are not
independent: until `RenderIdBuffer` calls them, those nine symbols are not in the
port's undefined list at all, so a `port` item cannot be written for them
either - the judge requires the target to have been undefined at the branch head.

One incidental port finding: `CMaterialFilter::skPassEverything` is **private**
(`include/Collision/CMaterialFilter.hpp:9`); the public accessor is
`CMaterialFilter::GetPassEverything()`. mwcc 2.7 does not enforce access, so the
matching build accepted the private name and the host build rejected it. Any
future body that passes the pass-everything filter must use the accessor.

## Render (1484 bytes) not attempted

It is Prime 1's `Render` body with Echoes' third parameter
(`const CTexture& shadowTexture` in place of reading `mTexture`), and Echoes
drops the `renderer.GetSphereRamp()` line Prime 1 has. It calls the same family
of CGX/GX/`CModel`/`CLight` methods, so it hits the identical port-gap wall:
landing it needs the same port-side definitions. It also cannot raise
`matched_functions` unless it reaches 100%.

## Gates

`./tools/gate.sh build/report.base.json` -> `GATE PASS a2a78c5+2 changed`
(`matched 9908 -> 9910`, `linked 4895 -> 4895`, `+2 functions at 100%, 0 units
newly linked`, everything else `ok`). `python3 tools/check_symbol_names.py` ->
`checked 503 units; 0 declared names are missing from their object`.
`python3 tools/check_decl_order.py --unit MetroidPrime/Player/CMorphBallShadow.cpp`
-> ok. `main.dol` sha1 and all 86 REL hashes unchanged (the gate's
`ninja + build.sha1` and `hashes vs config.yml` rows). No assembly added.

WALL: RenderIdBuffer 96.24% - reaches retail's call sequence and frame size, but mwcc allocates the CModelFlags temp and gpRender differently and four reorderings all regressed
