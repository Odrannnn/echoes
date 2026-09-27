/**
 * The port's two `CCubeRenderer` list-item destructors. **This file is port-only**:
 * `configure.py` does not declare it, so mwcceppc never sees it and it is not a decompilation
 * unit - the same arrangement as `src/Kyoto/Graphics/CTexturePortStub.cpp`.
 *
 * ## Why this file exists at all, and why it is not in `PortCCubeRenderer.cpp`
 *
 * `src/MetaRender/Carve80270848.cpp` is `~CCubeRenderer`, the class's key function, so it emits
 * `_ZTV13CCubeRenderer`; the destructor then tears down two `rstl::list` members and each list
 * destroys its nodes' values. `rstl::list`'s own destructor is
 *
 * ```
 * include/rstl/list.hpp:272   list<T, Alloc>::~list() {
 * include/rstl/list.hpp:278     it->get_value()->~T();          <- the reference is emitted here
 * ```
 *
 * so the two missing symbols come out of a **template in `rstl`**, from `T` = the item type:
 *
 * ```
 * include/rstl/list.hpp:278:(.text+0x11d): undefined reference to `SFogVolumeListItem::~SFogVolumeListItem()'
 * include/rstl/list.hpp:278:(.text+0x1bd): undefined reference to `SAreaListItem::~SAreaListItem()'
 * ```
 *
 * They belong to no `CCubeRenderer` member. `SAreaListItem` and `SFogVolumeListItem` are declared
 * as bare structs in `include/MetaRender/CCubeRenderer.hpp` (lines 40-45) purely so the two
 * `rstl::list` template instantiations in the class have a complete type, and **this tree's copies
 * of both have no members at all** - which is what makes the honest body a log and a comment
 * rather than a reproduction.
 *
 * ## Retail's two bodies, read rather than assumed
 *
 * Neither of the two is where it looks. Each is a **32-byte forwarder with no body of its own**:
 *
 * ```
 * fn_8027372C  .text:0x8027372C  size:0x20   stwu/mflr/stw/bl fn_8027374C/epilogue   -> SAreaListItem
 * fn_80273880  .text:0x80273880  size:0x20   stwu/mflr/stw/bl fn_802738A0/epilogue   -> SFogVolumeListItem
 * ```
 *
 * **`SAreaListItem`** - `fn_8027374C` (0x24 bytes) is `r4 = -1; bl fn_802704F0(this)`, and
 * `fn_802704F0` (`.text:0x802704F0`, `size:0x88`) is itself a member deleting destructor that
 * unwinds four sub-objects, one after another, each with the same `-1` flag:
 *
 * ```
 * 80270510:  addi r3,r30,72  ; li r4,-1 ; bl fn_802702F0    <- member at +0x48
 * 8027051c:  addi r3,r30,56  ; li r4,-1 ; bl fn_80004A4C    <- member at +0x38
 * 80270528:  addi r3,r30,40  ; li r4,-1 ; bl ...            <- member at +0x28
 * ```
 *
 * So retail's `SAreaListItem` is a ~0x50-byte record with **four owning members**, and
 * `rstl::list::~list`'s `x0_allocator.deallocate(it)` afterwards frees the node storage itself.
 * The `-1` on every call is the same member-not-deleted convention `CFont::~CFont` sees.
 *
 * **`SFogVolumeListItem`** - `fn_802738A0` (0x50 bytes) has a real body, and it is *guarded*:
 *
 * ```
 * 802738a8:  cmplwi r3,0 ; beq 802738e0                     <- return if this == nullptr
 * 802738b4:  addic. r0,r3,76 ; beq 802738e0                 <- return if the token slot is null
 * 802738bc:  lbz   r0,88(r3) ; cmplwi r0,0 ; beq 802738e0   <- return unless the byte at +0x58 is set
 * 802738d8:  li    r4,0 ; bl 8030154c <__dt__6CTokenFv>     <- destroy the CToken at +0x4C
 * ```
 *
 * One `CToken` at +0x4C, released only when the flag byte at +0x58 is non-zero. **A `TToken` is
 * the object that keeps a pool object alive** - its destructor hands the `(type, id)` back to the
 * `CSimplePool` - so this is the one of the two that owns something a pool counts.
 *
 * ## What a no-op here costs, per type, and why it is not detectable
 *
 * Both structs are empty, so a no-op leaves an object that looks valid: nothing is nulled, no
 * counter moves, no flag clears. The two costs are different in kind.
 *
 *   * **`SAreaListItem`** - four members' allocations are never freed and whatever tokens they hold
 *     are never released. The node storage *is* still freed, by `rstl::list::~list` itself, so this
 *     is a leak of the members' contents and not of the node.
 *   * **`SFogVolumeListItem`** - the pool's live-token count stays high. `CSimplePool` keys its map
 *     on `(type, id)`, so a released-early token makes a later `GetObj` for the same name hand back
 *     an object another holder still believes it owns. That is a use-after-free with a plausible
 *     value, and it is the worst failure mode in this file - which is the reason the log line is
 *     here and the reason this body must not be filled in with a plausible release.
 *
 * **Neither runs during boot.** Nothing in the port deletes the renderer, and both lists are empty
 * after `CCubeRenderer`'s constructor anyway, so `rstl::list::~list` never reaches a node. These
 * lines will not appear in a boot log; they exist because the vtable's destructor slots point at
 * `~CCubeRenderer`, which would reach them.
 */

#include "MetaRender/CCubeRenderer.hpp"

#include <stdio.h>

#if defined(__MWERKS__) || defined(CLANGD)
#error "this file is host-only: it defines TARGET_PC stand-ins for retail .text 0x8027372C/0x80273880"
#endif

namespace {
/** Announce that this destructor is a stand-in. Never frees, never returns a value. */
void mpUnwrittenListItemDtor(const char* name) {
  printf("[CCubeRenderer] list item %s has no decompiled body - stand-in, retail behaviour NOT "
         "reproduced\n",
         name);
  fflush(nullptr);
}
} // namespace

/**
 * `SAreaListItem::~SAreaListItem()` - retail `fn_8027372C` (`.text:0x8027372C`, 0x20 bytes), a
 * forwarder to `fn_8027374C`, which calls `fn_802704F0(this, -1)` to unwind four members at
 * +0x48, +0x38, +0x28 and one more.
 *
 * **The struct has no members, so there is nothing to unwind.** What a real destruction would have
 * to do is named in the file header; what it would cost is four leaks per node.
 */
SAreaListItem::~SAreaListItem() { mpUnwrittenListItemDtor("~SAreaListItem"); }

/**
 * `SFogVolumeListItem::~SFogVolumeListItem()` - retail `fn_80273880` (`.text:0x80273880`, 0x20
 * bytes), a forwarder to `fn_802738A0`, which destroys the `CToken` at +0x4C when the byte at
 * +0x58 is non-zero.
 *
 * **The struct has no members, so there is no token to release** - and releasing nothing is what
 * leaves a `CSimplePool` thinking an object is still spoken for.
 */
SFogVolumeListItem::~SFogVolumeListItem() { mpUnwrittenListItemDtor("~SFogVolumeListItem"); }
