# progress-prime1-dolphincgraphics

Unit main/Kyoto/Graphics/DolphinCGraphics: 96 -> 98 / 102 matched (build/report.json via decomp_build.sh).
`tools/goal_check.sh build/goal/item.json`: PASS (matched 12450 -> 12452, gates clean, no asm).
Unit stays NonMatching; flip_test not run.

Per function (before% -> after%, Prime 1 source usefulness):
- TexRegionCallback: 92.73 -> 100. Prime 1's source (int statics, `!= C4 && != C8 && != C14X2`) alone was
  worse (51.8%): retail has char statics and calls GXGetTexObjFmt first. Fix = keep our char statics /
  fmt-first shape but write the C-format test as `!= && != &&` (positive `==` form let MWCC merge
  C4/C8 into an addi/cmplwi range) and put the non-CI path first. Small edit.
- StreamBegin: 69.96 -> 100. Retail calls GetLockedCacheAllocationBase() once, not LCGetBase() (Prime 1
  uses LCGetBase, did not help). Declare `uchar* lcBase = GetLockedCacheAllocationBase()` as a local;
  VTX_BUFFER_ADDR now expands to it (calling it in the macro emits five calls: 54.8%). Adds
  `#include "Kyoto/Alloc/LockedCache.hpp"`.
- LoadLight: already 100% at measurement time (item reason was stale).
- CalculatePerspectiveMatrix: 96.42 -> 96.42 (unchanged). Tried Prime 1's `CMatrix4f mtx(...); return mtx;`
  = 91.31%; reordering top/right/bottom/left declarations = 92.84%. Remaining diff is only FPR
  allocation (f7/f8/f9 swap) and one fmr placement.
  WALL: CalculatePerspectiveMatrix 96.42% - only FPR allocation/scheduling differs after 3 spellings.
- EndScene: 77.14 -> 81.71 (Prime 1's EndScene is a different function, no use). Done: `if (sGXAborted)
  GXAbortFrame(); else {...}` branch order, address-held `int&`/`bool&` locals for the wait loop,
  `const bool interrupts = OSDisableInterrupts() != FALSE`. Prologue now only differs in frame size
  (retail 272, ours 288) and the 96-triangle loop. Retail's loop is 32 iterations x 3 triangles, every
  store kept; ours (GXWGFifo is non-volatile under __MWERKS__) is collapsed by MWCC to an empty loop
  plus one store (`li r0,12; mtctr; bdnz self`). Tried 32x3 nested loop: still collapsed. Not solved;
  the loop needs a spelling that stops MWCC sinking the same-address stores while still unrolling 3x.
  This also explains the 16-byte frame difference in the tail (int->float temporaries are placed
  differently after the loop).
- FullRenderWithVertexDelay (87.7%), ClipScreenQuadFromVS (59.9%): not touched.

---

# Run 2 (lane 5, branch tip 0808f66c; the state block already carried run 1's two matches)

Re-measured on this tree first: `main/Kyoto/Graphics/DolphinCGraphics` was 98/102 matched,
fuzzy 95.37841, with 4 functions below 100% - CalculatePerspectiveMatrix 96.42% (296 B),
EndScene 81.08% (1468 B), FullRenderWithVertexDelay 87.73% (1260 B),
ClipScreenQuadFromVS 59.93% (1484 B). The item reason was stale: it named StreamBegin and
TexRegionCallback, which run 1 had already taken to 100%.

Result: **98 -> 99 / 102 matched**, unit fuzzy 95.37841 -> 97.20185, global matched
12465 -> 12466, DOL 10917 -> 10918. `tools/goal_check.sh build/goal/item.json`: **PASS**
(gate.sh clean, no function worse anywhere, no asm added). Unit stays NonMatching; flip_test
not run (a progress item).

## The one that reached 100%: FullRenderWithVertexDelay 87.73% -> 100.00%

Retail loads a vertex with **both** forms in the same breath:
`lwz r6,0(0)` / `add r5,r6,r3` / **`lfsx f0,r6,r3`** / `lfs f2,8(r5)` / `lfs f1,4(r5)`.
The `lfsx` is an *indexed* load off the buffer base plus the byte offset; the other two are
displacement loads off the materialised address. Our build produced only displacement loads.
The lever is the named reference: `const Vec& vtx = mVertexBuffer[i];` made MWCC materialise the
element address once and use it for all three components. **Deleting the local and subscripting
the buffer inline** - `GXPosition3f32(mVertexBuffer[i].x, mVertexBuffer[i].y, mVertexBuffer[i].z);`
in all eight switch cases - reproduces the `lfsx` and the rest of the function byte for byte
(0 differing instructions over 315). No other change; the `const Vec*`/pointer/float*-pointer
spellings all made it worse (see the list below).

**Generalisable: a `const T& local = array[i];` binding is not codegen-neutral on this compiler.**
When retail's object shows one indexed load (`lfsx`/`lwzx`) next to displacement loads of the
same struct, the source almost certainly had no named binding. Cheap to test, and it is the
difference between 87% and 100% here.

Spellings measured for this function (all whole-function bodies, diff = differing instructions
after normalising branch targets, out of 315):
- `const Vec& vtx = mVertexBuffer[i]` (shipped): 314
- `const Vec* vtx = &mVertexBuffer[i]; vtx->x`: 314
- fully inline subscripts: **0** (100.00%)
- `const float* vtx = (const float*)&mVertexBuffer[i]; vtx[0..2]`: 330
- `const Vec vtx = mVertexBuffer[i]` (a copy): 353

## ClipScreenQuadFromVS 59.93% -> 77.10% (not a match, kept because nothing regressed)

Two changes, each measured:
1. **Retail calls `GetProjectionState()` (a real `bl`, eight times) for every near/far test**; we
   read the `mProj` static directly, which MWCC hoists into one `lis r3,0 / addi` pair. Writing
   `GetProjectionState().GetNear()` / `.GetFar()` restores the calls. Retail also splits the test
   into two `if`s (four near tests, then four far tests) rather than one 8-term `||` chain - the
   two-ifs form is worth a further ~1.8 points on its own.
2. **Retail's `points` is a plain 4-element array, not a `reserved_vector`.** Retail's stores are
   `stw r5,48(r1)` / `stw r0,52(r1)` with a fixed frame slot and no `lwz/stw` count traffic;
   `push_back` emits the `lwz count / slwi / add / stw count+1` chain that does not match at all.
   `CVector2i points[4];` with `points[0..3] = ProjectPoint(pN);` matches.

The two are worth much more together than apart: gps-call alone 66.32%, array alone 68.30%,
both 77.10%. The `texCoords` loop is better left as `push_back` (indexing it costs 3 points).
Not finished: 240 differing instructions remain, mostly the min/max chain (retail loads
`points[i]` from the frame in a different order and folds the pairs differently) and register
allocation around the two early returns.

Grid measured (diff / %): mproj+push 358/59.93, mproj+arr 276/68.30, mproj+subs 375/60.95,
mproj+ptrc 318/60.68, gps+push 332/66.32, gps+arr 250/75.98, gps+subs 348/67.35,
gps+ptrc 293/67.30, gps2if+push 319/68.18, **gps2if+arr 240/77.10**,
gps2if+subs 336/69.48, gps2if+ptrc 281/69.58. `+texsub` on every one of those lost 3-5 points.
Also tried and no help: hoisting `const float near/far` into locals first (60.53%, the calls
disappear), a `const CProjectionState& proj` alias (62.66%), interleaving near/far per point
instead of near-group then far-group (66.27%).

## CalculatePerspectiveMatrix 96.42% - unchanged, wall re-confirmed

The previous run's WALL line was correct and this run did not break it. The whole function is
74 instructions on both sides and the *only* difference is which of f7/f8/f9 holds `t`
(`frsp` of the `tan` result) versus `(zfar - znear)` versus `2*znear`: retail puts them in f7/f8/f9
in that order, we put `t` in f9. Nothing about the arithmetic, the schedule or the store order
differs. About 40 further spellings measured this run, none moved it off 12 differing
instructions:

- named `twoZnear = 2.f * znear` used in both arguments: **11** (the best; 96.62%, still short)
- named `zRange = zfar - znear`, and both `twoZnear`+`zRange`: 14
- `zSum`/`twoZfar*znear` as locals, declared before `t`: 53 (71.15%) and 12 in the after position
- all 16 ctor arguments as named locals, in ctor order and with the zfar/znear pair first: 12
- `CMatrix4f mtx(...); return mtx;` (Prime 1's `#if !NONMATCHING` arm): 34 (91.31%) - confirms
  run 1's measurement
- reordering the five local declarations: 12 for t/right/left/top/bottom, **24** for any order
  with top before right
- `t` computed last, `2*znear` first, `halfT = t*0.5f`, `aspect*znear` precombined,
  `aspect` last, `2*` as a division, inlining `tan()` at both use sites (two calls, 44.57%),
  dropping `left`/`bottom` and folding them into the arguments (42): all worse or equal
- `const` vs plain `float` on every local: no difference at all (12 either way)

**Superseded 2026-10-02 - not a wall; see "CalculatePerspectiveMatrix matched" at the end.**

WALL: CalculatePerspectiveMatrix 96.42% - only the f7/f8/f9 assignment of t / (zfar-znear) /
2*znear differs, and ~40 spellings this run (best 11 differing instructions, naming `twoZnear`)
did not move it. Run 1's three spellings plus these; the next run should try the register
allocator itself, not the expression shape - e.g. perturbing how many FPRs are live at the
`frsp`, or a source form that makes MWCC number the three values in definition order.

## EndScene 81.08% - unchanged, the loop is the wall

Run 1's finding holds and this run did not break it. The 96-triangle loop is 54 instructions in
retail (32 iterations x 3 triangles, every store kept: 45 `stfs` + 9 `stw`); MWCC collapses ours
to an empty `mtctr`/`bdnz` pair plus one store. Re-measured this run, all 155 differing
instructions:

- volatile local (`volatile PPCWGPipe& fifo = *(volatile PPCWGPipe*)GXFIFO_ADDR`, 32 or 96
  iterations, plain or with `0.f`/`1.f`/`0` hoisted into const locals): 361 (37.22%) - **volatile
  keeps all 54 stores but then unrolls eight-wide and reloads the constants**, which is further
  from retail than the collapse is. Raw `GXWGFifo.f32 =` / `.u32 =` in the body: 155, identical
  to the macros.
- inner loop over the 3 vertices with `(v == 2) ? o : z`: 171 (81.34%) - 1 point, not a match
- volatile pointer with explicit `pf[j]` indices over 18 floats: 188 (79.47%); through a
  `PPCWGPipe*` with `pf[j*2]`: 177 (80.52%)
- 32 iterations x 3 triangles written out, via the macros and via raw fifo stores: 155 / 154
- `++i` at the end of the body instead of in the `for` header: 155
- The filter loop (`for i < 7`, `adjustedFilter[i] = u8(brightness * filter[i])`) is *not* the
  problem: it matches structurally; the tail differences are all `int`->`float` temporaries
  placed differently because the frame is 288 bytes instead of retail's 272.

WALL: EndScene 81.08% - the 96-triangle loop's 54 same-address stores are collapsed by MWCC
because GXWGFifo is non-volatile under __MWERKS__; making it volatile restores the stores but
triggers an eight-wide unroll with constant reloads (37%), and five other loop shapes are no
better. Also 16 bytes of frame difference in the tail, which is downstream of the loop.

## NEW (not filed as items - neither can reach a count from here)

- The `const T& local = array[i]` / inline-subscript lever above is the most transferable finding
  in this run and applies to any Echoes unit that reads a vertex or normal out of
  `mVertexBuffer` / `mNormalBuffer` / `mTexCoordBuffer*` in a loop. Worth a sweep across the
  graphics units; I did not file it because its target would be a guess, and a `NEW:` line needs
  one real unit.
- `rstl::reserved_vector::push_back` in this repo emits `lwz count / slwi / add / stw count+1`
  per element. Where retail shows a bare `stw x,N(r1); stw y,N+4(r1)` into a fixed frame slot,
  the source is a plain C array. `ClipScreenQuadFromVS` was one instance; other units using
  `reserved_vector` locally are likely to have the same mismatch.

## CalculatePerspectiveMatrix matched (2026-10-02, orchestrator) - the wall was a missing inline

100.00%, 74 of 74 instructions, unit 100 of 102. The fix is not in the expression shape at all:
retail calls `tan` through an **inline float wrapper** (MSL's `tanf`), and the value returned by an
inlined function gets its own temporary, which is what moves `t` from f9 to retail's f7.

```cpp
static inline float tan_inline(float x) { return (float)tan((double)x); }
...
float t = tan_inline(CRelAngle::FromDegrees(fovy).AsRadians() / 2.f);
```

Measured as differing objdump lines against retail, same harness for each: bare `(float)tan(...)`
20, `double t` 12, half angle as a named local 20, `top` declared first 38, `top` derived from a
height local 52, the inline wrapper **0**. `libc/math.h` has `_MATH_INLINE` float wrappers for
sin/cos/fabs/atan2/fmod/acos but only an extern `float tanf(float)`, which `Dolphin/mtx` really
calls, so the wrapper is file-local rather than a header change.

General lesson: when the only residual is which FPR holds the result of a libm call, look for a
float wrapper the original went through before trying ~40 spellings of the caller.

Still open in this unit: `EndScene` 81.08% (1468 B) and `ClipScreenQuadFromVS` 77.10% (1484 B).
