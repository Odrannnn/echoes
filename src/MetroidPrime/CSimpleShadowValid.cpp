// Retail `Valid__13CSimpleShadowCFv` = `_ZNK13CSimpleShadow5ValidEv`,
// .text 0x800DF268..0x800DF274, 0xC = 12 bytes:
//
//     800df268  lbz     r0,72(r3)        ; 0x48
//     800df26c  rlwinm  r3,r0,25,31,31
//     800df270  blr
//
// `rlwinm r3,r0,25,31,31` is the **first** `bool : 1` of a `u8`-sized bitfield group, read: the
// encoding sequence for a read is `,25,31,31` / `,26,31,31` / `,27,31,31` for the first, second
// and third field, measured here against the three fields
// `include/MetroidPrime/CSimpleShadow.hpp` already declares. The first is
// `x48_24_collision`, so **`Valid()` returns the collision flag** - the header's names and
// retail's encodings line up, which is the check that this is the same field and not a
// coincidence of the numbering. (The write side of the same rule is
// `src/MetroidPrime/CSimpleShadowSetAlwaysCalculateRadius.cpp`: `,7,24,24` for the first field,
// `,6,25,25` for the second.)
//
// Its own unit: `Calculate` (`fn_800DF274`, 0x168 bytes) starts immediately below at 0x800DF274
// and `GetBounds` is claimed by `CSimpleShadowGetBounds.cpp` further down, and a unit may not
// claim two discontiguous ranges in one section.
#include "MetroidPrime/CSimpleShadow.hpp"

bool CSimpleShadow::Valid() const { return x48_24_collision; }
