#include "Kyoto/CToken.hpp"

// `CStringTable` has no class definition in this tree and does not need one here: the only thing
// these bodies touch of it is the `T*` of `TLockedToken`'s `mItem`, so it is left incomplete and
// named.  See the file header for why the wrapper below is spelled out rather than taken from
// `include/Kyoto/TToken.hpp`.
class CStringTable;

// Carved out of dtk's unclaimed `main/auto_03_8025CA80_text` range.  Every number here is
// measured: the addresses and sizes come from `config/G2ME01/symbols.txt:10608-10609`, the
// instructions are the ones dtk itself emitted into `build/G2ME01/asm/auto_03_8025CA80_text.s:1031`
// and `:1071`, and the body below is the C++ those bytes are the compilation of.
//
// .text 0x8025D8C8..0x8025D9D0, 0x108 = 264 bytes, 2 functions:
//
//   fn_8025D94C    0x8025D94C  0x84    33 instructions
//   fn_8025D8C8    0x8025D8C8  0x84    33 instructions
//
// **What they are: `rstl::optional_object<TLockedToken<CStringTable>>::operator=(const
// TLockedToken<CStringTable>&)`, twice.**  Retail names neither copy, so this is read off the call
// edges and the member offsets, and it is confirmed by a byte-shape twin already at 100.00% in
// this tree: `__as__Q24rstl47optional_object<28TLockedToken<12CStringTable>>FRC28TLockedToken<12CStringTable>`
// at `0x8000835C` (`symbols.txt:157`, weak, 0x84 = 132 bytes), which
// `src/MetroidPrime/main.cpp` emits through `include/rstl/optional_object.hpp`'s
// `operator=(const T& item)` (line 46) -> `assign()` (lines 82-89).  Compared instruction by
// instruction, **all 33 of these are the twin's 33 with only the three `bl` targets different** -
// the same 0x20 frame, the same three saved registers in the same order, the same branch targets
// at the same offsets.
//
// Instruction by instruction (offsets are for `fn_8025D8C8`; `fn_8025D94C` is identical):
//
//   +0x00..0x1C  prologue, 0x20 frame: `mflr/stw 0x24`, `stw r31,0x1c / mr r31,r4`,
//                `stw r30,0x18 / mr r30,r3`, `stw r29,0x14`
//   +0x20        `lbz r0,0xc(r3)` - `m_valid`, `include/rstl/optional_object.hpp:80`
//   +0x28        `bne +0x58` - the already-valid arm, laid out after the construct arm
//   +0x2C        `mr. r0,r30` + `+0x30 beq +0x4c` - **the placement-new destination guard**, i.e.
//                `if (!dest) skip the constructor` out of `rstl::construct_impl`'s
//                `new (dest) T(src)` (`include/rstl/construct.hpp:54-56`).  This is why the
//                frame is 0x20 and not 0x10
//   +0x34        `mr r29,r0` - the destination in its own callee-saved register, so it survives
//                the two calls below while `r30` keeps `this` for the `m_valid` store
//   +0x38        `bl __ct__6CTokenFRC6CToken` (0x803015B4, `symbols.txt:13923`) - `CToken`'s copy
//                constructor, the `mToken(token)` mem-init of `TLockedToken`'s copy constructor
//                (`include/Kyoto/TToken.hpp:76`)
//   +0x3C..0x44   `lwz r0,0x8(r31)` + `mr r3,r29` + `stw r0,0x8(r29)` - `mItem(*token)`, the
//                `T*` member at +0x08
//   +0x48        `bl Lock__6CTokenFv` (0x80301490, `symbols.txt:13919`) - `mToken.Lock()`, the
//                third statement of the same copy constructor.  `TCachedToken` has no `Lock()` in
//                its implicit copy, so those two calls are what makes the member a
//                `TLockedToken` rather than a `TCachedToken`
//   +0x4C..0x54   `li r0,1` + `stb r0,0xc(r30)` + `b +0x64` - `m_valid = true; return;`
//   +0x58        `bl __as__6CTokenFRC6CToken` (0x803013D0, `symbols.txt:13917`) - `CToken::operator=`,
//                the first statement of `TLockedToken::operator=` (`include/Kyoto/TToken.hpp:78-82`)
//   +0x5C        `lwz r0,0x8(r31)` - the same `mItem` member, so the two statements of that
//                `operator=` are these two instructions
//   +0x64..0x80   epilogue; `mr r3,r30` is the `optional_object&` this overload returns
//
// **The layout is fixed twice over.**  `TLockedToken<CStringTable>` is `include/Kyoto/TToken.hpp:71-92`:
// a `TToken<CStringTable>` (a `CToken` with no extra members, so 8 bytes - `mObjRef` and
// `mLockHeld`, `include/Kyoto/CToken.hpp:34-35`) followed by `T* mItem` at +0x08.  `m_valid` is at
// +0x0C, which the `lbz`/`stb` at +0x20/+0x50 fix, and is the byte after `mItem`, so
// `sizeof(TLockedToken<CStringTable>) == 0xC`.  This is the same 0x10-byte flag at +0xC,
// value at +8 shape `src/MetroidPrime/CModelDataCopyCtor.cpp:18-29` measures on
// `optional_object<TLockedToken<CModel>>`.
//
// **Why `TLocked8025` and not `TLockedToken<CStringTable>` directly.**  The three callees *are*
// the real ones - they come from `include/Kyoto/CToken.hpp`, are defined in `Kyoto/CToken.cpp`,
// and the compiled object has three undefined symbols and no stubs.  Naming the real class instead
// was measured and rejected: `TLockedToken` has no destructor of its own, so the instantiation
// drags in `__dt__22TToken<12CStringTable>Fv` (0x54 = 84 bytes of weak `.text` this claim does not
// hold and cannot hold), which is a fourth function in the object for a two-function range.
// `TLocked8025` below is that class with its three members spelled out and no destructor, so the
// object carries the two functions above and nothing else (`nm` on it: two `T` symbols, three `U`).
//
// **Why this file is `.cpp` and the definitions are `extern "C"`.**  The carve vein wants plain C
// so the `fn_` names do not mangle; `extern "C"` gives that identically and is already used by
// `src/MetroidPrime/Carve8024492C.cpp`.  The reason to reach for C++ at all is measured: written as
// plain C with the callees declared `extern`, both bodies compile to **30 instructions, not 33** -
// the destination guard folds to `cmplwi r30,0 / beq`, there is no `mr r29,r0`, the frame is 0x10
// and the `mItem` store uses `r30` where retail uses `r29`.  All three missing instructions are the
// placement-new front end keeping `dest` a value distinct from `this`; MWCC's C front end
// copy-propagates the two together and the register disappears.  Writing the construct as
// `new (dest) TLocked8025(item)` - the same expression `rstl::construct` makes - restores all
// three, and `tools/carve_diff.sh` then reports the 33 instructions byte-for-byte against
// `build/G2ME01/main.elf` apart from the three `bl` immediates the linker fills in.
//
// `src/MetroidPrime/PortLinkStubs.cpp` needed no entry: `grep` for `fn_8025D8C8`/`fn_8025D94C`
// across `src/`, `include/`, `configure.py`, `config/G2ME01/splits.txt` and `files.cmake` finds
// nothing but this claim, and the three callees are retail's own symbols.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `tools/check_decl_order.py --unit Weapons/Carve8025D8C8` is
// the cheap check.
//
// Its own unit because a claim may not span an unclaimed gap and may not sit where a neighbouring
// unit's `.text` ends.  Below, `Weapons/CProjectileWeapon.cpp` ends at 0x8025CA80 - which is where
// dtk's `auto_03_8025CA80_text` starts - and above, `Weapons/CCollisionResponseData.cpp` starts at
// 0x8025DD1C, so the claim sits strictly inside the unclaimed 0x8025CA80..0x8025DD1C gap with
// dtk's bytes on both sides of it.  `fn_8025D9D0` (0x8025D9D0, 0x8C = 140 bytes,
// `symbols.txt:10610`) is 4 bytes above the claim and stays dtk's.  The directory is retail's own,
// taken from those neighbours: this address is in the `Weapons/` neighbourhood.

/** `TLockedToken<CStringTable>` (`include/Kyoto/TToken.hpp:71-92`) as far as these bytes read it:
 *  `TToken<CStringTable> mToken` - which is a `CToken` and adds nothing, so the real class, and
 *  `T* mItem` at +0x08.  Its copy constructor is retail's three statements at +0x38..+0x48 and its
 *  assignment operator is retail's two at +0x58..+0x5C, so both are written here as the template
 *  writes them - the template spells the member copy `mItem(*token)` through its own
 *  `operator*()`, which is the same `lwz 0x8` / `stw 0x8` pair.  See the header for why the class
 *  is spelled out rather than included. */
struct TLocked8025 {
  CToken mToken;
  CStringTable* mItem;
  TLocked8025() {}
  TLocked8025(const TLocked8025& token) : mToken(token.mToken), mItem(token.mItem) { mToken.Lock(); }
  TLocked8025& operator=(const TLocked8025& token) {
    mToken = token.mToken;
    mItem = token.mItem;
    return *this;
  }
};

/** `rstl::optional_object<TLockedToken<CStringTable>>` (`include/rstl/optional_object.hpp:11-90`)
 *  as far as these bytes read it: the 0xC-byte payload at +0 and `bool m_valid` at +0x0C.  Only
 *  those two members are touched; the template's constructor, destructor and
 *  `operator=(const optional_object&)` are not used by these two functions. */
struct SOptional8025 {
  TLocked8025 mData;
  bool mValid;
};

extern "C" void* fn_8025D94C(struct SOptional8025* self, const struct SOptional8025* item);
extern "C" void* fn_8025D8C8(struct SOptional8025* self, const struct SOptional8025* item);

/** `fn_8025D94C` - retail `.text:0x8025D94C`, 0x84 = 132 bytes: the `optional_object` assignment
 *  operator below.  The construct arm is the placement new, which is what supplies the destination
 *  guard and the third saved register; the other arm is the assignment. */
extern "C" void* fn_8025D94C(struct SOptional8025* self, const struct SOptional8025* item) {
  if (!self->mValid) {
    new ((void*)self) TLocked8025(item->mData);
    self->mValid = true;
  } else {
    self->mData = item->mData;
  }
  return self;
}

/** `fn_8025D8C8` - retail `.text:0x8025D8C8`, 0x84 = 132 bytes: the same operator, the second
 *  instantiation retail emitted, with its own three `bl` targets. */
extern "C" void* fn_8025D8C8(struct SOptional8025* self, const struct SOptional8025* item) {
  if (!self->mValid) {
    new ((void*)self) TLocked8025(item->mData);
    self->mValid = true;
  } else {
    self->mData = item->mData;
  }
  return self;
}