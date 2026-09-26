// Retail `GetInvalidAlloc__12CARAMManagerFv` = `_ZN12CARAMManager15GetInvalidAllocEv`,
// .text 0x80301710..0x80301718, 0x8 = 8 bytes:
//
//     80301710  li   r3,-1
//     80301714  blr
//
// The ARAM "allocation failed" sentinel, returned by `Alloc` and compared against by
// `IsAllocValid` immediately below (0x80301718). It is a compile-time constant, which is why
// the body is a materialise and not a load of the static: **the header's
// `static const int kInvalidAlloc` has no in-class initialiser**, so a body that read the
// member would emit an SDA2 `lwz` and the four bytes would not match. The value is written as
// the literal `-1` and the header's own comment (`// { return (const void*)kInvalidAlloc; }`)
// records the identity.
//
// The alternative - giving `kInvalidAlloc` an in-class initialiser so the read folds - was not
// taken: `src/Kyoto/CARAMManager.cpp` carries the out-of-class definition
// `const int CARAMManager::kInvalidAlloc = -1;`, and an initialiser in both places is a
// redefinition in the host build, which is the same duplicate-definition trap
// `tools/gate.sh`'s `port link dups` step exists to catch.
#include "Kyoto/CARAMManager.hpp"

const void* CARAMManager::GetInvalidAlloc() { return (const void*)-1; }
