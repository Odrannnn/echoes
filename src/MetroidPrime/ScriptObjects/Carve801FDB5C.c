// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt` (lines 8289-8292), the instructions are the
// ones dtk emitted into `build/G2ME01/asm/auto_03_801FDC88_text.s` before the claim existed, and
// are still readable with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x801FDBE0 --stop-address=0x801FDC88
// build/G2ME01/main.elf`.  The body below is the C those bytes are the compilation of.
// `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801FDB5C.s` is this unit's own generated
// listing, not the retail one - it is our compile, so it can only confirm, never establish, what
// retail had.
//
// .text 0x801FDBE0..0x801FDC88, 0xA8 = 168 bytes, 3 functions:
//
//   fn_801FDBE0    0x801FDBE0  0x38    14 instructions
//   fn_801FDC18    0x801FDC18  0x50    20 instructions
//   fn_801FDC68    0x801FDC68  0x20     8 instructions
//
// **What the three are: `rstl::destroy`'s two halves and the element destructor they call.**
// Retail names none of them, so this is read off the call edges and the argument registers, not
// off a name - and each is byte-for-byte the shape of a symbol retail *does* name, elsewhere in
// the DOL:
//
//   fn_801FDC68  takes a pointer in r3 and does nothing but forward it to fn_801FDC88.  That is
//                `destroy<T>(T*)`, whose body is only `destroy_impl(in)`; fn_801FDC88 (0x801FDC88,
//                **not** claimed here) materialises `li r4,-1` and calls fn_801FDCAC, the -1 being
//                the "do not free me afterwards" flag of MWCC's deleting-destructor convention.
//   fn_801FDC18  walks a block in strides of 0x30 = 48, calling fn_801FDC68 on each element.  The
//                loop bound is an inequality between two pointers (`lwz r0,0(r30)` / `cmplw
//                r31,r0` / `bne`) and not an index against a count, because retail keeps the loop
//                even where the element destructor is a call.
//   fn_801FDBE0  copies its two pointer arguments into its own frame and forwards their addresses.
//
// The byte-identical named twins of all three are in `MetroidPrime/CGameHintInfo.cpp` (`.text
// 0x8017F988..0x80180E94`), one instantiation each with a different element type:
// `config/G2ME01/symbols.txt:6339-6341` has `destroy<Q24rstl144pointer_iterator<...>>` at 0x38
// bytes, `destroy_impl<Q24rstl144pointer_iterator<...>>` at 0x50 and
// `destroy<Q213CGameHintInfo9CGameHint>__4rstlFPQ213CGameHintInfo9CGameHint` at 0x20 - the same
// three sizes, in the same order.  That is what fixed the shapes here.  The 0x30 stride is that
// vector's `T`, and nothing is asserted about the class: the receiver never appears in these
// three bodies.
//
// **The `It` arguments are load-bearing, and they are why this is a struct at all.**  fn_801FDBE0
// is `destroy(It,It)` and fn_801FDC18 is `destroy_impl(It,It)` written out as C.  MWCC passes a
// struct parameter by reference and copies it into the callee's own frame: that is why
// fn_801FDBE0 dereferences r3 and r4 (`lwz r5,0(r4)` / `lwz r0,0(r3)`) and then passes the
// addresses of its own +0x8 and +0xc, and why fn_801FDC18 keeps r30 = r4 and reloads
// `lwz r0,0(r30)` on every iteration while r31, the begin cursor, is loaded once and walked with
// `addi r31,r31,0x30`.  Spelled with `void**` parameters instead, both still emit 14 and 20
// instructions but in a different order (measured: 7 and 5 differing instructions each), and
// neither reaches 100%.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order").  The claim starts at 0x801FDBE0 rather
// than at 0x801FDB5C because the 132-byte function in front of it, fn_801FDB5C, is **not**
// matched by any spelling measured so far - see `docs/goal-notes/carve-801fdb5c.md`.  Claiming
// it would make the unit un-matchable, so it stays retail's and dtk fills it.
//
// The directory is retail own, taken from the nearest claimed range: 0x801FDBE0 is 0x4B0C bytes
// into `MetroidPrime/ScriptObjects/CUnknown90.cpp` (0x801F9050..0x801F9190), so the code is that
// unit's neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.

/** 0x801FDC88, `symbols.txt:8292`, the callee fn_801FDC68 forwards to.  Also unclaimed, so also
 *  supplied by dtk's own `auto_*` object. */
extern void fn_801FDC88(void* ptr);

/** The one pointer of `rstl::pointer_iterator`: `current`, protected in retail's C++ and named
 *  there too.  Retail names the iterator's class only inside the mangled twin symbols above; what
 *  the bytes need of it is exactly this, that it is a struct of one pointer passed by value. */
struct SCarve801FDB5CIterator {
  void* current;
};

void fn_801FDC68(void* ptr);
void fn_801FDC18(struct SCarve801FDB5CIterator begin, struct SCarve801FDB5CIterator end);
void fn_801FDBE0(struct SCarve801FDB5CIterator begin, struct SCarve801FDB5CIterator end);

void fn_801FDC68(void* ptr) { fn_801FDC88(ptr); }

void fn_801FDC18(struct SCarve801FDB5CIterator begin, struct SCarve801FDB5CIterator end) {
  char* cur = (char*)begin.current;
  while (cur != (char*)end.current) {
    fn_801FDC68(cur);
    cur += 0x30;
  }
}

void fn_801FDBE0(struct SCarve801FDB5CIterator begin, struct SCarve801FDB5CIterator end) {
  fn_801FDC18(begin, end);
}
