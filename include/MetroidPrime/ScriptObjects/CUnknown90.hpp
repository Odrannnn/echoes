#ifndef _CUNKNOWN90
#define _CUNKNOWN90

#include "MetroidPrime/CEntity.hpp"

#include "rstl/string.hpp"

/**
 * The entity class `LoadTimeKeyframe` (FourCC `TKEY`) constructs.
 *
 * Named from retail's own symbol table, which calls it `CUnknown90` - the
 * `TypesMatch` at vtable slot +0x0c of its vtable (`lbl_803B7AE0`) is
 * `TypesMatch__10CUnknown90CFi`, and it is *not* inherited, so the name in that
 * slot is the class's own and not a base's. `docs/research/missing_classes.md`
 * records the measurement.
 *
 * Layout, all of it from the retail constructor at 0x801F9474 (104 bytes), which
 * is the only place the object is written:
 *
 *   - `CEntity`               0x00 .. 0x24, CHECK_SIZEOF says 0x24
 *   - `+0x24` a `float`       the constructor's 4th argument, stored verbatim
 *   - `+0x28` end of object   `operator new` is called with 0x28 = 40
 *
 * The constructor is declared but not defined here on purpose: retail's own
 * constructor object is linked for the range, and this unit only has to name it.
 * See `docs/research/missing_classes.md`, "the loader does not need the ctor".
 */
class CUnknown90 : public CEntity {
public:
  CUnknown90(TUniqueId uid, const rstl::string& name, const CEntityInfo& info, float time);

  CEntity* TypesMatch(int typeId) const;

  float GetTime() const { return m_time; }

private:
  float m_time; // x24
};
CHECK_SIZEOF(CUnknown90, 0x28)

#endif // _CUNKNOWN90
