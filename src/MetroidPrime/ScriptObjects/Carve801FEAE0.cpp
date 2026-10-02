// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:8313`
// (`fn_801FEAE0 = .text:0x801FEAE0; // type:function size:0x68`), and the 104 bytes below are
// retail's own, read out of the disc with `python3 tools/dol_read.py 0x801FEAE0 0x68`
// (`orig/G2ME01/sys/main.dol`) and **not** out of `build/G2ME01/main.elf`, which holds our own
// bytes once this unit is in the link.  The body below is the C++ those bytes are the
// compilation of, and `tools/flip_test.sh` is what says so.
//
// .text 0x801FEAE0..0x801FEB48, 0x68 = 104 bytes, 1 function:
//
//   fn_801FEAE0    0x801FEAE0  0x68   26 instructions   the 0x24-byte element's copy constructor
//
//   801feae0  94 21 ff f0   stwu   r1,-0x10(r1)
//   801feae4  7c 08 02 a6   mflr   r0
//   801feae8  3c a0 80 3b   lis    r5,lbl_803B7BCC@ha
//   801feaec  90 01 00 14   stw    r0,0x14(r1)
//   801feaf0  38 05 7b cc   addi   r0,r5,lbl_803B7BCC@l   ; the base vtable
//   801feaf4  93 e1 00 0c   stw    r31,0xc(r1)
//   801feaf8  7c 9f 23 78   mr     r31,r4                 ; the source, kept whole in r31
//   801feafc  38 9f 00 04   addi   r4,r31,0x4              ; &src->mName
//   801feb00  93 c1 00 08   stw    r30,0x8(r1)
//   801feb04  7c 7e 1b 78   mr     r30,r3                 ; the receiver
//   801feb08  3c 60 80 3b   lis    r3,lbl_803B7BF0@ha
//   801feb0c  90 1e 00 00   stw    r0,0x0(r30)            ; base vptr, then
//   801feb10  38 03 7b f0   addi   r0,r3,lbl_803B7BF0@l   ; this class's own vtable
//   801feb14  38 7e 00 04   addi   r3,r30,0x4
//   801feb18  90 1e 00 00   stw    r0,0x0(r30)            ; ... over it, both at +0x0
//   801feb1c  48 10 06 19   bl     0x802ff134              ; __ct__...basic_string<c,...>FRC...
//   801feb20  38 7e 00 14   addi   r3,r30,0x14             ; &mMember
//   801feb24  38 9f 00 14   addi   r4,r31,0x14             ; &src->mMember
//   801feb28  4b ff fd 91   bl     fn_801fe8b8             ; the member's own copy constructor
//   801feb2c  80 01 00 14   lwz    r0,0x14(r1)
//   801feb30  7f c3 f3 78   mr     r3,r30                 ; the receiver is returned
//   801feb34  83 e1 00 0c   lwz    r31,0xc(r1)
//   801feb38  83 c1 00 08   lwz    r30,0x8(r1)
//   801feb3c  7c 08 03 a6   mtlr   r0
//   801feb40  38 21 00 10   addi   r1,r1,0x10
//   801feb44  4e 80 00 20   blr
//
// **What it is: the copy constructor of one 0x24-byte script-object element.**  Retail names it
// nothing, so everything here is read off those bytes and off the two claims that bracket it:
//
//   `src/MetroidPrime/ScriptObjects/Carve801FEA98.c` (0x801FEA98..0x801FEAE0, `Matching`) is
//     `rstl::construct<T>`'s two halves for this element: its `fn_801FEAB8` (0x801FEAB8, 0x28) is
//     the null-guarded `if (dest != 0) T(dest, src)` and its one call **is** the `bl` at
//     0x801FEACC into this function.  That file also fixes the element's size at 0x24 = 36 bytes,
//     measured twice off retail's own copy walks (`fn_801FEA2C` steps its destination cursor by
//     36, and so does `fn_801FF838` in the `Matching` `Carve801FF720.cpp`), and it names the two
//     `rstl::basic_string` callees this body reaches.
//   `src/MetroidPrime/ScriptObjects/Carve801FD924.cpp` (0x801FD924..0x801FD998, `Matching`) is
//     **the same class's deleting destructor**: it restores this class's own vtable
//     (`lbl_803B7BF0`, the second store above) into +0x0, destroys the `rstl::string` at +0x04 and
//     calls `fn_801FD6F0` on the member at +0x14 - the same member this constructor hands to
//     `fn_801FE8B8`.  Its header records the layout below, word for word, and it is why nothing
//     here has to be guessed: the destructor says what the bytes in front of them mean.
//
// **The layout, all of it from the two claims above.**  0x24 = 36 bytes: the vptr at +0x00, an
// `rstl::string` at +0x04 (16 bytes under mwcceppc - `mPtr`, `mCow`, `mSize` and the word-sized
// empty `rmemory_allocator`, `rstl/string.hpp:106-110` - which is what puts the next member at
// +0x14 and not at +0x10), and a 0x10-byte member at +0x14.  `fn_801FE8B8` reads the count at +4
// and the capacity at +8 of that member and allocates `count * 0x14` into +0xC, the same
// count/capacity/buffer shape `fn_801FD6F0` reads, so the four words are named here the same way
// `Carve801FD924.cpp` names them.  Nothing beyond those two offsets is asserted about a class
// retail does not name.
//
// **Why this is spelled as a constructor, in three measured pieces.**  The members are
// **copy-constructed in place** - a bare `addi r3,r30,4` with **no null test** in front of the
// `bl`, which `rstl::construct` / placement `new` cannot produce (it costs an `addic.` + `beq`
// guard); the two vptr stores come **before** the member inits, which is where a constructor's
// base-class constructors run and not where a hand-written body would; and the receiver is
// returned in `r3` (`mr r3,r30`), mwcceppc's constructor epilogue - the same `mr r3,r30` the
// `Matching` `__ct__6CIOWinFRCQ24rstl66basic_string<...>` at 0x80049E98 ends with.
//
// **The recipe, and the three spellings measured against it.**  A compiler-generated constructor
// can be reproduced without knowing the class retail wrote: the vptr is an ordinary `void*`
// member, and each vptr store is a **base class's constructor**.  Four spellings were compiled
// with `tools/probe_cc.sh` this run and compared instruction by instruction with
// `tools/bytescmp.py` against retail's own bytes:
//
//   1. a non-empty base whose constructor stores `lbl_803B7BCC`, plus a **second empty base** at
//      the same offset meant to store `lbl_803B7BF0`: 26 instructions, but the second store came
//      out `stw r0,4(r30)` - mwcceppc gave the empty base offset 0x4, not 0x0, so the shape was
//      25 of 26 with one wrong displacement;
//   2. an **empty** base writing `lbl_803B7BCC` through `this` and a `void*` member initialised
//      in the mem-init list: the two stores came out at 0x0 and 0x4 for the same reason, and the
//      member at +0x8 pushed `mName` off +0x4;
//   3. a non-empty base storing `lbl_803B7BCC` with a `void*` member initialised in the mem-init
//      list: same two failures, plus `addi r4,r31,8` instead of `+4`;
//   4. **the one in this file**: a two-level base chain, `SCarve801FEAE0Base` (the non-empty one,
//      storing `lbl_803B7BCC`) and `SCarve801FEAE0Self` derived from it and storing
//      `lbl_803B7BF0` over the same word.  A single-inheritance chain gives every level offset
//      0x0, so both stores land at `stw r0,0(r30)`, and mwcceppc emits each base's constructor
//      before the derived class's mem-init list, which is what puts both vptr stores in front of
//      the string and the member.  **Measured: 26 of 26 instructions, 104 of 104 bytes, and the
//      six differing words are all relocated fields** - the four `lis`/`addi` immediates of the two
//      `.data` vtables and the two `bl` displacements.
//
// **`lbl_803B7BCC` and `lbl_803B7BF0` are named, not written as their addresses, and that is what
// makes the unit match.**  Both are retail's own `.data` objects (`symbols.txt:18315` and
// `:18318`, `size:0xC` each; `objdump -s --start-address=0x803B7BC0 --stop-address=0x803B7C08
// build/G2ME01/main.elf` gives `lbl_803B7BF0 = {0, 0, &fn_801FF4B4}`, a one-virtual-method vtable
// whose accessor is defined by `src/MetroidPrime/ScriptObjects/Carve801FF4A4.c:33`, a `Matching`
// unit).  Neither object is claimed by any unit, so the matching build takes the `.data` from dtk's
// own `auto_07_803B7AE0_data.o` (`powerpc-eabi-nm` shows `D lbl_803B7BCC` and `D lbl_803B7BF0` in
// it) while the port link gets a stand-in.  Taking their addresses is what puts the four
// `R_PPC_ADDR16_HA` / `R_PPC_ADDR16_LO` relocations into our object, which is what objdiff pairs
// against retail's - the trade `Carve801FD924.cpp` records for the same vtable.
//
// **The member's copy constructor is a thunk, because retail's is out of line under its own name.**
// `fn_801FE8B8` (0x801FE8B8, 0xC4, unclaimed) is a `bl`, not an inlined body: nothing is read out
// of the member before it and nothing is written back after it.  `SCarve801FEAE0Member`'s copy
// constructor is therefore spelled as a one-line forwarder to `fn_801FE8B8`, which mwcceppc inlines
// into this constructor, so the object's only call is retail's `bl` with retail's relocation.
// Declaring the member's copy constructor out of line instead would emit a `bl` under the *mangled*
// member name, and objdiff pairs relocations by name - `Carve801FD924.cpp`'s header is the writeup
// for that rule.
//
// **A `Matching` unit needs its callees' symbols, not their bodies.**  `fn_801FE8B8` is still
// unclaimed and dtk supplies its 0xC4 bytes to the DOL from `auto_03_801FDC88_text.o`
// (`powerpc-eabi-nm` shows `T fn_801FE8B8` in it); the `rstl::basic_string` copy constructor at
// 0x802FF134 is claimed and `Matching` (`rstl/rstl_strings.cpp`, `symbols.txt:13852`).  Only the
// port's flat link, which does not carry the `auto_*` objects, needs a stand-in, and that is one
// stub line.  (The header of `Carve801FEC64.c` and of this claim's neighbour say that matching
// these 0x68 bytes "needs the two `.data` vtables as well as the body of `fn_801FE8B8`, none of them
// claimed" - that is the port's trade written down as a limit on the DOL, and it is wrong on all
// three counts.  See "What the previous lane's blocker was wrong" in
// `docs/goal-notes/carve-801fecac.md`.)
//
// **One config change, and the rename it needs.**  A constructor's symbol is mangled and retail's
// name for this one is the placeholder `fn_801FEAE0`, so `config/G2ME01/symbols.txt:8313` is
// renamed to the name mwcceppc emits - **read out of this object with `powerpc-eabi-nm`, not
// guessed**: `__ct__21SCarve801FEAE0ElementFRC21SCarve801FEAE0Element`.  The class name is the
// carve's own placeholder, after the `SCarve801FD924Element` naming `Carve801FD924.cpp` already
// uses: retail's symbol table has no name for this class either, so nothing is being renamed *to a
// retail name*.  The precedent is `fn_80049E98` -> `__ct__6CIOWinFRCQ24rstl66basic_string<...>`
// (`src/MetroidPrime/CIOWinCtor.cpp`): an unnamed function whose bytes are a constructor's can only
// be reproduced by a constructor, so the symbol has to be given the constructor's name.
// `Carve801FEA98.c`'s declaration and call carry the same name - that reference is what proves the
// definition in the DOL link.  **The rename is load-bearing for the count, not for the hash**: with
// the placeholder name kept the DOL would still be byte-exact, but objdiff would pair nothing and
// the unit would count 0.
//
// **The port's half.**  This file is in `files.cmake`, so the PC build compiles it, and its
// `#ifndef __MWERKS__` block defines the same name as a plain `extern "C"` entry point - the host
// compiler mangles the constructor the Itanium way (`_ZN21SCarve801FEAE0ElementC1ERKS_`), while
// the port's call site is `Carve801FEA98.c`, a C file, which can only call an unmangled name.  That
// is what retires `stub_225` (`fn_801FEAE0`) in `src/MetroidPrime/PortLinkStubs.cpp`: this unit
// defines the symbol for the port's link too, and leaving both would be two definitions of one
// symbol in the port's flat link.  The three symbols the new body reaches the linker for -
// `fn_801FE8B8`, `lbl_803B7BCC` and `lbl_803B7BF0` - are a function stand-in and two data
// stand-ins there (`lbl_803B7BF0` is `stub_data_6`, already present from `Carve801FD924.cpp`).
//
// **`fn_801FE8B8` is a callee, not a dependency, and nothing is asserted about it.**  Its 0xC4
// bytes are dtk's; all this claim needs is the symbol.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  One function, so the rule has nothing to bite on here;
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FEAE0` is run
// anyway.
//
// Its own unit because a claim may not span an unclaimed gap.  In front of it
// `src/MetroidPrime/ScriptObjects/Carve801FEA98.c` (0x801FEA98..0x801FEAE0, `Matching`) ends
// exactly where this claim starts, and behind it `fn_801FEB48` (0x801FEB48, 0xB0) starts exactly
// where this claim ends and is still unclaimed, so the claim stops at 0x801FEB48.
// dtk's own `build/G2ME01/asm/auto_03_801FEAE0_text.s` holds
// `# 0x801FEAE0..0x801FEC64 | size: 0x184`, whose first per-function header is
// `# .text:0x0 | 0x801FEAE0 | size: 0x68` and whose second is
// `# .text:0x68 | 0x801FEB48 | size: 0xB0`, so 0x801FEAE0..0x801FEB48 is this function and
// nothing else.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FEA98.c` (0x801FEA98..0x801FEAE0) and `ScriptObjects/Carve801FEC64.c`
// (0x801FEC64..0x801FECAC).

// Placement `new`, for the port's build only: under `__MWERKS__` the constructor is reached
// through retail's own mangled name and nothing here needs it.  See the port paragraph above.
#if !defined(__MWERKS__)
#include <new>
#endif

#include "rstl/string.hpp"

/** `lbl_803B7BCC` = 0x803B7BCC, `symbols.txt:18315`, `.data` `size:0xC` - the **base** class's
 *  vtable, the first of the two stores above.  Retail's `.data`, supplied to the DOL by dtk's own
 *  `auto_07_803B7AE0_data.o` and to the port link by a data stand-in.  Taking its address is what
 *  puts its two `R_PPC_ADDR16_HA` / `R_PPC_ADDR16_LO` relocations into our object, which is what
 *  objdiff pairs against retail's - see the header. */
extern "C" char lbl_803B7BCC[];

/** `lbl_803B7BF0` = 0x803B7BF0, `symbols.txt:18318`, `.data` `size:0xC`, holding
 *  `{0, 0, &fn_801FF4B4}` - **this** class's own vtable, the second of the two stores above and
 *  the same one the `Matching` `Carve801FD924.cpp` restoring destructor writes back.  Supplied
 *  and paired exactly as `lbl_803B7BCC` above is. */
extern "C" char lbl_803B7BF0[];

/** 0x801FE8B8, `symbols.txt:8308`, 0xC4 = 196 bytes: the +0x14 member's own **copy constructor**,
 *  and a callee rather than part of this claim - dtk supplies its bytes to the DOL from its own
 *  `auto_03_801FDC88_text.o` while the port link gets a stand-in.  Declared, never defined. */
extern "C" void fn_801FE8B8(void* self, const void* src);

/** The 0x24-byte element's +0x14 member: 0x10 = 16 bytes, none of which this constructor reads or
 *  writes - `fn_801FE8B8` does all of it.  The four words are `Carve801FD924.cpp`'s layout for the
 *  same class and the same member (`fn_801FD6F0` reads the count at +4 and the buffer at +0xC of
 *  it; `fn_801FE8B8` reads +4 and +8 and allocates `count * 0x14` into +0xC), and they are here
 *  for the same reason: they are what puts the member at +0x14 and ends the object at +0x24.
 *
 *  The copy constructor is a **thunk to `fn_801FE8B8`** because retail's is: `0x801feb28` is a
 *  `bl` with nothing read out of the member in front of it and nothing written back after it, so
 *  the member's whole copy is that call.  Declaring it inline and letting mwcceppc inline it is
 *  what leaves the object a single `bl` carrying retail's relocation. */
struct SCarve801FEAE0Member {
  unsigned int x00;
  int x04_count;
  unsigned int x08_capacity;
  void* x0c_buffer;

  SCarve801FEAE0Member(const SCarve801FEAE0Member& src) { fn_801FE8B8(this, &src); }
};

/** The element's base class: the word at +0x0 and the first vptr store, and nothing else.  It
 *  carries no virtual function on purpose - a really polymorphic class would make mwcceppc emit a
 *  `__vt__...` into `.data`, which this claim must not own. */
struct SCarve801FEAE0Base {
  void* mVTable;  // +0x00

  SCarve801FEAE0Base() : mVTable(lbl_803B7BCC) {}
};

/** The class retail wrote, as far as its bytes here reach: the same single word, this class's own
 *  vtable.  The chain exists so both stores land at `stw r0,0(r30)` - see spelling 4 in the header.
 */
struct SCarve801FEAE0Self : SCarve801FEAE0Base {
  SCarve801FEAE0Self() { mVTable = lbl_803B7BF0; }
};

/** The 0x24-byte element: vptr at +0x00, `rstl::string` at +0x04, the member at +0x14.  Nothing
 *  else is asserted about it - retail names no such class. */
struct SCarve801FEAE0Element : SCarve801FEAE0Self {
  rstl::string mName;
  SCarve801FEAE0Member mMember;

  SCarve801FEAE0Element(const SCarve801FEAE0Element& src);
};

SCarve801FEAE0Element::SCarve801FEAE0Element(const SCarve801FEAE0Element& src)
    : SCarve801FEAE0Self(), mName(src.mName), mMember(src.mMember) {}

#if !defined(__MWERKS__)
/** The port's entry point under the name retail's symbol table gives this constructor,
 *  `config/G2ME01/symbols.txt:8313` after this carve's rename.  `Carve801FEA98.c` is a C file and
 *  can only call an unmangled name; the host compiler mangles the constructor above to
 *  `_ZN21SCarve801FEAE0ElementC1ERKS_`.  It is a real construction, not a stand-in: the same
 *  constructor, reached by the name the caller uses.  The guard is `__MWERKS__`, not `TARGET_PC`,
 *  for the reason `src/MetroidPrime/ScriptObjects/Carve801FD4B0.cpp` and
 *  `src/MetroidPrime/Cameras/Carve801E8028.c` give - the matching build must not get a second
 *  definition of one symbol. */
extern "C" void __ct__21SCarve801FEAE0ElementFRC21SCarve801FEAE0Element(void* self, const void* src) {
  ::new (self) SCarve801FEAE0Element(*static_cast< const SCarve801FEAE0Element* >(src));
}
#endif
