// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:10009-10010`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_802328A4_text.s:1299-1365`, and the bodies below
// are the source those bytes are the compilation of.  The byte evidence is the pristine disc:
// `python3 tools/dol_read.py 0x80233A90 0xC8 orig/G2ME01/sys/main.dol` gives 50 instructions /
// 200 bytes, word for word what the asm listing above holds.
//
// **`tools/carve_diff.sh` cannot be that evidence once the unit is in the link**, which is worth
// recording because the tool's own header says "retail".  Its retail side is
// `build/G2ME01/main.elf`, and `build.ninja` has `build build/G2ME01/main.elf: link ...` - it is
// *our own* link output.  Run against a unit whose object was already linked in, it compares the
// unit with itself: it reported "50 instructions, 200 bytes / differing instructions: 1" (the one
// `bl`) for a build whose `main.dol` differed from the disc in four more words.  A green
// `carve_diff.sh` is therefore only evidence for a unit that is not yet a `Matching` one, and the
// independent check is `sha1sum build/G2ME01/main.dol` or a word diff against
// `orig/G2ME01/sys/main.dol`.
//
// .text 0x80233A90..0x80233B58, 0xC8 = 200 bytes, 2 functions:
//
//   fn_80233A90  0x80233A90  0x4C = 76 bytes   19 instructions
//   fn_80233ADC  0x80233ADC  0x7C = 124 bytes  31 instructions
//
// **Both are byte-shape twins of matched code elsewhere in this tree, apart from the one call
// target, and both are byte-exact** - checked against the disc word by word, with only
// `bl fn_80233ADC`'s displacement differing (it is address-relative and is filled by the link).
// The twins are the two lowest functions of `CStateManager.cpp`
// (`build/G2ME01/asm/MetroidPrime/CStateManager.s:7084-7176`), retail 0x8003C28C / 0x8003C2D8, both
// at 100.00% in `build/report.json`:
//
//   fn_80233ADC  is `fn_8003C2D8` (`src/MetroidPrime/CStateManager.cpp:849-871`) - the same 31
//                instructions, including the rotated `while` (the `b` goes to the compare, the
//                body sits above it), `li r6,0` for the `found` initialiser *between* the two
//                first loads, and the two-`blr` tail `if (noResult) return nullptr; return
//                found;` with the test as `clrlwi. r0,r5,24`.
//   fn_80233A90  is `fn_8003C28C` (`src/MetroidPrime/CStateManager.cpp:913-918`) - the same 19
//                instructions: the interleaved save block (`mr r31,r4` / `mr r4,r5` / `stw r30,8`
//                / `mr r30,r3`), the call with `ids` moved back into `r3`, and the
//                `addi r0,r31,8 ; stw r0,4(r30)` that writes the iterator's second word.
//
// **This unit is a `.cpp` with `extern "C"`, not a `.c`, and both differences are measured.**  The
// name stays unmangled because of `extern "C"` (29 of the tree's carve units are `.c`; a
// C++ definition without it would mangle to `_Z<len>fn_80233A90...` and objdiff would pair
// nothing).  The language is not a style choice:
//
//   `bool noResult` (fn_80233ADC)  Written as `int noResult = 0; ... = 1;` in a `.c`, the flag is
//     hoisted out of the loop into `r7` as `li r7,0` before the first load and the test becomes
//     `cmpwi r7,0`.  Retail keeps it in `r5`, materialises `li r5,0` *after* the loop and tests it
//     as `clrlwi. r0,r5,24`, which is MWCC's code for a `bool`.  Same 31 instructions, four of
//     them in the wrong place, and the claim needs all 31.
//   `struct ScriptIdConstIter80233ADC` with a user-declared constructor (fn_80233A90).  As a
//     plain two-word POD returned by value, MWCC returns it in `r3`/`r4` and the body comes to
//     **14 instructions / 56 bytes** (`stw r3,8(r1)` / `stw r4,12(r1)` into the caller's frame),
//     not retail's 19 / 76.  A class with a constructor is not trivially constructible, so the
//     return goes through memory and the hidden pointer `r3` is retail's.
//
// **And the last three words of `fn_80233A90` are an explicit copy-construction.**  Retail's
// epilogue restores `lr` *first* and then `r31` / `r30` (`lwz r0,0x14(r1)` / `lwz r31,0xc(r1)` /
// `lwz r30,0x8(r1)` / `mtlr r0`); the twin's `return CStateManager::TIdList::const_iterator(it)`
// is what produces it.  Written as a bare `return it;` the same function is 19 instructions and
// 76 bytes and restores them the other way round (`r31` / `r30` / `lr`), which is 3 wrong words in
// the one function whose bytes nothing else can vouch for.  Measured both, on this body.
//
// **What fixes the parameter list.**  `fn_80233ADC` takes `ids` in `r3` and `eid` in `r4` and
// reads `*(r4)` in its very first instruction (`lwz r0,0(r4)`), so `eid` is a pointer to the
// 4-byte `TEditorId`, not a value: `TEditorId` is one word (`include/MetroidPrime/TGameTypes.hpp:38`,
// `CHECK_SIZEOF(TEditorId, 0x4)`) whose `Value()` is `value & 0x3FFFFFF`
// (`TGameTypes.hpp:44`).  That mask is the whole reason the comparisons are `clrlwi r, r, 6`
// (`r & ~0x3F` is `(r & 0x3FFFFFF) << 6`, still order-equivalent for an unsigned `cmplw`) and not
// a plain `cmplw`, and it is why an `unsigned int`-typed key reproduces retail here.
//
// **`&eid` in `r4` also fixes which operand is which, and the order decides the registers.**
// Inside the loop retail loads only the *node's* key into `r3` and compares it against `r0`,
// which already holds the masked `*eid` - that is `cur->value.first < eid`, not the reverse.  The
// second comparison is the mirror image (`*eid` into `r3`, `found->value.first` into `r0`, `bge` to
// skip), i.e. `eid < found->value.first`.  Both halves are spelled here in the twin's operand
// order and must not be swapped: this is the same family as the `IsAllocValid` finding in
// `RUNNING_THE_DECOMP.md` ("mwcceppc keeps a comparison's source operand order").
//
// **`fn_80233A90`'s result is two words, and the second is `ids + 8`.**  It is
// `CStateManager::TIdList::const_iterator`, whose only two members are `node* mNode` and
// `const header* mHeader` (`include/rstl/red_black_tree.hpp:73-74`).  `&mHeader` is `ids + 8`:
// the tree's header is the last member of `red_black_tree` and `ScriptIdMapView`'s first 8 bytes
// are the members in front of it (`src/MetroidPrime/CStateManager.cpp:838-847`), which is also why
// the root at `+0x10` and the two `leftmost`/`rightmost` pointers all line up.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `fn_80233ADC` (0x80233ADC) is the higher address and
// therefore comes first here, so the object leads with `fn_80233A90` at offset 0, which
// `powerpc-eabi-nm` confirms (`00000000 T fn_80233A90`, `0000004c T fn_80233ADC`).
//
// **A wrong-size object is not a local failure.**  While `fn_80233A90` was the 56-byte version,
// `tools/decomp_build.sh` reported *all 87* checksums wrong, not one: `main.dol` came out 32
// bytes short, its first section offset moved 0x3a2280 -> 0x3a22a0's worth of bytes, and a `bl` in
// `auto_00_80003100_init` at 0x80003264 pointed 0x50 bytes below retail's target because every
// object after it in link order had shifted.  `tools/unit_fit.sh` is the check that predicts this
// before a full build, and it is why `RUNNING_THE_DECOMP.md` says to run it before promising a
// promotion.  Restoring 76 bytes fixed all 87 at once, so "87 files OK" said nothing about which
// unit was at fault and `tools/carve_diff.sh` said nothing at all.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ... link
// order").  The claim starts at `fn_80233A90`, which is exactly where `fn_80233A04` ends
// (`symbols.txt:10008` gives it 0x8C), and stops at `fn_80233B58`, which `symbols.txt` names -
// `CScriptObjectLoaderHelper::RegisterScriptObjects`, 0xF4 bytes - so nothing above the item's two
// functions is taken.  Neither neighbour is a unit boundary, which is the "proximity to another
// carve is fine" case rather than the link-order cycle `RUNNING_THE_DECOMP.md` records for a carve
// that starts where a `Matching` unit ends: the nearest claimed range below is
// `MetroidPrime/ScriptLoader/Carve8023289C.c` (`.text` 0x8023289C..0x802328A4) and above is
// `MetroidPrime/ScriptLoader/EyeBall.cpp` (0x80234900..0x8023492C), and this run sits inside dtk's
// `auto_03_802328A4_text`, whose last function `fn_8023485C` ends at 0x80234900 - that unit now
// runs 0x802328A4..0x80233A90 and 0x80233B58..0x80234900 as two objects, and 0x11EC + 0xC8 + 0xDA8
// is the 0x205C it was before.
//
// The directory is retail's own, taken from the nearest claimed range below
// (`MetroidPrime/ScriptLoader/Carve8023289C.c`, which ends where `auto_03_802328A4_text` begins):
// this claim is 0x11EC bytes into that auto unit, which is all the `ScriptLoader` neighbourhood
// evidence there is for an anonymous function.
//
// The one callee, `fn_80233ADC`, is in this same unit, so nothing is declared extern and the unit
// adds no undefined symbol to the DOL link or to the host build.

/** The script-ID map's red-black node.  Only three of the six words are read here: `left`,
 *  `right` and the key at +0x10.  `+0x8` is the parent and `+0xC` the colour (neither is touched
 *  by either function), and `+0x14` is the `TUniqueId` half of the node's
 *  `pair<TEditorId, TUniqueId>`.  The layout and the 0x18 size are
 *  `src/MetroidPrime/CStateManager.cpp:836-848` (`CHECK_SIZEOF(ScriptIdNode, 0x18)`).  Named here
 *  locally because `CStateManager.cpp` keeps them file-local and there is no header that declares
 *  them - the tree view is "kept local until its lower/upper-bound template instantiations can be
 *  named in rstl itself" (`CStateManager.cpp:834`), which is exactly why this copy is anonymous. */
struct ScriptIdNode80233ADC {
  struct ScriptIdNode80233ADC* x0_left;
  struct ScriptIdNode80233ADC* x4_right;
  struct ScriptIdNode80233ADC* x8_parent;
  int xc_color;
  unsigned int x10_key;
  unsigned int x14_id;
};

/** The map's header, as `fn_80233ADC` sees it: `+0x8` / `+0xC` / `+0x10` are the tree's
 *  `mLeftmost` / `mRightmost` / `mRootNode` (`include/rstl/red_black_tree.hpp:63-64`) and the eight
 *  bytes in front of them are the members `red_black_tree` declares before its header.  Only
 *  `x10_root` is read, and `+0x8` is the address `fn_80233A90` stores as the iterator's
 *  `mHeader`. */
struct ScriptIdMapView80233ADC {
  char x0_header[8];
  struct ScriptIdNode80233ADC* x8_leftmost;
  struct ScriptIdNode80233ADC* xc_rightmost;
  struct ScriptIdNode80233ADC* x10_root;
};

/** `red_black_tree::const_iterator`'s two members, `include/rstl/red_black_tree.hpp:73-74`.
 *  It is spelled **with a constructor**, like the `const_iterator` it stands for
 *  (`red_black_tree.hpp:78-79`), and that is load-bearing rather than decoration - see the header
 *  comment. */
struct ScriptIdConstIter80233ADC {
  struct ScriptIdNode80233ADC* x0_node;
  const struct ScriptIdMapView80233ADC* x4_header;
  ScriptIdConstIter80233ADC(struct ScriptIdNode80233ADC* node,
                             const struct ScriptIdMapView80233ADC* header)
  : x0_node(node), x4_header(header) {}
};

extern "C" struct ScriptIdNode80233ADC* fn_80233ADC(const struct ScriptIdMapView80233ADC* ids,
                                                     const unsigned int* eid);
extern "C" struct ScriptIdConstIter80233ADC
fn_80233A90(const struct ScriptIdMapView80233ADC* ids, const unsigned int* eid);

/** `fn_80233ADC` - retail `.text:0x80233ADC`, 0x7C = 124 bytes: the map's
 *  `lower_bound`-shaped search for `eid`, returning the node that holds it or `nullptr`.
 *  The tree is walked with `!(cur->x10_key < eid)` rather than `cur->x10_key >= eid`, which is what
 *  produces the compare-then-`blt`-to-the-right-load arrangement retail has. */
extern "C" struct ScriptIdNode80233ADC* fn_80233ADC(const struct ScriptIdMapView80233ADC* ids,
                                                    const unsigned int* eid) {
  struct ScriptIdNode80233ADC* cur = ids->x10_root;
  struct ScriptIdNode80233ADC* found = 0;
  while (cur) {
    if (!((cur->x10_key & 0x3FFFFFF) < (*eid & 0x3FFFFFF))) {
      found = cur;
      cur = cur->x0_left;
    } else {
      cur = cur->x4_right;
    }
  }
  bool noResult = false;
  if (!found || ((*eid & 0x3FFFFFF) < (found->x10_key & 0x3FFFFFF))) {
    noResult = true;
  }
  if (noResult) {
    return 0;
  }
  return found;
}

/** `fn_80233A90` - retail `.text:0x80233A90`, 0x4C = 76 bytes: the search above, wrapped in the
 *  map's `const_iterator`.  `x0_node` is the search's result and `x4_header` is the address of the
 *  tree header, `ids + 8`, which is what `end()` would have put there.  The explicit
 *  copy-construction in the `return` is what orders the epilogue - see the header comment. */
extern "C" struct ScriptIdConstIter80233ADC
fn_80233A90(const struct ScriptIdMapView80233ADC* ids, const unsigned int* eid) {
  struct ScriptIdConstIter80233ADC it(
      0, (const struct ScriptIdMapView80233ADC*)((const char*)ids + 8));
  it.x0_node = fn_80233ADC(ids, eid);
  return ScriptIdConstIter80233ADC(it);
}