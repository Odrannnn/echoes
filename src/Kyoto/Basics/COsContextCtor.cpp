// COsContext::COsContext(bool, bool) - retail 0x8028C09C, 0xE0 bytes.
//
// dtk left this `fn_8028C09C`; it is COsContext's constructor, renamed in
// config/G2ME01/symbols.txt. Retail's `main` (0x801EFB00) calls it at 0x801EFB38
// as `fn_8028C09C(&osContext, 1, 1)` with osContext a stack object at sp+44, and
// the frame is 0xB0 bytes, so 0x44 + sizeof(COsContext) = 0xB0 - which is where
// sizeof(COsContext) = 0x6C comes from.
//
// Three things about the body are retail's and not obvious:
//
//  1. +0x10..+0x0F's siblings are NOT all zeroed. Only +0x18..+0x2C are, and
//     +0x00..+0x0F are left as the caller's stack garbage: there is no store to
//     any of them anywhere in the function. So this constructor has no
//     initialiser list, and adding one to "be tidy" moves real bytes.
//  2. +0x10 is written only on five of the seven paths through the switch, so
//     the switch has **no default arm**. The paths that reach the end of the
//     switch without storing leave +0x10 exactly as the caller left it. An
//     `EConsoleType x = kCT_Retail;` initialiser, or a `default:` arm, changes
//     the instruction count.
//  3. +0x14 is not the console type: it is a language value, stored *before*
//     CBasics::Init. The console type is at +0x10. The two members were named the
//     other way round in this header; see include/Kyoto/Basics/COsContext.hpp.
//
// ## State: `NonMatching`, 12 bytes short of retail, and the 12 bytes are located
//
// Retail's 0xE0 bytes against this object's 0xD4. Everything outside the switch's
// compare chain is byte-identical, including the framebuffer constant: retail
// materialises it as `lis r4,32 ; addi r0,r4,-8192`, which is what the two-part
// `0x200000 - 0x2000` below produces. (A single `0x1E0000` literal compiles to one
// `lis r0,30` and loses 4 bytes; that is the one measurement here that is not obvious.)
//
// The missing 12 bytes are three instructions in the switch, and they are a
// strength reduction mwcceppc 2.7 is not choosing:
//
//   retail   lis   r4,4096 ; cmpw r3,r4      addi r0,r4,4 ; cmpw r3,r0
//            addi  r0,r4,7 ; cmpw r3,r0
//   here     cmpwi r3,4096                    cmpwi r3,4100
//            cmpwi r3,4103
//
// So retail keeps 0x1000 in a register and reaches 0x1004 and 0x1007 with `addi` from
// it, while the same switch written here folds each case into its own `cmpwi`. The
// decision tree, the branch polarity and the block layout are otherwise right, and
// the case-body order in the object is retail's.
//
// What has been tried and measured, and does **not** produce it: `0x1000 + n` and
// `0x1000 - 0xFFF` as the case labels (folded to `cmpwi` anyway); dropping the
// 0x1005 arm and keeping only 0x1004/0x1006 (worse, 14 differing instructions);
// an explicit `default: break;` (identical); and an `if`/`else if` chain instead of
// the switch (much worse, 24). The remaining lever is the compiler's `-O` peephole
// set, not the source, so the next attempt should be a flags experiment rather than
// more spellings.
//
// Two constants here are retail's, read off its own compares, and must not be
// "cleaned up" to the SDK's: `OSGetConsoleType()` returns 0x1000/1/0x1004/0x1005/0x1006
// on retail, while include/dolphin/os.h in this tree defines OS_CONSOLE_EMULATOR as
// 0x10000000 and OS_CONSOLE_RETAIL as 0. Writing the switch with those names
// compiles and produces a 0x89%-shaped function that is wrong everywhere.
//
// The framebuffer is allocated here, not in OpenWindow: +0x2C is set to 0x1E0000
// and then passed straight back through AllocFromArena, whose result lands in
// +0x24. OpenWindow is what replaces +0x24 with the second, real framebuffer.
#include "Kyoto/Basics/COsContext.hpp"

#include "dolphin/os.h"

// CBasics::Init - retail `fn_8028BF68`, 0x8028BF68, 0x70. Unnamed in
// symbols.txt, so it is called through `extern "C"` under its own name, which is
// the spelling src/MetroidPrime/CInputGeneratorUpdate.cpp uses for its three
// retail callees. Its return value is discarded: the body calls OSInit, the
// four cache/scratch `mtspr` masks, DVDInit and CStopwatch::InitGlobalTimer,
// and returns a "did I do it" flag nobody here reads.
extern "C" bool fn_8028BF68();

COsContext::COsContext(bool, bool) {
  x14_language = OSGetLanguage() & 0xF;
  x18_arenaLo1 = nullptr;
  x1c_arenaHi = nullptr;
  x20_arenaLo2 = nullptr;
  x24_frameBuffer1 = nullptr;
  x28_frameBuffer2 = nullptr;
  x2c_frameBufferSize = 0;
  fn_8028BF68();
  switch (OSGetConsoleType()) {
  case 0x1000:
    x10_consoleType = kCT_Emulator;
    break;
  case 0x1:
    x10_consoleType = kCT_Retail;
    break;
  case 0x1004:
    x10_consoleType = kCT_Development1;
    break;
  case 0x1005:
  case 0x1006:
    x10_consoleType = kCT_Development2Or3;
    break;
  }
  x2c_frameBufferSize = 0x200000 - 0x2000;
  x24_frameBuffer1 = AllocFromArena(x2c_frameBufferSize);
}
