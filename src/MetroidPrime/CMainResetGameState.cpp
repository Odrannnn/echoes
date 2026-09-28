/**
 * `CMain::ResetGameState()` - retail `ResetGameState__5CMainFv`, .text:0x80003A48,
 * `size:0x1A0` = 416 bytes, 104 instructions. `config/G2ME01/symbols.txt:55` carries the size.
 *
 * One of only three functions the port's boot still waits on
 * (`src/MetroidPrime/PortBoot.cpp`, at its final stop), and one of only three the `CMain`
 * constructor's call path needs, because `CGameArchitectureSupport`'s constructor
 * (`src/MetroidPrime/main.cpp:350`) calls it and then dereferences `gpGameState` with no null
 * test.
 *
 * ## The shape
 *
 * Five **copy-out** calls, a reallocation, five **copy-in** calls, a teardown - and every one of
 * the ten callees is unnamed in `config/G2ME01/symbols.txt`, so each is called by its dtk label.
 * Retail's own relocations for the function are in the header of
 * `src/MetroidPrime/Player/CGameStateStreamCtor.cpp`'s sibling object, `build/G2ME01/obj/
 * auto_03_80003840_text.o` (this function is at .text+0x208 there, and the relocations at
 * .text+0x220..+0x390 are its). The five pairs:
 *
 *   stack local   member  copy out      copy in
 *   r1+0x4C (76)  +0x54   fn_80005108   fn_80003F08
 *   r1+0xCC (204) +0x80   fn_80004E84   fn_80003D00, then EnsureOptions__12CGameOptionsFv
 *   r1+0x18 (24)  +0x144  fn_80004C90   fn_80142920
 *   r1+0x08 (8)   +0x178  fn_80004AA0   fn_801427DC
 *   r1+0x78 (120) +0x1A0  fn_80004990   fn_80003BE8
 *
 * Each is `(dst, src)` in r3/r4 and each returns `this` in r3, which is what identifies them as
 * **copy constructors** on the way out and **copy assignment** on the way in. The identification
 * is the store pattern, not a guess: `fn_80004E84` writes 0x00..0x27 word by word and then calls a
 * sub-object helper on +0x28, `fn_80004C90` writes +0x00 and hands +0x04 to a helper, `fn_80004990`
 * writes +0x00, the byte at +0x04, +0x08 and +0x0C and hands +0x10 to a helper - the same field
 * list the constructors write for these members, one member at a time.
 *
 * Between them, `fn_80004154(this->gameGlobalObjects + 0x130, 0)`, then `gpGameState = 0`, then
 * `new CGameState()` and the same call again with the new object. **`fn_80004154` is
 * `rstl::single_ptr<CGameState>::operator=(T*)`**: it calls the destructor of `*this` with flag 1
 * and stores the new pointer, and it returns the pointer-to-the-slot, which is what makes
 * `self->gameGlobalObjects + 304` its first argument. `CMain::gameGlobalObjects` is a
 * `CGameGlobalObjects*` at +0x54 and `CGameGlobalObjects::gameState` is the `single_ptr` at +0x130
 * (`include/MetroidPrime/CGameGlobalObjects.hpp`), and `+0x130 + 4 == 0x134` is
 * `memoryCard`, so the slot is one word.
 *
 * `li r3,752 ; bl __nw__FUlPCcPCc` is the **independent confirmation that
 * `sizeof(CGameState) == 0x2F0`**: `CHECK_SIZEOF(CGameState, 0x2f0)` in the header was derived
 * from `CGameGlobalObjects`' constructor's own `li r3,752` at 0x800084C4, and this is a second
 * site with the same constant. The `file` argument of that `new` is **`lbl_803A56C0`**, the merged
 * `.rodata` pool at the start of the section (its first seven bytes are `"??(??)"`) - a *different*
 * object from the `lbl_803A9208` of `CGameStateStreamCtor.cpp`, which is what the relocation says:
 * `R_PPC_ADDR16_HA/LO lbl_803A56C0` at .text+0x282 and +0x28A.
 *
 * ## The state: **`NonMatching` at 98.61%, and the last seven instructions are not reachable from
 * the source as far as I could find**
 *
 * `tools/flip_test.sh MetroidPrime/CMainResetGameState.cpp` **FAILs** and reverts, so this is
 * `NonMatching` and its object is not in the DOL link. `matched` and `linked` are both unchanged
 * by anything in this file; `main.dol` is still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and
 * all 86 RELs are byte-identical, because a `NonMatching` object is not the one the link uses.
 *
 * (The `matched 3187 / linked 1803` this paragraph used to quote was stale - it was written when
 * the tree was at that count. Measured 2026-09-26 in this tree: `matched 3955`, `linked 2532`,
 * unchanged by every variant below.)
 *
 * Everything above is written. What is left is **two register-allocation decisions and nothing
 * else** - 104 instructions, 97 of them byte-identical:
 *
 * 1. **`mr. r4,r3 ; beq` where this file emits `mr. r0,r3 ; mr r4,r0 ; beq`** (one instruction
 *    long). Writing the allocation as `new CGameState` instead of by hand emits retail's exact
 *    three instructions - `mr. r4,r3`, `beq`, `bl`, `mr r4,r3` - because mwcceppc's `new`
 *    expansion puts the result straight into the *argument register* of the following call and
 *    tests it there. That form is **99.62%** and it is one word away: it relocates against
 *    `__ct__10CGameStateFv`, because `CGameState::CGameState()` is declared in the header, and
 *    that symbol is defined nowhere. Making it resolve needs a rename in
 *    `config/G2ME01/symbols.txt:5403` **and** a C++ constructor definition in
 *    `CGameStateCtor.cpp`, and a C++ constructor cannot `return self;` - which is the very reason
 *    that unit is an `extern "C"` function named for its address.
 *
 *    ### MEASURED 2026-09-26: the trade is a hard no, and `new CGameState` really is the only
 *    ### way to lose the 4 bytes. Do not re-run either half.
 *
 *    **`new CGameState` is the only spelling that fits the claim.** With the `new`, `unit_fit.sh`
 *    goes 420 -> **416, "fits"**, and 98.61% -> **99.62%**; `flip_test` then fails on
 *    `undefined: 'CGameState::CGameState()'`, which is the whole of what is left. Eleven
 *    hand-written allocation spellings were measured with `tools/try_batch.py` and **all eleven
 *    are worse than the incumbent**: the `if` this file spells, 9 differing instrs; the ternary,
 *    11; `if/else` with an explicit `= 0`, 13; the two-variable `made = self` spelling from
 *    `CGameGlobalObjectsCtor.cpp`, 9; `register CGameState*`, 9; a separate variable for the
 *    argument, 9; `while (...) { ...; break; }`, 13; the ternary called in argument position, 13;
 *    the ternary with a literal `0` false arm, 13; and hoisting the slot into a local first, 16.
 *    The `mr r0,r3` is the phi node of the null-test and there is no spelling that drops it.
 *
 *    **And the ctor cannot be made a real C++ constructor, which is where the trade dies.** It
 *    was tried in full: `config/G2ME01/symbols.txt:5403` renamed to `__ct__10CGameStateFv`, the
 *    two `fn_801449C8` references in `CGameGlobalObjectsCtor.cpp` renamed with it (the DOL link
 *    needs both), `self` -> `this` throughout, and `return self;` dropped - which MWCC does not
 *    need, because it returns `this` in r3 from a constructor anyway. The object goes
 *    **748 bytes against a 740-byte claim** (`unit_fit.sh`: "over by 8"), and the mechanism is
 *    this: `CGameOptions` has a declared default constructor (`CGameOptions.hpp:20`), so a *real*
 *    `CGameState::CGameState()` makes mwcceppc emit the implicit member construction
 *    `addi r3,r29,128 ; bl __ct__12CGameOptionsFv` at the top of the function (.text+0x1C) **in
 *    addition to** the explicit `CTOR_GAMEOPTIONS(&this->gameOptions)` in the body - the two
 *    instructions that are the 8 bytes. Retail calls it once, at .text+0xA8, between
 *    `fn_80145950(&this->x54)` and `fn_80180738(&this->hintOptions)`, which is the body position,
 *    so retail's own compiler did **not** hoist it either: in retail's headers `CGameOptions` had
 *    no *declared* default constructor to hoist. Deleting the explicit call instead gives 740
 *    bytes that "fit" but only **93.90%**, the call now at .text+0x1C. Neither spelling is
 *    `Matching`, and a mem-initializer list cannot help - mem-inits are all emitted before the
 *    body, and retail's call is in the middle of it.
 *
 *    So the two walls are independent and both are closed: the 4 bytes need a real constructor,
 *    and a real constructor costs 8 bytes on a `Matching` unit. **1 function for 0.**
 * 2. **The empty loop's counter and unroller temporary are in r3/r4 the other way round**
 *    (**7** differing instructions as `tools/try_batch.py` counts them, all register renames of
 *    each other - the "six" an earlier note gave was a `diff` line count, not instructions).
 *    Retail keeps the count in r5, the counter
 *    in r3 - the register the guarded address has just died in - and the unroller's `count - 8`
 *    in r4. This file keeps the count in r5 and puts the counter in r4 and the temporary in r3.
 *    The bound in r5 needs the two-variable `for (int i = 0, n = *count; ...)` spelling, the
 *    signed `int` needs the same, and the `addic.`+`beq` guard needs the pointer to be tested -
 *    all three are here. The remaining swap resisted about thirty spellings, measured with
 *    `tools/try_batch.py`: one variable versus two, `n` declared first, the counter declared
 *    first, `while`, `do/while`, comma-initialisers, `register`, `i++` against `++i` against
 *    `i = i + 1`, `i != n`, a nested `if (n > 0)`, killing the pointer's live range explicitly, a
 *    three-declarator initialiser, a `static inline` wrapper to pass the new pointer as a
 *    *parameter*, and an `extern "C"` asm label (mwcceppc rejects it - it parses `asm("...")` as
 *    its local-register syntax and reports "type cannot be made into a global register
 *    variable"). Everything lands on 98.61%. **A lane with mwcceppc time should try the
 *    `rstl::reserved_vector` route rather than a spelled-out loop**: retail's loop is almost
 *    certainly an inlined `~reserved_vector` (the same ten instructions are inlined into
 *    `fn_8000419C` at 0x800041D4), and the element type must be non-trivially destructible with an
 *    empty destructor for the loop to survive, which is why a spelled-out `for` may never be the
 *    same code.
 *
 *    ### MEASURED 2026-09-26, after the 4 bytes were fixed: the `new` change does **not** move
 *    ### the loop, and 31 more spellings land on the same 7.
 *
 *    The premise that the two interact - that the register pressure from `mr. r4,r3` might change
 *    the loop's allocation - is **false**, and it is cheap to re-check: with `new CGameState` in
 *    place, `tools/try_batch.py` still reports the identical **7 differing instrs**, with the same
 *    seven lines (`li`, `addi rX,r5,-8`, `addi r0,rX,7`, `cmpwi rX,0`, `addi rX,rX,8`,
 *    `subf r0,rX,r5`, `cmpw rX,r5`) and the same register pair. Removing 2 instructions from
 *    .text+0x90 does not perturb .text+0x114.
 *
 *    Thirty-one further spellings, all measured, all 7 or worse: `nullptr != count`,
 *    `0 != count`, `count != 0` (**the operand-order rule does not apply here - the three are
 *    identical**), `const int* const`, a non-`const` `int*`, a `u32*` with a `static_cast<int>`,
 *    `const volatile int*`, `long` counter, `i += 1`, `++i`, a nested block, `if (count)` with the
 *    braces on the `if` rather than the `for`, a `while` with the declarations hoisted above it,
 *    `n` hoisted and `i` in the `for` (10), the counter hoisted and `n` in the `for` (7),
 *    `for (int n = *count, i = 0; ...)` (10), non-`const` `n` hoisted (10), `do/while` (18),
 *    a pointer the loop bumps alongside the counter, and `static_cast<bool>(count)`.
 *
 *    Three results worth keeping:
 *
 *    - **`if (&local1a0.x10_count)` is not an option, and neither is casting at each use.** Both
 *      fold the null test away completely - mwcceppc knows a stack address is not null - and give
 *      `lwz r5,136(r1)` with no `addic.`/`beq` pair at all (10 differing). The guard *has* to be
 *      a null test of a pointer **variable**, or retail's two instructions do not exist.
 *    - **Wrapping the guard+loop in a `static inline` helper is much worse (22), and it is worse
 *      in an informative way**: the inlined `*count` load keeps the bound in the *same register
 *      as the address* (`lwz r3,0(r3)`), so the loop's two temporaries are pushed to r4 and r5.
 *      Retail loads the bound into r5, three registers clear of the guard's r3. That is the
 *      clearest statement of what retail's shape is: the guard's register dies at the `beq` and
 *      the bound is loaded into a *fresh* register.
 *    - **`~reserved_vector` in this tree does produce retail's register order, which is why the
 *      idea is right and only the entry cost is wrong.** `__dt__7CPlayerFv` in
 *      `MetroidPrime/TypesMatch.cpp` (a `Matching` unit) is a compiler-generated
 *      `~rstl::reserved_vector<T,N>` over `CPlayer+0x61C` and it emits
 *      `addic. r0,r30,1564 ; beq ; lwz r5,1564(r30) ; li r3,0 ; ... ; addi r4,r5,-8` -
 *      **counter in r3, temporary in r4, retail's assignment** - where
 *      `__dt__Q24rstl26reserved_vector<6CPlane,6>Fv` in `Kyoto/Math/CFrustumPlanes.cpp` emits
 *      the *wrong* order (`li r6,0` then `addi r3,r5,-8`). The difference is the guard: the
 *      good one tests `r30+1564` into r0 and then re-addresses the load off r30, so the tested
 *      register is free at once; the bad one tests `r3` and loads through it. Retail here tests
 *      r3 **and** loads through it, and still gets the good order - so the guard's shape is not
 *      the discriminator either, and the distinguishing factor has not been found.
 *
 *    Given the entry cost already measured (an out-of-line weak `__dt__`, or 10 differing for a
 *    hand-written `~()`), the remaining honest statement is: **this loop is not reachable from a
 *    spelled-out `for` in this compiler, and the mechanism behind retail's allocation is still
 *    unidentified.** Anything further needs a different idea, not another spelling.
 *
 * The identification of every callee is a measurement and is in the notes on each declaration
 * below; the two that are *not* copy helpers are `fn_80004154`, which is
 * `rstl::single_ptr<CGameState>::operator=(T*)` and is proved by its own body, and
 * `EnsureOptions__12CGameOptionsFv`, which is the one named callee in the function and needs no
 * work.
 */

// `#define _CMEMORY` keeps `Kyoto/Alloc/CMemory.hpp`'s own throwing `operator new` out, so the
// one below is the only `new` in this translation unit - see `CGameStateCtor.cpp`. **Two
// consequences of that define that no other unit in the tree has to carry, and both are measured
// (mwcceppc 2.7, `-nodefaults`):**
//
//   - `CMain.hpp` then does not compile, because `COsContext` - the first parameter type of
//     `CMain`'s constructor - reaches it only through `Kyoto/Alloc/CMemory.hpp`'s include chain,
//     and the error mwcceppc reports is the misleading `illegal constructor/destructor
//     declaration` on `CMain(COsContext*, ...)` at CMain.hpp:29. `Kyoto/Basics/COsContext.hpp`
//     below is included first to put it back.
//   - the `operator new` block has to come **before** the includes, not after as
//     `CGameStateCtor.cpp` has it: `rstl/construct.hpp:67` spells `new (dest) T(src)`, so the
//     placement form must already be declared when `rstl/reserved_vector.hpp` is parsed, and
//     `CMain.hpp` includes that.
#define _CMEMORY

#include "types.h"

// The merged `.rodata` pool object this file's one `new` passes as its file operand. The string
// is the same `"??(??)"` the stream constructor's `lbl_803A9208` begins with; the *object* is not
// the same one, and naming the right one is what the `R_PPC_ADDR16_HA/LO` relocation pins.
extern "C" const char lbl_803A56C0[];

#if defined(__MWERKS__) || defined(CLANGD)
void* operator new(size_t sz, const char*, const char*);
inline void* operator new(size_t sz) { return operator new(sz, lbl_803A56C0, nullptr); }
inline void* operator new(size_t n, void* ptr) { return ptr; }
#endif

#include "Kyoto/Basics/COsContext.hpp"

#include "MetroidPrime/CMain.hpp"

#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CGameStateBlocks.hpp"

#include "rstl/single_ptr.hpp"

// **`MetroidPrime/CGameGlobalObjects.hpp` is deliberately NOT included**, and that is measured
// rather than tidy: it is the only header of the tree's that makes mwcceppc emit a stray 1-byte
// `.sdata` (a local object, `@106`, in an object with no other data) - the chain is
// `CGameGlobalObjects.hpp` -> `Kyoto/CMemoryCardSys.hpp`, and every header on that chain was
// measured one at a time. A unit that owns `.sdata` it has not claimed cannot be `Matching`, so
// the include goes and the two things this file wanted from it - the type of
// `CMain::gameGlobalObjects`, and the offset of the slot inside it - are spelled out below.
// `CMain.hpp` already forward-declares `class CGameGlobalObjects`, and a pointer is all
// `gameStateSlot` takes.

extern "C" {
// CGameState's default constructor - `src/MetroidPrime/Player/CGameStateCtor.cpp`, `Matching`,
// so this is a link to our own object and not a gap. It returns `this`.
CGameState* fn_801449C8(CGameState* self);
// `rstl::single_ptr<CGameState>::operator=(T*)`: `~CGameState(*slot, 1) ; *slot = value`.
void fn_80004154(rstl::single_ptr< CGameState >* self, CGameState* value);
// The five copy constructors and the five copy assignments, in the member order above. **All ten
// are unnamed in `config/G2ME01/symbols.txt`** (`fn_80005108` at :104, `fn_80004E84` at :101,
// `fn_80004C90` at :95, `fn_80004AA0` at :90, `fn_80004990` at :87, `fn_80003F08` at :62,
// `fn_80003D00` at :58, `fn_80003BE8` at :56, `fn_80142920` at :5363, `fn_801427DC` at :5360), so
// each is called by its dtk label.
//
// **How each was identified**, since "it takes two pointers and returns one" does not by itself
// say *which* copy: all ten return `this` in r3 and take `(dst, src)` in r3/r4, and the split
// between them is the *caller's* argument order - the five on the way out get the stack local in
// r3 and the member in r4 and are therefore copy **constructors**, the five on the way in get the
// member in r3 and the local in r4 and are therefore copy **assignment**. Which member each one
// belongs to is then read off its own body, and each body writes exactly the fields retail's
// constructors write for that member, one member at a time:
//
//   fn_80005108  SGameStateCardOpts  fn_800052A0(this), fn_80005158(this+0x18, src+0x18), then
//                `lwz 0x28(src) ; stw 0x28(dst)` - the 0x2C member at +0x54
//   fn_80004E84  CGameOptions         0x00..0x27 word by word, then a helper on +0x28 - the 0x44
//                member at +0x80, and its +0x28 is the `rstl::vector`
//   fn_80004C90  SGameStateSlots      `stw 0x00`, then fn_80004CD4(+0x04) - the 0x34 member at
//                +0x144
//   fn_80004AA0  SGameStateBlock      reads 0x04 and 0x08, writes 0x04/0x08/0x0C and allocates -
//                exactly the `{?, count, cap, data}` shape in
//                `include/MetroidPrime/Player/CGameStateBlocks.hpp`
//   fn_80004990  SGameStateWorlds     0x00, the byte at 0x04, 0x08, 0x0C, then fn_800049EC(+0x10) -
//                the 0x54 member at +0x1A0
//
// The five dtor labels come the same way: `__dt__PersistentOptions_800050A4` is 0x64 bytes and
// destroys the +0x54 member, `fn_80004D84` is 0xAC and is the only destructor sized like
// CGameOptions' two containers, `__dt__80004B9C` is 0x50 and destroys a 0x34 member's three
// 16-byte elements, and `fn_80004A4C` is the `{?, ..., void* heap}` destructor the stream
// constructor's notes already identify.
void fn_80005108(SGameStateCardOpts* self, const SGameStateCardOpts* src);
void fn_80004E84(CGameOptions* self, const CGameOptions* src);
void fn_80004C90(SGameStateSlots* self, const SGameStateSlots* src);
void fn_80004AA0(SGameStateBlock* self, const SGameStateBlock* src);
void fn_80004990(SGameStateWorlds* self, const SGameStateWorlds* src);
void fn_80003F08(SGameStateCardOpts* self, const SGameStateCardOpts* src);
void fn_80003D00(CGameOptions* self, const CGameOptions* src);
// **These two take the CGameState, not the member**: `fn_80142920` opens with
// `addi r3,r3,324` and `fn_801427DC` with `addi r3,r3,376` and then tail-calls the member's own
// copy assignment, so they are CGameState methods and retail passes `gpGameState` straight
// through (0x80003B2C-0x80003B30 and 0x80003B38-0x80003B3C, where there is no `addi r3,r3,...`).
// `fn_80003BE8` is the opposite - it takes the member's own address - which is why one of the
// five copy-ins is spelled on the member and the other four on the object.
void fn_80142920(CGameState* self, SGameStateSlots* src);
void fn_801427DC(CGameState* self, SGameStateBlock* src);
void fn_80003BE8(SGameStateWorlds* self, const SGameStateWorlds* src);
// The four destructors, `(this, -1)`, all of them unnamed in `config/G2ME01/symbols.txt`.
void __dt__PersistentOptions_800050A4(SGameStateCardOpts* self, int flag);
void fn_80004D84(CGameOptions* self, int flag);
void __dt__80004B9C(SGameStateSlots* self, int flag);
void fn_80004A4C(SGameStateBlock* self, int flag);
} // extern "C"

// The `CGameOptions` local. **`CGameOptions` has a declared destructor**
// (`include/MetroidPrime/Player/CGameOptions.hpp:21`), so a local of that type would have the
// compiler run `~CGameOptions()` at scope exit - and it would call it through the C++ name
// `__dt__12CGameOptionsFv`, which is not a symbol retail's symbol table carries. Retail's is the
// same function at the same address, under the unnamed `fn_80004D84`, so the local is a POD
// mirror of the right size and the destructor is called explicitly.
struct SGameOptionsCopy {
  u8 x00[sizeof(CGameOptions)];
};
CHECK_SIZEOF(SGameOptionsCopy, 0x44)

// `CGameGlobalObjects::gameState` is at **+0x130** in retail and at **+0x108 in this tree**:
// `include/MetroidPrime/CGameGlobalObjects.hpp:69` has `characterFactoryBuilder` (0x28 bytes)
// commented out, and the comment above it says so - "everything below it reads 0x28 lower than
// retail until it does". So `GameState()` hands out the wrong address for retail's bytes, and the
// offset is spelled out here instead. **+0x130 is independently pinned by this function**:
// `fn_80004154` is 0x80004154..0x8000419C and it `lwz`s the slot it is given at +0x00, so the
// `lwz r3,304(r3)` at 0x80003AFC is that slot and not a field of it. And `+0x130 + 4 == +0x134`
// is `memoryCard`, the next member.
// `static inline`, not `static`: the unit is compiled with `-inline deferred,noauto`, so an
// unmarked static is emitted out of line and *called* - an extra function the retail object does
// not have, and a `bl` where retail has an `addi`.
static inline rstl::single_ptr< CGameState >* gameStateSlot(CGameGlobalObjects* objects) {
  return reinterpret_cast< rstl::single_ptr< CGameState >* >(
      reinterpret_cast< char* >(objects) + 0x130);
}

// The four `CGameState` blocks this function copies, by retail offset. Upstream's `CGameState`
// layout (the matching build's) keeps them private and unnamed; the port's (`TARGET_PC`) names
// them. Offsets are the same in both, so the address arithmetic is layout-independent.
template < typename T >
static inline T* gameStateAt(CGameState* state, int offset) {
  return reinterpret_cast< T* >(reinterpret_cast< char* >(state) + offset);
}

void CMain::ResetGameState() {
  SGameStateCardOpts local54;
  SGameOptionsCopy local80;
  SGameStateSlots local144;
  SGameStateBlock local178;
  SGameStateWorlds local1a0;

  fn_80005108(&local54, reinterpret_cast< SGameStateCardOpts* >(&gpGameState->SystemOptions()));
  fn_80004E84(reinterpret_cast< CGameOptions* >(&local80), gameStateAt< CGameOptions >(gpGameState, 0x80));
  fn_80004C90(&local144, gameStateAt< SGameStateSlots >(gpGameState, 0x144));
  fn_80004AA0(&local178, gameStateAt< SGameStateBlock >(gpGameState, 0x178));
  fn_80004990(&local1a0, gameStateAt< SGameStateWorlds >(gpGameState, 0x1A0));

  // 0x80003AAC-0x80003AF0. `fn_80004154` is retail's out-of-line
  // `rstl::single_ptr<CGameState>::operator=(T*)` - `stw value,0(slot)` after `~CGameState(*slot, 1)`,
  // returning the slot - so the new pointer's null test is written by hand below rather than
  // spelled as an assignment, and the `new` is `::operator new` plus a null test plus the
  // constructor, which is what mwcceppc expands `new CGameState` into. See the header for the one
  // instruction that costs.
  fn_80004154(gameStateSlot(gameGlobalObjects), 0);
  gpGameState = 0;
  CGameState* newState = static_cast< CGameState* >(::operator new(sizeof(CGameState)));
  if (newState) {
    newState = fn_801449C8(newState);
  }
  fn_80004154(gameStateSlot(gameGlobalObjects), newState);
  gpGameState = gameStateSlot(gameGlobalObjects)->get();

  fn_80003F08(reinterpret_cast< SGameStateCardOpts* >(&gpGameState->SystemOptions()), &local54);
  fn_80003D00(gameStateAt< CGameOptions >(gpGameState, 0x80), reinterpret_cast< const CGameOptions* >(&local80));
  gameStateAt< CGameOptions >(gpGameState, 0x80)->EnsureOptions();
  fn_80142920(gpGameState, &local144);
  fn_801427DC(gpGameState, &local178);
  fn_80003BE8(gameStateAt< SGameStateWorlds >(gpGameState, 0x1A0), &local1a0);

  // 0x80003B50-0x80003BA0: `~SGameStateWorlds` inlined, which is the destructor of the +0x1A0
  // block's record array at +0x10 and nothing else - the 16-byte records are trivially
  // destructible, so the loop survives with an **empty body**, unrolled by eight
  // (`srwi r0,r0,3` / `mtctr` / `bdnz`, the `addi r4,r5,-8` peel). The same ten instructions are
  // inlined into `fn_8000419C` over the member at +0x1B0, so the loop is retail's, not this
  // function's. Three details are load-bearing and all three are measured: the pointer has to be
  // **tested**, which is what the `addic.`+`beq` pair is (MWCC's address-and-test idiom - the same
  // two instructions the placement `new` in `CGameStateCtor.cpp` emits); the bound has to be a
  // separate `int` **and signed**, or mwcceppc picks r4 for it and emits `cmplwi` against an
  // unsigned bound. What is still not reproduced is which of r3 and r4 the counter and the
  // unroller's temporary get - see the header.
  const int* count = reinterpret_cast< const int* >(&local1a0.x10_count);
  if (count) {
    for (int i = 0, n = *count; i < n; ++i) {
    }
  }

  fn_80004A4C(&local178, -1);
  __dt__80004B9C(&local144, -1);
  fn_80004D84(reinterpret_cast< CGameOptions* >(&local80), -1);
  __dt__PersistentOptions_800050A4(&local54, -1);
}
