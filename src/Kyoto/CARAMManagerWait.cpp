// Retail 0x8030184C, 0x40 bytes. Unnamed in retail; it sits in dtk's
// auto_03_80301710_text blob, between CancelDMA (0x80301800) and WaitForDMACompletion
// (0x8030188C), both of which are named `*__12CARAMManagerFUi`. Together with the
// 0x74-byte function it calls (0x8030174C) this is a pair of
// `CARAMManager::WaitForAllDMAsToComplete` / `CARAMManager::RefreshActiveDMAList`.
//
// Identified by measurement, not by the name:
//   * 0x8030174C loads 0x804175B8, walks it with `lwz 4(reg)` as the cursor and
//     `lwz 8(reg)` as the end, and for each cursor step it reads the *next* node and tests a
//     byte at next+0x24 - which is `item + 0x1C`, since rstl::list<T>::node puts the item at
//     +8. It frees and erases when that byte is set. x4_start / x8_end, and rstl::list is
//     0x18 bytes with `int x14_count` at +0x14 (include/rstl/list.hpp).
//   * 0x804175B8 is a 0x18-byte .bss object, so it is one rstl::list.
//   * This function then loops while `*(int*)(0x804175B8 + 0x14) > 0` calling that
//     refresh: "spin until the active-DMA list is empty". src/Kyoto/CARAMManager.cpp
//     already carries that body under the name WaitForAllDMAsToComplete, and
//     include/Kyoto/CARAMManager.hpp already declares it; neither is in the port build,
//     because that header is still a stub (see the head of files.cmake).
//   * Its two callers agree: 0x80301C80, next to Alloc/Free/DMAToARAM, and
//     CStateManager::fn_8003FF24 at 0x8003FF34, which brackets it with
//     CFrameDelayedKiller::StallAndFlushAllAllocations and CARAMToken::UpdateAllDMAs.
//
// The port keeps the `fn_` name deliberately. Retail does not name it, and CStateManager
// already calls it under that name, so a rename would only mean editing two files to move
// a symbol that has to be called by the same name from both builds.
#ifndef TARGET_PC
extern "C" {
// 0x804175B8: rstl::list< CARAMManager::SAramDMARequest* > CARAMManager::mActiveDMAs.
extern int lbl_804175B8[];
// 0x8030174C: CARAMManager::RefreshActiveDMAList - 0x74 bytes, walks the list above.
extern void fn_8030174C();
} // extern "C"

extern "C" void fn_8030184C() {
  // x14_count is word 5 of the list. A plain `while` is what retail's source says: MWCC
  // rotates it into a do-while with an unconditional branch to the test, which is the `b`
  // at 0x80301864 and the `bgt` back to the call at 0x80301874.
  while (lbl_804175B8[5] > 0) {
    fn_8030174C();
  }
}
#else
// The host has neither the ARAM manager nor the list, so there is nothing to wait for.
// The body is deliberately not faked: calling a stub that pretends a DMA finished would
// hide the fact that the port has no ARAM at all.
extern "C" void fn_8030184C() {}
#endif // TARGET_PC
