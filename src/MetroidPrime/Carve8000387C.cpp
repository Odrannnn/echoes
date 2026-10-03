/**
 * `fn_8000387C` - retail `.text:0x8000387C`, `config/G2ME01/symbols.txt:52`, 0x148 = 328 bytes:
 * the copy constructor of `rstl::vector< rstl::pair< uint, uint >, rstl::rmemory_allocator >`,
 * `include/rstl/vector.hpp:127-136`.  Carved out of the unclaimed dtk range
 * `main/auto_03_8000387C_text` (`build/G2ME01/asm/auto_03_8000387C_text.s:9-96`).
 *
 * It is a **byte-shape twin** of `fn_801EF8B8` (0x801EF8B8, `symbols.txt:7980`, 0x148, the
 * `Matching` `src/MetroidPrime/Carve801EF84C.cpp:144`), and the twin was measured before a word
 * of this was written: `powerpc-eabi-objdump -d` over
 * `build/G2ME01/obj/MetroidPrime/Carve801EF84C.o` and over
 * `build/G2ME01/main.elf --start-address=0x8000387C --stop-address=0x800039C4` produces two
 * 656-hex-digit streams that differ **only in the three bytes of the `bl` at instruction 20**
 * (0x800038CC, into `allocate__Q24rstl17rmemory_allocatorFi`).  The other four byte-identical
 * copies across `main` are the named COMDAT instantiation at 0x80004F5C, `fn_80005158` and
 * `fn_801EF8B8`, and `fn_8018FE6C` (`src/MetroidPrime/CSlideShow.cpp:412-416`).
 *
 * The 82 instructions read as one thing:
 *
 *   - `lwz r0,0x8(r4)` / `lwz r3,0x4(r4)` then `stw r3,0x4(r30)` / `stw r0,0x8(r30)` is the
 *     member-initialiser list `mCount(other.mCount), mCapacity(other.mCapacity)`
 *     (vector.hpp:129); `mAllocator` is `rstl::rmemory_allocator`, which is empty and so
 *     contributes only the word at +0.
 *   - `cmpwi r3,0 / bne / cmpwi r0,0 / bne` then `li r0,0 / stw r0,0xc(r30)` is the guard
 *     `if (other.mCount == 0 && other.mCapacity == 0) { mItems = nullptr; }` (vector.hpp:130-132)
 *     - both tests are on **other**, which is what the two loads at +4 and +8 of r4 are for.
 *   - `lwz r0,0x8(r30) / slwi r3,r0,3 / bl allocate__Q24rstl17rmemory_allocatorFi / stw r3,0xc(r30)`
 *     is `mAllocator.allocate(mItems, mCapacity)` (vector.hpp:133); the `slwi` is
 *     `count * sizeof(rstl::pair<uint,uint>)`, the 8-byte stride.
 *   - `lwz r5,0xc(r31) / lwz r3,0x4(r30) / lwz r4,0xc(r30)` is
 *     `uninitialized_copy_n(other->mItems, mCount, mItems)` (vector.hpp:134), and the two
 *     different bases are retail's own: the **source** comes from *other* (r31) and the count
 *     and the **destination** from *self* (r30).  So the loop is driven by `self->mCount`, not by
 *     `other->mCount`.
 *   - `srwi. r0,r3,3 / mtctr / beq` at 0x800038E8 and `andi. r3,r3,7` at 0x80003980 are mwcceppc's
 *     unroll of that loop eight elements per iteration with a remainder, and the loop body has no
 *     per-element null test and no constructor call, which is what
 *     `include/rstl/pair.hpp:37-44` declaring `rstl::pair<uint, uint>` trivially destructible with
 *     a `construct_impl` that assigns exists to produce.
 *
 * **The body has to be written against the real `rstl` headers, and that is measured here, not
 * preferred.**  Written in plain C - the two words of the header plus a hand-written block loop -
 * the same logic compiles to the right *length* (82 instructions, 328 bytes, measured) and is 49
 * instructions off: retail keeps the count in **r3** and derives both `srwi. r0,r3,3` and
 * `andi. r3,r3,7` from it, while every C spelling measured puts the count in r0 and then shuffles
 * it into r3 with an extra `mr r3,r0` at the loop head.  `rstl::uninitialized_copy_n`
 * (`include/rstl/construct.hpp:140-150`) takes the count as its **second** argument and that is
 * what carries it in the register retail uses; the body below is `Carve801EF84C.cpp`'s, which is
 * already `Matching` and byte-identical to this range's bytes.  Four C spellings were measured
 * there (`int remaining` block loop; the same with the parameter decremented; both again as a
 * three-argument `static`/`static inline` helper with and without `const` on the source), and the
 * two that keep the count in r3 do it by **outlining** the helper - a fifth function in the
 * object, which breaks `tools/unit_fit.sh`.
 *
 * `rstl::vector`'s own copy constructor cannot simply be named instead:
 * `include/rstl/vector.hpp:65` and the notes at `Carve801EF84C.cpp:65-69` record that MWCC rejects
 * explicit instantiation of a member (`template V::vector(const V&);` is a syntax error) and
 * `template class rstl::vector<...>` instantiates only the non-`inline` members, so neither emits
 * the COMDAT - which is why `CSlideShow.cpp:414-416` forces the instantiation out of a real
 * function instead.  Writing the body out is the same approach and emits no symbol beyond
 * `fn_8000387C` itself.
 *
 * **Why a `.cpp` and not a `.c`.**  Retail names this symbol nothing (`symbols.txt:52` carries the
 * `fn_8000387C` placeholder) and objdiff pairs by name, so the definition must land in the object
 * unmangled.  `extern "C"` does that, which is the arrangement
 * `src/MetroidPrime/Player/CGameStateBlockCopyCtor.cpp:34` and `Carve801EF84C.cpp:88` already use
 * for Matching carves of exactly this shape; a `.c` file cannot reach 100% here for the register
 * reason above, so the two requirements pull in opposite directions and `extern "C"` is the only
 * spelling that satisfies both.
 *
 * Its three callees are declared, never defined here.  `allocate__Q24rstl17rmemory_allocatorFi`
 * (0x802FDAB8, `symbols.txt:13824`, 0x3C) is `rstl::rmemory_allocator::allocate(int)` under
 * CodeWarrior's own mangling, and it reaches that call by retail's name because
 * `rmemory_allocator::allocate(T*&, int)` (`include/rstl/rmemory_allocator.hpp:21-25`) tail-calls
 * the `int` overload; for the host `src/MetroidPrime/PortLinkStubs.cpp:888` supplies it as
 * `stub_179`.  `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`, 0x64) is claimed by
 * `Kyoto/Alloc/CMemory.cpp` in the DOL, and `src/Kyoto/Alloc/PortMwccNew.cpp:39` defines it for
 * the host.
 *
 * Its own unit, and no gap is spanned.  Below, `MetroidPrime/Carve80003858.c` ends **exactly** at
 * 0x8000387C.  Above, `fn_800039C4` ends at 0x80003A18 and the next symbol is the real-named
 * `CMain::GetLanguage() const` (0x80003A18, 0x30), which nothing claims -
 * `MetroidPrime/CMainResetGameState.cpp` starts above it at 0x80003A48.  What is left of the run
 * above is therefore `auto_03_80003A18_text` (0x80003A18..0x80003A48).
 *
 * Source order is **descending by address** and that is load-bearing: mwcceppc emits function
 * definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
 * ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
 * `tools/flip_test.sh` catches that; `python3 tools/check_decl_order.py --unit
 * MetroidPrime/Carve8000387C.cpp` checks it without a build.
 */

/**
 * `fn_800039C4` - retail `.text:0x800039C4`, `symbols.txt:53`, 0x54 = 84 bytes, 21 instructions.
 * **It is the same deleting-destructor step as `fn_80004744`** (0x80004744, 0x54, the `Matching`
 * `src/MetroidPrime/Carve80004744.c`), word for word apart from the two `bl`s into
 * `Free__7CMemoryFPCv`; measured over `build/G2ME01/obj/MetroidPrime/Carve80004744.o` and
 * `build/G2ME01/main.elf --start-address=0x800039C4 --stop-address=0x80003A18`, the two 168-hex-digit
 * streams differ only at instructions 9 and 13, the two calls.  `Carve80004744.c` documents the
 * whole family and its three call sites, so the body below is that file's:
 *
 *   - `mr r31,r4` before `mr. r30,r3 / beq` is the incoming 16-bit flag; `extsh. r0,r31 / ble`
 *     is the `flag > 0` test that decides whether the receiver itself is freed afterwards.
 *   - `lwz r3,0xc(r30)` is the one thing destroyed - the pointer at +0xC, freed through the same
 *     `Free__7CMemoryFPCv` that frees the object.  There is **no count load, no stride and no
 *     element loop**, so the member is a plain pointer, not a `vector`'s item array.
 *   - `mr r3,r30` in the epilogue is the return of the receiver, as in every member of the family.
 */

#include "rstl/construct.hpp"
#include "rstl/pair.hpp"
#include "rstl/rmemory_allocator.hpp"

extern "C" {

/** 0x802FDAB8, `symbols.txt:13824`, 0x3C: `rstl::rmemory_allocator::allocate(int)`.  Reached by
 *  retail's own mangled name, because `rmemory_allocator::allocate(T*&, int)` calls it; declared
 *  never defined here.  `src/MetroidPrime/PortLinkStubs.cpp:888` supplies it as `stub_179`. */
void* allocate__Q24rstl17rmemory_allocatorFi(int size);

/** 0x802CE388, `symbols.txt:12992`, 0x64: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` in the DOL; `src/Kyoto/Alloc/PortMwccNew.cpp:39` defines it for the
 *  host.  Declared, never defined here. */
void Free__7CMemoryFPCv(const void* ptr);

/** The 8-byte element `fn_8000387C` copies, from the two loads and two stores per element it
 *  emits and from its `addi r5,r5,8 / addi r4,r4,8` stride. */
typedef rstl::pair< uint, uint > SPairU32U32;

/** The header `fn_8000387C` builds: `rstl::vector`'s four words in `include/rstl/vector.hpp:18-21`'s
 *  order and at retail's displacements.  Only the three the bytes touch are named; `mAllocator` is
 *  empty and the word it contributes is padding. */
struct SPairVec {
  rstl::rmemory_allocator mAllocator;
  int mCount;
  int mCapacity;
  SPairU32U32* mItems;
};

/** The object `fn_800039C4` tears down.  Only the one field the bytes read is modelled: the
 *  `void*` at +0xC that `lwz r3,0xc(r30)` loads.  The three words before it are padding, and what
 *  they hold is not this function's business - the callers hand it a member subobject at a fixed
 *  offset inside a much larger class. */
struct SBufferHolder {
  int m0;
  int m4;
  int m8;
  void* mC;
};

void* fn_800039C4(struct SBufferHolder* self, short flag);

void* fn_8000387C(struct SPairVec* self, const struct SPairVec* other);

void* fn_800039C4(struct SBufferHolder* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(self->mC);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_8000387C(struct SPairVec* self, const struct SPairVec* other) {
  self->mCount = other->mCount;
  self->mCapacity = other->mCapacity;
  if (other->mCount == 0 && other->mCapacity == 0) {
    self->mItems = nullptr;
  } else {
    rstl::rmemory_allocator::allocate(self->mItems, self->mCapacity);
    rstl::uninitialized_copy_n(other->mItems, self->mCount, self->mItems);
  }
  return self;
}

} // extern "C"