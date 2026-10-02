// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:8267` (`fn_801FD4B0 = .text:0x801FD4B0; // size:0x7C`),
// and the 124 bytes below are retail's own, read this run out of the disc with
// `python3 tools/dol_read.py 0x801FD4B0 0x7C` (`orig/G2ME01/sys/main.dol`, text section 1: file
// offset 0x640 at address 0x80003840, so this address is file offset 0x1FA2B0) and **not** out of
// `build/G2ME01/main.elf`, which holds our own bytes once this unit is in the link.  The body
// below is the C++ those bytes are the compilation of, and `tools/flip_test.sh` is what says so.
//
// .text 0x801FD4B0..0x801FD52C, 0x7C = 124 bytes, 1 function:
//
//   fn_801FD4B0    0x801FD4B0  0x7C   31 instructions   the four-container element's deleting
//                                                          destructor
//
//   801fd4b0  94 21 ff f0   stwu   r1,-16(r1)
//   801fd4b4  7c 08 02 a6   mflr   r0
//   801fd4b8  90 01 00 14   stw    r0,0x14(r1)
//   801fd4bc  93 e1 00 0c   stw    r31,0xc(r1)
//   801fd4c0  7c 9f 23 78   mr     r31,r4            ; the deleting flag, kept whole in r31
//   801fd4c4  93 c1 00 08   stw    r30,0x8(r1)
//   801fd4c8  7c 7e 1b 79   mr.    r30,r3            ; receiver guard: MWCC's null `this` test
//   801fd4cc  41 82 00 44   beq    .+0x44
//   801fd4d0  38 7e 00 30   addi   r3,r30,0x30       ; &x30
//   801fd4d4  38 80 ff ff   li     r4,-1            ; "do not free me afterwards"
//   801fd4d8  48 00 06 85   bl     0x801fdb5c        ; fn_801FDB5C(&x30, -1)
//   801fd4dc  38 7e 00 20   addi   r3,r30,0x20       ; &x20
//   801fd4e0  38 80 ff ff   li     r4,-1
//   801fd4e4  48 00 04 b5   bl     0x801fd998        ; fn_801FD998(&x20, -1)
//   801fd4e8  38 7e 00 10   addi   r3,r30,0x10       ; &x10
//   801fd4ec  38 80 ff ff   li     r4,-1
//   801fd4f0  48 00 02 e5   bl     0x801fd7d4        ; fn_801FD7D4(&x10, -1)
//   801fd4f4  7f c3 f3 78   mr     r3,r30            ; &x00, which is the receiver itself
//   801fd4f8  38 80 ff ff   li     r4,-1
//   801fd4fc  48 00 00 31   bl     0x801fd52c        ; fn_801FD52C(&x00, -1)
//   801fd500  7f e0 07 35   extsh. r0,r31            ; the flag, sign-extended to 16 bits
//   801fd504  40 81 00 0c   ble    .+0x0c            ; ... so `flag > 0` is the free test
//   801fd508  7f c3 f3 78   mr     r3,r30
//   801fd50c  48 0d 0e 7d   bl     0x802ce388        ; CMemory::Free(self)
//   801fd510  80 01 00 14   lwz    r0,0x14(r1)
//   801fd514  7f c3 f3 78   mr     r3,r30            ; the receiver is returned
//   801fd518  83 e1 00 0c   lwz    r31,0xc(r1)
//   801fd51c  83 c1 00 08   lwz    r30,0x8(r1)
//   801fd520  7c 08 03 a6   mtlr   r0
//   801fd524  38 21 00 10   addi   r1,r1,16
//   801fd528  4e 80 00 20   blr
//
// **What it is: the deleting destructor of a script-object element that owns four 0x10-byte
// containers, and nothing else.**  The shape is the port's stock one - `mr. r30,r3 / beq` guards
// the receiver, the members go in reverse declaration order, each behind its own `li r4,-1`,
// `CMemory::Free(self)` is reached only behind `extsh. r0,r31 / ble` (i.e. `flag > 0`), and the
// receiver comes back in `r3` - and `src/MetroidPrime/CIOWinDtor.cpp` documents and reproduces the
// same shape.  What makes this one its own function is the *count*: three of the four teardowns
// carry a `addi r3,r30,+0x10/+0x20/+0x30` and the fourth carries `mr r3,r30`.
//
// **The `-1` is MWCC's "destroy, but do not free me afterwards" flag, and every caller in retail
// writes it.**  The four callees all take `(this, int)` and all end in the same
// `extsh. r0,r31 / ble / mr r3,r30 / bl CMemory::Free` tail this function has, so `-1` is what
// keeps them from freeing themselves; the one caller of this function,
// `fn_801FD420` (0x801FD420, 0x90, unclaimed), passes `li r4,1` instead at 0x801FD458 and takes
// the `Free`.
//
// **All four containers are 0x10 bytes, and that is measured off the four callees rather than
// assumed.**  They are the same function three times over with one `mulli` immediate changed, and
// each reads the same three words of its own receiver - the count at +4 (`lwz r0,4(r30)`), the
// capacity at +8 and the buffer at +0xC (`lwz r5,12(r30)` / `lwz r0,12(r30)`) - which is exactly
// the `SCarve801FDAE8Member` layout `src/MetroidPrime/ScriptObjects/Carve801FDAE8.cpp` already
// spells for the same callee shape:
//
//   callee         address      size   element stride (`mulli`)   inner walk
//   fn_801FDB5C    0x801FDB5C   0x84   48 = 0x30                  fn_801FDBE0, 0x801FDBE0
//   fn_801FD998    0x801FD998   0x84   44 = 0x2C                  fn_801FDA1C, 0x801FDA1C
//   fn_801FD7D4    0x801FD7D4   0x84   36 = 0x24                  fn_801FD858, 0x801FD858
//   fn_801FD52C    0x801FD52C   0x84   36 = 0x24                  fn_801FD5B0, 0x801FD5B0
//
// (all four addresses and sizes `symbols.txt:8288`, `:8282`, `:8276`, `:8268`; the strides are the
// `1c 00 00 nn` words at 0x801FDB8C, 0x801FD9C8, 0x801FD804 and 0x801FD55C and the inner walks are
// the `48 00 00 39` `bl` words at 0x801FDBA8, 0x801FD9E4, 0x801FD820 and 0x801FD578).  Four
// containers 0x10 apart, and the one at +0x00 is the receiver's own first word - `&self->x00` *is*
// `self`, which is what the `mr r3,r30` at 0x801FD4F4 is.
//
// **The strides name the element sizes of four other classes, and nothing else is asserted about
// any class here.**  0x30 is the element stride `src/MetroidPrime/ScriptObjects/Carve801FDB5C.c`
// (Matching) measures off retail's own two walks; 0x2C is the one
// `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c` measures; 0x24 is the one
// `src/MetroidPrime/ScriptObjects/Carve801FD924.cpp` (Matching) measures.  This destructor reaches
// no member's words itself, so the element's own stride is **not** measured by anything in this
// file and nothing claims it: all that is asserted is four 0x10-byte containers at +0x00, +0x10,
// +0x20 and +0x30, which is what these 31 instructions address.
//
// **The deleting flag is a `short`, which is what `extsh. r0,r31` measures, and every teardown is
// a plain call on an address rather than a named destructor.**  Retail has **no** null test before
// any of the four `bl`s, and MWCC emits that test only for a named destructor call - which is the
// opposite of `fn_801FD924`'s `rstl::string` teardown, spelled
// `self->mName.~basic_string()` precisely because retail *does* test there
// (`addic. r0,r30,4 / beq` at 0x801FD95C).  Both halves of that rule are measured elsewhere in this
// tree - `src/MetroidPrime/ScriptLoader/Carve802201F8.cpp:23-36` and
// `src/MetroidPrime/CAnimData.cpp:519` for the named call,
// `src/MetroidPrime/Carve800E10EC.cpp` for the plain one.  Note the name after the `~` has to be
// the **unqualified injected class name**; there is no `~` here, so the unit needs no `rstl`
// header at all, which is the only reason it is not shaped like its 0x74-byte siblings.
//
// **Nothing is stored back into the element, and that is a difference from the siblings worth
// naming.**  `fn_801FD924` and `fn_801FDAE8` each restore their own vtable first
// (`lis r4,lbl_803B7BF0@ha` / `addi r0,r4,...@l` / `stw r0,0(r30)`), so both name a `.data`
// vtable and both need a `stub_data_*` in `src/MetroidPrime/PortLinkStubs.cpp` for the port link.
// This one has no store into `self` anywhere in its 31 instructions, so it names no vtable and the
// carve costs **no data stub** - the four stand-ins below are all it needs.
//
// Retail names none of these, so the definition has to be `extern "C"`: a C++ one would mangle to
// `_Z<len>fn_801FD4B0<len>...` and objdiff would pair nothing.  That is also why the unit is a
// `.cpp` and not a `.c`: the three `Matching` siblings that fix this shape
// (`Carve801FD67C.cpp`, `Carve801FD924.cpp`, `Carve801FDAE8.cpp`) are all `.cpp` compiled by
// mwcceppc, and reproducing their codegen is the point.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  One function, so the rule has nothing to bite on here;
// `python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FD4B0` is run
// anyway, and prints `ok: 1 unit(s) checked, none emits its functions out of retail order`.
//
// Its own unit because a claim may not span an unclaimed gap.  In front of it `fn_801FD420`
// (0x801FD420, 0x90) ends exactly where this claim starts (0x801FD420 + 0x90 = 0x801FD4B0) and is
// unclaimed; behind it `fn_801FD52C` (0x801FD52C, 0x84) starts exactly where this claim ends and
// is unclaimed - the nearest claimed range above it, `ScriptObjects/Carve801FD5E8.c`, starts at
// 0x801FD5E8, past `fn_801FD5B0` (0x801FD5B0, 0x38) as well - so the claim stops at 0x801FD52C and
// both of those stay retail's.  Extending it up to 0x801FD5E8 to take `fn_801FD52C` and
// `fn_801FD5B0` with it would have been possible and was not done: those are two more bodies
// (0x84 and 0x38) whose spelling is its own job, and one unclaimed callee fewer is not worth
// burying them in this unit.  dtk's own `build/G2ME01/asm/auto_03_801FBD68_text.s` holds
// `# 0x801FBD68..0x801FD5E8 | size: 0x1880`, and its per-function headers are
// `# .text:0x16B8 | 0x801FD420 | size: 0x90`, `# .text:0x1748 | 0x801FD4B0 | size: 0x7C`,
// `# .text:0x17C4 | 0x801FD52C | size: 0x84` and `# .text:0x1848 | 0x801FD5B0 | size: 0x38`
// (`build/G2ME01/asm/auto_03_801FBD68_text.s:1707,1750,1786,1824`), so 0x801FD4B0..0x801FD52C is
// this function and nothing else.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FBC58.c` (0x801FBC58..0x801FBD68) and
// `ScriptObjects/Carve801FD5E8.c` (0x801FD5E8..0x801FD638), and the three 0x74-byte siblings this
// shape comes from are in the same directory.

/** 0x802CE388, `symbols.txt:12992`: `CMemory::Free(void const*)`, claimed by
 *  `Kyoto/Alloc/CMemory.cpp` in both builds.  Declared under retail's own emitted spelling so the
 *  call needs no header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** The four unclaimed callees.  All four are `symbols.txt` placeholders of 0x84 = 132 bytes with
 *  the shape the header measures - count at +4, capacity at +8, buffer at +0xC, a `mulli` element
 *  walk handed to an inner `fn_` , `CMemory::Free(x0c_buffer)`, then the same
 *  `extsh. r0,r31 / ble` free of their own receiver - and all four are called here with the `-1`
 *  flag, so none of them frees itself.  None is part of this claim and none is claimed, so the
 *  matching build takes each from dtk's own `auto_*` object and the port link gets the stand-ins
 *  `stub_228`..`stub_231` in `src/MetroidPrime/PortLinkStubs.cpp`.  Declared, never defined.
 *
 *  The four `int` flag parameters are not pinned by these bytes - the caller writes a literal `-1`
 *  and the callees' own bodies sign-extend it - and `Carve801FD8E0.c` already declares
 *  `fn_801FD924(void*, int)` against `Carve801FD924.cpp`'s `(SCarve801FD924Element*, short)`, so
 *  the two spellings are the same two declarations of one `extern "C"` symbol in two translation
 *  units. */
extern "C" void fn_801FDB5C(void* self, int flag);
extern "C" void fn_801FD998(void* self, int flag);
extern "C" void fn_801FD7D4(void* self, int flag);
extern "C" void fn_801FD52C(void* self, int flag);

/** One of the four 0x10-byte containers this destructor tears down.  `x00` is never written or
 *  read by anything this file names; it is here because it is what puts the count at +4 of the
 *  container and so, for the one at +0x00, the count at +4 of the element. */
struct SCarve801FD4B0Member {
  unsigned int x00;
  int x04_count;
  unsigned int x08_capacity;
  void* x0c_buffer;
};

/** The element, as far as this destructor reaches into it: four containers 0x10 apart.  The
 *  offsets are the `addi r3,r30,+0x10/+0x20/+0x30` and the `mr r3,r30` above, and the container
 *  size is measured off the four callees (see the header).  Nothing else about the class is
 *  asserted - retail does not name it, and no store into it appears in these 31 instructions. */
struct SCarve801FD4B0Element {
  SCarve801FD4B0Member x00;  // +0x00, `mr r3,r30` above: &x00 is the receiver itself
  SCarve801FD4B0Member x10;  // +0x10, `addi r3,r30,0x10` above
  SCarve801FD4B0Member x20;  // +0x20, `addi r3,r30,0x20` above
  SCarve801FD4B0Member x30;  // +0x30, `addi r3,r30,0x30` above
};

extern "C" void* fn_801FD4B0(SCarve801FD4B0Element* self, short flag) {
  if (self) {
    fn_801FDB5C(&self->x30, -1);
    fn_801FD998(&self->x20, -1);
    fn_801FD7D4(&self->x10, -1);
    fn_801FD52C(&self->x00, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}