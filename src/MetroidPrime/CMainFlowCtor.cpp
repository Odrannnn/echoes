// Retail 0x8001E008-0x8001E070: `CMainFlow::CMainFlow`, 104 bytes.
//
// It is on the frame loop's path because `CGameArchitectureSupport`'s constructor calls
// `new CMainFlow()` (main.cpp:243), which registers the front-end IOWin. Nothing in the frame
// loop itself calls it.
//
// **Three renames in `config/G2ME01/symbols.txt` exist only for this unit**, all of symbols nothing
// in the tree referenced before, and the DOL link is what proves each: `fn_80049E98` ->
// `__ct__6CIOWinFRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>`,
// because retail's `CIOWin` constructor is *unnamed* and a mem-init list naming it would not link;
// `lbl_803B1BA0` -> `__vt__6CIOWin`; and `lbl_803B1770` -> `__vt__9CMainFlow`. The C++ name was
// read out of the compiled object with `nm`, not guessed.
#include "MetroidPrime/CMainFlow.hpp"

#include "rstl/string.hpp"

// 0x8001E008, 0x68 bytes, and the order is the Itanium one: the temporary and the base-constructor
// call first, this class's own vptr last.
//
//   lis/addi x3 -> 0x803A60A7, and the *second* addi is a literal +7, so the string is not at a
//   symbol's start: retail's rodata pool has "MainFlow" at 0x803A60A7 with the seven bytes
//   "??(??)\0" in front of it at 0x803A60A0 - the tail of a different, longer literal that the
//   linker merged into the same blob. So this unit cannot use a string literal of its own: a
//   `Matching` object's .rodata is linked into the DOL and would grow the section and move every
//   address above it, which is exactly the failure the sha1 gate catches (this unit's .rodata would
//   be 9 bytes, which pads to 16). The name is therefore claimed where retail put it, and the +7 is
//   reproduced as retail wrote it. `lbl_803A60A0 + 7` is what the source says; the definition with
//   retail's bytes is in src/MetroidPrime/PortGlobals.cpp, because a PC link has no retail object
//   to bind it to.
//   `bl string_l` with r3 = the return slot at r1+8: MWCC passes a class return value in r3, so
//   this is `rstl::string_l("MainFlow")` materialised as a temporary.
//   `bl CIOWin::CIOWin` with r3 = this, r4 = &temporary.
//   `bl internal_dereference` on the temporary: `~rstl::basic_string` is `{ internal_dereference(); }`
//   and the temporary dies at the end of the full expression.
//   `lis r3,0x803B; addi r4,r3,0x1770` -> 0x803B1770, CMainFlow's vtable, stored at +0.
//   `stw r0,20(r31)` with r0 = -1 -> x14_gameState = kCFS_Unspecified.
extern "C" const char lbl_803A60A0[];

CMainFlow::CMainFlow()
: CIOWin(rstl::string_l(lbl_803A60A0 + 7)), x14_gameState(kCFS_Unspecified) {}
