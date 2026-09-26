#include "types.h"

#include "MetroidPrime/Player/CGameStateBlocks.hpp"

// Retail .text:0x80142A10, size:0x20 = 32 bytes, 8 instructions: a 16-byte frame, `bl`, and the
// epilogue. The copy constructor of `SGameStateBlock`, the 16-byte element of the two
// `{count; block[3]}` members at `CGameState+0x110` and `+0x144`.
//
// It forwards `r3` to `fn_80004D5C` (0x80004D5C) and passes `r4`/`r5` through untouched - there
// is no register shuffle in the whole body. `fn_80004D5C` reads only `r3`, so the two higher
// argument registers are dead here and retail left them alone; that is what makes the forward a
// one-liner, and writing anything that *uses* them adds instructions retail does not have.
//
// It is `extern "C"` rather than `SGameStateBlock::SGameStateBlock(const SGameStateBlock&)`
// because retail's symbol table has no name for it: `config/G2ME01/symbols.txt:5366` calls it
// `fn_80142A10`, and a C++ copy constructor would mangle to something objdiff has no retail
// symbol to pair against. The same reason as `src/MetroidPrime/Player/CHintOptionsCtor.cpp`.
//
// Own range: .text 0x80142A10..0x80142A30. That is **discontiguous** from the 164 bytes of
// `fn_80144924` + `fn_8014495C` (0x80144924..0x801449C8), which is 0x848 bytes away and is a
// second unit, `MetroidPrime/Player/CGameStateSlotsCtor.cpp` - a `configure.py` unit may claim
// only one range.
extern "C" void fn_80004D5C(SGameStateBlock* self, const SGameStateBlock* src);

extern "C" void fn_80142A10(SGameStateBlock* self, const SGameStateBlock* src) {
  fn_80004D5C(self, src);
}
