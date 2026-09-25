#include "MetroidPrime/CActor.hpp"

#include "MetroidPrime/CStateManager.hpp"

#include "Kyoto/Math/CAABox.hpp"

// Retail 0x801ECD8C, 0x40 bytes. Unnamed in retail, inside dtk's auto_03_801E70B0_text
// blob. The `fn_` name is kept: retail names none of the four functions in this class and
// CActor.cpp already calls this one under this name.
//
// `this` is the object CActor holds at +0xC8 (CActor::xc8_unk, which the port types as an
// int and retail fills with a pointer). Measured, not guessed:
//   * CActor::AcceptScriptMsg (0x8004B8EC) reaches it from the kSM_XCRT case with
//     `lwz 200(r29)` - +0xC8 - and this function's own r4, the CStateManager&.
//   * 0x803B78A0 holds a four-entry vtable: 0x801ECDCC, 0x801ECD78, 0x801ECBB0,
//     0x801EC9C0. 0x801ECDCC is the deleting destructor (it writes a vtable at +0 and
//     tail-calls Free__7CMemoryFPCv on a positive (short) destructor flag), and
//     0x801ECD78 is this function's own 5-instruction half - it sets bit 1 of the same byte
//     and returns.
//   * 0x801EC9C0 calls GetPoint__6CAABoxCFi with `addi r4, r30, 20`, so CAABox is at
//     +0x14; CAABox is 0x18 bytes, which puts +0x2C next.
//   * 0x801ECE54 zeroes 0x2C, 0x30, 0x34, 0x38 and writes one .rodata float to 0x3C,
//     0x40, 0x44, 0x48. 0x801ECE14 then walks those two four-element arrays in lockstep,
//     filling the ints with `*(int*)(*(mgr + 0x14FC + 4*i) + 0x1328) - 1`.
//
// The class's name is not recovered, and neither is what the two bits mean. What is
// recoverable is that bit 1 of the byte at +0x5C is the "on" one (0x801ECD78 sets it and
// does nothing else) and bit 0 is the one this function sets before clearing bit 1 and
// recomputing - so it reads as "stop being on, and mark the cached four-element arrays
// stale". +0x04..+0x14 and +0x4C..+0x5C are never read, so the struct below reproduces the
// offsets, which is all this function needs, and claims to be nothing more.
#ifndef TARGET_PC
namespace {
class CField25 {
public:
  void* x00_vtable;   // 0x00, written by the deleting destructor 0x801ECDCC
  uchar x04_unk[0x10];
  CAABox x14_bounds;  // 0x14, from GetPoint__6CAABoxCFi(this + 0x14, i)
  int x2c_token[4];   // 0x2C, zeroed by 0x801ECE54
  float x3c_time[4];  // 0x3C, set to one .rodata float by 0x801ECE54
  uchar x4c_unk[0x10];
  // Bit positions from the rlwimi masks, calibrated against CGameOptions' constructor
  // (0x80161B9C), whose six bool : 1 members produce the same encodings for a byte at the
  // same alignment: `rlwimi ..,7,24,24` is bit 0, `..,6,25,25` is bit 1, and so on. So this
  // function sets bit 0 and clears bit 1, and 0x801ECD78 (rlwimi ..,6,25,25) sets bit 1.
  // Bits 2-7 are declared only so that bit 1 lands where it has to.
  bool x5c_bit0 : 1;
  bool x5c_bit1 : 1;
  bool x5c_bit2 : 1;
  bool x5c_bit3 : 1;
  bool x5c_bit4 : 1;
  bool x5c_bit5 : 1;
  bool x5c_bit6 : 1;
  bool x5c_bit7 : 1;
};
} // namespace

// 0x801ECE14: this class's own refresh, 0x40 bytes, walking the CStateManager's world list.
// Unwritten, and not on the port's link-gap list, so calling it from the host build would
// add one symbol and remove none - see the TARGET_PC branch.
extern "C" void fn_801ECE14(CField25* self, CStateManager& mgr);

extern "C" void fn_801ECD8C(CActor* actor, CStateManager& mgr) {
  CField25* self = reinterpret_cast< CField25* >(actor);
  self->x5c_bit0 = true;
  self->x5c_bit1 = false;
  fn_801ECE14(self, mgr);
}
#else
// The port does not model this class, and it has to stay a no-op: at retail's offsets
// +0x5C lands inside CActor::m_position (0x54..0x60, so it is the low byte of the z float),
// and the two flag writes would silently corrupt the player's position in order to imitate a
// class the port does not have. CActor::xc8_unk is never assigned either, so nothing would
// ever notice. The behaviour gap is real and is recorded here rather than papered over.
extern "C" void fn_801ECD8C(CActor* actor, CStateManager& mgr) {
  (void)actor;
  (void)mgr;
}
#endif // TARGET_PC
