/**
 * `fn_8000934C` - retail `.text:0x8000934C`, `size:0x50` = 80 bytes, 0x8000934C..0x8000939C.
 * The next symbol, `__dt__12CPlayerStateFv` (retail 0x8000939C, 0x70), is `CPlayerState`'s
 * deleting destructor and starts there, so that is the exact end of the range this unit claims.
 *
 *     80009360  lwz   r4,4(r3)     the refcount word
 *     80009364  lwz   r3,0(r4)     *refcount
 *     80009368  addic. r0,r3,-1    --*refcount, and the branch condition
 *     8000936c  stw   r0,0(r4)
 *     80009370  bgt   0x80009388   still referenced: done
 *     80009374  lwz   r3,0(r31)    the object
 *     80009378  li    r4,1         the deleting destructor's flag
 *     8000937c  bl    0x8000939c   __dt__12CPlayerStateFv
 *     80009380  lwz   r3,4(r31)    the refcount word again
 *     80009384  bl    0x802ce388   CMemory::Free
 *
 * So it is `rstl::rc_ptr<CPlayerState>::ReleaseData()` on the **8-byte** rc_ptr that
 * `CGameState::CGameState()` builds four of - the `{CPlayerState*, int*}` pair whose second word
 * is a separately `new(4)`'d counter set to 1. `include/rstl/rc_ptr.hpp` has the whole argument
 * for that layout, and `src/MetroidPrime/Player/CGameStateCtor.cpp` carries the same pair under
 * the name `SPlayerStateRef` because that unit needs the copy side too. Retail instantiates
 * `ReleaseData` per type and leaves this one unnamed in the symbol table, so this is an
 * `extern "C"` free function under retail's own name: a C++ spelling would mangle to
 * `ReleaseData__Q24rstl15rc_ptr<12CPlayerState>Fv`, which this symbol table does not have.
 *
 * **Both callers already declare this name** - `CGameStateCtor.cpp:93` and
 * `CGameStateStreamCtor.cpp:222`, both as `void fn_8000934C(void*)` - so this definition is what
 * they link against and nothing in `config/G2ME01/symbols.txt` is renamed.
 *
 * ## Claiming this range is BLOCKED - `dtk`'s link-order cycle. Do not register this file.
 *
 * **The body below is byte-exact and correct, and it cannot be linked.** Measured, this session:
 * adding `MetroidPrime/Player/CPlayerStateRefRelease.cpp: .text 0x8000934C..0x8000939C` to
 * `config/G2ME01/splits.txt` makes `dtk dol split` fail with
 *
 * ```
 * Cyclic dependency encountered while resolving link order:
 *     MetroidPrime/main.cpp -> MetroidPrime/Player/CPlayerStateRefRelease.cpp
 * ```
 *
 * and the cycle is **not** the two units named:
 *
 *   1. `tools/range_owner.py` puts 0x8000934C inside `MetroidPrime/main.cpp`'s claim of
 *      `.text 0x800053B8..0x80009880`, so the range has to come out of that unit first. And a
 *      `configure.py` unit **cannot claim two discontiguous ranges in one section** - the same
 *      failure `docs/research/unidentified.md` records for `CModelDataModelSlots.cpp` - so
 *      main.cpp cannot keep the two pieces either side of a 0x50-byte hole.
 *   2. Splitting main.cpp and re-adding the range does not fix it either. The real cycle is four
 *      hops: `main.cpp` calls `fn_801449C8` (`CGameStateCtor.cpp`, reloc
 *      `R_PPC_REL24 fn_801449C8` in `build/G2ME01/obj/MetroidPrime/main.o`), which calls
 *      `fn_8000934C` at 0x80144C40, which is this unit, which calls `__dt__12CPlayerStateFv` at
 *      0x8000939C - and that address is still inside main.cpp.
 *   3. Moving `__dt__12CPlayerStateFv` out does not fix it either. Its own callees are
 *      `__dt__Q212CPlayerState16SPersistentStateFv` (0x80009508),
 *      `__dt__19CStaticInterferenceFv` (0x80009460) and
 *      `__dt__Q24rstl45vector<9TUniqueId,...>Fv` (0x8000940C) - all in main.cpp - and the tail of
 *      that group reaches `ReleaseData__Q24rstl34rc_ptr<24IArchitectureMessageParm>Fv` at
 *      0x80008f40, which is main.cpp too.
 *
 * **The minimum carve-out that would work**, all of it measured off `symbols.txt`:
 * `0x80008f40..0x8000934C` (11 functions, 0x40C bytes) **and** `0x8000939C..0x80009880`
 * (13 functions, 0x4E4 bytes) must both leave main.cpp, as two new units, before this 0x50-byte
 * range can be claimed. That is 24 functions of `main.cpp` re-homed to unwind an 80-byte win, and
 * it is a separate piece of work with a real chance of moving main.cpp's own report, so it is not
 * bundled with the `CGameState` constructor work. `fn_8000934C` is **not** the port's blocker -
 * `fn_80009898`/`fn_800098CC` are, and they are 0x5498 bytes away in an unclaimed range.
 *
 * **The file is deliberately left in `src/` and deliberately not registered.** It is not a
 * finished unit and the tree must not look as though it were; a source that no `configure.py`
 * entry compiles reads in `build/report.json` exactly like work that was never started.
 *
 * ## Two register facts, both measurements
 *
 * - **`self` lives in `r31`, not `r3`.** It is read twice (`0(r31)` and `4(r31)`) with a call in
 *   between, so it has to survive in a callee-saved register; the prologue's `stw r31,12(r1)`
 *   with no `r30` at all is the consequence.
 * - **The destructor is called by name, not through `delete`.** `delete ptr` on a
 *   `CPlayerState*` makes mwcceppc emit the `cmplwi r3,0 ; beq` null test of its own, which
 *   retail has no trace of - the same effect `include/rstl/rc_ptr.hpp` records when it says
 *   writing the test by hand makes mwcceppc emit it twice. Naming `__dt__12CPlayerStateFv` and
 *   passing `1` reproduces `lwz r3,0(r31) ; li r4,1 ; bl` exactly, and `CGameStateBlockDtor.cpp`
 *   uses the same naming for the same reason.
 *
 * **Not in `files.cmake`, measured** - see this file's entry in `tools/check_files_cmake.py`.
 */
#include "types.h"

#include "Kyoto/Alloc/CMemory.hpp"

#include "MetroidPrime/Player/CPlayerState.hpp"

// `__dt__12CPlayerStateFv` is retail's own name for `CPlayerState`'s deleting destructor
// (0x8000939C), so it links without a rename. See the header comment for why it is called
// directly instead of through `delete`.
extern "C" void __dt__12CPlayerStateFv(CPlayerState* self, int flag);

// The 8-byte `rstl::rc_ptr<CPlayerState>`: a pointer and a separately allocated refcount word.
// `CGameStateCtor.cpp` needs the same pair *with* its copy constructor and declares its own copy
// of it, so nothing is shared and no header changes.
struct SPlayerStateRef {
  CPlayerState* x0_ptr;
  int* x4_refCount;
};
CHECK_SIZEOF(SPlayerStateRef, 8)

extern "C" void fn_8000934C(void* self) {
  SPlayerStateRef* ref = static_cast< SPlayerStateRef* >(self);
  if (--(*ref->x4_refCount) > 0) {
    return;
  }
  __dt__12CPlayerStateFv(ref->x0_ptr, 1);
  CMemory::Free(ref->x4_refCount);
}
