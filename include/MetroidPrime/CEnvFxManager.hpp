#ifndef _CENVFXMANAGER
#define _CENVFXMANAGER

// **No data members are declared, deliberately.** What retail's three named methods measure:
//
//   Play_801620A8  (0x801620A8)  li r0,0 ; stb r0,0x1384(r3)          +0x1384  a byte, 0 = playing
//   Stop_801620B4  (0x801620B4)  li r0,1 ; stb r0,0x1384(r3)          +0x1384  the same byte, 1
//   SetDensity     (0x801620C0)  stfs f1,0x34(r3) ; int->float, stfs +0x38  a float and the
//                                                                      int argument as a float
//
// so the object is at least 0x1385 bytes, with floats at +0x34/+0x38. Its size, and everything
// else in it, is unmeasured: no `stw` to `CStateManager`'s `+0x1630` (`m_envFxManager`) turns up
// in the DOL's disassembly as a plain store, so the allocation and its `li r3,<size>` were not
// found. A layout padded out to these three offsets would be a guess, and nothing needs one
// yet - `Initialize` is static and touches only globals.
class CEnvFxManager {
public:
  static void Initialize();

  void SetDensity(float, int);
  void Stop_801620B4();
  void Play_801620A8();
};

#endif // _CENVFXMANAGER
