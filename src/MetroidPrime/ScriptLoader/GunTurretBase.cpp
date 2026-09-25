#include "MetroidPrime/ScriptLoader.hpp"

// GunTurretBase - retail registers 2 loaders through one loader-struct pointer at
// .sbss 0x80419420; each is a vtable thunk calling one member of it.
//
//   LoadGunTurretBase            0x80218B9C -> member 0
//   LoadGunTurretTop             0x80218B70 -> member 1
//
// This unit claims .text 0x80218B70..0x80218BC8 and .sbss 0x80419420..0x80419428. The 8-byte
// setter at 0x80218BC8 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SGunTurretBaseLoaders {
  FScriptLoader slot0;
  FScriptLoader slot1;
};

struct SLoaderSlot {
  SGunTurretBaseLoaders* value;
  unsigned int padding;
};

SLoaderSlot gLoader_GunTurretBase;

CEntity* LoadGunTurretBase(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_GunTurretBase.value->slot0(mgr, input, info);
}

CEntity* LoadGunTurretTop(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_GunTurretBase.value->slot1(mgr, input, info);
}
