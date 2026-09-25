#include "MetroidPrime/Weapons/CGunWeapon.hpp"

#include "MetroidPrime/CStateManager.hpp"

// Retail 0x801D9F5C, 0x34 bytes. Unnamed in retail, and inside dtk's
// auto_03_801D5C70_text blob; CGunWeapon.hpp's `Touch`/`TouchHolo` pair is the port's
// name for the two functions here, so the `fn_` name is kept - retail does not name
// either of them, and CPlayerGun already calls this one under this name.
//
// CGunWeapon's own members, identified by measurement:
//   * The two calls are fn_801D18D0 (0x801D18D0), which the tree already reads as
//     CPlayerGun's teardown, and it passes `lwz 1612(r28)` - CPlayerGun::m_loadingBeam
//     at +0x64C (include/MetroidPrime/Player/CPlayerGun.hpp:316). So `this` is a
//     CGunWeapon and the second argument is a CStateManager& that retail also threads
//     through, but that this function never reads.
//   * fn_801D9F90, the sibling 0x88 bytes up at 0x801D9F90, has the same shape over
//     `this + 0x2C` and `this + 0xCC`, guarded by bytes at +0x78 and +0x118. Adding
//     this function's +0x7C and +0xC8 gives three sub-objects of one type at
//     +0x2C / +0x7C / +0xCC, each exactly 0x50 apart, each with its "is it loaded" flag
//     at +0x4C.
//   * The callee, fn_800E5D80, reads bit 5 of the byte at +0x14 of the sub-object and then
//     walks two of them (i = 0..1) through fn_800E4E50 / fn_800E4E9C, ending in
//     Touch__6CModelCFi. Two model handles at +0x10 and +0x18 is CGunWeapon's solid /
//     holo pair, which is what the sibling's own `x250` argument (a touch value) and
//     CGunWeapon::Touch/TouchHolo are for. fn_800E5D80 is itself unwritten and is on the
//     port's link-gap list, so calling it here does not add to the gap.
extern "C" void fn_800E5D80(void* subObject, CStateManager& mgr, int arg);

extern "C" void fn_801D9F5C(CGunWeapon* weapon, CStateManager& mgr) {
  uchar* self = reinterpret_cast< uchar* >(weapon);
  if (self[0xC8] != 0) {
    fn_800E5D80(self + 0x7C, mgr, 0);
  }
}
