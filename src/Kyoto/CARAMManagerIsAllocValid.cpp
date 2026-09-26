// Retail `IsAllocValid__12CARAMManagerFPCv` = `_ZN12CARAMManager12IsAllocValidEPKv`,
// .text 0x80301718..0x8030172C, 0x14 = 20 bytes:
//
//     80301718  addi    r4,r3,1
//     8030171c  subfic  r0,r3,-1
//     80301720  or      r0,r4,r0
//     80301724  srwi    r3,r0,31
//     80301728  blr
//
// `ptr != kInvalidAlloc`, i.e. "is this a pointer ARAM handed out rather than the -1
// failure marker". The idiom is MWCC's compare-with-(-1) folded to a sign bit:
//
//     r4 = ptr + 1        (zero exactly when ptr == -1)
//     r0 = ~ptr           (zero exactly when ptr == -1)
//     r0 = r4 | r0        (zero exactly when ptr == -1)
//     r3 = r0 >> 31       (1 iff r0's top bit is set, i.e. iff ptr != -1)
//
// `r0 >> 31` as the bool is what makes this 20 bytes rather than the 16 a `cmpw`+`bne` pair
// would take: MWCC computes the value in a register and normalises it in one shift, so there
// is no branch at all.
//
// Its own unit because `GetInvalidAlloc` (0x80301710, 8 bytes) is the function immediately
// above it, and one unit may not claim two `.text` ranges in a section.
//
// The order of the comparison is load-bearing: **MWCC keeps the source order of the two
// operands**, so `ptr != (const void*)-1` computes `~ptr` first and lands it in `r0`, while
// `(const void*)-1 != ptr` computes `ptr + 1` first and puts it in `r4`. Retail is the second,
// and the two differ by exactly two register assignments. All twelve other spellings tried
// (`(unsigned)ptr`, `(int)ptr`, `(long)ptr`, `(intptr_t)ptr`, `!=(0xFFFFFFFFu)`, the
// explicitly written `((x+1) | ~x) >> 31`) produce either this same swap or a different
// shape; only the reversed comparison matches.
//
// `src/Kyoto/CARAMManager.cpp` is not a `configure.py` unit, so this body is in the port's
// build and nothing else defines the symbol there.
#include "Kyoto/CARAMManager.hpp"

bool CARAMManager::IsAllocValid(const void* ptr) { return (const void*)-1 != ptr; }
