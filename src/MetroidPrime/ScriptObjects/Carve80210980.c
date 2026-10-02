// Carved out of an unclaimed dtk `auto_*` range by lane `11`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_8020EE18_text.s`, and the bodies below
// are the C those bytes are the compilation of, read off their matched byte-shape twins.
//
// .text 0x80210980..0x80210990, 0x10 = 16 bytes, 2 functions:
//
//   fn_80210980    0x80210980  0x8    lwz       r3, 0x4(r3)
//   fn_80210988    0x80210988  0x8    lwz       r3, 0x0(r3)
//
// Both are one-word accessors on the pointer in `r3` and neither calls anything nor touches
// any data, so the only evidence for their shape is the 8 bytes each.  Each is a byte-shape
// twin of a function objdiff reports at 100.00% (in a unit that is not `Matching`, so the twin
// is evidence for the shape and not a result), and the twins say what the loaded word is:
//
//   fn_80210980  `uint CCollisionActorManager::GetNumCollisionActors() const
//                 { return mJointDescriptions.size(); }`
//                 `src/MetroidPrime/CCollisionActorManager.cpp`, retail 0x80135AE4, the
//                 size word at +4 of an `rstl::vector` whose pointer is at +0.
//   fn_80210988  `int IGameArea::Dock::GetReferenceCount() const { return mReferenceCount; }`
//                 `src/MetroidPrime/CGameArea.cpp`, retail 0x80056FEC, the member at +0.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt:8514-8515` carries the `fn_<addr>` placeholder
// and this file reproduces that symbol verbatim, so the definitions have to stay C: a C++
// one would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why
// the unit is a `.c` rather than a `.cpp`.
//
// Its own unit because the dtk range it comes out of, `auto_03_8020EE18_text`, is otherwise
// unclaimed, and because the functions on either side of this run are not part of the claim:
// below, `fn_80210978` (0x80210978, 0x8) loads a float at +8; above, `fn_80210990`
// (0x80210990, 0x104) is a real body.  Neither boundary is another unit's boundary, so no
// link-order cycle is at risk (`docs/RUNNING_THE_DECOMP.md`, "The carve vein").
//
// The directory is retail own, taken from the nearest claimed range: this address is
// 0x1D90 bytes into `MetroidPrime/ScriptObjects/CScanTreeInventory.cpp`, so the code is that
// unit neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.
unsigned int fn_80210988(const void* self) { return *(const unsigned int*)self; }

unsigned int fn_80210980(const void* self) {
  return *(const unsigned int*)((const char*)self + 4);
}
