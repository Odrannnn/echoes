// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:8318` (`fn_801FECAC = .text:0x801FECAC; // type:function
// size:0x78`), and the 120 bytes below are retail's own, read this run out of the disc with
// `python3 tools/dol_read.py 0x801FECAC 0x78` (`orig/G2ME01/sys/main.dol`, `.text` at file offset
// 0x640 for address 0x80003840) and rendered by `./tools/dis.sh 0x801FECAC 0x78`, which reads
// `build/G2ME01/main.elf`.  Those two agree on all 30 words **while the range is unclaimed** - once
// this unit is in the link `main.elf` holds our bytes, so the disc is the source and `dis.sh` is
// only the renderer.  The body below is the C++ those bytes are the compilation of, and
// `tools/flip_test.sh` is what says so.
//
// .text 0x801FECAC..0x801FED24, 0x78 = 120 bytes, 1 function:
//
//   fn_801FECAC    0x801FECAC  0x78   30 instructions   the 0x2C-byte element's copy constructor
//
//   801fecac  94 21 ff f0  stwu   r1,-16(r1)
//   801fecb0  7c 08 02 a6  mflr   r0
//   801fecb4  3c a0 80 3b  lis    r5,lbl_803B7BCC@ha
//   801fecb8  90 01 00 14  stw    r0,0x14(r1)
//   801fecbc  38 05 7b cc  addi   r0,r5,lbl_803B7BCC@l
//   801fecc0  93 e1 00 0c  stw    r31,0xc(r1)
//   801fecc4  7c 9f 23 78  mr     r31,r4            ; the source
//   801fecc8  38 9f 00 04  addi   r4,r31,4           ; &src->mName, hoisted as the 2nd argument
//   801feccc  93 c1 00 08  stw    r30,0x8(r1)
//   801fecd0  7c 7e 1b 78  mr     r30,r3            ; the receiver
//   801fecd4  3c 60 80 3b  lis    r3,lbl_803B7BE4@ha
//   801fecd8  90 1e 00 00  stw    r0,0(r30)         ; base ctor: lbl_803B7BCC at +0x00
//   801fecdc  38 03 7b e4  addi   r0,r3,lbl_803B7BE4@l
//   801fece0  38 7e 00 04  addi   r3,r30,4           ; &this->mName
//   801fece4  90 1e 00 00  stw    r0,0(r30)         ; ... then this class's own vtable over it
//   801fece8  48 10 04 4d  bl     0x802ff134        ; rstl::string's copy constructor, +0x04
//   801fecec  80 1f 00 14  lwz    r0,0x14(r31)      ; src->x14
//   801fecf0  38 7e 00 18  addi   r3,r30,0x18        ; &this->mMember
//   801fecf4  38 9f 00 18  addi   r4,r31,0x18        ; &src->mMember
//   801fecf8  90 1e 00 14  stw    r0,0x14(r30)      ; this->x14 = src->x14
//   801fecfc  4b ff fb bd  bl     0x801fe8b8         ; the +0x18 member's own copy
//   801fed00  88 1f 00 28  lbz    r0,0x28(r31)      ; src->x28
//   801fed04  7f c3 f3 78  mr     r3,r30            ; the receiver comes back
//   801fed08  98 1e 00 28  stb    r0,0x28(r30)      ; this->x28 = src->x28
//   801fed0c  83 e1 00 0c  lwz    r31,0xc(r1)
//   801fed10  83 c1 00 08  lwz    r30,0x8(r1)
//   801fed14  80 01 00 14  lwz    r0,0x14(r1)
//   801fed18  7c 08 03 a6  mtlr   r0
//   801fed1c  38 21 00 10  addi   r1,r1,16
//   801fed20  4e 80 00 20  blr
//
// **What it is: the copy constructor of one 0x2C-byte element of a script-object block.**  The
// element is the one `Carve801FEC64.c` (Matching, 0x801FEC64..0x801FECAC) walks: that unit's
// `fn_801FEC84` is `cmplwi r3,0 / beq / bl` - `rstl::construct`'s own null guard - and the `bl`
// target is this address, so `construct<T>(dest, src)` forwards to this constructor whenever `dest`
// is non-null.
//
// **The layout is read off this function's own bytes and cross-checked against the class's default
// constructor and the `Matching` sibling units, and every offset below is one of the displacements
// in the listing above:**
//
//   +0x00  the vptr        `lbl_803B7BCC` (0x803B7BCC, `symbols.txt:18315`) first, then this
//                          class's own `lbl_803B7BE4` (0x803B7BE4, `symbols.txt:18317`) over it.
//                          Two stores to the *same* word is the shape that fixes the hierarchy
//                          below: both are base constructors.
//   +0x04  rstl::string    16 bytes under mwcceppc - `mPtr`, `mCow`, `mSize` and the word-sized
//           mName          empty `rmemory_allocator` (`rstl/string.hpp:106-110`), which is what
//                          puts the next member at +0x14 and not at +0x10.
//   +0x14  unsigned int    `lwz r0,0x14(r31)` / `stw r0,0x14(r30)` - a plain scalar copied
//                          inline, 4 bytes.
//   +0x18  the member      `addi r3,r30,24 / addi r4,r31,24 / bl fn_801FE8B8`, 0x10 bytes, whose
//                          own copy is an out-of-line call because it is not a scalar copy.
//   +0x28  unsigned char   `lbz r0,0x28(r31)` / `stb r0,0x28(r30)` - a **byte**, which is what
//                          puts the total at 0x2C and not at 0x30.
//
// for **0x2C = 44 bytes**.  The stride is measured independently twice, off retail's own walks
// rather than off this claim: `fn_801FEBF8` (0x801FEBF8, 0x6C, still unclaimed) calls `fn_801FEC64`
// once per element at 0x801FEC30-0x801FEC34 and steps its cursor with `addi r31,r31,0x2c` at
// 0x801FEC38, and the `Matching` `ScriptObjects/Carve801FF8A0.cpp`'s `fn_801FF9B8` is the same
// 0x2C-strided walk (`p += 44`, `bl fn_801FEC64` at 0x801FF9E8).  The +0x18 member's shape is the
// count/capacity/buffer one: `fn_801FE8B8` (0x801FE8B8, 0xC4) reads +4 and +8 of its receiver and
// allocates `count * 0x14` into +0xC, the same shape `SCarve801FD924Member` and
// `SCarve801FDAE8Member` carry.  The +0x14 member is spelled as an `unsigned int` and the +0x28 one
// as an `unsigned char` because those are the load and store widths retail used; nothing is asserted
// about a class retail does not name, and the members are named here only because the bytes reach
// them.
//
// **Why it is spelled as a constructor.**  Two things in the 30 instructions decide it:
//
// 1. **there is no null test.**  Both member calls are a bare `addi r3,r30,4` / `bl` and a bare
//    `addi r3,r30,24` / `bl`.  `rstl::construct` and placement `new (dest) T(src)` cannot produce
//    that: a hand-written body that returns `this` and placement-`new`s each member measures **128
//    bytes against retail's 120**, and the 8 are exactly the two `addic. r3,...` + `beq` guards
//    placement new emits.  The members are in the mem-init list, which is the only spelling with no
//    test.  This is the decisive argument.
// 2. **the two vptr stores come before the first member init.**  Base-class constructors run
//    there; a hand-written body runs after the mem-init list.  Hence the two base classes below.
//
// The receiver coming back in `r3` (`mr r3,r30`) is mwcceppc's constructor epilogue, and it is the
// **weakest** of the three signals and is *not* by itself proof - the 128-byte hand-written body
// emits it too.  The null test is what settles it.
//
// **The recipe, measured here rather than recalled.**  A compiler-generated constructor can be
// reproduced without knowing the class: a non-polymorphic struct, **one base class per vptr store**
// (their constructors run in declaration order and both land on the same word, which is what makes
// it two stores), the base **one word wide**, and the member copy-initialisations in the init list in
// declaration order.  Two shapes that do **not** work, both compiled and measured on this tree with
// the exact `mwcc_sjis` flags out of `build.ninja` (which is what `tools/probe_cc.sh` omits - it
// leaves out `-i extern/musyx/include`, the `-DMUSY_*` defines and `-pragma
// "inline_max_size(125)"`) and `tools/bytescmp.py`:
//
//   * the +0x18 member as a **plain struct of four words** - the compiler inlines a memberwise copy
//     (`lwz`/`stw` pairs with `mr r3,r30` interleaved) instead of calling anything.  That measures
//     **148 bytes and 37 instructions against retail's 120 and 30**.  It needs a user-declared copy
//     constructor, which is then inlined into this constructor as a one-instruction thunk to
//     `fn_801FE8B8`.
//   * a **real `virtual`** on the base - the object then carries `V __vt__21SCarve801FECACElement`,
//     `V __vt__12SBaseVTable2`, `U __vt__11SBaseVTable` and `U f__11SBaseVTableFv` (all four
//     measured with `powerpc-eabi-nm`) and the constructor grows to 0x9C = 156 bytes.  Retail's 120
//     bytes reference none of them.  **The two vptr stores are a spelled hierarchy, not a real one**,
//     and this is the measurement that says so.  A real polymorphic class is not an option here.
//
// With both fixed the object comes out at **120 bytes and 30 instructions**, and
// `tools/bytescmp.py` reports **6 differing instructions of 30**: the two `lis`/`addi` pairs for the
// two vtables and the two `bl` displacements, i.e. relocations only.  `powerpc-eabi-objdump -r` on
// that object lists the four `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` records for
// `lbl_803B7BCC`/`lbl_803B7BE4` and the two `R_PPC_REL24` calls - which is the point of **naming**
// the vtables rather than writing `0x803B7BCCu`: spelled as the bare constant the four instruction
// words stay identical and the function counts 0, because objdiff pairs against retail's
// *relocations*.  `Carve801FD924.cpp` states the trade at length; the check for it is
// `matched_functions` in `build/report.json`, not the sha1 and not the percentage.
//
// **A `Matching` unit needs its callees' *symbols*, not their bodies** - this is the sentence that
// unblocks the carve, and it corrects two claims in the tree that say otherwise.
// `Carve801FEC64.c` and `stub_226`'s block both record that matching these 0x78 bytes "needs the
// two `.data` vtables `lbl_803B7BCC`/`lbl_803B7BE4` and the bodies of the `rstl::basic_string` copy
// constructor and `fn_801FE8B8`, none of which any unit claims".  Three separate errors in one
// sentence:
//   * the string copy constructor **is** claimed and `Matching`: `rstl/rstl_strings.cpp`
//     (`MatchingFor("G2ME01")`), and it is `symbols.txt:13852`'s
//     `__ct__Q24rstl66basic_string<c,...>FRCQ24...` at 0x802FF134;
//   * the vtables are **dtk's `.data`**, and a `Matching` unit needs their *relocations*, not their
//     objects - `extern "C" char lbl_803B7BCC[];` plus taking its address is enough, the same fact
//     `Carve801FD924.cpp` records for `lbl_803B7BF0`;
//   * `fn_801FE8B8` is **a callee, not a dependency of the claim**.  A `bl` to a symbol dtk already
//     defines (`auto_03_801FEAE0_text.o`; `powerpc-eabi-nm` shows `T fn_801FE8B8` in it) needs no
//     body anywhere - only the *port's* flat link needs a stand-in for it, which is one stub line.
// Every "claiming X would only move the gap one function along" paragraph in this tree is about the
// **port**, and none of it is a reason the **DOL** cannot claim X.
//
// **The rename is load-bearing for the count, not for the hash.**  Retail's name for this function
// is the placeholder `fn_801FECAC`, but a constructor's symbol is mangled and these bytes are a
// constructor's, so `config/G2ME01/symbols.txt:8318` carries the name mwcceppc emits - **read out
// of the object with `powerpc-eabi-nm`, not guessed**:
//
//   __ct__21SCarve801FECACElementFRC21SCarve801FECACElement
//
// The class name is the carve's own placeholder (`SCarve801FECACElement`, after the
// `SCarve801FD924Element` naming `Carve801FD924.cpp` already uses); retail's symbol table has no
// name for this class either, so nothing is being renamed *to a retail name*.  The precedent is
// `docs/research/frame_loop.md`'s `fn_80049E98` -> `__ct__6CIOWinFRCQ24rstl66basic_string<...>`: an
// unnamed function whose bytes are a constructor's can only be reproduced by a constructor, so the
// symbol has to be given the constructor's name.  With the placeholder kept the DOL would still be
// byte-exact - the caller's reference has to be renamed anyway - but objdiff would pair nothing and
// the unit would count 0.  `Carve801FEC64.c`'s declaration and its call carry the same name, and
// that reference is what proves the definition reaches the DOL link.
//
// **Do not "tidy up" the declaration split.**  mwcceppc **drops an unreferenced inline class
// constructor from the object entirely** - the object then has no `.text` at all - so the spelling
// that is emitted is to **declare** the constructor inside the class and **define** it after the
// class body, which is what the two halves at the foot of this file do.  This is not a style
// preference; it is the only way this unit has an object at all.  (The +0x18 member's thunk is the
// opposite case and *is* defined in the class, because the one thing it must do is be inlined into
// this constructor.)
//
// **Source order is descending by address and that is load-bearing**: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order verbatim, so
// an ascending file is a permuted `.text` - 100.00% per function, a green `main.dol`, and a broken
// DOL.  Only `tools/flip_test.sh` catches that.  One function, so the rule has nothing to bite on;
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FECAC` is run anyway.
//
// Its own unit because a claim may not span an unclaimed gap.  In front of it the `Matching`
// `ScriptObjects/Carve801FEC64.c` (0x801FEC64..0x801FECAC) ends exactly where this claim starts,
// and behind it `fn_801FED24` (0x801FED24, 0xB0) starts exactly where this claim ends and is
// unclaimed - the nearest claimed range above is `ScriptObjects/Carve801FEE40.c` at 0x801FEE40 -
// so the claim stops at 0x801FED24 and those two functions stay retail's.  Before this carve the
// whole 404-byte range was dtk's `main/auto_03_801FECAC_text` (0x78 + 0xB0 + 0x6C = 404, measured in
// `build/report.json`); afterwards that unit holds the remaining 0x14C and is renamed
// `main/auto_03_801FED24_text`.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FEC64.c` (0x801FEC64..0x801FECAC) and
// `ScriptObjects/Carve801FEE40.c` (0x801FEE40..0x801FEE88).

#include "rstl/string.hpp"

/** 0x801FE8B8, `symbols.txt:8325`, 0xC4 = 196 bytes: the +0x18 member's own copy constructor.  A
 *  callee, not a dependency of this claim: dtk's own `auto_03_801FEAE0_text.o` defines it in the DOL
 *  (`powerpc-eabi-nm` shows `T fn_801FE8B8` in it), so all this file needs is the symbol.
 *  Declared, never defined. */
extern "C" void fn_801FE8B8(void* self, const void* src);

/** `lbl_803B7BCC` = 0x803B7BCC, `symbols.txt:18315`, and `lbl_803B7BE4` = 0x803B7BE4,
 *  `symbols.txt:18317`: retail's own `.data` objects, supplied to the DOL by dtk's own
 *  `auto_07_803B7AE0_data.o` and to the port link by a data stub.  **Taking their address is what
 *  puts the four `R_PPC_ADDR16_HA` / `R_PPC_ADDR16_LO` relocations into our object**, which is what
 *  objdiff pairs against retail's - see the header. */
extern "C" char lbl_803B7BCC[];
extern "C" char lbl_803B7BE4[];

/** The base class whose constructor writes `lbl_803B7BCC` at +0x00.  **One word wide is load
 *  bearing**: an empty base puts the derived's first member at +0x00 and every offset in the body
 *  comes out 4 bytes low (`addi r3,r30,20` where retail has 24, `lbz 36(` where retail has 40).
 *  Empty-base optimisation is not what retail did - the base occupies a word. */
struct SBaseVTable {
  void* mVTable;
  SBaseVTable() : mVTable(lbl_803B7BCC) {}
};

/** The second base, whose constructor writes this class's own vtable over the base's. */
struct SBaseVTable2 : SBaseVTable {
  SBaseVTable2() : SBaseVTable() { mVTable = lbl_803B7BE4; }
};

/** The element's +0x18 member, as far as this constructor and `fn_801FE8B8` are concerned: 0x10
 *  bytes, count at +4 and buffer at +0xC of the member.  The copy constructor is declared and **not**
 *  defaulted on purpose - see the header's measurement of the plain-struct spelling. */
struct SCarve801FECACMember {
  unsigned int x00;
  int x04_count;
  unsigned int x08_capacity;
  void* x0c_buffer;
  SCarve801FECACMember(const SCarve801FECACMember& src) { fn_801FE8B8(this, &src); }
};

/** The 0x2C-byte element, as far as this copy constructor reaches.  The offsets and the total are
 *  the measured layout in the header; nothing else is asserted about a class retail does not name. */
struct SCarve801FECACElement : SBaseVTable2 {
  rstl::string mName;            // +0x04, `addi r3,r30,4`
  unsigned int x14;              // +0x14, `lwz r0,20(r31)` / `stw r0,20(r30)`
  SCarve801FECACMember mMember;  // +0x18, `addi r3,r30,24` / `bl fn_801FE8B8`
  unsigned char x28;             // +0x28, `lbz r0,40(r31)` / `stb r0,40(r30)`

  /** Declared here and defined below the class body, which is the only spelling mwcceppc emits. */
  SCarve801FECACElement(const SCarve801FECACElement& src);
};

/** Retail `.text:0x801FECAC`, 0x78 = 120 bytes.  The four mem-init list entries are in declaration
 *  order, and that order is what puts `x14`'s `lwz`/`stw` between the string call and the member
 *  call, exactly as the listing above has them. */
SCarve801FECACElement::SCarve801FECACElement(const SCarve801FECACElement& src)
    : mName(src.mName),
      x14(src.x14),
      mMember(src.mMember),
      x28(src.x28) {}

#ifndef __MWERKS__
// mwcceppc has no `new` header on this include path (`the file 'new' cannot be opened`), and it
// never sees the placement new below, so the include is host-only too.
#include <new>

/** The port's flat link reaches this address through `Carve801FEC64.c`'s `fn_801FEC84`, which is C
 *  and needs retail's own name for it.  Under `__MWERKS__` the symbol is the mangled constructor
 *  name the rename above gives it, and under the host compiler a constructor would be mangled the
 *  Itanium way, so the port's definition is a plain `extern "C"` forwarder instead - the same
 *  arrangement `Carve801FD924.cpp` uses for its `fn_801FD924`.  This is what **retires
 *  `stub_226`** in `src/MetroidPrime/PortLinkStubs.cpp` rather than moving it.
 *
 *  Retail's `fn_801FEC84` holds the null guard (`cmplwi r3,0 / beq`) this function does not have, so
 *  a null destination never arrives here and the placement `new` needs no check of its own.  That is
 *  the same split `rstl::construct` has everywhere: the guard is in `construct`, the constructor has
 *  none. */
extern "C" void fn_801FECAC(void* self, const void* src) {
  SCarve801FECACElement* dest = reinterpret_cast<SCarve801FECACElement*>(self);
  const SCarve801FECACElement& from = *reinterpret_cast<const SCarve801FECACElement*>(src);
  new (dest) SCarve801FECACElement(from);
}
#endif