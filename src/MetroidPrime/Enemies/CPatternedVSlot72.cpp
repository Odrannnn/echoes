// Retail `VSlot72__10CPatternedFv` = `_ZN10CPatterned7VSlot72Ev`,
// .text 0x80073CD0..0x80073CD4, 0x4 = 4 bytes:
//
//     80073cd0  blr
//
// A bare `blr` is an empty `void` function: mwcceppc has nothing to save `r3` and nothing to
// restore, because `r3` is caller-saved and the frame is empty. The same four bytes as
// `CAxisAngle::GetVector` above, arrived at from the other direction - there the reference
// return is the `this` pointer, here there is no return value at all - so the size alone
// does not identify the body.
//
// `VSlot71` (0x80073CD4, 0x38 = 56 bytes) is the CAABox-returning slot immediately above and
// is still unclaimed; it is the next thing in this vtable to write.
#include "MetroidPrime/Enemies/CPatterned.hpp"

void CPatterned::VSlot72() {}
