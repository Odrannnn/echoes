// `CGraphics::GetUseVideoFilter`, carved out of dtk's `auto_03_802BE9D8_text` range.
// Retail .text 0x802BEC1C..0x802BEC24, 0x8 = 8 bytes:
//
//     lbz r3,-29313(r13)   ; .sdata 0x80418AFF
//     blr
//
// One byte load and a return, so the body is a read of a single-byte global. Retail leaves that
// global unnamed; it is `lbl_80418AFF` in `config/G2ME01/symbols.txt` and it is the word
// `SetUseVideoFilter` (0x802BEC24) writes with `stb r3,-29313(r13)`, so the pair is one flag.
//
// **The `lbl_` spelling is forced, and it is the same rule the two existing CGraphics units
// follow.** `CGraphics` has no `.cpp` anywhere in the tree, so a reference to a C++-named
// static data member would mangle to a symbol nothing defines and the DOL link would fail.
// `include/Kyoto/Graphics/CGraphics.hpp:364` declares the accessor and its
// `mLastFrameUsedAbove`-style members stay **undefined**; `src/MetroidPrime/PortGlobals.cpp`
// defines the `lbl_` objects for the port build. Read that file before touching either
// spelling. The returned value is a byte in `.sdata`, i.e. initialised, not `.sbss`.
//
// Its own unit because `fn_802BEC14` (0x802BEC14..0x802BEC1C) sits directly above and
// `SetUseVideoFilter` (0x802BEC24) directly below; `fn_802BEC14` is an unnamed 8-byte accessor
// for a different `.sbss` word and is not written here.

#include "Kyoto/Graphics/CGraphics.hpp"

extern "C" uchar lbl_80418AFF;

GXBool CGraphics::GetUseVideoFilter() { return lbl_80418AFF; }
