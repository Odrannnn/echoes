// CDarkSamus::GetFloatParameter, retail DarkSamus .text 0x00016ADC, 8 bytes.
//
// `li r3, 8 ; blr`. The class is not named in retail and nothing else in the
// module is identified, so the unit is a single eight-byte accessor claimed on
// its own: a unit may not claim two discontiguous ranges in one section, and
// 0x16AE4 (a bounding-box height scaled by a .rodata float) is 0x8 bytes away.
//
// Keep definitions in descending retail .text order: MWCC emits them in reverse
// source order.
#include "types.h"

extern "C" {

int fn_10_16ADC(void*) { return 8; }

}
