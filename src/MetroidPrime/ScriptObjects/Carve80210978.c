// Carved out of an unclaimed dtk `auto_*` range by lane `1`.  Every number here is
// measured: the addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are
// the ones dtk itself emitted into `build/G2ME01/asm/auto_03_8020EE18_text.s`, and the body below
// is the C those bytes are the compilation of.
//
// .text 0x80210978..0x80210980, 0x8 = 8 bytes, 1 function:
//
//   fn_80210978    0x80210978  0x8    lfs f1, 0x8(r3) ; blr
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text` order
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function, `unit_fit.sh`
// still saying "fits", the link still succeeding, and a broken DOL.  Only
// `tools/flip_test.sh` catches it.  With one function the order is trivially right; the rule is
// recorded because the next run to extend this claim has two.
//
// Retail names none of these.  `symbols.txt:8513` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle
// to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// **What the load is.**  The two callers both pass `this + 0x1B8` of a `CPauseScreen`:
// `UpdateHistoryColors` (`mr r15, r3` at 0x80202088, then `addi r3, r15, 0x1b8 / bl
// fn_80210978 / fmr f30, f1` at 0x802020FC..0x80202104) and `CPauseScreen::Draw`
// (`addi r3, r29, 0x1b8 / bl fn_80210978 / fmr f29, f1` at 0x80205F24..0x80205F2C).  That offset
// is `CScanTree mScanTree`, and retail says so itself: `CPauseScreen`'s constructor calls
// `addi r3, r28, 0x1b8` (0x8020C7DC) and then, with `r3` untouched across the eight stores in
// between, `bl __ct__9CScanTreeFv` (0x8020C824).  So the receiver is a `CScanTree` and the word
// at `+8` is its first float member, which `include/MetroidPrime/CScanTree.hpp` declares as
// `float mTransition` after `int mSelectedNode` (+0) and `int mPreviousNode` (+4).  Retail's
// `CScanTree::CScanTree` (0x80211C3C, 0x80 bytes) agrees: it stores `lbl_8041D8BC` -
// `symbols.txt:24407`, typed `data:float` - at `0x8(r31)` at 0x80211C70, and `-1` at `+0` and
// `+4` at 0x80211C64 and 0x80211C6C.
//
// **The struct is by offset, not by a class.**  `CScanTree` is a C++ class in a C++ header, and
// this unit is plain C, so the three words the instruction and its callers name are declared in
// a local struct whose only claims are the three offsets above - exactly as
// `src/MetroidPrime/Tweaks/Carve80216D2C.c` does for `CTweakGame`.  Nothing at or past `+0xC` is
// asserted here and nothing reads it; the header's own later members are retail's business and
// this file does not restate them.
//
// **Port.**  Nothing in `src/` or `include/` names `fn_80210978` by symbol: its two callers are
// `CPauseScreen`'s, inside the retail unit `MetroidPrime/CPauseScreen.cpp`, which is
// `Object(NonMatching, ...)` in `configure.py` and is compiled from our own source only on the
// DOL build.  So no `TARGET_PC` arm is needed and the body below is the DOL's, offset and all.
//
// Its own unit because the dtk range it comes out of, `auto_03_8020EE18_text`, is otherwise
// unclaimed, and because the functions on either side of this run are not part of the claim:
// below, `fn_80210968` (0x80210968, 0x10) is a `cntlzw`/`srwi` pair over the word at `+0x24`,
// and above, `fn_80210980` (0x80210980, 0x8) is the start of the landed
// `MetroidPrime/ScriptObjects/Carve80210980.c`.  Neither boundary is another unit's boundary, so
// no link-order cycle is at risk (`docs/RUNNING_THE_DECOMP.md`, "The carve vein") - proximate
// carves link, a carve that *starts* where an existing unit's `.text` ends does not.
//
// The directory is retail's own, taken from the nearest claimed range: this address is
// 0x1D88 bytes past the end of `MetroidPrime/ScriptObjects/CScanTreeInventory.cpp`
// (0x8020EBF0..0x8020EE18).  For an anonymous function that is the only evidence there is, and
// it beats a lane picking the directory it happened to own.
//
// The shape itself - one `lfs` of a member at a constant displacement, then `blr` - is what the
// matched `float CRuleValue::GetFloat() const { return *reinterpret_cast< const float* >(
// &m_value); }` compiles to (`src/MetroidPrime/CRuleSet.cpp:57`, retail 0x801F5E04,
// `lfs f1, 0x4(r3) ; blr`, in the `Matching` unit `MetroidPrime/CRuleSet.cpp`).  That is the
// twin: same two instructions, differing only in the displacement, in a unit objdiff reports at
// 100.00%.  It is evidence for the shape, not a result of this one.
typedef struct {
  int x00;     /* 0x00 - CScanTree::mSelectedNode, stored -1 by __ct__9CScanTreeFv */
  int x04;     /* 0x04 - CScanTree::mPreviousNode, stored -1 by __ct__9CScanTreeFv */
  float x08;   /* 0x08 - CScanTree::mTransition, and fn_80210978's load target */
} SScanTreeHead;

float fn_80210978(const void* self) { return ((const SScanTreeHead*)self)->x08; }