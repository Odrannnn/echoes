/**
 * `fn_80049244`, retail 0x80049244, 0x118 = 280 bytes, one function.
 *
 * ## It is **not** the draw, and `boot_path.md` row 21c is wrong about that
 *
 * `docs/research/cube_renderer_vtable.md` (lane `display`, 2026-09-26) measured all 82 slots of
 * `__vt__13CCubeRenderer` out of `build/G2ME01/main.elf`: **slot +0x94 - the one
 * `lwz r12,148(r12); mtctr; bctrl` at 0x800061BC names - is `CCubeRenderer::BeginScene` at
 * 0x8026FBFC, 0x180 bytes, slot 35.** This function is a different subsystem, and the file is the
 * evidence rather than the assertion: it walks an `IOWinPQNode` list and calls two `CIOWin`
 * virtuals, so it belongs with `CIOWinManager` (and with correction 3 of `boot_path.md`).
 * **Writing it does not move step 21c** and must not be reported as if it did.
 *
 * ## What it is
 *
 * Two bottom-tested walks with the same six-step body. Per node:
 *
 *   CopyInto(r1+N, node)            bl 0x80049010 - the OUT-OF-LINE rc_ptr copy constructor
 *   r3 = *(r1+N)                    lwz
 *   r12 = (*r3)->vptr[k]            lwz 0(r3) ; lwz k*4+8(r12) ; mtctr ; bctrl
 *   ReleaseData(r1+N)               bl 0x80008FA4
 *   ... the same pair again, with the other k ...
 *   r31 = <the second call's result>
 *   if ((r31 & 0xFF000000) == 0) break;            clrlwi. r0,r31,24 ; beq
 *   node = node->xC_next
 *   while (node != 0)
 *
 * k is **4** in the first loop and **3** in the second, against `CIOWin`'s declaration order -
 * slot 0 `~CIOWin`, slot 1 `OnMessage`, slot 2 `GetIsContinueDraw`, slot 3 `Draw`, slot 4
 * `PreDraw`. So the first loop pre-draws and the second draws, and the loop-exit test is the low
 * byte of `GetIsContinueDraw()`. **The second `rc_ptr` copy in each loop is not a redundant copy**:
 * it is the call that decides whether the walk continues, which is why it gets its own frame
 * slot. The `clrlwi`/`beq` pair is retail testing the low byte, so that call returns `bool` and
 * the other two are the `void` ones.
 *
 * **Both loops walk the SAME list.** Retail reads the root twice and it is `0(r3)` at 0x80049258
 * and `0(r29)` at 0x800492D0 - **offset 0 both times, i.e. `x0_drawRoot` twice**, not
 * `x0_drawRoot` then `x4_pumpRoot`. (The first reading of these bytes suggested two lists; the
 * disassembly does not support it, and pre-draw-then-draw over one list is what the two virtuals
 * mean anyway.) `x4_pumpRoot` is never touched here.
 *
 * The four `rc_ptr` temporaries all live at once - loop one uses `r1+32` and `r1+24`, loop two
 * uses `r1+16` and `r1+8` - so they are stack locals in disjoint scopes, and **mwcceppc hands out
 * frame slots from the top of the frame downwards**: the loop written first gets the high ones,
 * which is what retail has. This unit is written descending like every other unit in the tree.
 *
 * `CopyInto` is *called* rather than inlined, which is what the `rstl::CRcPtrData::OutOfLine`
 * tag asks for; `src/MetroidPrime/CIOWinManagerRemoveAllIOWins.cpp` has the full argument and
 * `docs/research/rc_ptr.md` the measurement.
 *
 * ## `NonMatching`, for the same reason as `RemoveAllIOWins`, one line away in `configure.py`
 *
 * Measured: **280 of 280 bytes and 70 instructions, with two differing instructions** - 8 bytes,
 * 97.1% - and both are one instruction of prologue scheduling:
 *
 * ```
 *                       ours                    retail
 * +0c  stw   r31,60(r1)  stw   r31,60(r1)
 * +10  stw   r30,56(r1)  stw   r30,56(r1)
 * +14  stw   r29,52(r1)  lwz   r30,0(r3)      <- retail hoists the first root load
 * +18  mr    r29,r3      stw   r29,52(r1)
 * +1c  lwz   r30,0(r3)   mr    r29,r3
 * ```
 *
 * The other nine differences objdiff would print are the eight `bl` displacements and are
 * relocations, not bytes. `lwz r30,0(r3)` cannot move earlier than `mr r29,r3` by construction -
 * it reads r3 - and three spellings were measured and none moved it: `while`, `for`, and hoisting
 * `bool cont` to function scope (FACTS' "callee-saved registers follow temporary creation
 * order"). This is a scheduler difference between two mwcceppc invocations, not a body
 * difference.
 *
 * **The structural blocker is larger than those two instructions.** `rstl::rc_ptr<CIOWin>`
 * instantiates `ReleaseData` and its own destructor *in this translation unit*, because
 * `include/rstl/rc_ptr.hpp` defines `rc_ptr<T>::ReleaseData()` in the header and C++98 has no
 * `extern template`. That is `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` at 0x64 and
 * `__dt__Q24rstl15rc_ptr<6CIOWin>Fv` at 0x50: **180 bytes of weak `.text` in an object whose
 * claim is 280**, and they are called, so `-strip_partial` cannot drop them. Retail calls
 * `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` at 0x80008FA4, which is inside `mainTail.cpp`'s
 * claim (0x80008680..0x80009880), so retail's instantiation lived in another TU and ours cannot
 * be moved out of this one. `src/MetroidPrime/CIOWinManagerRemoveAllIOWins.cpp` has exactly the
 * same 180 bytes over its claim and is `NonMatching` for the same reason; the fix for both is
 * one header change (an out-of-line `ReleaseData` for `rc_ptr<CIOWin>`, or a non-template base
 * carrying it), and that is a bigger job than this carve.
 *
 * `fn_80049010` is **not** byte-exact either (four register-operand differences, measured) and
 * that does not stop this unit: objdiff compares *this* object's bytes, and a `bl` to a symbol is
 * relocation-free-equal whatever the callee does.
 *
 * ## Its own unit, claimed exactly
 *
 * `.text` 0x80049244..0x8004935C. The 0x210 bytes below it are unclaimed and stay that way:
 * **`dtk` fills the gaps inside a claim with retail's own bytes and objdiff then scores them
 * 100%**, so a claim reaching into the gap would report functions this source does not contain
 * (`tools/audit_rel_claim.py` is the REL-side analogue). `rstl/rc_ptr_copy.cpp` ends at
 * 0x80049034 and the next claimed unit, `MetroidPrime/CIOWinManagerPumpMessages.cpp`, starts at
 * 0x800495F0, so the 0x210 gap between 0x8004935C and 0x800495F0 is real and untouched.
 *
 * ## Why a `.cpp` and not a `.c`
 *
 * Retail's symbol is the unmangled placeholder `fn_80049244` (`config/G2ME01/symbols.txt` line
 * 1385), so the definition must not mangle. The usual answer is a `.c`, but this body is four
 * virtual calls, two `rstl::rc_ptr` copy constructions and four releases, and **a `.c` file is
 * compiled with `-lang=c`** - it cannot express any of that, and hand-rolling the vtable dispatch
 * would be a worse description of the code than `win->PreDraw()`. **`extern "C"` is the same
 * mechanism the tree already uses for this**: `src/MetroidPrime/Weapons/CGunWeaponTouch.cpp`
 * defines `fn_801D9F5C` that way, with a body, and `src/MetroidPrime/main.cpp:563` defines two
 * more. The exported name is `fn_80049244`, unmangled, which is what objdiff pairs against, and
 * `tools/check_symbol_names.py` checks it because the unit is a `.cpp`.
 *
 * `self` is a **first parameter, not a member function**, and that is not a style choice: retail's
 * name is a free placeholder, so a member definition would mangle to
 * `_ZN12CIOWinManager11fn_80049244Ev` and pair nothing. The disassembly is a member function's -
 * `mr r29,r3` and `lwz r30,0(r3)` - and passing `self` in r3 reproduces that exactly.
 */

#include "MetroidPrime/CIOWin.hpp"

#include "MetroidPrime/CIOWinManager.hpp"

#include "rstl/rc_ptr.hpp"

extern "C" void fn_80049244(CIOWinManager* self);

// `x0_drawRoot` is **private** and retail reads it as r3+0 and r3+4. `include/MetroidPrime/
// CIOWinManager.hpp` already measures the two roots as plain `IOWinPQNode*` at +0 and +4 and says
// so in the comment above them, so this is that measurement used as a **local duplicate shape**
// rather than a `friend` declaration: no shared header changes, and nothing that includes
// `CIOWinManager.hpp` can move. `CHECK_SIZEOF(CIOWinManager, 0x20)` covers the offsets and the
// shape is exactly two pointers, so it is layout-immune by construction.
struct CIOWinManagerRoots {
  CIOWinManager::IOWinPQNode* x0_drawRoot;
  CIOWinManager::IOWinPQNode* x4_pumpRoot;
};

// mwcceppc hands out frame slots from the TOP of the frame downwards, so the loop written first
// gets the HIGH ones - which is what retail has (`r1+32`/`r1+24` for pre-draw, `r1+16`/`r1+8` for
// draw). See the header.
extern "C" void fn_80049244(CIOWinManager* self) {
  CIOWinManager::IOWinPQNode* node = reinterpret_cast< CIOWinManagerRoots* >(self)->x0_drawRoot;
  while (node) {
    {
      rstl::rc_ptr< CIOWin > win(rstl::CRcPtrData::OutOfLine(), node->x0_iowin);
      win->PreDraw();
    }
    bool cont;
    {
      rstl::rc_ptr< CIOWin > win(rstl::CRcPtrData::OutOfLine(), node->x0_iowin);
      cont = win->GetIsContinueDraw();
    }
    if (!cont) {
      break;
    }
    node = node->xc_next;
  }

  node = reinterpret_cast< CIOWinManagerRoots* >(self)->x0_drawRoot;
  while (node) {
    {
      rstl::rc_ptr< CIOWin > win(rstl::CRcPtrData::OutOfLine(), node->x0_iowin);
      win->Draw();
    }
    bool cont;
    {
      rstl::rc_ptr< CIOWin > win(rstl::CRcPtrData::OutOfLine(), node->x0_iowin);
      cont = win->GetIsContinueDraw();
    }
    if (!cont) {
      break;
    }
    node = node->xc_next;
  }
}
