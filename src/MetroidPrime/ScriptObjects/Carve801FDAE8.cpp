// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:8287` (`fn_801FDAE8 = .text:0x801FDAE8; // size:0x74`),
// and the 116 bytes below are retail's own, read this run out of the disc with
// `python3 tools/dol_read.py 0x801FDAE8 0x74` (`orig/G2ME01/sys/main.dol`, text section 1: file
// offset 0x640 at address 0x80003840, so this address is file offset 0x1FA8E8; the 116 bytes
// sha1 `d341b6d3727c6e21c266b1bdd48d089137c2fbdf`) and **not** out of `build/G2ME01/main.elf`,
// which holds our own bytes once this unit is in the link - measured this run before the claim
// existed, that range of the linked ELF was still byte-identical to the disc's.  The body below is
// the C++ those bytes are the compilation of, and `tools/flip_test.sh` is what says so.
//
// .text 0x801FDAE8..0x801FDB5C, 0x74 = 116 bytes, 1 function:
//
//   fn_801FDAE8    0x801FDAE8  0x74   29 instructions   the 0x2C-byte element's deleting destructor
//
//   801fdae8  94 21 ff f0   stwu   r1,-16(r1)
//   801fdaec  7c 08 02 a6   mflr   r0
//   801fdaf0  90 01 00 14   stw    r0,20(r1)
//   801fdaf4  93 e1 00 0c   stw    r31,12(r1)
//   801fdaf8  7c 9f 23 78   mr     r31,r4            ; the deleting flag, kept whole in r31
//   801fdafc  93 c1 00 08   stw    r30,8(r1)
//   801fdb00  7c 7e 1b 79   mr.    r30,r3            ; receiver guard: MWCC's null `this` test
//   801fdb04  41 82 00 3c   beq    .+0x58
//   801fdb08  3c 80 80 3b   lis    r4,lbl_803B7BE4@ha
//   801fdb0c  38 7e 00 18   addi   r3,r30,0x18       ; &mMember
//   801fdb10  38 04 7b e4   addi   r0,r4,lbl_803B7BE4@l
//   801fdb14  38 80 ff ff   li     r4,-1            ; "do not free me afterwards"
//   801fdb18  90 1e 00 00   stw    r0,0(r30)         ; the vptr, restored to this class
//   801fdb1c  4b ff fb d5   bl     0x801fd6f0        ; fn_801FD6F0(&mMember, -1)
//   801fdb20  34 1e 00 04   addic. r0,r30,4          ; &mName - the *named* destructor call's
//   801fdb24  41 82 00 0c   beq    .+0x48              ; null test on the member's own address
//   801fdb28  38 7e 00 04   addi   r3,r30,4
//   801fdb2c  48 10 0e 8d   bl     0x802fe9b8        ; ~rstl::basic_string
//   801fdb30  7f e0 07 35   extsh. r0,r31            ; the flag, sign-extended to 16 bits
//   801fdb34  40 81 00 0c   ble    .+0x58            ; ... so `flag > 0` is the free test
//   801fdb38  7f c3 f3 78   mr     r3,r30
//   801fdb3c  48 0d 08 4d   bl     0x802ce388        ; CMemory::Free(self)
//   801fdb40  80 01 00 14   lwz    r0,20(r1)
//   801fdb44  7f c3 f3 78   mr     r3,r30            ; the receiver is returned
//   801fdb48  83 e1 00 0c   lwz    r31,12(r1)
//   801fdb4c  83 c1 00 08   lwz    r30,8(r1)
//   801fdb50  7c 08 03 a6   mtlr   r0
//   801fdb54  38 21 00 10   addi   r1,r1,16
//   801fdb58  4e 80 00 20   blr
//
// **What it is: the deleting destructor of one 0x2C-byte element of a script-object block.**  The
// shape is the port's stock one (`mr. r30,r3 / beq` guards the receiver, the vptr goes back first,
// the members are destroyed in reverse declaration order, `CMemory::Free(self)` is reached only
// behind `extsh. r0,r31 / ble`, i.e. `flag > 0`), and `src/MetroidPrime/CIOWinDtor.cpp` documents
// and reproduces the same shape.
//
// **The `-1` comes from the claim immediately in front of this one, and that claim is `Matching`
// for real.**  `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c` (0x801FDAA4..0x801FDAE8) is
// `rstl::destroy<T>`'s two halves for exactly this element: its `fn_801FDAC4` (0x801FDAC4, 0x24) is
// `li r4,-1` / `bl fn_801FDAE8` - that is `rstl::destroy_impl`, and it is the caller this
// destructor's flag is written for.  It also measures the element size: its header records the two
// retail callers of `fn_801FDAA4`, `fn_801FDA54` (0x801FDA54) and `fn_801FF96C`
// (`ScriptObjects/Carve801FF8A0.cpp`, `Matching`), each stepping its cursor by `addi r31,r31,0x2C`
// over one pointer, so the stride is **0x2C = 44**.
//
// **The layout is read off the class's own constructor**, `fn_801FF418` (0x801FF418, 0x8C, still
// unclaimed), which writes in order:
//
//   801ff420  lis r4,0x803b / 801ff428 addi r0,r4,0x7bcc   -> stw at +0x00   the *base* vtable
//                                                                     lbl_803B7BCC
//   801ff438  lis r3,0x803b / 801ff444 addi r0,r3,0x7be4   -> stw at +0x00   then this class's own
//                                                                     lbl_803B7BE4 over it
//   801ff454  addi r3,r31,4 ... bl __ct__Q24rstl66basic_string<...>            -> +0x04 mName
//   801ff474  stfs f0,20(r31)                                               -> +0x14 a float
//   801ff478  stw r4,28(r31) / 801ff47c stw r4,32(r31) /
//   801ff480  stw r4,36(r31)                                                -> +0x1C +0x20 +0x24
//   801ff484  lbz r0,40(r31) ... 801ff48c stb r0,40(r31)                    -> +0x28 a byte
//
// so the element is
//
//   +0x00  void*                the vptr (base vtable `lbl_803B7BCC`, own `lbl_803B7BE4`)
//   +0x04  rstl::string mName   16 bytes under mwcceppc - `mPtr`, `mCow`, `mSize` and the
//                               word-sized empty `rmemory_allocator` (`rstl/string.hpp:106-110`),
//                               so the float lands at +0x14 and not at +0x10
//   +0x14  float                never touched by this destructor
//   +0x18  the member this calls `fn_801FD6F0` on - 0x10 bytes: `fn_801FD6F0` reads the count at
//                               +4 of its receiver (`lwz r0,4(r30)`) and the buffer at +0xC
//                               (`lwz r5,12(r30)`), which are +0x1C and +0x24 of the element, the
//                               first two of the three words the constructor zeroes
//   +0x28  bool                 never touched by this destructor
//
// for **0x2C = 44 bytes** with the trailing `bool` padded out to the type's 4-byte alignment - the
// stride the two retail callers above measure.  Nothing writes +0x18 and nothing in these 116 bytes
// reads it, so the member's own first word is carried as an unnamed `unsigned int x00`: it is there
// because it is what puts the count at +0x1C.
//
// **Its twin is exact and it is `Matching`, which is what fixes the shape.**  `src/MetroidPrime/
// ScriptObjects/Carve801FD924.cpp` (0x801FD924..0x801FD998, a `Matching` unit in this directory)
// is these 29 instructions word for word apart from two immediates - `addi r3,r30,20` where this
// one has `addi r3,r30,24` (its member is at +0x14, this element's at +0x18) and `addi r0,r4,31728`
// where this one has `31716` (`lbl_803B7BF0` against `lbl_803B7BE4`).  The third copy of the shape,
// `fn_801FD67C` (0x801FD67C, 0x74, unclaimed), is the same 29 again with `addi r3,r30,20` and
// `lbl_803B7BFC`; all three `bl` the same `fn_801FD6F0` at +0x18/+0x14, the same
// `internal_dereference__Q24rstl66basic_string<...>` at 0x802FE9B8 and the same
// `Free__7CMemoryFPCv` at 0x802CE388.  The parts that are *not* the two immediates are therefore
// read straight off a byte-exact sibling rather than guessed - and what the sibling is worth is
// more than its two immediates: it is the measured record of which of the two teardown spellings
// MWCC compiles which way, below.
//
// **The deleting flag is a `short`, which is what `extsh. r0,r31` measures, and the two teardown
// calls are deliberately spelled differently.**  `self->mName`'s teardown is a *named* destructor
// call because retail has the null test on the member's address (`addic. r0,r30,4 / beq`) and MWCC
// emits that test only for a named one; leaving the same teardown to scope exit emits no test at
// all, and spelling it as a plain call on `&self->mName` loses it too.  `fn_801FD6F0` is the
// opposite case and is spelled as a plain call on an address: retail has **no** test before that
// `bl`.  Both halves of the rule are measured elsewhere in this tree -
// `src/MetroidPrime/ScriptLoader/Carve802201F8.cpp:23-36` and `src/MetroidPrime/CAnimData.cpp:519`
// for the named call, `src/MetroidPrime/Carve800E10EC.cpp` for the plain one - and both are
// reproduced here.  Note the name after the `~` has to be the **unqualified injected class name**
// (`~basic_string()`, not `~rstl::string()`): the qualified spelling is a syntax error in MWCC 2.7.
//
// **`lbl_803B7BE4` is named, not written as its address, and that is what makes the unit match.**
// It is retail's own `.data` object (`symbols.txt:18317`, `size:0xC`; `objdump -s
// --start-address=0x803B7BE0 --stop-address=0x803B7BF0 build/G2ME01/main.elf` gives `801ff4a4
// 00000000 00000000 801ff4ac`, i.e. `{0, 0, &fn_801FF4AC}` at 0x803B7BE4 - a one-virtual-method
// vtable whose accessor returns the type constant 0, and that accessor is defined by
// `src/MetroidPrime/ScriptObjects/Carve801FF4A4.c`, a `Matching` unit).  Neither this object's bytes
// nor its unit is claimed, so the matching build takes the `.data` from dtk's own
// `auto_07_803B7AE0_data.o` while the port link gets a stand-in.  Spelling the constant out instead
// - `= 0x803B7BE4` or `= 0x803B7BE4u` - leaves the four instruction words identical and **loses the
// `R_PPC_ADDR16_HA` / `R_PPC_ADDR16_LO` relocations objdiff pairs against retail's**, which is a
// green `main.dol` and no matched function; that trade is recorded in
// `build/goal/notes/carve-801fdae8.md`, and it was measured on these same 29 instructions three
// times before this run.  `matched_functions` in `build/report.json` is the check for it, not the
// sha1 and not the percentage.
//
// Retail names none of these, so the definition has to be `extern "C"`: a C++ one would mangle to
// `_Z<len>fn_801FDAE8<len>...` and objdiff would pair nothing.  That is also why the unit is a
// `.cpp` rather than a `.c`: it needs `rstl::string` for `mName`'s destructor, and the mangled name
// of that destructor is not made of identifier characters.  Same convention as
// `ScriptObjects/Carve801FF5A0.cpp`, `Carve801FF8A0.cpp` and `ScriptObjects/Carve801FD924.cpp`.
//
// The `short` here and the `int` in `Carve801FDAA4.c`'s `extern void fn_801FDAE8(void*, int)` are
// two declarations of one `extern "C"` symbol in two translation units, which is what the matching
// twin already does (`Carve801FD8E0.c` declares `fn_801FD924(void*, int)` against
// `Carve801FD924.cpp`'s `(SCarve801FD924Element*, short)`).  Only the `short` is pinned by these
// bytes - `extsh.` - and the call passes a literal `-1`, so both spellings compile to the same
// `li r4,-1`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  One function, so the rule has nothing to bite on here;
// `python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FDAE8` is run
// anyway.
//
// Its own unit because a claim may not span an unclaimed gap.  In front of it the claim
// `ScriptObjects/Carve801FDAA4.c` (0x801FDAA4..0x801FDAE8, `Matching`) ends exactly where this
// claim starts, and behind it `fn_801FDB5C` (0x801FDB5C, 0x84) starts exactly where this claim
// ends and is unclaimed - the nearest claimed range above it, `ScriptObjects/Carve801FDB5C.c`,
// starts at 0x801FDBE0 - so the claim stops at 0x801FDB5C and that function stays retail's.  dtk's
// own `build/G2ME01/asm/auto_03_801FDAE8_text.s` holds `# 0x801FDAE8..0x801FDBE0 | size: 0xF8`,
// whose first per-function header is `# .text:0x0 | 0x801FDAE8 | size: 0x74` and whose second is
// `# .text:0x74 | 0x801FDB5C | size: 0x84`, so 0x801FDAE8..0x801FDB5C is this function and
// nothing else.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FDAA4.c` (0x801FDAA4..0x801FDAE8) and
// `ScriptObjects/Carve801FDB5C.c` (0x801FDBE0..0x801FDC88).

#include "rstl/string.hpp"

/** 0x802CE388, `symbols.txt:12992`: `CMemory::Free(void const*)`, claimed by
 *  `Kyoto/Alloc/CMemory.cpp` in both builds.  Declared under retail's own emitted spelling so the
 *  call needs no header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x801FD6F0, `symbols.txt:8274`, 0x84 = 132 bytes: the +0x18 member's own deleting destructor -
 *  it walks `x04_count` 0x14-strided elements through `fn_801FD774` (which releases each
 *  `rstl::basic_string` in turn), frees `x0c_buffer`, and then frees its own receiver behind its own
 *  `extsh. r0,r31 / ble` flag test - the same tail this function has.  It is a callee, not part of
 *  this claim, and it is still unclaimed, so dtk supplies it to the DOL from its own `auto_*` object
 *  while the port link gets a stand-in (`stub_227`).  Declared, never defined. */
extern "C" void fn_801FD6F0(void* self, int flag);

/** `lbl_803B7BE4` = 0x803B7BE4, `symbols.txt:18317`, `.data` `size:0xC`, holding
 *  `{0, 0, &fn_801FF4AC}`.  Retail's `.data`, supplied to the DOL by dtk's own
 *  `auto_07_803B7AE0_data.o` and to the port link by a data stub.  **Taking its address is what
 *  puts the two `R_PPC_ADDR16_HA` / `R_PPC_ADDR16_LO` relocations into our object**, which is
 *  what objdiff pairs against retail's - see the header. */
extern "C" char lbl_803B7BE4[];

/** The element's +0x18 member, as far as this destructor, the class's constructor `fn_801FF418`
 *  and `fn_801FD6F0` are concerned.  `fn_801FD6F0` reads the count at +4 of it and the buffer at
 *  +0xC (`lwz r0,4(r30)` / `lwz r5,12(r30)`), which are +0x1C and +0x24 of the element - the first
 *  two of the three words the constructor zeroes.  `x00` is never written or read by anything this
 *  file names; it is here because it is what puts the count at +0x1C. */
struct SCarve801FDAE8Member {
  unsigned int x00;
  int x04_count;
  unsigned int x08_capacity;
  void* x0c_buffer;
};

/** The 0x2C-byte element, as far as this destructor reaches into it.  The offsets and the total are
 * the measured layout described in the header, and nothing else is asserted about a class retail
 * does not name. */
struct SCarve801FDAE8Element {
  void* mVTable;                    // +0x00, the `stw r0,0(r30)` above; lbl_803B7BE4 is this class's
                                     // own vtable and lbl_803B7BCC its base's
  rstl::string mName;               // +0x04, `addic. r0,r30,4` / `addi r3,r30,4` above
  float x14;                        // +0x14, the constructor's `stfs f0,20(r31)`; not reached here
  SCarve801FDAE8Member mMember;     // +0x18, `addi r3,r30,24` above
  bool x28;                         // +0x28, the constructor's `stb r0,40(r31)`; not reached here
};

extern "C" void* fn_801FDAE8(SCarve801FDAE8Element* self, short flag) {
  if (self) {
    self->mVTable = lbl_803B7BE4;
    fn_801FD6F0(&self->mMember, -1);
    self->mName.~basic_string();
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}