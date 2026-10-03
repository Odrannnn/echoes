// Carved out of an unclaimed dtk `auto_*` range by lane `carve-802fdaf4`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_*.s`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x802FDAF4..0x802FDB94, 0xA0 = 160 bytes, 2 functions:
//
//   fn_802FDB34    0x802FDB34  0x60   bl allocate__Q24rstl17rmemory_allocatorFi
//   fn_802FDAF4    0x802FDAF4  0x40   `bl Free__7CMemoryFPCv` behind `--mRefCount == 0`
//
// Both are `rstl::basic_string<char>`'s private `internal_allocate(int)` and
// `internal_dereference()`.  They are **not** claimed by `rstl/rstl_strings.cpp` - that unit
// claims 0x802FDD90..0x802FF3AC, the `wchar_t` and `char` instantiations that retail named - so
// the `char` copies in this gap are anonymous, and `symbols.txt` carries only the
// `fn_<addr>` placeholder (`symbols.txt:13825-13826`).
//
// The bodies are read off the two *matched* twins, and the twin is what makes them writable in
// C: `src/rstl/rstl_strings.cpp:150` and `:141` compile to
// `build/G2ME01/asm/rstl/rstl_strings.s:133` and `:155`, and those two are byte-for-byte the
// same instruction sequences as this range's, with one difference each:
//
//   fn_802FDB34  twin `internal_allocate<wchar_t>` has `slwi r3,r4,1 / addi r3,r3,0x8`
//                because `sizeof(wchar_t) == 2`; this copy's `addi r3,r31,0x8` is the same
//                line with the multiply already folded away, i.e. the `char` instantiation.
//   fn_802FDAF4  twin `internal_dereference<wchar_t>` is identical instruction for
//                instruction; only the `bl` target differs in the object, not in the text.
//
// So the source below is the twins' source with `char` substituted for `wchar_t`, the two
// member fields named after what the bytes read, and the two callees named by their mangled
// symbols.  Both callee names are pure identifier characters (`<` and `>` only appear in
// *template* names), which is what makes them declarable from a `.c` file at all; see
// `docs/goal-notes/carve-802fdaf4.md`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// **The claim stops at 0x802FDB94 and `fn_802FDB94` is left to retail.**  It is the
// case-insensitive character scan (`fn_8016BED0`'s twin: two ranges on the sign-extended byte,
// folded with `subi 0x20` / `subi 0x60`) at 0x802FDB94, size 0xB8, and it reads
// `lbl_80419AEC@sda21` - a `.sdata2` datum, not something a `.c` file can name here.  Its
// caller `fn_802FDC4C` (0x802FDC4C, 0x144) follows.  dtk's own `auto_*` object supplies those
// bytes in the DOL link; `fn_802FDB94`'s call to `fn_802FDB34` resolves to the definition
// here.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the
// functions on either side of this run are not trivial: the run below ends at
// `rstl/rstl_misc.cpp`'s `rstl::rmemory_allocator::allocate(int)` (0x802FDAB8, 0x3C) and the
// run above is the scan.
//
// The directory is retail own, taken from the nearest claimed range: this address is
// 0x0 bytes into `rstl/rstl_misc.cpp`, so the code is that unit
// neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.

/** 0x802CE388, `symbols.txt:12992`: retail's `CMemory::Free(void const*)`, size 0x64.  **Not
 *  claimed by any unit**, so dtk's own `auto_*` object supplies the bytes in the DOL link and
 *  the one `bl` below resolves to retail's address.  Declared, never defined here.
 *  `src/Kyoto/Alloc/PortMwccNew.cpp` defines it under that name for the host link. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x802FDAB8, `symbols.txt:13824`: retail's `rstl::rmemory_allocator::allocate(int)`, size
 *  0x3C, a **static** member (`include/rstl/rmemory_allocator.hpp:16` declares it
 *  `static void* allocate(int size);`), so it takes the byte count in `r3` and returns the
 *  block in `r3` - which is what `addi r3,r31,0x8 / bl` is.  **Claimed** by
 *  `rstl/rstl_misc.cpp`, so the `bl` below binds to that unit's own definition. */
extern void* allocate__Q24rstl17rmemory_allocatorFi(int size);

/** `rstl::basic_string<char>::control`: the two words in front of the character data.
 *  `fn_802FDB94`'s caller reads `sizeof(control) == 8` by seeking `mPtr` at `mCow + 8`, and the
 *  twins name `+0` `mCapacity` and `+4` `mRefCount`. */
struct SCarve802FDAF4Control {
  int mCapacity;
  int mRefCount;
};

/** `rstl::basic_string<char>`'s own three words, named after the members of
 *  `include/rstl/string.hpp:120-122`.  Only `+0` and `+4` are read by this range - the `size`
 *  member at `+8` is `unsigned int` and `+0xC` is the inline `rmemory_allocator`, neither of
 *  which these two functions touch - so the tail is left out rather than guessed at. */
struct SCarve802FDAF4 {
  const char* mPtr;
  struct SCarve802FDAF4Control* mCow;
};

void fn_802FDB34(struct SCarve802FDAF4* self, int size);
void fn_802FDAF4(struct SCarve802FDAF4* self);

void fn_802FDB34(struct SCarve802FDAF4* self, int size) {
  self->mCow = (struct SCarve802FDAF4Control*)allocate__Q24rstl17rmemory_allocatorFi(
      (int)sizeof(struct SCarve802FDAF4Control) + size);
  self->mPtr = (const char*)(self->mCow + 1);
  self->mCow->mCapacity = size;
  self->mCow->mRefCount = 1;
}

void fn_802FDAF4(struct SCarve802FDAF4* self) {
  if (self->mCow && --self->mCow->mRefCount == 0) {
    Free__7CMemoryFPCv(self->mCow);
  }
}