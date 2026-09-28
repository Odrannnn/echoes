#include "MetroidPrime/Player/CPlayerGun.hpp"
#include "MetroidPrime/Weapons/CGunEffectUnk.hpp"

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

// The effect object itself, spelled out here rather than taken from
// `include/MetroidPrime/Player/CPlayerGun.hpp`: upstream's version of that header no longer
// declares it - the gun's own members are recovered there and this sub-object is not among them -
// and this port-only unit only ever reads the two fields below. The layout is the pre-upstream
// `CPlayerGunUnk570` verbatim: `x10` is a pointer to the shared beam-model slots, and the byte at
// 0x14 is a run of one-bit flags whose second field is the one retail tests here.
//
// **The one-bit run keeps the pre-upstream declaration order on purpose.** MWCC 2.7 lays a run of
// one-bit fields out so that a store to the k-th declared field is `rlwimi rA,rS,7-k,24+k,24+k`
// (the k-th field is bit k) but a *test* of the k-th field is `rlwinm. rX,rS,25+k,31,31` (which
// reads bit 6-k). Retail's shift 26 is field index 1 by the test rule and bit 5 by the store rule;
// the two cannot both be right, and only the test side is reproduced here, so the field is named
// by its declaration index. Measured on a standalone struct, 2026-09-25.
class CPlayerGunUnk570 {
public:
  char x0_pad[0x10];
  void* x10;
  bool x14_0 : 1;
  bool x14_1_modelsLoaded : 1;

private:
  char x15_pad[0x67];
  CGunEffectUnk x7c;
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
