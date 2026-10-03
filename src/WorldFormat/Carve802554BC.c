// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:10475`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80255128_text.s:293-300` (the same range is now
// `build/G2ME01/asm/WorldFormat/Carve802554BC.s`), and the body below is the C those bytes are
// the compilation of.
//
// .text 0x802554BC..0x802554D0, 0x14 = 20 bytes, 1 function:
//
//   fn_802554BC    0x802554BC  0x14    5 instructions
//
//   802554bc  li   r0,0
//   802554c0  stw  r0,4(r3)
//   802554c4  stw  r0,8(r3)
//   802554c8  stw  r0,0xc(r3)
//   802554cc  blr
//
// **It is a byte-shape twin of `__ct__19CInGameTweakManagerFv`**
// (`config/G2ME01/symbols.txt`, 0x8016C230, `size:0x14`), matched in
// `src/MetroidPrime/CInGameTweakManagerCtor.cpp` - these 20 bytes word for word, with no call and
// no data address to differ.  That twin's source is one empty constructor
// (`CInGameTweakManager::CInGameTweakManager() {}`, `include/MetroidPrime/CInGameTweakManager.hpp`),
// so all of its work is the member's own constructor inlined: `rstl::vector<CTweakValue> mValues`
// at +0 of a 0x10-byte class (`CHECK_SIZEOF(CInGameTweakManager, 0x10)`).  The three stores are
// `include/rstl/vector.hpp:18-21` (`mAllocator, mCount, mCapacity, mItems`) zeroed at +4, +8 and
// +0xC, and **+0x00 is not stored at all** because `rmemory_allocator` is the empty type - the
// same reason the twin leaves the word at +0 as the heap had it.  So the shape here is "default-
// construct an `rstl::vector`", and `include/rstl/vector.hpp:37-38`'s
// `vector(const Alloc& alloc = Alloc()) : mAllocator(alloc), mCount(0), mCapacity(0),
// mItems(nullptr)` is the declaration those bytes come from.
//
// **The receiver is a `rstl::vector<SLdrConnection, rmemory_allocator>`, fixed by this function's
// one caller.**  `grep -rn "bl fn_802554BC" build/G2ME01/asm/` returns exactly one call site,
// 0x801E1364 in `LoadSequenceTimer(CStateManager&, CInputStream&, CEntityInfo&)`
// (`MetroidPrime/ScriptObjects/CScriptSequenceTimer`, `.text` 0x801E1338, size 0x2B4), reached
// with `addi r28,r1,0x20` at 0x801E1348 making `r28` the frame's `SLdrEditorProperties`
// temporary (`bl __ct__20SLdrEditorPropertiesFv` at 0x801E135C; retail's own ctor writes up to
// `stw r0,0x38(r31)` at 0x8023F124, so the object is 0x3C bytes) and then `addi r3,r28,0x3c`.  The
// object at that offset is the next temporary in the same frame, and 0x801E1484..0x801E149C name
// it: `__ct__23SLdrSequenceConnectionsFR12CInputStream` builds one at `r1+0x10`,
// `__as__23SLdrSequenceConnectionsFRC23SLdrSequenceConnections` at 0x801E1490 copies it into
// `r29 = r1+0x5c`, the temporary is destroyed through
// `__dt__Q24rstl51vector<14SLdrConnection,Q24rstl17rmemory_allocator>Fv`, and `mr r7,r29` at
// 0x801E15A8 hands `r1+0x5c` to
// `__ct__20CScriptSequenceTimerF9TUniqueIdRCQ24rstl66basic_string<...>RC11CEntityInfoRCQ24rstl51vector<14SLdrConnection,Q24rstl17rmemory_allocator>fffbbb`.
// **So `fn_802554BC` is that vector's default constructor**, emitted out of line and called once.
// Retail names no such constructor - the two `SLdrSequenceConnections` spellings it does have are
// its stream constructor (`__ct__23SLdrSequenceConnectionsFR12CInputStream`, `symbols.txt:10465`,
// 0x80255198) and its assignment operator (`__as__23SLdrSequenceConnectionsFRC23SLdrSequenceConnections`,
// `symbols.txt:7750`, 0x801E15EC, right after the caller's own `bl` at 0x801E1490) - so the symbol
// keeps the `fn_<addr>` placeholder.
//
// **The neighbourhood is the same container family, which is why the reading fits.**  In the
// `auto_*` range around it: 0x80255198 `__ct__23SLdrSequenceConnectionsFR12CInputStream` (0x34)
// and 0x80255418 `__ct__Q24rstl36vector<f,Q24rstl17rmemory_allocator>FR12CInputStreamRCQ24rstl17rmemory_allocator`
// (0xA4) - a stream constructor for the same `vector` and for `vector<float>`.  Above it,
// `fn_802554D0` (0x802554D0, 0xB8) is a bigger body that opens by default-constructing something
// at `+0x40` of its own receiver (`addi r3,r30,0x40` then `bl fn_80256BAC`) before its loop.
// Nothing else in the tree references this address.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z11fn_802554BCPv` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The single word `mAllocator` at +0 is padding here - nothing in these bytes
// reads it - and exists only so the three fields land on retail's displacements, the same
// convention `src/MetroidPrime/Carve80004744.c` documents.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  With one function the order cannot be wrong, and the rule is
// recorded because the next carve added to this file would break it.
//
// Its own unit because a claim may not span an unclaimed gap and may not sit where a neighbouring
// unit's `.text` ends: the claim starts exactly where
// `__ct__Q24rstl36vector<f,...>` ends (0x802554BC) and ends where `fn_802554D0` begins
// (0x802554D0), and neither neighbour is trivial, so it absorbs neither side.
// `tools/check_decl_order.py --unit main/WorldFormat/Carve802554BC` is the cheap check.  The
// directory is retail's own, taken from the nearest claimed ranges: below is
// `WorldFormat/CAreaRenderOctTree.cpp` (`.text` 0x80254BAC..0x80255128) and above is
// `WorldFormat/Carve80255900.c` (0x80255900..0x80255A0C).
//
// The function is a leaf, so nothing needs declaring and this unit has no `extern` at all.  For
// the DOL, dtk's own `auto_*` object defined `fn_802554BC` and the two halves of that object do
// without it; for the port link, `src/WorldFormat/Carve802554BC.c` is the definition and no
// `PortLinkStubs.cpp` stand-in is needed or wanted - `fn_802554BC` is not stubbed there, because
// nothing in the port-linked set references it (`grep -n 802554BC src/MetroidPrime/PortLinkStubs.cpp`
// finds nothing).

/** The 0x10-byte object `fn_802554BC` default-constructs, as far as these bytes read it:
 *  `include/rstl/vector.hpp:18-21`'s `mAllocator, mCount, mCapacity, mItems` in retail's order,
 *  the three that are cleared being the count, the capacity and the item array.  `+0x00` is the
 *  empty `rmemory_allocator` and is never stored.  The element type does not matter here: nothing
 *  in this function reads it, and the caller proves which one it is. */
struct SConnections800554BC {
  int mAllocator;
  int mCount;
  int mCapacity;
  void* mItems;
};

/** retail `.text:0x802554BC`, 0x14 = 20 bytes: `rstl::vector<SLdrConnection,
 *  rmemory_allocator>::vector()` for the stack temporary `LoadSequenceTimer` assigns into at
 *  0x801E1490.  Written as the three stores rather than through `rstl::vector` itself because the
 *  symbol has to stay C - see the header - and because the header above fixes the element type
 *  from the caller, not from anything retail names. */
void fn_802554BC(struct SConnections800554BC* self) {
  self->mCount = 0;
  self->mCapacity = 0;
  self->mItems = 0;
}