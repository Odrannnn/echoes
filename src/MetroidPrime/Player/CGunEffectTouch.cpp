#include "MetroidPrime/Player/CPlayerGun.hpp"

#include "Kyoto/Graphics/CModel.hpp"

// Retail 0x800E5D20 .. 0x800E5DE8: two of the three "make sure the beam models are resident" paths
// of the effect object the gun keeps at +0x570 and the grapple arm keeps at +0x2C.
//
//   0x800E5D20  touch one part of one beam
//   0x800E5D80  touch one part of both beams
//
// Both open on bit 5 of the byte at 0x14 (`rlwinm. r0,r0,26,31,31`) and then on whether x10 is
// null: non-null means the beams live behind x10 and are reached through 0x800E4E50, null means
// they are inline and are reached through 0x800E4E9C. 0x800E5C78, the third path, is in
// `CGunEffectTouchAll.cpp` - it is 0xA8 bytes earlier and not yet byte-exact, and a unit that
// claims a range it does not reproduce breaks the DOL, so it cannot share a range with these two.
//
// Neither selector is written yet and retail names neither; both return one of the two beam
// holders, which is where the CModel lives.
struct CBeamHolder {
  char x0_pad[8];
  CModel* x8_model;
};

extern "C" CBeamHolder* fn_800E4E50(CPlayerGunUnk570*, int beam);
extern "C" CBeamHolder* fn_800E4E9C(CPlayerGunUnk570*, int beam);
extern "C" void fn_80027B44(CBeamHolder*, int part);
extern "C" void fn_800E5D20(CPlayerGunUnk570*, int beam, int part);

// The second parameter is dead in retail: 0x800C122C and 0x801C5990 both pass the state manager
// and 0x800E5D80 never reads r4. It is kept because the call sites have it.
extern "C"
void fn_800E5D80(CPlayerGunUnk570* self, CStateManager& mgr, int part) {
  if (self->x14_1_modelsLoaded) {
    int beam = 0;
    do {
      fn_800E5D20(self, beam, part);
    } while (++beam <= 2);
  }
}

extern "C"
void fn_800E5D20(CPlayerGunUnk570* self, int beam, int part) {
  if (self->x14_1_modelsLoaded) {
    if (self->x10 != nullptr) {
      fn_80027B44(fn_800E4E50(self, beam), part);
    } else {
      fn_800E4E9C(self, beam)->x8_model->Touch(part);
    }
  }
}
