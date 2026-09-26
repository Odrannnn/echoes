#include "Kyoto/Animation/CSkinnedModel.hpp"

// 0x8030F048, 0xC bytes:
//   li      r0,0
//   stw     r0,spointGeneratorFunc(r13)
//   blr
// The displacement is -25032, which through `_SDA_BASE_` 0x8041FD80 is 0x80419BB8: a 4-byte
// uninitialised word in `.sbss` that `config/G2ME01/symbols.txt` called `lbl_80419BB8` and which
// is renamed to the definition below. Two other, still-unclaimed functions store to the same word
// (`fn_8030F054` and `fn_8030F3A4`), so the symbol has to keep existing in the link - which is why
// this unit claims the 4 `.sbss` bytes at 0x80419BB8 rather than letting the linker place the
// static itself. That is the same treatment `gCurrentTimeProvider__13CTimeProvider` gets in
// Kyoto/CTimeProvider.cpp.
void* CSkinnedModel::spointGeneratorFunc = 0;

void CSkinnedModel::ClearPointGeneratorFunc() { spointGeneratorFunc = 0; }
