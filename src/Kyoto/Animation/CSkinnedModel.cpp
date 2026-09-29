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
void* CSkinnedModel::sPointGenData = nullptr;

// `SetPointGeneratorFunc__13CSkinnedModelFPvPFRC13CSkinnedModelRC18SSkinningWorkspacePv_v`
// `config/G2ME01/symbols.txt:14285`, `.text:0x8030F054`, size 0xC:
//   stw     r4,-25032(r13)   ; sPointGen   = callback
//   stw     r3,-25028(r13)   ; sPointGenData = context
//   blr
// The companion context word is `sPointGenData__13CSkinnedModel`, `.sbss:0x80419BBC`, the
// word right after `sPointGen`. Retail never clears it, and neither does this.
//
// The GameCube-side copy of this pair is `DolphinCSkinnedModel.cpp` (NonMatching) at
// `config/G2ME01/splits.txt:2315`; only one of the two TUs is in any given build, so neither
// this definition nor that one is a duplicate. This file is port-only (it is not a
// `configure.py` unit), and before this definition the port had no symbol for the setter at
// all: nothing called it until `CActorModelParticles::SetupHook` did.
void CSkinnedModel::SetPointGeneratorFunc(void* context, TPointGenFunc callback) {
  sPointGen = callback;
  sPointGenData = context;
}

void CSkinnedModel::ClearPointGeneratorFunc() { sPointGen = nullptr; }
