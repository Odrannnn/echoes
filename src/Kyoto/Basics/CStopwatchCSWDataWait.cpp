// Retail 0x8028C1F8-0x8028C28C: `CStopwatch::CSWData::Wait`, 148 bytes. **NonMatching, 69.16%.**
//
// The two halves of what is left are register allocation, not logic, and both are the kind of
// thing MWCC's idiom selection decides rather than the source:
//
//   * Retail materialises the biased frequency in memory - `stw 0x43300000,24(r1)`,
//     `stw (freq>>2),28(r1)`, then one `lfd f0,24(r1)` - because 2^52 plus an integer below 2^28
//     *is* a double whose high word is the constant 0x43300000 and whose low word is the integer.
//     Written as source-level double arithmetic, mwcceppc keeps it in a register instead and emits
//     `fadd f0,f3,f0 ; fsub f0,f0,f3` for the round trip. The algebra is identical; the instruction
//     sequence is not, and no spelling of the source changes that.
//   * Retail's spin loop spills each `OSGetTime()` result to the stack, reloads it, subtracts,
//     spills *that*, reloads it again and only then does `cmpwi r0,0 / blt`. mwcceppc here keeps
//     the difference in a register and branches on `subfc.` directly. The loop is the same loop.
//
// What this unit *is* good for: it closes `_ZNK10CStopwatch7CSWData4WaitEf` on the port's link-gap
// ratchet, which `CStopwatch::Wait(float)` in src/Kyoto/Basics/CStopwatch.cpp calls, and the port
// gets a working busy-wait. It is `NonMatching`, so none of its bytes are in the DOL and retail's
// stay; that is why claiming the range here is safe.
#include "Kyoto/Basics/CStopwatch.hpp"

// Retail's own 2^52, at .sdata2 0x8041E260: `lbl_8041E260` is 0x4330000000000000 as a double.
extern "C" const double lbl_8041E260;

// The tick count is `wait * ((2^52 + freq) - 2^52)`, and the round trip through 2^52 is what
// retail's `stw 0x43300000,24(r1) ; stw (freq>>2),28(r1) ; lfd f0,24(r1)` is: 2^52 plus an
// integer below 2^28 *is* a double whose high word is 0x43300000 and whose low word is the integer.
void CStopwatch::CSWData::Wait(float wait) const {
#ifdef TARGET_PC
  // Same reason as in CStopwatchCSWData.cpp: 0x800000F8 is a guest address.
  const uint freq = OS_TIMER_CLOCK;
#else
  const uint freq = *reinterpret_cast< volatile uint* >(0x800000F8) >> 2;
#endif
  const double period = (lbl_8041E260 + freq) - lbl_8041E260;
  const u64 ticks = static_cast< u64 >(period * wait);
  const u64 end = ticks + OSGetTime();
  while (static_cast< s32 >(OSGetTime() - end) < 0) {
  }
}
