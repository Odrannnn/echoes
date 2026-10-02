// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:8273` (`fn_801FD67C = .text:0x801FD67C; // size:0x74`),
// and the 116 bytes below are retail's own, read this run out of the disc with
// `python3 tools/dol_read.py 0x801FD67C 0x74` (`orig/G2ME01/sys/main.dol`, text section 1: file
// offset 0x640 at address 0x80003840, so this address is file offset 0x1FA47C; the 116 bytes were
// compared word for word with `build/G2ME01/main.elf`'s and are identical) and **not** out of
// `build/G2ME01/main.elf` as an authority - once this unit is in the link that range of the linked
// ELF holds *our* bytes.  The body below is the C++ those bytes are the compilation of, and
// `tools/flip_test.sh` is what says so.
//
// .text 0x801FD67C..0x801FD6F0, 0x74 = 116 bytes, 1 function:
//
//   fn_801FD67C    0x801FD67C  0x74   29 instructions   the 0x24-byte element's deleting destructor
//
//   801fd67c  94 21 ff f0   stwu   r1,-16(r1)
//   801fd680  7c 08 02 a6   mflr   r0
//   801fd684  90 01 00 14   stw    r0,20(r1)
//   801fd688  93 e1 00 0c   stw    r31,12(r1)
//   801fd68c  7c 9f 23 78   mr     r31,r4            ; the deleting flag, kept whole in r31
//   801fd690  93 c1 00 08   stw    r30,8(r1)
//   801fd694  7c 7e 1b 79   mr.    r30,r3            ; receiver guard: MWCC's null `this` test
//   801fd698  41 82 00 3c   beq    .+0x58
//   801fd69c  3c 80 80 3b   lis    r4,lbl_803B7BFC@ha
//   801fd6a0  38 7e 00 14   addi   r3,r30,0x14       ; &mMember
//   801fd6a4  38 04 7b fc   addi   r0,r4,lbl_803B7BFC@l
//   801fd6a8  38 80 ff ff   li     r4,-1            ; "do not free me afterwards"
//   801fd6ac  90 1e 00 00   stw    r0,0(r30)         ; the vptr, restored to this class
//   801fd6b0  48 00 00 41   bl     0x801fd6f0        ; fn_801FD6F0(&mMember, -1)
//   801fd6b4  34 1e 00 04   addic. r0,r30,4          ; &mName - the *named* destructor call's
//   801fd6b8  41 82 00 0c   beq    .+0x48              ; null test on the member's own address
//   801fd6bc  38 7e 00 04   addi   r3,r30,4
//   801fd6c0  48 10 12 f9   bl     0x802fe9b8        ; ~rstl::basic_string
//   801fd6c4  7f e0 07 35   extsh. r0,r31            ; the flag, sign-extended to 16 bits
//   801fd6c8  40 81 00 0c   ble    .+0x58            ; ... so `flag > 0` is the free test
//   801fd6cc  7f c3 f3 78   mr     r3,r30
//   801fd6d0  48 0d 0c b9   bl     0x802ce388        ; CMemory::Free(self)
//   801fd6d4  80 01 00 14   lwz    r0,20(r1)
//   801fd6d8  7f c3 f3 78   mr     r3,r30            ; the receiver is returned
//   801fd6dc  83 e1 00 0c   lwz    r31,12(r1)
//   801fd6e0  83 c1 00 08   lwz    r30,8(r1)
//   801fd6e4  7c 08 03 a6   mtlr   r0
//   801fd6e8  38 21 00 10   addi   r1,r1,16
//   801fd6ec  4e 80 00 20   blr
//
// **What it is: the deleting destructor of one 0x24-byte element of a script-object block.**  The
// shape is the port's stock one (`mr. r30,r3 / beq` guards the receiver, the vptr goes back first,
// the members are destroyed in reverse declaration order, `CMemory::Free(self)` is reached only
// behind `extsh. r0,r31 / ble`, i.e. `flag > 0`), and `src/MetroidPrime/CIOWinDtor.cpp` documents
// and reproduces the same shape.
//
// **The `-1` comes from the claim immediately in front of this one, and that claim is `Matching`
// for real.**  `src/MetroidPrime/ScriptObjects/Carve801FD638.c` (0x801FD638..0x801FD67C) is
// `rstl::destroy<T>`'s two halves for exactly this element: its `fn_801FD658` (0x801FD658, 0x24) is
// `li r4,-1` / `bl fn_801FD67C` (measured: `0x801FD660 li r4,-1`, `0x801FD668 bl 801fd67c`) - that
// is `rstl::destroy_impl`, and it is the caller this destructor's flag is written for.  It also
// measures the element size: its header records the two retail callers of `fn_801FD638`,
// `fn_801FD5E8` (0x801FD5E8) and `fn_801FF66C` (0x801FF66C, in the `Matching`
// `src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp`), each stepping its cursor by 36
// (`addi r31,r31,36` at 0x801FD610 and 0x801FF694), so the stride is **0x24 = 36**.
//
// **The layout is read off this class's own copy constructor**, `fn_801FEE88` (0x801FEE88, 0x68,
// still unclaimed, standing in the port link as `stub_200`), which is the one function retail
// wrote that touches every member this destructor reaches.  Disassembled this run it writes:
//
//   801fee90  lis r5,0x803b / 801fee98 addi r0,r5,31692   -> stw at +0x00   the *base* vtable
//                                                                     lbl_803B7BCC
//   801feeb0  lis r3,0x803b / 801feeb8 addi r0,r3,31740   -> stw at +0x00   then this class's own
//                                                                     lbl_803B7BFC over it
//   801feebc  addi r3,r30,4 ... bl __ct__Q24rstl66basic_string<...>  -> +0x04 mName
//   801feec8  addi r3,r30,20 / 801feecc addi r4,r31,20    -> +0x14 bl fn_801FE8B8, the member's own
//                                                       copy constructor
//
// so the element is
//
//   +0x00  void*                the vptr (base vtable `lbl_803B7BCC`, own `lbl_803B7BFC`)
//   +0x04  rstl::string mName   16 bytes under mwcceppc - `mPtr`, `mCow`, `mSize` and the
//                               word-sized empty `rmemory_allocator` (`rstl/string.hpp:106-110`),
//                               so the member lands at +0x14 and not at +0x10
//   +0x14  the member this calls `fn_801FD6F0` on - 0x24 - 0x14 = 0x10 bytes: `fn_801FD6F0` reads
//                               the count at +4 of its receiver (`lwz r0,4(r30)`) and the buffer at
//                               +0xC (`lwz r5,12(r30)`), and the member's own copy constructor
//                               `fn_801FE8B8` reads +4 and +8 and allocates `count * 0x14` into
//                               +0xC - the same count/capacity/buffer shape
//
// for **0x24 = 36 bytes**, the stride the two retail callers above measure.  Nothing writes +0x14's
// first word and nothing in these 116 bytes reads it, so it is carried as an unnamed
// `unsigned int x00`: it is there because it is what puts the count at +0x18 of the element.
// Nothing else about the class is asserted - retail never names it, and these members are named
// here only because the bytes reach them.
//
// **Its twin is byte-for-byte identical to a `Matching` unit except one immediate.**  `src/
// MetroidPrime/ScriptObjects/Carve801FD924.cpp` (0x801FD924..0x801FD998, `Matching` in this
// directory, and the element is the same 0x24 bytes) is these 29 instructions word for word apart
// from `addi r0,r4,31728` where this one has `addi r0,r4,31740` - `lbl_803B7BF0` against
// `lbl_803B7BFC`.  The same member offset (`addi r3,r30,20`) in both.  The third copy of the shape,
// `src/MetroidPrime/ScriptObjects/Carve801FDAE8.cpp` (0x801FDAE8..0x801FDB5C, also `Matching`, on a
// 0x2C-byte element) differs from this one in the member offset (`addi r3,r30,24`) and the vtable
// immediate (31716).  All three `bl` the same `fn_801FD6F0` at +0x14/+0x18, the same
// `internal_dereference__Q24rstl66basic_string<...>` at 0x802FE9B8 and the same
// `Free__7CMemoryFPCv` at 0x802CE388.  So the parts that are *not* the one immediate are read
// straight off two byte-exact siblings rather than guessed - and what the siblings are worth is
// more than that immediate: they are the measured record of which of the two teardown spellings
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
// **`lbl_803B7BFC` is named, not written as its address, and that is what makes the unit match.**
// It is retail's own `.data` object (`symbols.txt:18319`, `size:0xC`; `objdump -s
// --start-address=0x803B7BF0 --stop-address=0x803B7C08 build/G2ME01/main.elf` gives `803b7bf0
// 00000000 00000000 801ff4b4` and `803b7c00 00000000 801ff4bc`, i.e. at 0x803B7BFC
// `{0, 0, &fn_801FF4BC}` - a one-virtual-method vtable whose accessor returns the type constant 1,
// and that accessor is defined by `src/MetroidPrime/ScriptObjects/Carve801FF4A4.c:31`, a `Matching`
// unit, which claims 0x801FF4A4..0x801FF4C4 and so covers `fn_801FF4BC`).  Neither this object's
// bytes nor its unit is claimed, so the matching build takes the `.data` from dtk's own
// `auto_07_803B7AE0_data.o` while the port link gets a stand-in (`stub_data_8`).  Spelling the
// constant out instead - `= 0x803B7BFC` or `= 0x803B7BFCu` - leaves the four instruction words
// identical and **loses the `R_PPC_ADDR16_HA` / `R_PPC_ADDR16_LO` relocations objdiff pairs against
// retail's**, which is a green `main.dol` and no matched function; that trade was measured on the
// two identical siblings by the lanes whose notes are `build/goal/notes/carve-801fdae8.md` and
// `build/goal/notes/carve-801fd924` before this run.  `matched_functions` in `build/report.json` is
// the check for it, not the sha1 and not the percentage.
//
// Retail names none of these, so the definition has to be `extern "C"`: a C++ one would mangle to
// `_Z<len>fn_801FD67C<len>...` and objdiff would pair nothing.  That is also why the unit is a
// `.cpp` rather than a `.c`: it needs `rstl::string` for `mName`'s destructor, and the mangled
// name of that destructor is not made of identifier characters.  Same convention as
// `ScriptObjects/Carve801FF5A0.cpp`, `Carve801FF8A0.cpp` and `ScriptObjects/Carve801FD924.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  One function, so the rule has nothing to bite on here;
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD67C` is run
// anyway.
//
// Its own unit because a claim may not span an unclaimed gap.  In front of it the claim
// `ScriptObjects/Carve801FD638.c` (0x801FD638..0x801FD67C, `Matching`) ends exactly where this
// claim starts, and behind it `fn_801FD6F0` (0x801FD6F0, 0x84) starts exactly where this claim
// ends and is unclaimed - the nearest claimed range above it, `ScriptObjects/Carve801FD8E0.c`,
// starts at 0x801FD8E0 - so the claim stops at 0x801FD6F0 and that function stays retail's.  dtk's
// own `build/G2ME01/asm/auto_03_801FD67C_text.s` holds `# 0x801FD67C..0x801FD8E0 | size: 0x264`,
// whose first per-function header is `# .text:0x0 | 0x801FD67C | size: 0x74` and whose second is
// `# .text:0x74 | 0x801FD6F0 | size: 0x84`, so 0x801FD67C..0x801FD6F0 is this function and
// nothing else.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FD638.c` (0x801FD638..0x801FD67C) and
// `ScriptObjects/Carve801FD8E0.c` (0x801FD8E0..0x801FD924).

#include "rstl/string.hpp"

/** 0x802CE388, `symbols.txt:12992`: `CMemory::Free(void const*)`, claimed by
 *  `Kyoto/Alloc/CMemory.cpp` in both builds (`powerpc-eabi-nm` shows `802ce388 T
 *  Free__7CMemoryFPCv`).  Declared under retail's own emitted spelling so the call needs no
 *  header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x801FD6F0, `symbols.txt:8274`, 0x84 = 132 bytes: the +0x14 member's own deleting destructor -
 *  it walks `x04_count` 0x14-strided elements through `fn_801FD774` (which releases each
 *  `rstl::basic_string` in turn), frees `x0c_buffer`, and then frees its own receiver behind its
 *  own `extsh. r0,r31 / ble` flag test - the same tail this function has.  It is a callee, not part
 *  of this claim, and it is still unclaimed, so dtk supplies it to the DOL from its own `auto_*`
 *  object while the port link gets a stand-in (`stub_227`, already present for the two identical
 *  siblings).  Declared, never defined. */
extern "C" void fn_801FD6F0(void* self, int flag);

/** `lbl_803B7BFC` = 0x803B7BFC, `symbols.txt:18319`, `.data` `size:0xC`, holding
 *  `{0, 0, &fn_801FF4BC}`.  Retail's `.data`, supplied to the DOL by dtk's own
 *  `auto_07_803B7AE0_data.o` and to the port link by a data stub.  **Taking its address is what
 *  puts the two `R_PPC_ADDR16_HA` / `R_PPC_ADDR16_LO` relocations into our object**, which is
 *  what objdiff pairs against retail's - see the header. */
extern "C" char lbl_803B7BFC[];

/** The element's +0x14 member, as far as this destructor, this class's copy constructor
 *  `fn_801FEE88` and `fn_801FD6F0` are concerned.  `fn_801FD6F0` reads the count at +4 of it and
 *  the buffer at +0xC (`lwz r0,4(r30)` / `lwz r5,12(r30)`), and the member's own copy constructor
 *  `fn_801FE8B8` reads +4 and +8 and allocates `count * 0x14` into +0xC, which is the same
 *  count/capacity/buffer shape.  Nothing in this file reads these words; they are here because
 *  they are what puts the buffer at +0xC. */
struct SCarve801FD67CMember {
  unsigned int x00;
  int x04_count;
  unsigned int x08_capacity;
  void* x0c_buffer;
};

/** The 0x24-byte element, as far as this destructor reaches into it.  Its two members and the
 *  total are the measured layout described in the header; nothing else is asserted about a class
 *  retail does not name. */
struct SCarve801FD67CElement {
  void* mVTable;                // +0x00, the `stw r0,0(r30)` above; lbl_803B7BFC is this class's
                                // own vtable, and lbl_803B7BCC its base's
  rstl::string mName;           // +0x04, `addic. r0,r30,4` / `addi r3,r30,4` above
  SCarve801FD67CMember mMember; // +0x14, `addi r3,r30,20` above
};

extern "C" void* fn_801FD67C(SCarve801FD67CElement* self, short flag) {
  if (self) {
    self->mVTable = lbl_803B7BFC;
    fn_801FD6F0(&self->mMember, -1);
    self->mName.~basic_string();
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}