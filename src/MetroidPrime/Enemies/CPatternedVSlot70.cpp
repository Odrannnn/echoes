// Retail `VSlot70__10CPatternedFv` = `_ZN10CPatterned7VSlot70Ev`,
// .text 0x80073D0C..0x80073D14, 0x8 = 8 bytes:
//
//     80073d0c  li   r3,0
//     80073d10  blr
//
// The base `CAi`/`CPatterned` default for a virtual the derived creatures do not override,
// so it returns a constant. `VSlot68` (0x80073C90) is the same shape with `li r3,1`, and
// `VSlot69` (0x80073C9C) returns a float, which is why the three are 8, 8 and 12 bytes rather
// than all 8.
//
// Its own unit: `VSlot72` at 0x80073CD0 is 0x3C bytes below with `VSlot71` between them, and a
// unit may not claim two discontiguous ranges in one section.
#include "MetroidPrime/Enemies/CPatterned.hpp"

int CPatterned::VSlot70() { return 0; }
