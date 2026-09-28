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
//
// The name is upstream's: the merged `CSkinnedModel.hpp` declares
// `static TPointGenFunc sPointGen;`, `config/G2ME01/symbols.txt:21159` pins
// `sPointGen__13CSkinnedModel` to the same `.sbss:0x80419BB8`, and the GameCube-side
// `DolphinCSkinnedModel.cpp` (NonMatching) already defines the pair `sPointGen`/`sPointGenData`
// and clears it as `sPointGen = nullptr`. So `spointGeneratorFunc` is `sPointGen` renamed: a
// 4-byte static (a function pointer and a `void*` are both one word on GC), not an instance
// member, so no class offset moves, and the store of 0 is unchanged.
//
// This unit is the GameCube matching unit and is listed in `files.cmake` but not in
// `configure.py`; `DolphinCSkinnedModel.cpp` is the reverse. Each build sees exactly one of
// the two definitions of `sPointGen`/`ClearPointGeneratorFunc`.
CSkinnedModel::TPointGenFunc CSkinnedModel::sPointGen = nullptr;

void CSkinnedModel::ClearPointGeneratorFunc() { sPointGen = nullptr; }
