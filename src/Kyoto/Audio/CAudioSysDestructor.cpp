// Retail .text 0x8030889C-0x80308930: 148 bytes, one function - CAudioSys's destructor.
//
// mwcceppc emits function definitions in reverse source order and mwldeppc keeps the
// object's .text order verbatim, so a one-function unit needs no ordering care.
//
// All five callees are unnamed in retail, and none of them needs a name to be linked:
// they are plain C-linkage symbols (`fn_80307BC4` and friends in config/G2ME01/symbols.txt),
// so declaring them `extern "C"` and calling them by name reproduces retail's `bl`
// relocations exactly. The same convention is already used by
// Kyoto/Audio/CAudioSysSurround.cpp. The four words this touches live in .sbss that
// splits.txt does not claim, so they resolve against dtk's base objects - which is why
// this unit claims .text and nothing else, exactly like CAudioSysSysVolume.cpp.
//
// What each callee is, from its own disassembly in main.elf:
//   fn_80307BC4  walks the emitter database (stride 0x54 == sizeof(CAudioSys::CEmitterData))
//                and stops every emitter whose +0x50 flag is set, then clears 0x80419B78.
//   fn_80307EEC  if 0x80419B69 is set, clears it and calls fn_8039CF40(0x80419B74).
//   fn_8039E2A0  a four-call ARAM / audio-library shutdown chain, then clears 0x803E7378.
//   fn_803089B4  a cleanup thunk: null-checks its argument, optionally calls fn_80308D38,
//                zeroes +4/+8/+0xC/+0x10, and frees the argument if the int flag is > 0.
//   fn_80308930  the same shape over an array of 0x54-byte elements.
//
// fn_803089B4 and fn_80308930 are compiler-emitted static-cleanup thunks, not source
// functions: the retail map gave them no name, which is why dtk calls them fn_<addr>.
// Giving them invented names would not make anything decompilable; writing the
// destructor that calls them is the whole of the reachable work here.

#include "Kyoto/Audio/CAudioSys.hpp"

#include <Kyoto/Alloc/CMemory.hpp>

extern "C" {
/// 0x80419B68, .sbss, 1 byte. Cleared last.
extern bool lbl_80419B68;
/// 0x80419B6C, .sbss, 4 bytes. The listener; handed to fn_803089B4.
extern void* lbl_80419B6C;
/// 0x80419B70, .sbss, 4 bytes. The emitter database; handed to fn_80308930.
extern void* lbl_80419B70;
/// 0x80419B74, .sbss, 4 bytes. Freed directly through CMemory.
extern void* lbl_80419B74;
/// 0x80307BC4. Unnamed in retail: stops every live emitter.
void fn_80307BC4();
/// 0x80307EEC. Unnamed in retail.
void fn_80307EEC();
/// 0x8039E2A0. Unnamed in retail: the ARAM shutdown chain.
void fn_8039E2A0();
/// 0x803089B4. Unnamed in retail: a static-cleanup thunk.
void fn_803089B4(void*, int);
/// 0x80308930. Unnamed in retail: the same thunk over an array.
void fn_80308930(void*, int);
}

CAudioSys::~CAudioSys() {
  fn_80307BC4();
  fn_80307EEC();
  fn_8039E2A0();
  fn_803089B4(lbl_80419B6C, 1);
  lbl_80419B6C = 0;
  fn_80308930(lbl_80419B70, 1);
  lbl_80419B70 = 0;
  CMemory::Free(lbl_80419B74);
  lbl_80419B74 = 0;
  lbl_80419B68 = false;
}
