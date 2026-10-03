// Carved out of an unclaimed dtk `auto_*` range by lane `carve6`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_*.s`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x80335B30..0x80335B38, 0x8 = 8 bytes, 1 function:
//
//   fn_80335B30    0x80335B30  0x8    lwz        r3, 0x18(r3)
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.  With one function the order is trivially
// satisfied; the rule is written down anyway so the file can grow.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit
// is a `.c` rather than a `.cpp`.  For the same reason the parameter is a bare `void *`
// and not a typed `this`: the class this slot belongs to is not one this file can name
// without inventing a type, and a named class would change the symbol.
//
// The body is a 4-byte member load off the incoming pointer, byte-identical to the already
// matched `CDummyWorld::IGetSaveWorldAssetId() const`
// (`IGetSaveWorldAssetId__11CDummyWorldCFv` at 0x8004F968, `build/G2ME01/asm/MetroidPrime/CWorld.s`),
// which is `return mSavwId;` on a `CAssetId` (`typedef uint`, so 4 bytes) at 0x18.  The same
// two instructions occur at slot +0x28 of the `CInstruction`-derived vtable
// `lbl_803BBB68` (`build/G2ME01/asm/auto_07_803BBB68_data.s`), so retail's own shape here is
// an accessor returning a 4-byte id member and nothing else.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the
// functions on either side of this run are not trivial.
//
// The directory is retail own, taken from the nearest claimed range: this address is
// 0xDF20 bytes into `Kyoto/Math/CMayaSpline.cpp`, so the code is that unit
// neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.
unsigned int fn_80335B30(void* self) { return *(unsigned int*)((char*)self + 0x18); }
