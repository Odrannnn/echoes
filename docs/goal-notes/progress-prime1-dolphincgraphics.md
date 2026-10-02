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
