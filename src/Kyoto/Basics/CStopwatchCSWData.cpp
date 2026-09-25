// Retail 0x8028C17C-0x8028C1F8: `CStopwatch::CSWData::Initialize`, 124 bytes.
//
// Why this exists as its own file: `CStopwatch::Reset()` and `GetElapsedTime()` are `inline` in
// include/Kyoto/Basics/CStopwatch.hpp but both reach `CSWData::Initialize`, so
// `CGameArchitectureSupport::UpdateTicks` pulls these two in *without naming them*. They were
// therefore invisible to the link gap until something above them compiled, and
// `_ZN10CStopwatch7CSWData10InitializeEv` / `_ZNK10CStopwatch7CSWData4WaitEf` were on the ratchet
// with nothing in the tree to blame.
//
// `CStopwatch::CSWData::Wait` is retail's next 148 bytes and is in
// src/Kyoto/Basics/CStopwatchCSWDataWait.cpp, which is `NonMatching` - see that file and
// docs/research/frame_loop.md for why it is not 100%.
#include "Kyoto/Basics/CStopwatch.hpp"

// Retail's own 1.0f, at .sdata2 0x8041E258 - `lbl_8041E258`, `lfs f0,-16744(r2)` in the body
// below. It is named here rather than written as `1.0f` because a literal puts four bytes in
// *this* object's .sdata2, and a `Matching` object's .sdata2 is linked into the DOL: it grew
// .sdata2 from 0x54C0 to 0x54E0, which moved the BSS address and broke the DOL's sha1 with every
// function in every unit still at 100%. The whole CMainFlow string and this float are the same
// trap - see docs/research/frame_loop.md.
extern "C" const float lbl_8041E258;

// 0x8028C17C, 0x7C bytes.
//
// The three fields, in this order, and nothing else:
//   `lwz r5, 0xF8(0x80000000)`  - the CPU-frequency register, which on hardware is at
//                                 0xCC0000F8; in this DOL the read lands at 0x800000F8.
//   `srwi r3, r5, 2`            - divided by 4, because the timebase ticks at a quarter of the
//                                 CPU clock. Stored as a 64-bit value, low word first and then
//                                 the zero high word.
//   `__div2i(x0_timerFreq, 1000000)` - r5:r6 is `li r5,0` / `addi r6,r4,0x4240` with r4 =
//                                 `lis r4,15`, i.e. the *second* argument is the constant
//                                 1000000 built as 0x000F4240, and the result lands in r3:r4.
//                                 This is x8_timerFreqO1M: ticks per microsecond, which is what
//                                 `GetElapsedMicros()` divides by.
//   `__cvt_sll_flt(x0_timerFreq)` then `fdivs` against the float 1.0f at 0x8041E258 - seconds
//   per tick, which is what `GetElapsedTime()` multiplies by.
//
// The return value is 1 (`li r3,1`), so retail's is `bool`, matching the declaration in the
// header.
bool CStopwatch::CSWData::Initialize() {
#ifdef TARGET_PC
  // The retail body below reads the CPU-frequency register, and it does so at the *guest* address
  // 0x800000F8, which is a null-page address on a host: left alone this is a segfault the moment
  // `CStopwatch::Reset()` calls it, and `Reset()` is on the frame loop's path
  // (`CGameArchitectureSupport::UpdateTicks`, main.cpp:268). Aurora's `OSGetTime()` counts in units
  // of 1/OS_TIMER_CLOCK second (extern/aurora/lib/dolphin/os/OSTime.cpp, `duration_to_ticks`), so
  // OS_TIMER_CLOCK is the frequency the period has to be derived from for `GetElapsedTime()` to
  // return seconds. One consequence is recorded in docs/research/frame_loop.md:
  // x8_timerFreqO1M is *ticks per microsecond* and 60750/1000000 truncates to 0, so
  // `GetElapsedMicros()` divides by zero. Nothing calls it; whoever does has to fix it.
  x0_timerFreq = OS_TIMER_CLOCK;
#else
  x0_timerFreq = *reinterpret_cast< volatile uint* >(0x800000F8) >> 2;
#endif
  x8_timerFreqO1M = x0_timerFreq / 1000000;
  x10_timerPeriod = lbl_8041E258 / static_cast< float >(x0_timerFreq);
  return true;
}
