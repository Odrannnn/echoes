// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:11256-11257`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_80280340_text.s:1163-1206`, and the body below
// is the C those bytes are the compilation of.
//
// .text 0x80281310..0x8028139C, 0x8C = 140 bytes, 2 functions:
//
//   fn_80281310    0x80281310  0x6C    27 instructions
//   fn_8028137C    0x8028137C  0x20     8 instructions
//
// **What the two are: `rstl::uninitialized_fill_n` and the element constructor it calls.**
// Retail names neither of them, so this is read off the call edge and the argument registers,
// and both are byte-identical to a symbol retail *does* name, elsewhere in the DOL:
//
//   fn_8028137C  takes two pointers and forwards both unchanged - `r3` and `r4` are never
//                written between the prologue and the `bl`.  That is the shape of
//                `__sys_free` (`src/MetroidPrime/main.cpp:396`, retail 0x8008DEB4) reduced to
//                its forwarding half, and it is why nothing is loaded into the frame: the
//                callee reads through the pointers it is handed.
//   fn_80281310  is the loop that drives it.  `r30` is a counter from `li r30,0`, compared
//                `cmpw r30,r28` against the second argument, and `r31` walks the first
//                argument in strides of `addi r31,r31,0x10`.  That is
//                `include/rstl/construct.hpp:153` verbatim, and the byte-identical named twin
//                says so: `build/G2ME01/obj/MetroidPrime/CGameArea.o:0x9928` is
//                `uninitialized_fill_n<rstl::vector<CToken>*, rstl::vector<CToken>>`, 27
//                instructions, same frame, same `mr r3,r31 / mr r4,r29 / bl` call and the
//                same 0x10 stride.  Its `construct(&*cur, value)` is this carve's
//                `fn_8028137C`; its `D dest` and `const S& value` are this carve's `void*`
//                and `const void*`, and the third argument arrives in `r5` as the *address* of
//                the value, which is what makes the callee's second parameter a pointer.
//
// **The stride is what fixes the element's size, and nothing else here names it**: 0x10 = 16
// bytes, so `S` is 16 bytes wide.  It is not named in retail and it is not named here.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// **The claim stops at 0x8028139C and the two functions above it are left to retail, and the
// reason is measured rather than stylistic.**  The seed planned four functions here; the other
// two each have exactly one `bl`, and both targets are MWCC-mangled C++ template symbols that no
// C declaration can name:
//
//   fn_8028139C  0x8028139C  0x28   `cmplwi r3,0 / beq` then
//                `bl __ct__Q24rstl37vector<Pv,Q24rstl17rmemory_allocator>FRCQ24rstl37vector<Pv,Q24rstl17rmemory_allocator>`
//                (retail 0x8005C2D4, `symbols.txt:1811`)
//   fn_802813C4  0x802813C4  0x94   byte-identical to the named instantiation
//                `__ct__Q24rstl61vector<Q24rstl17auto_ptr<6IWorld>,Q24rstl17rmemory_allocator>FiRC...RC...`
//                (`build/G2ME01/obj/MetroidPrime/CAutoMapper.o:0x9e5c`), whose one call is
//                `bl allocate__Q24rstl17rmemory_allocatorFi` (retail 0x802FDAB8,
//                `symbols.txt:13824`)
//
// A C identifier cannot contain `<` or `>`, and there is no alias syntax to work around that:
// `extern void f(void*, const void*) asm("<mangled>");` is rejected by this compiler in **both**
// C and C++ mode with "type cannot be made into a global register variable" (measured with
// mwcceppc.exe GC/2.7, the version `configure.py` selects), and an explicit specialization
// declared without a definition cannot be called at all ("function call 'g(void **, const void
// *)' does not match").  Letting mwcceppc *define* the instantiation instead would put a
// 0x100-byte copy of retail's 0x8005C2D4 in this object next to the claim, which
// `tools/unit_fit.sh` reports as an extra function and which no claim can absorb.  So the two
// stay retail's, dtk's `auto_*` object supplies their bytes, and the claim is the prefix that
// can be written in C.  `docs/goal-notes/carve-80281310.md` records the measurements.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order"), and because neither neighbour belongs
// to this pair: `fn_802812A4` ends the run beneath and `fn_80281458` (0x38 = 56 bytes, six
// calls to the same function) starts the run above and is not part of the fill.
//
// The directory is retail's own, taken from the nearest claimed range: the claim immediately
// below is `MetroidPrime/Carve80280338.c` (`.text` 0x80280338..0x80280340) and the one above
// is `Collision/CCollidableAABox.cpp` (`.text` 0x80281490..0x80281EF0), so this address sits in
// that unit's neighbourhood.  For an anonymous function that is the only evidence there is, and
// it beats a lane picking the directory it happened to own.  The claim starts at 0x80281310
// rather than at the `auto_*` unit's own start of 0x80280340, which is what keeps `dtk dol
// split` from reporting a link-order cycle against `Carve80280338.c`.

/** 0x8028139C, `symbols.txt:11258`, size 0x28: the null-guard and copy-construct
 *  `fn_8028137C` forwards to.  Unnamed in retail and not claimed here, so dtk's own `auto_*`
 *  object supplies the bytes in the DOL link and the one `bl` resolves to retail's address.
 *  Declared, never defined here. */
extern void fn_8028139C(void* self, const void* src);

void fn_8028137C(void* self, const void* src);
void fn_80281310(void* dest, int count, const void* value);

void fn_8028137C(void* self, const void* src) { fn_8028139C(self, src); }

void fn_80281310(void* dest, int count, const void* value) {
  char* cur = (char*)dest;
  int i;
  for (i = 0; i < count; ++i, cur += 16) {
    fn_8028137C(cur, value);
  }
}
