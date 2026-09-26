/**
 * `fn_80145C98` - retail `.text:0x80145C98`, `size:0x2F4` = 756 bytes, 0x80145C98..0x80145F8C.
 * The class's own default initialiser: `CPersistentOptions`'s only callee from
 * `fn_80146154` (`CPersistentOptionsCtor.cpp`, Matching at 100%), and therefore a live dependency
 * of a Matching unit on the boot path, because `CGameState::CGameState()`
 * (`CGameStateCtor.cpp`, Matching) runs `fn_80146154(&self->x0dc, 1)`.
 *
 * ## The shape
 *
 * It branches on the word the constructor just stored at +0x00 - `lwz r0,0(r3) ; cmpwi r0,0 ;
 * bne` to the epilogue - so the constructor's `int` argument is a "which game" selector and this
 * is the *first game's* table. Everything after the branch is eleven copies of one statement,
 * straight-line, no loop and no counter:
 *
 *     addi r3,r1,N        bl string_l__4rstlFPCc    // rstl::string_l(lbl_803A9208 + K)
 *     addi r3,r1,N-12     li r4,0 ; li r5,V ; li r6,F
 *                        bl __ct__23SPersistentOptionsValueFiii
 *     mr r5,r3 ; mr r3,r31 ; addi r4,r1,N
 *                        bl fn_80145ACC             // the map insert
 *     addi r3,r1,N        bl internal_dereference   // ~rstl::string, at the end of the statement
 *
 * so it is written here as eleven straight-line statements rather than a loop over a table: a
 * loop would give mwcceppc a `ctr` and a body to unroll, and retail has neither.
 *
 * Note the asymmetry in how the two addresses reach the register file: the **string's** is
 * re-derived (`addi r4,r1,N`), because r3 has since been reused for the value's slot, while the
 * **value's** is forwarded out of its own constructor call (`mr r5,r3`). That is why the value
 * has to be a real C++ constructor - see the note on `SPersistentOptionsValue` below.
 *
 * ## The eleven rows, and the two things that are not logic
 *
 * The name is **not a string literal**. Retail materialises each one as
 * `lis r4,0x803B ; addi r4,r4,-28152 ; addi r4,r4,K`, i.e. `lbl_803A9208 + K` with a relocation
 * against the one merged `.rodata` pool object - the same spelling, and the same reason, as
 * `CGameStateStreamCtor.cpp`'s two `CBasics::Stringize` sites: a literal would make this a
 * `Matching` unit that owns `.rodata`. `lbl_803A9208` is `.rodata:0x803A9208`, `size:0x1C8`, and
 * the eleven names sit at these offsets inside it, each paired with the value's three words
 * (measured with `tools/lanediff.sh`; the names are the game's own option keys and the values
 * are `{lo, hi, default}`):
 *
 *   +213 "FreezeInstructionsFirstPerson"  (0, 3,   0)   +346 "AllPickupsFound"        (0, 1, 0)
 *   +243 "FreezeInstructionsMorphBall"    (0, 3,   0)   +362 "AutoMapperPaneMode"     (0, 2, 1)
 *   +271 "PowerbombPickupMessages"        (0, 1,   0)   +381 "LogbookLegendVisible"   (0, 1, 1)
 *   +295 "PercentScans"                   (0, 100, 0)   +402 "IngAttachedWarningCount"(0, 3, 0)
 *   +308 "NormalModeCompleted"            (0, 1,   0)   +426 "SeenIntroText"          (0, 1, 0)
 *   +328 "HardModeCompleted"              (0, 1,   0)
 *
 * The value is the 12-byte `{lo, hi, value}` of retail's constructor at 0x801462DC, which stores
 * the three arguments at +0/+4/+8 and then calls 0x801461AC - a clamp of +8 into [+0,+4]. All
 * eleven rows have `lo == 0`, so the clamp is a no-op for every one of them; the three numbers
 * are still passed because retail passes them.
 *
 * ## The frame
 *
 * `stwu r1,-336(r1)`, LR at 340(r1) and r31 at 332(r1), and the eleven rows' two objects occupy
 * 8(r1)..312(r1) - eleven disjoint 28-byte regions, the `rstl::string` in the upper twelve and
 * the value in the lower twelve, each region's base stepping down by exactly 28. That is
 * mwcceppc giving every statement its own slot and **not** reusing one slot across eleven
 * statements whose temporaries do not overlap, which is also why these are eleven separate
 * statements and not a loop or a table walk.
 */

// `#define _CMEMORY` keeps `Kyoto/Alloc/CMemory.hpp`'s own throwing `operator new` out of this
// translation unit, and it has to come before *any* include because `rstl/string.hpp` pulls
// `Kyoto/Alloc/CMemory.hpp` in through `rstl/rmemory_allocator.hpp`. See `CGameStateCtor.cpp`.
#define _CMEMORY

#include "types.h"

#include "MetroidPrime/Player/CPersistentOptions.hpp"
#include "MetroidPrime/Player/SPersistentOptionsValue.hpp"

#include "rstl/string.hpp"

#if !defined(__MWERKS__)
#include <new>
#endif

// The merged `.rodata` pool object, declared only - a `Matching` unit may not own one. **Its
// definition is in `src/MetroidPrime/PortGlobals.cpp`**, byte for byte the DOL's own `.rodata`,
// and it was NOT there until this lane added it: the symbol appeared nowhere in the tree, so
// listing this unit in the port build made the link ask for it. See the comment on the definition
// for the eleven offsets and for the one stale claim about +60.
extern "C" const char lbl_803A9208[];

// `fn_801462DC`'s `this`, and this function's second argument type. The class is retail's, and it
// now lives in `include/MetroidPrime/Player/SPersistentOptionsValue.hpp` rather than being spelled
// out here, because the unit that *defines* the constructor
// (`SPersistentOptionsValueCtor.cpp`, Matching at 100%) has to have the same class. The two copies
// were token-identical, so this include is a no-op for codegen - measured, this unit stays at
// 100.00% with the local class removed.
//
// **The constructor has to be a real C++ constructor, not an `extern "C"` helper.** Retail's
// caller forwards the address out of the call - `bl fn_801462DC ; mr r5,r3` - and mwcceppc only
// knows to do that for its own constructors, which are contractually `this` in and `this` out.
// Declaring `SPersistentOptionsValue fn_801462DC(int,int,int)` as a by-value return instead
// produces the identical frame and the identical call sequence but re-derives the address with
// `addi r5,r1,288` instead of `mr r5,r3` - 11 instructions out of 189, measured, and the only
// thing standing between this unit and Matching. The price is that the constructor is a mangled
// symbol, so `config/G2ME01/symbols.txt` has to carry that name for 0x801462DC; only
// `fn_80145A2C` and this function call it, and neither is written as source, so the rename
// touches no other unit. **Confirmed, and the constructor is now written**: see
// `SPersistentOptionsValueCtor.cpp`.

extern "C" {
// The map insert. `fn_80145B90(&r1+16, self+4)` searches the map at `this+4`; on a miss it copies
// the key and calls `fn_80146338` with `self->x14`, so this is a "set if absent". Both class
// arguments are taken by reference - `fn_80145ACC` copies the key itself, at 0x80145B34.
void fn_80145ACC(CPersistentOptions* self, const rstl::string& name,
                 const SPersistentOptionsValue& value);
} // extern "C"

// C linkage for the same reason as every other retail-named function in this tree: retail's
// symbol table has no name for it, and a C++ `CPersistentOptions::` spelling would mangle to
// something objdiff has nothing to pair with. `CPersistentOptionsCtor.cpp` is the only caller.
extern "C" void fn_80145C98(CPersistentOptions* self) {
  // The +0x00 word is the constructor's `int` argument; a non-zero value is the other game and
  // skips the whole table. Read through `int*` because the header keeps +0x00..+0x1B as one
  // opaque `u8[0x1C]`.
  if (reinterpret_cast< const int* >(self)[0] != 0) {
    return;
  }

  fn_80145ACC(self, rstl::string_l(lbl_803A9208 + 213), SPersistentOptionsValue(0, 3, 0));
  fn_80145ACC(self, rstl::string_l(lbl_803A9208 + 243), SPersistentOptionsValue(0, 3, 0));
  fn_80145ACC(self, rstl::string_l(lbl_803A9208 + 271), SPersistentOptionsValue(0, 1, 0));
  fn_80145ACC(self, rstl::string_l(lbl_803A9208 + 295), SPersistentOptionsValue(0, 100, 0));
  fn_80145ACC(self, rstl::string_l(lbl_803A9208 + 308), SPersistentOptionsValue(0, 1, 0));
  fn_80145ACC(self, rstl::string_l(lbl_803A9208 + 328), SPersistentOptionsValue(0, 1, 0));
  fn_80145ACC(self, rstl::string_l(lbl_803A9208 + 346), SPersistentOptionsValue(0, 1, 0));
  fn_80145ACC(self, rstl::string_l(lbl_803A9208 + 362), SPersistentOptionsValue(0, 2, 1));
  fn_80145ACC(self, rstl::string_l(lbl_803A9208 + 381), SPersistentOptionsValue(0, 1, 1));
  fn_80145ACC(self, rstl::string_l(lbl_803A9208 + 402), SPersistentOptionsValue(0, 3, 0));
  fn_80145ACC(self, rstl::string_l(lbl_803A9208 + 426), SPersistentOptionsValue(0, 1, 0));
}
