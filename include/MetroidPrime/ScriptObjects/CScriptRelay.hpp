#ifndef _CSCRIPTRELAY
#define _CSCRIPTRELAY

#include "MetroidPrime/CEntity.hpp"

#include "rstl/string.hpp"

/**
 * The entity class `LoadRelay` (FourCC `SRLY`) constructs.
 *
 * Named from retail's own symbol table: the `TypesMatch` at vtable slot +0x0c of its
 * vtable (`lbl_803B35B8`, 0x20 bytes, 6 function slots) is
 * `TypesMatch__12CScriptRelayCFi`, and it is not inherited, so the name in that slot is
 * the class's own. `docs/research/missing_classes.md` records the measurement and the
 * 76-row table it comes from.
 *
 * Layout, all of it from the retail constructor at 0x800B919C (120 bytes), which is the
 * only place the object is written:
 *
 *   - `CEntity`          0x00 .. 0x24, CHECK_SIZEOF says 0x24
 *   - `+0x24` a `short`  a value read from a `.sdata` global, not from an argument
 *   - `+0x26` a byte     one bit of it, from the constructor's 4th argument
 *   - `+0x27` padding
 *   - end of object      `operator new` is called with 0x28 = 40
 *
 * The byte at +0x26 is written `lbz` / `rlwimi`(MB=ME 24) / `stb`, i.e. as a *byte* whose
 * one field sits at bit 24 of the 32-bit register. `stb` writes bits 0..7, so on this
 * compiler the field's initialisation is a dead write - see the same open question in
 * `docs/research/missing_classes.md` for `CScriptSpecialFunction` and `CScriptActorKeyframe`,
 * which have the identical shape. The header models the byte as a byte, which is what the
 * store width says, and the accessor reads it as a byte.
 *
 * The constructor is declared and not defined on purpose: retail's constructor object is
 * linked for that range and this unit only has to name it, so no vtable and no ctor range
 * is claimed.
 */
class CScriptRelay : public CEntity {
public:
  CScriptRelay(TUniqueId uid, const rstl::string& name, const CEntityInfo& info, bool oneShot);

  CEntity* TypesMatch(int typeId) const;

  bool GetOneShot() const { return (m_flags & 1) != 0; }

private:
  short m_x24;         // x24
  unsigned char m_flags; // x26, bit 0 is the constructor's 4th argument
};
CHECK_SIZEOF(CScriptRelay, 0x28)

#endif // _CSCRIPTRELAY
