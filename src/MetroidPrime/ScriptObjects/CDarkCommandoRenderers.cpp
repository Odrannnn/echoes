// CDarkCommandoRenderers.cpp - DarkCommando's (module 3) accessor at .text 0x648C..0x64AC:
// `fn_3_648C`, this class's `AddToRenderer` handing straight to a **qualified**
// `CPatterned::AddToRenderer`.
//
// Same shape as `MetroidPrime/ScriptObjects/CIngSnatchingSwarmBounds.cpp`'s `fn_33_3A34` (module
// 33, Matching at 4/4) and as this module's own `fn_3_91F8`, in `CDarkCommandoRender.cpp`: retail
// calls the base implementation directly rather than dispatching through the vtable - the imported
// name is `AddToRenderer__10CPatternedCFRC13CStateManager`, with no `Fv` suffix a virtual call
// carries - and `self` is already in r3 and `mgr` already in r4, so nothing is moved and the frame
// holds only the saved LR at r1+0x14.
//
//   0x648C fn_3_648C  0x20  stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) / bl
//                        AddToRenderer__10CPatternedCFRC13CStateManager / lwz / mtlr / addi / blr
//
// The range is claimed exactly and nothing else. `fn_3_6284` (0x6284, 0x4C) below and `fn_3_64AC`
// (0x64AC, 0x7C) above stay retail, so dtk fills them and the module's sha1 against
// `config/G2ME01/config.yml` still holds. `fn_3_648C` is in the module's `ldscript.lcf` FORCEACTIVE
// block, so nothing dead-strips it.
//
// `tools/unit_fit.sh` prints `.data claimed - ours 40 <- NOT CLAIMED BY splits.txt` for this
// object: the 0x28 bytes are `SolidMaterial` and nine companions from
// `Collision/CMaterialList.hpp`, which `MetroidPrime/Enemies/CPatterned.hpp` reaches. That is the
// same 40 unclaimed bytes `CIngSnatchingSwarmBounds.cpp` carries with its module's hash holding,
// and `CDarkCommandoRel.cpp` avoids them by not including the real class header at all. The one
// place the difference matters is a unit claiming the module's `.data`, and no unit here does.

#include "types.h"

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Enemies/CPatterned.hpp"

// The receiver, named only so the qualified call below has something to be qualified against.
// Nothing here touches a member, so the class carries no layout at all - the whole function is a
// pass-through, and a stand-in that claimed one would be asserting an offset retail never reads.
class CDarkCommandoRenderers {};

// `tools/check_symbol_names.py` reads this name out of the object, so it has to be exactly what
// `config/G2ME01/rels/DarkCommando/symbols.txt` calls it.
extern "C" {
// .text 0x648C, 0x20 bytes. CDarkCommando derives from CPatterned, so this is the override handing
// its parent's implementation up rather than doing anything of its own.
void fn_3_648C(CDarkCommandoRenderers* self, const CStateManager& mgr) {
  reinterpret_cast< CPatterned* >(self)->CPatterned::AddToRenderer(mgr);
}
}