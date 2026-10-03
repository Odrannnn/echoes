// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:10508-10509`, the instructions are retail's own,
// read this run with `build/binutils/powerpc-eabi-objdump -d --start-address=0x80256250
// --stop-address=0x80256278 build/G2ME01/main.elf`, and the body below is the C those bytes are
// the compilation of.  Before the claim existed dtk emitted them into
// `build/G2ME01/asm/auto_03_80255D2C_text.s`, whose `.fn fn_80256250` block is lines 391-404 of
// that file.  `build/G2ME01/asm/WorldFormat/Carve80256250.s` is this unit's own generated
// listing, not the retail one - it is our compile, so it can only confirm, never establish, what
// retail had.
//
// .text 0x80256250..0x80256278, 0x28 = 40 bytes, 1 function:
//
//   fn_80256250    0x80256250  0x28   10 instructions
//
//   80256250  stwu   r1,-0x10(r1)
//   80256254  mflr   r0
//   80256258  cmplwi r3,0x0
//   8025625c  stw    r0,0x14(r1)
//   80256260  beq    0x80256268
//   80256264  bl     fn_80256278
//   80256268  lwz    r0,0x14(r1)
//   8025626c  mtlr   r0
//   80256270  addi   r1,r1,0x10
//   80256274  blr
//
// **It is a byte-shape twin of `fn_80004D5C`** (0x80004D5C, `size:0x28`,
// `config/G2ME01/symbols.txt:98`, matched at 100.00% in
// `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp:12-16`): ten instruction words, and the
// **only** difference is the `bl` at offset 0x14 - `4b ff fd 31` to `fn_80004AA0` there,
// `48 00 00 15` to `fn_80256278` here.  The twin's source is three lines,
// `if (self != 0) { fn_80004AA0(self, src); }`, and nothing else in this file is invented: the
// comparison is on `r3` before the link register is spilled, `r4` is forwarded to the callee
// untouched, and the `beq` target is the epilogue, so a null destination copies nothing and still
// returns.
//
// **Two more twins are byte-exact, `bl` word included**, which pins the encoding and not just the
// shape: `fn_801FEE60` (0x801FEE60, 0x28, `src/MetroidPrime/ScriptObjects/Carve801FEE40.c`,
// `Matching`, 100.00%) and `fn_801FEC84` (0x801FEC84, 0x28,
// `src/MetroidPrime/ScriptObjects/Carve801FEC64.c`, `Matching`, 100.00%) are both these ten words
// with the *same* `48 00 00 15`, because in all three the callee sits 0x14 bytes past its own `bl`
// (0x80256264 + 0x14 = 0x80256278, 0x801FEE74 + 0x14 = 0x801FEE88, 0x801FEC98 + 0x14 =
// 0x801FECAC).  All three are `rstl::construct`'s per-element null guard for a **copy**: the
// in-class `construct` at `include/rstl/construct.hpp:74-77` is the frame-and-forwarder above it,
// and the guard itself is the placement-new test in the `construct_impl` on the next lines.
//
// **What the callee is, read off its bytes, since it is outside this claim.**  `fn_80256278`
// (`symbols.txt:10509`, 0x74 = 116 bytes, 29 instructions, unclaimed) is a **copy constructor for
// a 0x2C-byte element**: `bl fn_802562EC` on entry (0x802562EC, 0x84 bytes - the base-class half),
// then one `lha r0,0x10(r31)` / `sth r0,0x10(r30)` short and six `lfs`/`stfs` float pairs at
// +0x14 through +0x28 out of `r31` into `r30`, and `mr r3,r30` before the epilogue.  **The element
// size is 0x2C, measured from retail's own caller**: `fn_802561F8` (0x802561F8, 0x38, unclaimed,
// the function directly in front of this claim) does `mulli r0,r5,0x2c` at 0x8025620C, `addi
// r5,r5,0x1` and `stw r5,0x4(r3)` at 0x80256214 (the count), then `add r3,r6,r0` at 0x80256218
// (the item array at +0x0C plus index times stride) and one `bl fn_80256230` - so it is a
// push-back that constructs in place, and the stride is what fixes `sizeof(T)` at 44.
//
// `fn_80256230` (0x80256230, 0x20, unclaimed, one `bl fn_80256250` and a frame) is retail's
// `rstl::construct` for that element, so **this claim is the guard beneath it and no more**: the
// claim starts exactly where `fn_80256230` ends (0x80256230 + 0x20 = 0x80256250) and stops where
// `fn_80256278` begins.  Nothing is asserted about the class itself - no byte of this claim reads
// a member - so the pointers are passed as `void*`, and both callees are declared, never defined.
//
// **The port's flat link needs a stand-in for `fn_80256278`, and it has one:**
// `stub_carve80256250_0` in `src/MetroidPrime/PortLinkStubs.cpp`, an announced empty body.  It
// cannot reach main.dol (that file is not in `configure.py`, and dtk's `auto_03_80255D2C_text.o`
// defines the symbol in the DOL link); the port has no `auto_*` objects, so without it the carve's
// `bl` would open a new entry in `docs/research/port_link_gap_list.md` and fail
// `tools/link_gap.py`.  There is no `PortLinkStubs.cpp` duplicate of `fn_80256250` itself to
// delete: `grep -rn 80256250 src/ include/` matches only this file.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z11fn_80256250Pv` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// `void*`, which is compile-time only and leaves the object byte-identical.
//
// Its own unit because a claim may not span an unclaimed gap.  The directory is retail's own,
// taken from the nearest claimed ranges: below is `WorldFormat/Carve80255C54.c`
// (`.text` 0x80255C54..0x80255D2C) and above is `WorldFormat/Carve802563C8.c`
// (0x802563C8..0x802563E8), so the code is the `WorldFormat/` neighbourhood.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  With one function the order cannot be wrong, and the rule is
// recorded because the next carve added to this file would break it.
// `python3 tools/check_decl_order.py --unit main/WorldFormat/Carve80256250` is the cheap check.

/** `fn_80256278` - retail `.text:0x80256278`, 0x74 = 116 bytes: the copy constructor of the
 *  0x2C-byte element this file constructs, unclaimed and still inside dtk's
 *  `auto_03_80255D2C_text.o` (`asm/auto_03_80255D2C_text.s:406-437`).  Declared, never defined
 *  here: a `Matching` unit needs its callees' symbols, not their bodies.  Its own name is the
 *  placeholder retail's `bl` already encodes, which is why this declaration can be a plain C one
 *  in a C file. */
extern void fn_80256278(void* self, const void* src);

/** `fn_80256250` - retail `.text:0x80256250`, 0x28 = 40 bytes: `rstl::construct`'s null guard for
 *  that element - the copy half, reached from retail's own push-back `fn_802561F8` through
 *  `fn_80256230`.  A null destination copies nothing and still returns; the destination is passed
 *  on in `r3` and the source in `r4` untouched.  Byte-exact twin of `fn_80004D5C`
 *  (`Player/CGameStateBlockConstruct.cpp`) apart from the `bl` target - see the header. */
void fn_80256250(void* self, const void* src);

void fn_80256250(void* self, const void* src) {
  if (self != 0) {
    fn_80256278(self, src);
  }
}
