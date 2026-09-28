#ifndef _CSFXMANAGERPORT
#define _CSFXMANAGERPORT

/**
 * Retail's sound-ID translation table, and the one port function that maintains it.
 *
 * **This header is port-only and must stay that way.** Nothing `configure.py` declares includes
 * it, so mwcceppc never sees it and it cannot move a byte of `main.dol` or of any of the 86 REL
 * modules. It is here rather than as a block on `include/Kyoto/Audio/CSfxManager.hpp` for the
 * reason `src/Kyoto/CSimplePoolPort.cpp`'s header gives about retail's `mTranslationTableTok`:
 * `Kyoto/CSimplePoolCtor.cpp` is a `Matching` unit and includes that header, so anything added
 * there is a change to a matching object.
 *
 * ## What the table is
 *
 * `CSfxManager` maps the game's own per-area sound id onto the runtime sound id the mixer plays.
 * The mapping is `rstl::vector< short >` at `.sbss` `lbl_80419884`, which is
 * `_SDA_BASE_ - 25852` (`python3 tools/sda.py -25852` -> `0x80419884 lbl_80419884 (in .sbss,
 * +0x0)`), and `fn_8029C79C` (`CSfxManager::TranslateSFXID`, 0x4C bytes) reads it as
 * `count = *(int*)(table + 4)` and `items = *(short**)(table + 12)` - **this tree's
 * `rstl::vector`'s own layout** (`include/rstl/vector.hpp:18-21`: `x0_allocator`, `x4_count`,
 * `x8_capacity`, `xc_items`), so the pointer really is a `rstl::vector< short >*` and not a
 * shape-compatible guess.
 *
 * The element type is `short`, not `ushort`, and that is load-bearing rather than cosmetic: a
 * negative entry means "this id has no sound in this area", and `TranslateSFXID` turns it into
 * the invalid id.
 *
 * ## Why the table is null on a PC
 *
 * Its bytes are the `sound_lookup_ATBL` resource, and that resource is in `Strings.pak`, which is
 * **not on the ISO** - measured, `docs/HANDOFF.md` "PROVEN, and it is the answer to 'what would
 * unblock a frame'": 20 `.pak`s on the disc and none named that, so retail's own
 * `CDvdFile::FileExists` probe at 0x800071A8 fails on it too. The pool therefore holds a token
 * over a null object (`src/Kyoto/CSimplePoolPort.cpp`, `fn_8029c7e8`), the `ATBL` factory
 * `fn_8029AB80` is a `return CFactoryFnReturn()` in `src/Kyoto/CFactoryFunctionsPort.cpp` for
 * want of a stream to build the vector from, and the vector is never built.
 *
 * **This is a real absence and not a stand-in.** Nothing here fabricates a mapping, and the
 * consequence is written down where it is observable: `TranslateSFXID` answers
 * `CSfxManager::kInternalInvalidSfxId` (0xFFFF), which is exactly what retail answers when
 * `mTranslationTable` is null, and a sound that cannot be translated is not started. A reach stub
 * instead returned 0 - a plausible-looking *valid* id, and the failure mode
 * `src/MetroidPrime/PortPoolStandIns.cpp` calls the most dangerous possible wrong answer.
 */
#include "types.h"

#include "rstl/vector.hpp"

namespace port {
namespace sfx {

/**
 * Retail's `fn_8029C7E8`'s second statement, verbatim in effect:
 * `if (mTranslationTable) { delete mTranslationTable; } mTranslationTable = nullptr;`
 * (`./tools/dis.sh 0x8029C7E8 0x150`: `+0x30` `lwz r3,-25852(r13)`, `+0x34` `cmplwi r3,0`,
 * `+0x40` `bl fn_80255C00` with `r4 = 1` - the CodeWarrior deleting-destructor spelling, so it
 * is `delete` - and `+0x4C` `stw r0,-25852(r13)`).
 *
 * The vector's storage is file-local to `src/MetroidPrime/PortAudio.cpp`, which also holds
 * `CSfxManager::TranslateSFXID`, the only reader. **No getter is declared on purpose**: the one
 * reader is in the same file, so a second accessor would be a symbol nothing calls.
 */
void ClearTranslationTable();

} // namespace sfx
} // namespace port

#endif // _CSFXMANAGERPORT
