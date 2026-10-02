// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:8281` (`fn_801FD924 = .text:0x801FD924; // size:0x74`),
// and the 116 bytes below are retail's own, read this run out of the disc with
// `python3 tools/dol_read.py 0x801FD924 0x74` (`orig/G2ME01/sys/main.dol`, text section 1: file
// offset 0x640 at address 0x80003840) and **not** out of `build/G2ME01/main.elf`, which holds our
// own bytes once this unit is in the link.  The body below is the C++ those bytes are the
// compilation of, and `tools/flip_test.sh` is what says so.
//
// .text 0x801FD924..0x801FD998, 0x74 = 116 bytes, 1 function:
//
//   fn_801FD924    0x801FD924  0x74   29 instructions   the 0x24-byte element's deleting destructor
//
//   801fd924  94 21 ff f0   stwu   r1,-16(r1)
//   801fd928  7c 08 02 a6   mflr   r0
//   801fd92c  90 01 00 14   stw    r0,0x14(r1)
//   801fd930  93 e1 00 0c   stw    r31,0xc(r1)
//   801fd934  7c 9f 23 78   mr     r31,r4            ; the deleting flag, kept whole in r31
//   801fd938  93 c1 00 08   stw    r30,0x8(r1)
//   801fd93c  7c 7e 1b 79   mr.    r30,r3            ; receiver guard: MWCC's null `this` test
//   801fd940  41 82 00 3c   beq    .+0x58
//   801fd944  3c 80 80 3b   lis    r4,lbl_803B7BF0@ha
//   801fd948  38 7e 00 14   addi   r3,r30,0x14       ; &mMember
//   801fd94c  38 04 7b f0   addi   r0,r4,lbl_803B7BF0@l
//   801fd950  38 80 ff ff   li     r4,-1            ; "do not free me afterwards"
//   801fd954  90 1e 00 00   stw    r0,0(r30)         ; the vptr, restored to this class
//   801fd958  4b ff fd 99   bl     0x801fd6f0        ; fn_801FD6F0(&mMember, -1)
//   801fd95c  34 1e 00 04   addic. r0,r30,4          ; &mName - the *named* destructor call's
//   801fd960  41 82 00 0c   beq    .+0x48              ; null test on the member's own address
//   801fd964  38 7e 00 04   addi   r3,r30,4
//   801fd968  48 10 10 51   bl     0x802fe9b8        ; ~rstl::basic_string
//   801fd96c  7f e0 07 35   extsh. r0,r31            ; the flag, sign-extended to 16 bits
//   801fd970  40 81 00 0c   ble    .+0x58            ; ... so `flag > 0` is the free test
//   801fd974  7f c3 f3 78   mr     r3,r30
//   801fd978  48 0d 0a 11   bl     0x802ce388        ; CMemory::Free(self)
//   801fd97c  80 01 00 14   lwz    r0,0x14(r1)
//   801fd980  7f c3 f3 78   mr     r3,r30            ; the receiver is returned
//   801fd984  83 e1 00 0c   lwz    r31,0xc(r1)
//   801fd988  83 c1 00 08   lwz    r30,0x8(r1)
//   801fd98c  7c 08 03 a6   mtlr   r0
//   801fd990  38 21 00 10   addi   r1,r1,16
//   801fd994  4e 80 00 20   blr
//
// **What it is: the deleting destructor of one 0x24-byte element of a script-object block.**  The
// shape is the port's stock one (`mr. r30,r3 / beq` guards the receiver, the vptr goes back first,
// the members are destroyed in reverse declaration order, `CMemory::Free(self)` is reached only
// behind `extsh. r0,r31 / ble`, i.e. `flag > 0`), and `src/MetroidPrime/CIOWinDtor.cpp` documents
// and reproduces the same shape.
//
// **The `-1` comes from the claim immediately in front of this one, and that claim is `Matching`
// for real.**  `src/MetroidPrime/ScriptObjects/Carve801FD8E0.c` (0x801FD8E0..0x801FD924) is
// `rstl::destroy<T>`'s two halves for exactly this element: its `fn_801FD900` (0x801FD900, 0x24)
// is `li r4,-1` / `bl fn_801FD924` - that is `rstl::destroy_impl`, and it is the caller this
// destructor's flag is written for.
//
// **The element is 0x24 = 36 bytes, and the two member offsets below are the measured ones.**
// The stride is measured twice independently, both off retail's own walks that call
// `rstl::destroy` on this element: `fn_801FD890` (0x801FD890, 0x50, unclaimed) at 0x801FD8B8
// (`bl fn_801FD8E0` at 0x801FD8B4, then `addi r31,r31,36`), and `fn_801FF7EC` in the `Matching`
// `src/MetroidPrime/ScriptObjects/Carve801FF720.cpp` (`bl fn_801FD8E0` at 0x801FF810, then
// `addi r31,r31,0x24`).  Inside the element this destructor touches exactly two members, both by
// offset taken from its own bytes above:
//
//   +0x00  the vptr               this destructor restores it, and the class's own copy
//                                 constructor `fn_801FEAE0` (0x801FEAE0, 0x68, unclaimed) writes
//                                 the base vtable `lbl_803B7BCC` at +0x0 and this class's
//                                 `lbl_803B7BF0` over it - which is where the constant below
//                                 comes from
//   +0x04  rstl::string mName     16 bytes under mwcceppc - `mPtr`, `mCow`, `mSize` and the
//                                 word-sized empty `rmemory_allocator` (`rstl/string.hpp:106-110`),
//                                 so the next member is at +0x14 and not at +0x10.  `fn_801FEAE0`
//                                 copy-constructs it at `addi r3,r30,4`.
//   +0x14  the member this calls `fn_801FD6F0` on - 0x24 - 0x14 = 0x10 bytes, `fn_801FEAE0`'s
//                                 third act is `addi r3,r30,20 / addi r4,r31,20 / bl fn_801FE8B8`,
//                                 i.e. the same offset it copies from
//
// and nothing else about the class is asserted: retail never names it, and the two members are
// named here only because the bytes reach them.
//
// **Its twin is exact, and it is worth naming because it is what fixes the shape.**
// `fn_801FD67C` (0x801FD67C, 0x74, unclaimed) is these 29 instructions word for word apart from a
// single immediate: it stores `31740` where this one stores `31728` (0x803B7BFC against
// 0x803B7BF0).  Its own claim above it, `src/MetroidPrime/ScriptObjects/Carve801FD638.c`
// (0x801FD638..0x801FD67C), is the `rstl::destroy` pair for *that* element.  The third copy of the
// shape is `fn_801FDAE8` (0x801FDAE8, 0x74), which differs from this one in the member offset
// (`addi r3,r30,24` against `addi r3,r30,20`) and the vtable immediate (31716 against 31728) -
// re-measured this run.  None of the three is `Matching`, so there is no matched sibling of the
// whole body to copy; what is matched is the destructor *tail* -
// `src/MetroidPrime/ScriptObjects/Carve801FBC58.c:172-180`, whose `fn_801FBC58` has the receiver
// guard (`mr. r30,r3 / beq`), the `Free__7CMemoryFPCv(self)` behind the `extsh. / ble` flag test
// and the pointer return in the epilogue - and that is where those four parts come from.
//
// **The deleting flag is a `short`, which is what `extsh. r0,r31` measures.**  `self->mName`'s
// teardown is spelled as a *named* destructor call because retail has the null test on the
// member's address (`addic. r0,r30,4 / beq`); leaving it to scope exit emits no test at all, and
// spelling it as a plain call on `&self->mName` loses it too.  `fn_801FD6F0` is the opposite case
// and is spelled as a plain call on an address: retail has **no** test before that `bl`.  Both
// halves of the rule are measured elsewhere in this tree -
// `src/MetroidPrime/ScriptLoader/Carve802201F8.cpp:23-36` and `src/MetroidPrime/CAnimData.cpp:519`
// for the named call, `src/MetroidPrime/Carve800E10EC.cpp` for the plain one.  Note
// the name after the `~` has to be the **unqualified injected class name** (`~basic_string()`,
// not `~rstl::string()`): the qualified spelling is a syntax error in MWCC 2.7.
//
// **`lbl_803B7BF0` is named, not written as its address, and that is what makes the unit match.**
// It is retail's own `.data` object (`symbols.txt:18318`, `size:0xC`; `objdump -s
// --start-address=0x803B7BF0 --stop-address=0x803B7BFC build/G2ME01/main.elf` gives `00000000
// 00000000 801ff4b4`, i.e. `{0, 0, &fn_801FF4B4}` - a one-virtual-method vtable whose accessor
// returns the type constant 2, and that accessor is defined by
// `src/MetroidPrime/ScriptObjects/Carve801FF4A4.c:33`, a `Matching` unit).  Neither this object's
// bytes nor its unit is claimed, so the matching build takes the `.data` from dtk's own
// `auto_07_803B7AE0_data.o` (`powerpc-eabi-nm` shows `00000110 D lbl_803B7BF0` in it) while the
// port link gets a stand-in.  Spelling the constant out instead - `= 0x803B7BF0` or
// `= 0x803B7BF0u` - leaves the four instruction words identical and **loses the
// `R_PPC_ADDR16_HA` / `R_PPC_ADDR16_LO` relocations objdiff pairs against retail's**, which is a
// green `main.dol` and no matched function; that trade was measured on the sibling `fn_801FDAE8`
// (the same 29 instructions with the member at +0x18) by the lane whose notes are
// `build/goal/notes/carve-801fdae8.md`, which is why this unit went straight to the spelling
// above rather than trying the constant.  `matched_functions` in `build/report.json` is the check
// for it, not the sha1 and not the percentage.
//
// Retail names none of these, so the definition has to be `extern "C"`: a C++ one would mangle to
// `_Z<len>fn_801FD924<len>...` and objdiff would pair nothing.  That is also why the unit is a
// `.cpp` rather than a `.c`: it needs `rstl::string` for `mName`'s destructor, and the mangled
// name of that destructor is not made of identifier characters.  Same convention as
// `ScriptObjects/Carve801FF5A0.cpp` and `ScriptObjects/Carve801FF8A0.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  One function, so the rule has nothing to bite on here;
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD924` is run
// anyway.
//
// Its own unit because a claim may not span an unclaimed gap.  In front of it the claim
// `ScriptObjects/Carve801FD8E0.c` (0x801FD8E0..0x801FD924, `Matching`) ends exactly where this
// claim starts, and behind it `fn_801FD998` (0x801FD998, 0x84) starts exactly where this claim
// ends and is unclaimed - the nearest claimed range above it, `ScriptObjects/Carve801FDAA4.c`,
// starts at 0x801FDAA4 - so the claim stops at 0x801FD998 and that function stays retail's.
// dtk's own `build/G2ME01/asm/auto_03_801FD924_text.s` holds `# 0x801FD924..0x801FDAA4 | size:
// 0x180`, whose first per-function header is `# .text:0x0 | 0x801FD924 | size: 0x74` and whose
// second is `# .text:0x74 | 0x801FD998 | size: 0x84`, so 0x801FD924..0x801FD998 is this function
// and nothing else.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FD8E0.c` (0x801FD8E0..0x801FD924) and `ScriptObjects/Carve801FDAA4.c`
// (0x801FDAA4..0x801FDAE8).

#include "rstl/string.hpp"

/** 0x802CE388, `symbols.txt:12992`: `CMemory::Free(void const*)`, claimed by
 *  `Kyoto/Alloc/CMemory.cpp` in both builds.  Declared under retail's own emitted spelling so the
 *  call needs no header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x801FD6F0, `symbols.txt:8275`, 0x84 = 132 bytes: the +0x14 member's own deleting destructor -
 *  it walks `x04_count` 0x14-strided elements through `fn_801FD774` (which releases each
 *  `rstl::basic_string` in turn), frees `x0c_buffer`, and then frees its own receiver behind its
 *  own `extsh. r0,r31 / ble` flag test.  Its bytes are measured: dtk's
 *  `build/G2ME01/asm/auto_03_801FD67C_text.s` carries it at `# .text:0x74 | 0x801FD6F0 | size:
 *  0x84`, i.e. the unclaimed range in front of this one.  It is a callee, not part of this claim, and it
 *  is still unclaimed, so dtk supplies it to the DOL from its own `auto_*` object while the port
 *  link gets a stand-in.  Declared, never defined. */
extern "C" void fn_801FD6F0(void* self, int flag);

/** `lbl_803B7BF0` = 0x803B7BF0, `symbols.txt:18318`, `.data` `size:0xC`, holding
 *  `{0, 0, &fn_801FF4B4}`.  Retail's `.data`, supplied to the DOL by dtk's own
 *  `auto_07_803B7AE0_data.o` and to the port link by a data stub.  **Taking its address is what
 *  puts the two `R_PPC_ADDR16_HA` / `R_PPC_ADDR16_LO` relocations into our object**, which is
 *  what objdiff pairs against retail's - see the header. */
extern "C" char lbl_803B7BF0[];

/** The element's +0x14 member, as far as this destructor and the class's own copy constructor
 *  `fn_801FEAE0` are concerned.  `fn_801FD6F0` reads the count at +4 of it and the buffer at +0xC
 *  (`lwz r0,4(r30)` / `lwz r5,12(r30)`), and the copy constructor `fn_801FE8B8` (0x801FE8B8, 0xC4,
 *  disassembled this run) reads +4 and +8 and allocates `count * 0x14` into +0xC, which is the same
 *  count/capacity/buffer shape.  Nothing in this file reads these words; they are here because
 *  they are what puts the buffer at +0xC. */
struct SCarve801FD924Member {
  unsigned int x00;
  int x04_count;
  unsigned int x08_capacity;
  void* x0c_buffer;
};

/** The 0x24-byte element, as far as this destructor reaches into it.  Its first two members and
 *  the total are the measured layout described in the header; nothing here is asserted about a
 *  class retail does not name. */
struct SCarve801FD924Element {
  void* mVTable;                 // +0x00, the `stw r0,0(r30)` above; lbl_803B7BF0 is this class's
                                 // own vtable, and lbl_803B7BCC its base's
  rstl::string mName;            // +0x04, `addic. r0,r30,4` / `addi r3,r30,4` above
  SCarve801FD924Member mMember;  // +0x14, `addi r3,r30,20` above
};

extern "C" void* fn_801FD924(SCarve801FD924Element* self, short flag) {
  if (self) {
    self->mVTable = lbl_803B7BF0;
    fn_801FD6F0(&self->mMember, -1);
    self->mName.~basic_string();
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
