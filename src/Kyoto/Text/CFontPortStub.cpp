/**
 * The port's `CFont` destructor. **This file is port-only**: `configure.py` does not declare it, so
 * mwcceppc never sees it and it is not a decompilation unit - the same arrangement as
 * `src/Kyoto/Graphics/CTexturePortStub.cpp` and `src/Kyoto/CSimplePoolPort.cpp`.
 *
 * ## Why it exists
 *
 * `src/MetaRender/Carve80270848.cpp` is `~CCubeRenderer`, the class's key function, so it is also
 * the translation unit that emits `_ZTV13CCubeRenderer`. Landing it put retail's 82-slot vtable
 * into the port link as `.data.rel.ro` with a relocation per slot, and the renderer's own
 * destructor destroys a `CFont` member at `this + 0x10`. The link therefore asks for
 * `CFont::~CFont()` from two objects, and both are the header's implicit member teardown rather
 * than anything a carve names:
 *
 * ```
 * Carve80270848.cpp:96:(.text+0x1d8): undefined reference to `CFont::~CFont()'
 * Carve80271238.cpp:315:(.text.unlikely+0x139): undefined reference to `CFont::~CFont()'
 * ```
 *
 * The second is the constructor's unwind path - placement-new calls the matching destructor if the
 * constructor throws past it - so this is not only a shutdown symbol.
 *
 * ## Retail's body is 0x3C bytes and 56 of them are the deleting half
 *
 * `fn_802BAD30` = `.text:0x802BAD30`, `size:0x3C`, is `CFont`'s destructor taking MWCC's
 * deleting flag:
 *
 * ```
 * 802bad30:  stwu   r1,-16(r1) / mflr r0 / stw r0,20(r1) / stw r31,12(r1)
 * 802bad40:  mr.    r31,r3 ; beq 802bad54          <- r31 = this, return if this == nullptr
 * 802bad48:  extsh. r0,r4 ; ble 802bad54          <- the *only* test: (short)flag > 0 ?
 * 802bad50:  bl     802ce388 <Free__7CMemoryFPCv> <- then free the 8-byte block
 * 802bad54:  lwz    r0,20(r1) / mr r3,r31 / ...   <- return this
 * ```
 *
 * **There is no member teardown in it at all.** `CFont` is `int mFontSize; float mScale;`
 * (`include/Kyoto/Text/CFont.hpp`) - eight bytes, no allocation, no token, no container - so the
 * whole of retail's destructor is "if I was deleted, give my eight bytes back".
 *
 * ## Why a no-op here is retail's own behaviour and not a shortcut
 *
 * **Every call the port makes passes -1, and `(short)(-1) > 0` is false, so retail frees nothing
 * on this path either.** `Carve80270848.cpp`'s header records the call as
 * `bl fn_802BAD30(this + 0x10, -1)`, and the `-1` is the `rstl::` / container convention for "I am
 * being destroyed as a member, not deleted": the storage is not mine to release. That is also why
 * the two missing symbols are the *member* form `_ZN5CFontD1Ev` and not a deleting form - the
 * tree's header declares `~CFont();` with no flag, and the host's Itanium ABI asks for exactly the
 * member destructor.
 *
 * So the honest body here is: log, change nothing. **That is not the "plausible lie" shape the
 * other four destructors have**, and the difference is measured rather than asserted - it is the
 * `ble` at 0x802BAD4C.
 *
 * ## What breaks if this is ever wrong
 *
 * Two ways, and both are silent:
 *
 *   * **Leak.** A future lane that writes `delete font` reaches the *deleting* form, which this
 *     file does not provide, and 8 bytes per `CFont` go to `CMemory::Free`'s allocator and stay
 *     there. Eight bytes is below anything a leak check would notice.
 *   * **Double free, the other direction.** A body that freed `this` here would be wrong on the
 *     path the port actually takes, because the object is a `CCubeRenderer` member at +0x10 and
 *     retail's flag is -1: freeing it would release memory the renderer still owns. This stub must
 *     never grow a `free`.
 *
 * **It does not run during boot.** Nothing in the port deletes the renderer - `fn_80272958` hands
 * out one `.bss` arena and `fn_80272988` is a refcount decrement, never a free - so this line is
 * not in a boot log. It is here because the vtable's two destructor slots point at
 * `~CCubeRenderer`, which would reach it.
 */

#include "Kyoto/Text/CFont.hpp"

#include <stdio.h>

#if defined(__MWERKS__) || defined(CLANGD)
#error "this file is host-only: it defines a TARGET_PC stand-in for retail .text 0x802BAD30"
#endif

namespace {
/** Announce that this destructor is a stand-in. Never frees, never returns a value. */
void mpUnwrittenCFontDtor(const char* name) {
  printf("[CFont] %s has no decompiled body - stand-in, retail behaviour NOT reproduced\n", name);
  fflush(nullptr);
}
} // namespace

/**
 * `CFont::~CFont()` - retail `fn_802BAD30` (`.text:0x802BAD30`, 0x3C bytes), the member form.
 *
 * Retail's `Free__7CMemoryFPCv` call at 0x802BAD50 is behind `extsh(flag) > 0`, and every
 * reference the port has passes -1; see the file header for the disassembly. **The one thing this
 * body must never do is free `this`.**
 */
CFont::~CFont() { mpUnwrittenCFontDtor("~CFont"); }

/**
 * `CFont::CFont(float scale)` - the **fifth wave** of the same kind as the destructor above, and it
 * arrived for the same reason: `include/Kyoto/Text/CFont.hpp:8` declares `CFont(float scale);`, and
 * asking the linker for `~CFont` is what made it ask for the constructor, because the header's
 * implicit member teardown pulls the other half of the pair in with it.
 *
 * **The `scale` argument is deliberately discarded, and that is the honest choice rather than a
 * convenient one.** Storing it would be inventing state the port has no use for and no measured
 * initial value for; ignoring it means a caller that later depends on font scaling sees text that
 * is the wrong size, which is visible. The log line says the body is missing either way.
 */
CFont::CFont(float scale) {
  (void)scale;
  mpUnwrittenCFontDtor("CFont(float)");
}
