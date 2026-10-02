// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:106-108`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_80004D84_text.s:385-471`, and the bodies below
// are the C those bytes are the compilation of.
//
// .text 0x800052A0..0x800053B8, 0x118 = 280 bytes, 3 functions:
//
//   fn_800052A0    0x800052A0  0x40   16 instructions
//   fn_800052E0    0x800052E0  0x30   12 instructions
//   fn_80005310    0x80005310  0xA8   42 instructions
//
// **It is one copy-assignment chain of a `u32`-first member followed by a counted three-node
// tree**: `fn_800052A0` copies the first word and forwards `+4` to `fn_800052E0`, which is a pure
// forwarder to `fn_80005310`, which is the tree's copy constructor.  All three are byte-shape
// twins of functions already matched in this tree - the same instructions apart from call
// targets - so the spellings below are the twins' own:
//
//   fn_800052A0  twin of `fn_8000401C` (`src/MetroidPrime/Carve80004010.c:92`, Matching,
//                0x8000401C): an `rstl::pair` copy-assign - copy the first word, forward the
//                second member's address to the member's own copy, return the destination.
//                Re-measured here from `build/G2ME01/main.elf` by disassembling with
//                `build/binutils/powerpc-eabi-objdump -d` and comparing the 16 instructions one
//                by one: **0 differences at all, the `bl` displacement included**, because each
//                copy's forwarder sits the same distance away.
//   fn_800052E0  twin of `fn_8000405C` (`src/MetroidPrime/Carve80004010.c:87`, Matching,
//                0x8000405C): the member's copy forwarder - `bl` the member's own copy, then
//                return the receiver.  Same measurement: 12 vs 12 instructions, **0 differences,
//                the `bl` displacement included**.
//   fn_80005310  twin of `__ct__Q24rstl158red_black_tree<Q24rstl10pair<Ui,i>,...>` at 0x802A4B38
//                (`src/Kyoto/Animation/CAnimSourceReaderBase.cpp:161`, Matching): the
//                `rstl::red_black_tree` copy constructor, whose source is
//                `include/rstl/red_black_tree.hpp:131-140`.  42 vs 42 instructions, **0 non-`bl`
//                differences and exactly 1 `bl` difference**: the C++ twin calls its own class's
//                `copy_from`, this copy calls the shared out-of-line `fn_80008C28`.  Everything
//                else - the three null stores, the `cmplwi r3,0 / mr r4,r3 / beq` null test that
//                the `const` on the local exists to force, both chain walks - is identical.
//
// The same body with the three scalar copies turned into frees is the copy *assignment* of the
// same tree, `fn_8000408C` at 0x8000408C, which `src/MetroidPrime/Carve80004010.c:34-42`
// documents in full: that one first tears the destination's root down through `fn_80008D68`,
// this one is the copy *constructor*, so it copies `+0x00`/`+0x01`/`+0x04` and stores three
// nulls instead.  Its `bl fn_80008C28` is the same callee this one calls.
//
// Who calls it: measured with `grep -rn 'bl fn_800052A0' build/G2ME01/asm/`, its only caller is
// 0x80005124 inside `fn_80005108` (0x80005108, 0x50, unclaimed), and
// `src/MetroidPrime/CMainResetGameState.cpp:258` already names `fn_80005108` as
// `SGameStateCardOpts`'s copy constructor - it calls this on the whole struct, then `fn_80005158`
// on `+0x18`.  That file's own notes are the identification, not a guess: it tells five copy
// constructors and assignments apart by their argument order and each one's field pattern.
//
// `fn_80008C28` (0x80008C28, 0xB8 = 184 bytes, `symbols.txt:178`) is the tree's post-order clone.
// It is **above** this claim and is therefore declared, never defined here.  Its DOL half is
// written - `src/MetroidPrime/main.cpp:346`, in a `Matching` unit - but `main.cpp` is not in
// `files.cmake`, so the port link has no body for it; for the port it is the announced stand-in
// `stub_195` in `src/MetroidPrime/PortLinkStubs.cpp`, the same trade `stub_182` and `stub_186`
// make for the two neighbouring carves.  Its **two** arguments are load-bearing: at 0x80005350
// `r3` still holds the tree (the `stb r5,0(r3)` / `stw r5,4(r3)` argument stores at
// 0x80005330/0x80005340 are through it), so the callee is `(dest, src->mRoot)` - the same two
// registers the C++ twin's member call `copy_from(root)` receives.  Its **return type** is
// spelled as `main.cpp:346` spells it and not as `void*`, because `tools/probe_sources.sh:78`
// syntax-checks every `src/**.c` that is not `src/LZO/*.c` or `platform/glibc_compat.c` with
// `g++ -std=c++20`, where a `void*` result does not implicitly convert to a node pointer.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges: below it, 0x80004D84..0x800052A0 is `auto_03_80004D84_text`'s seven
// unclaimed functions, and above it `MetroidPrime/main.cpp` already claims 0x800053B8 onward.
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/Player/CGameStateBlockConstruct.cpp` (0x80004D5C..0x80004D84) and above is
// `MetroidPrime/main.cpp`.

/** One node of the three-node tree.  Only `+0x00` and `+0x04` are touched here: `fn_80005310`'s
 *  two chain walks are `lwz r0,0(r3)` and `lwz r0,4(r3)` (`red_black_tree.hpp:280-296`'s
 *  `leftmost`/`rightmost`, which step to `get_left()`/`get_right()`).  It is the same node
 *  `src/MetroidPrime/main.cpp:346`'s `fn_80008C28` walks, whose own comment derives 44 bytes for
 *  its instantiation; only the first two words matter here. */
struct SNode {
  struct SNode* mLeft;
  struct SNode* mRight;
};

/** The 0x14-byte member at +4 of the pair above.  Every field offset is read off
 *  `fn_80005310`'s own displacements, and the member *set* behind them is retail's:
 *  `include/rstl/red_black_tree.hpp:274-278` declares `S mSelector; Cmp mCmp; Alloc mAllocator;
 *  int mCount; header mHeader;` and `:62-64` gives `header`'s three pointers
 *  (`mLeftmost`, `mRightmost`, `mRootNode`).  That class template is instantiated for retail's
 *  `rstl::pair<uint,int>` set by `src/Kyoto/Animation/CAnimSourceReaderBase.cpp:161,174,187`
 *  (`rstl::set<rstl::pair<uint,int>>`), which is the matched twin of `fn_80005310` - so with
 *  `S = identity<pair>` and `Cmp = less<pair>` both empty (one byte each, which is retail's two
 *  `lbz`/`stb` pairs at `+0`/`+1`) and `Alloc` stateless, the layout follows.  `mPad` makes
 *  the word's second half explicit instead of silently absorbing the alignment: the layout says
 *  `mAllocator` is at `+2`, but nothing in this tree has an object of this instantiation to read
 *  it from, so it is modelled as a byte that is never read or written.  `Carve80004010.c:66-72`
 *  models the same 0x14 bytes as five `int`s, and says why. */
struct STree {
  unsigned char mSelector;
  unsigned char mCmp;
  unsigned char mPad;
  int mCount;
  struct SNode* mLeftmost;
  struct SNode* mRightmost;
  struct SNode* mRoot;
};

extern struct SNode* fn_80008C28(void* self, struct SNode* node);

/** `red_black_tree.hpp:280-286`.  `static inline` is load-bearing, not style: the port compiles
 *  with `-inline deferred,noauto`, so a plain `static` helper is *outlined* and this unit would
 *  then emit two functions retail's object does not define. */
static inline struct SNode* Leftmost(struct SNode* n) {
  if (n != 0) {
    while (n->mLeft != 0) {
      n = n->mLeft;
    }
  }
  return n;
}

/** `red_black_tree.hpp:288-296`, and the reason for the same `static inline`. */
static inline struct SNode* Rightmost(struct SNode* n) {
  if (n != 0) {
    while (n->mRight != 0) {
      n = n->mRight;
    }
  }
  return n;
}

void* fn_80005310(struct STree* dest, const struct STree* src) {
  dest->mSelector = src->mSelector;
  dest->mCmp = src->mCmp;
  dest->mCount = src->mCount;
  dest->mLeftmost = 0;
  dest->mRightmost = 0;
  dest->mRoot = 0;
  // The block is load-bearing too, and so is the `const`.  This compiler's C mode is C89 for
  // declarations, so a declaration after the stores above is a syntax error; but as a plain
  // `struct SNode*` MWCC reuses the move into the inlined `Leftmost` as its own null test -
  // `mr r4,r3`, the record form, where retail has `cmplwi r3,0` / `mr r4,r3` / `beq` - and the
  // body comes out four bytes short.  `const` keeps the two apart, and `{ }` is what lets the
  // declaration sit after the stores.
  {
    struct SNode* const root = fn_80008C28(dest, src->mRoot);
    dest->mLeftmost = Leftmost(root);
    dest->mRightmost = Rightmost(root);
    dest->mRoot = root;
  }
  return dest;
}

void* fn_800052E0(struct STree* self, const struct STree* other) {
  fn_80005310(self, other);
  return self;
}

/** The pair these bytes copy: a `u32` at +0 (`lwz r0,0(r4)` / `stw r0,0(r31)`) followed by the
 *  member above at +4 (`addi r3,r31,4` / `addi r4,r4,4`). */
struct SPair {
  unsigned int mFirst;
  struct STree mSecond;
};

void* fn_800052A0(struct SPair* dest, const struct SPair* src) {
  dest->mFirst = src->mFirst;
  fn_800052E0(&dest->mSecond, &src->mSecond);
  return dest;
}
