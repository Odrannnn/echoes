// Retail .text 0x80307864-0x8030787C: 24 bytes, three functions, no calls and no
// register pressure. Declared descending by retail offset, which is the order
// mwcceppc has to see them in - it emits function definitions in reverse source
// order and mwldeppc keeps the object's .text order verbatim.
//
// The statics are declared, not defined, and that is the whole trick for this
// block. Two attempts at owning them are both dead ends, and both are worth
// recording:
//
//  * Claiming the seven bytes of .sdata CAudioSys actually owns
//    ([0x80418BE8, 0x80418BEF)) is refused outright - `dtk dol split` fails with
//    "Invalid alignment for split: ... .sdata expected 8, but starts at
//    0x80418BEA". Only the whole eight-byte slot [0x80418BE8, 0x80418BF0) will
//    pass. The nine non-aligned claims already in splits.txt do not disprove
//    that: every one belongs to a `NonMatching` unit, and dtk only checks the
//    alignment of an object it can read.
//
//  * Owning the slot then needs the five bytes *defined* under retail's names,
//    because retail text outside these three functions reads `lbl_80418BE8`,
//    `lbl_80418BEA` and `lbl_80418BEF` (dtk's base object for 0x8030787C) and
//    `lbl_80418BEE` (its base object for 0x80307030). A class static does not
//    satisfy them - the DOL link fails with "undefined: 'lbl_80418BEA'". And
//    defining them as file-scope globals puts them in `.sbss`, not `.sdata`,
//    whatever the initialiser, because their static value is zero.
//
// Leaving the slot unclaimed costs nothing: dtk keeps retail's copy, the DOL
// link resolves against it, and nothing in the port references these names, so
// the port's link gap is unchanged.

#include "Kyoto/Audio/CAudioSys.hpp"

extern "C" {
/// 0x80418BEA, .sdata, 2 bytes. The running volume scale.
extern short lbl_80418BEA;
/// 0x80418BEC, .sdata, 2 bytes. The default volume scale.
extern short lbl_80418BEC;
}

void CAudioSys::SetVolumeScale(short scale) {
  lbl_80418BEA = scale;
}

void CAudioSys::SetDefaultVolumeScale(short scale) {
  lbl_80418BEC = scale;
}

short CAudioSys::GetDefaultVolumeScale() {
  return lbl_80418BEC;
}
