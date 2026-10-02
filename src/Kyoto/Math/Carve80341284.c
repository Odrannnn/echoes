// Carved out of an unclaimed dtk `auto_*` range (`auto_03_8033F2D0_text`, 0x8033F2D0..0x803414FC).
// Every number here is measured: the addresses and sizes come from
// `config/G2ME01/symbols.txt:15360-15363`, the instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_8033F2D0_text.s` (dtk's own `.text:0x1FB4` offset from the auto
// range's start), and the bodies below are the C those bytes are the compilation of.
//
// .text 0x80341284..0x80341310, 0x8C = 140 bytes, 3 functions:
//
//   fn_80341284    0x80341284  0x20    8 instructions
//   fn_803412A4    0x803412A4  0x28   10 instructions
//   fn_803412CC    0x803412CC  0x44   17 instructions
//
// **It is one `rstl::construct` chain of a `reserved_vector`-shaped object**, each of the three a
// byte-shape twin of a function already matched in this tree - the same instructions apart from
// call targets - so the spellings below are the twins' own:
//
//   fn_80341284  twin of `__sys_free` (`src/MetroidPrime/main.cpp:396`, 100.00%): a frame and one
//                unconditional `bl` with both arguments forwarded untouched.  Here that is the
//                placement-construct of one element, forwarding to `fn_803412A4`; the same shape
//                `fn_80248DBC` (`rstl::construct<COctreeLeafCache>`) has in
//                `src/WorldFormat/CMetroidAreaCollider.cpp`.
//   fn_803412A4  twin of `fn_80004D5C`
//                (`src/MetroidPrime/Player/CGameStateBlockConstruct.cpp:12`, 100.00%): the null
//                test on the destination - `cmplwi r3,0` scheduled in front of the LR save - and
//                then the copy.  That twin is retail's `rstl::construct` for `SGameStateBlock`;
//                this copy is the same construct_impl shape with `fn_803412CC` as its callee.
//   fn_803412CC  twin of `fn_80248E60` (`src/WorldFormat/CMetroidAreaCollider.cpp`, 100.00%):
//                store the source's count, hand the two inline-data addresses to the element copy
//                and return the destination.  The reload of the count after the store
//                (`lwz r4,0(r31)` at 0x803412F0) is what makes the bound `self->mCount` and not
//                `other->mCount`, exactly as in the twin.
//
// **Its caller pins the object's layout**, and it is the function immediately below this claim:
// `fn_8034123C` (0x8034123C, size 0x48) is a `reserved_vector::push_back` - it loads the count at
// +0, scales it by 0x804 (`mulli r0,r0,0x804` at 0x80341254), adds 4, calls `fn_80341284` on that
// slot with its own second argument still in r4 (0x80341260), then increments the count.  So the
// object is a count at +0 followed by inline elements at +4 of 0x804 bytes each.
//
// Retail names none of these three.  `symbols.txt` carries the `fn_<addr>` placeholders and this
// file reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle
// to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section: below it, 0x8033F2D0..0x80341284 (`fn_8033F2D0`,
// `fn_8034123C`, ..., the whole 0x1FB4-byte head of the auto range) is unclaimed, and above it
// 0x80341310..0x803414FC (`fn_80341310`, ...) is too.  The directory is retail's own, taken from
// the nearest claimed range below: `Kyoto/Math/Carve8033F2CC.c` (0x8033F2CC..0x8033F2D0), 0x1FB4
// bytes below this claim; the range above is `Kyoto/Math/Carve803414FC.c`
// (0x803414FC..0x80341500).  For anonymous functions that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.

/** The `reserved_vector`-shaped object these bytes copy into: a count at +0 (`lwz r0,0(r4)` /
 *  `stw r0,0(r31)` in `fn_803412CC`) and the elements inline at +4 (`addi r3,r4,4` /
 *  `addi r5,r31,4`), which is `rstl::reserved_vector`'s layout and not `rstl::vector`'s
 *  count/capacity/items triple.  One element is modelled because nothing here does arithmetic on
 *  the array: the 0x804 stride is `fn_8034123C`'s above (`mulli r0,r0,0x804` at 0x80341254). */
struct SCarve80341284Vec {
  int mCount;
  unsigned char mElems[0x804];
};

/** 0x80341310, `symbols.txt:15363`, 0x68 = 104 bytes / 26 instructions.  The element copy loop
 *  this unit forwards to: a 0x20 frame, three saved cursors (source, count, destination) and a
 *  `bl fn_80340A5C` per element, the destination returned.  It is unclaimed, so this unit only
 *  declares it - its own 26 instructions are a spelling job of their own, which is why the claim
 *  stops at 0x80341310.
 *
 *  For the DOL that is enough: dtk splits the range above this claim into its own
 *  `auto_03_80341310_text` object (0x80341310..0x803414FC), and that object defines the symbol in
 *  the same link.  Declared with the order retail passes: the source's inline data in r3, the
 *  count in r4, the destination's in r5 - the same argument order the loop `fn_80248E60` forwards
 *  to, `fn_80248EA4`, takes at `src/WorldFormat/CMetroidAreaCollider.cpp`. */
extern void fn_80341310(const void* src, int count, void* dest);

/* Declared before use, then defined descending by address (see the header). */
struct SCarve80341284Vec* fn_803412CC(struct SCarve80341284Vec* self,
                                      const struct SCarve80341284Vec* other);
void fn_803412A4(struct SCarve80341284Vec* self, const struct SCarve80341284Vec* other);
void fn_80341284(struct SCarve80341284Vec* dest, const struct SCarve80341284Vec* src);

struct SCarve80341284Vec* fn_803412CC(struct SCarve80341284Vec* self,
                                      const struct SCarve80341284Vec* other) {
  self->mCount = other->mCount;
  fn_80341310(other->mElems, self->mCount, self->mElems);
  return self;
}

void fn_803412A4(struct SCarve80341284Vec* self, const struct SCarve80341284Vec* other) {
  if (self != 0) {
    fn_803412CC(self, other);
  }
}

void fn_80341284(struct SCarve80341284Vec* dest, const struct SCarve80341284Vec* src) {
  fn_803412A4(dest, src);
}

#ifdef TARGET_PC
/* The port's stand-in for `fn_80341310` (0x80341310, 104 bytes), which is above this claim and
 * therefore defined by nothing the port links: listing this unit in `files.cmake` puts its object
 * in `mp_game`, and `fn_803412CC`'s `bl fn_80341310` at 0x803412F4 is in retail's bytes, so the
 * carve cannot drop the call.  Measured without it: `python3 tools/link_gap.py --rebuild` exits 1
 * and names `gap grew: fn_80341310 is not in port_link_gap_list.md`.
 *
 * It lives here, behind `TARGET_PC`, and not as a `stub_18x` block in
 * `src/MetroidPrime/PortLinkStubs.cpp` - the audited home of the port's other stand-ins - for one
 * measured reason: that file is where two or more carve lanes append at the same time, and this
 * item's previous attempt was released for exactly that (`build/goal/run.log`:
 * `Applied patch to 'src/MetroidPrime/PortLinkStubs.cpp' with conflicts`, then
 * `carve-80341284 does not apply on 3fcf5f2 - releasing it for a fresh attempt`) even though its
 * judge had passed.  A port-only `#ifdef TARGET_PC` hunk in a decomp source is the same statement
 * the port already makes for its stand-in objects in `CSimplePool.cpp` and `CResFactory.hpp`
 * (`files.cmake:1189-1191`); the matching build does not define `TARGET_PC`, so nothing here can
 * reach `main.dol`.
 *
 * It is an announced empty body, like every other stand-in the port links, and it is **not** a
 * claim that `fn_80341310` is decompiled - it is not.  It is also strong, not weak, on purpose: the
 * lane that carves 0x80341310 for real gets a host link that refuses the duplicate by name, which
 * is the same signal `PortLinkStubs.cpp`'s stubs give, and **this block is then to be deleted**
 * exactly as those are. */
void fn_80341310(const void* src, int count, void* dest) {}
#endif
