#include "MetroidPrime/Player/CPlayerGun.hpp"
#include "MetroidPrime/Weapons/CGunEffectUnk.hpp"

#include "Kyoto/Graphics/CModel.hpp"

// Retail 0x800E5C78, 0xA8 bytes: touch every part of both beams. The third of the three paths -
// 0x800E5D20 and 0x800E5D80 are in `CGunEffectTouch.cpp`, which is byte-exact and `Matching`.
//
// NOT byte-exact and therefore `NonMatching`. The control flow, the calls, the loop shapes and the
// schedule all reproduce retail; the whole residue is register allocation. Retail keeps five
// values in r27..r31 and spends one `stmw`/`lmw` pair, and hands out r28 (part), r29 (the second
// beam counter), r30 (the first beam counter, reused for the part count) and r31 (the model). The
// same source here gets self in r28 rather than r27 and reuses r28 for `part`, so the frame is
// four `stw` instead of one `stmw`, and the remaining twelve instructions differ only in which
// register each name lands in. Twelve source shapes were tried (see docs/RUNNING_THE_DECOMP.md);
// none moved it. Reported as blocked on register allocation rather than on the body.

struct CBeamHolder {
  char x0_pad[8];
  CModel* x8_model;
};

// The effect object, spelled out here rather than taken from
// `include/MetroidPrime/Player/CPlayerGun.hpp`: upstream's version of that header no longer
// declares it, and this port-only unit only ever reads the two fields below. The layout is the
// pre-upstream `CPlayerGunUnk570` verbatim, and the one-bit run keeps its pre-upstream
// declaration order on purpose - MWCC 2.7 stores the k-th declared one-bit field at bit k but
// tests the k-th declared field with `rlwinm. rX,rS,25+k,31,31`, which reads bit 6-k, and retail's
// shift 26 is field index 1 by the test rule. Only the test side is reproduced here, so the field
// is named by its declaration index. Measured on a standalone struct, 2026-09-25.
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
extern "C" void fn_80027AE8(CBeamHolder*);

// 0x800E5C78 has no parameters beyond `this` and no part index, so where 0x800E5D20 touches one
// part this one walks the whole model - and it spells the walk out rather than calling
// 0x80027B44, which is the loop 0x80027AE8 also spells out.
extern "C"
void fn_800E5C78(CPlayerGunUnk570* self) {
  if (self->x14_1_modelsLoaded) {
    if (self->x10 != nullptr) {
      int beam = 0;
      do {
        fn_80027AE8(fn_800E4E50(self, beam));
      } while (++beam <= 2);
    } else {
      int beam = 0;
      do {
        CModel* const model = fn_800E4E9C(self, beam)->x8_model;
        const int count = model->GetMatSetCount();
        for (int part = 0; part < count; part++) {
          model->Touch(part);
        }
      } while (++beam <= 2);
    }
  }
}
