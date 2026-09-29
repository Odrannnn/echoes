/**
 * The port's `CGraphicsPalette` destructor. **This file is port-only**: `configure.py` does not
 * declare it, so mwcceppc never sees it and it is not a decompilation unit - the same arrangement
 * as `src/Kyoto/Graphics/CTexturePortStub.cpp` (and `CFontPortStub.cpp` until the 2026-09-29
 * upstream sync replaced it with the real `src/Kyoto/Text/CFont.cpp`).
 *
 * ## Why it exists, and why the reference comes out of `rstl`
 *
 * `src/MetaRender/Carve80270848.cpp` is `~CCubeRenderer`, the key function, so it emits
 * `_ZTV13CCubeRenderer` and the renderer's destructor tears down its
 * `single_ptr< CGraphicsPalette >` member at +0x550. The host's `rstl::single_ptr` is
 *
 * ```
 * include/rstl/single_ptr.hpp:15:  ~single_ptr() { delete x0_ptr; }
 * ```
 *
 * so the reference is emitted from inside a **template in `rstl`**, not from the carve:
 *
 * ```
 * include/rstl/single_ptr.hpp:15:(.text+0x62): undefined reference to `CGraphicsPalette::~CGraphicsPalette()'
 * ```
 *
 * This is the shape the whole 5-symbol job has, stated concretely: the vtable wave grew from one
 * class to five, and the reason a lane that only reads `CCubeRenderer.hpp` will not find them is
 * that the scope does not stop at a class boundary. It stops at `rstl`.
 *
 * ## Retail's body, and the part that is a genuine hazard
 *
 * `fn_802C3F50` = `.text:0x802C3F50`, `size:0x88`, is `CGraphicsPalette`'s destructor taking
 * MWCC's deleting flag. It is not a no-op and the reason is worth writing down:
 *
 * ```
 * 802c3f68:  mr.  r30,r3 ; beq 802c3fbc                 <- return this if this == nullptr
 * 802c3f70:  lwz  r3,4(r30)                             <- this->x4_frameLoaded
 * 802c3f74:  lwz  r0,-25456(r13)                        <- CGraphicsPalette::sCurrentFrameCount
 * 802c3f78:  subf r0,r3,r0 ; cmplwi r0,2 ; bge 802c3f9c <- skip if the frame delta is >= 2
 * 802c3f84:  lwz  r4,12(r30)                            <- this->xc_entries.get()
 * 802c3f88:  li   r3,0 ; stw r3,12(r30)                 <- and clear the member
 * 802c3f8c:  cntlzw r0,r0 ; srwi r3,r0,5                <- r3 = (x4_frameLoaded == sCurrentFrameCount)
 * 802c3f98:  bl   8032772c <CFrameDelayedKiller::ScheduleDeletion(EWhichFrame, void*)>
 * 802c3f9c:  addic. r0,r30,12 ; beq 802c3fac            <- if the buffer is still set, free it
 * 802c3fa4:  lwz  r3,12(r30) ; bl 802ce388 <Free__7CMemoryFPCv>
 * 802c3fac:  extsh. r0,r31 ; ble 802c3fbc                <- if (short)flag > 0, free this
 * 802c3fb4:  mr   r3,r30 ; bl 802ce388 <Free__7CMemoryFPCv>
 * ```
 *
 * **Retail does not free a palette that was loaded in the current or the next frame.** It hands
 * `xc_entries` to `CFrameDelayedKiller::ScheduleDeletion` and lets the frame counter come round,
 * because the palette's `GXTlutObj` at +0x10 may still be referenced by a tluT the GX FIFO has not
 * executed yet. Freeing it immediately would pull the texture lookup table out from under a draw
 * in flight.
 *
 * **The host has no such ordering, and this body does not add one.** `rstl::single_ptr`'s
 * `delete x0_ptr` fires after this destructor returns and frees the buffer unconditionally. The
 * consequences are stated rather than smoothed over:
 *
 *   * **`CFrameDelayedKiller::ScheduleDeletion` is not called here, on purpose.** Calling it would
 *     add a new undefined symbol to a link that is being brought to zero, and the class's header
 *     (`include/Kyoto/CFrameDelayedKiller.hpp`) is not the shape retail's call site uses.
 *   * **The freeing is retail's `single_ptr`, not this body.** Retail's own `CMemory::Free` at
 *     0x802C3FA4 is MWCC's inlined `single_ptr` teardown. On the host `rstl::single_ptr` does that
 *     job itself, *after* this body runs. **A `free` here would be a double free**, and that is the
 *     one line this stub must not grow.
 *   * **What actually breaks, and when.** Nothing logs, nothing draws, and nothing faults: the
 *     buffer is freed, `GXTlutObj` goes with the object, and a dead palette is indistinguishable
 *     from a live one afterwards. It becomes wrong the moment a draw in the same frame still
 *     references that tluT - which is to say, it is one of the standing reasons the port cannot
 *     yet produce a correct frame, and it is not visible until it is.
 *
 * `sCurrentFrameCount` is `private` and unwritten, so there is no honest way to reproduce the
 * comparison here even if the delayed killer existed; zeroing or guessing it would make the branch
 * pick a frame at random.
 */

#include "Kyoto/Graphics/CGraphicsPalette.hpp"

#include <stdio.h>

#if defined(__MWERKS__) || defined(CLANGD)
#error "this file is host-only: it defines a TARGET_PC stand-in for retail .text 0x802C3F50"
#endif

namespace {
/** Announce that this destructor is a stand-in. Never frees, never returns a value. */
void mpUnwrittenPaletteDtor(const char* name) {
  printf("[CGraphicsPalette] %s has no decompiled body - stand-in, retail behaviour NOT "
         "reproduced\n",
         name);
  fflush(nullptr);
}
} // namespace

/**
 * `CGraphicsPalette::~CGraphicsPalette()` - retail `fn_802C3F50` (`.text:0x802C3F50`, 0x88 bytes),
 * the member form the tree's header declares.
 *
 * **The entry buffer is `rstl::single_ptr`'s to free, not this body's**, and the
 * frame-delayed ordering retail applies to it is not reproduced. See the file header.
 */
CGraphicsPalette::~CGraphicsPalette() { mpUnwrittenPaletteDtor("~CGraphicsPalette"); }
