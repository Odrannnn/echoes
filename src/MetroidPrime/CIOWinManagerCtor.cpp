// Retail 0x80049D84-0x80049E10: `~CIOWinManager` then `CIOWinManager`, 140 bytes, and the two
// are adjacent in the DOL so they are one object here. `configure.py` claims only `.text` for
// this unit; everything the two bodies reach (the `rstl::list<CArchitectureMessage>` destructor
// instantiation, `CMemory::Free`) stays retail's.
//
// Declared descending by retail offset - mwcceppc emits function definitions in reverse source
// order and mwldeppc keeps .text order verbatim, so ascending source order permutes the unit's
// bytes and the DOL's hash while objdiff still reads 100%.
#include "MetroidPrime/CIOWinManager.hpp"

#include "Kyoto/Alloc/CMemory.hpp"

// 0x80049DE8, 0x28 bytes. The only two stores that are not the queue's are the two list heads;
// the other six are `rstl::list<CArchitectureMessage>`'s constructor, inlined into this one
// because it is a member initialiser rather than a call: the allocator word is 0, the four
// node pointers all become this+20 (the address of the list's own `xc_empty_prev`, at this+8+0xC),
// and the count is 0.
CIOWinManager::CIOWinManager() : mDrawRoot(nullptr), mPumpRoot(nullptr) {}

// 0x80049D84, 0x64 bytes. Three things, in this order: `RemoveAllIOWins()`, the out-of-line
// `rstl::list<CArchitectureMessage>` destructor, and `CMemory::Free(this)` - the last only when
// the destructor flag (r4) casts to a positive short, which is CodeWarrior's deleting-destructor
// convention. The `addic. r0,r30,8 / beq` is the dead test on the queue's allocator word that
// the compiler emits in front of the member's destructor.
CIOWinManager::~CIOWinManager() { RemoveAllIOWins(); }
