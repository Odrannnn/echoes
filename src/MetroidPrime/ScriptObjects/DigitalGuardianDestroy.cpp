// DigitalGuardianDestroy.cpp - a carve of DigitalGuardian's (module 15) .text
// 0x00006144..0x0000617C: `fn_14_6144`, the module's own copy of `rstl::destroy(It, It)`.
//
// The range is retail's: `config/G2ME01/rels/DigitalGuardian/symbols.txt:112` carries
// `fn_14_6144 = .text:0x00006144; // type:function size:0x38`, and the next function,
// `fn_14_617C` (0x617C, 0x60), begins exactly where this one ends. In front of it `fn_14_60C0`
// (0x60C0, 0x84) ends exactly where this claim starts.  Both neighbours are unclaimed, so this
// unit is its own file and its claim covers one function and nothing else (`0x38` bytes,
// 14 instructions).
//
// **What it is.**  Two one-pointer iterators arrive by value, each is copied into the frame once,
// and the *addresses* of those two copies are handed to the module's own `destroy_impl` at
// `fn_14_617C`:
//
//   0x6144 stwu r1,-0x10(r1) / mflr r0
//   0x614C lwz r5,0x0(r4)        ; end.current, the second argument
//   0x6150 stw r0,0x14(r1)
//   0x6154 addi r4,r1,0x8        ; &(the by-value copy of `end`)
//   0x6158 lwz r0,0x0(r3)        ; begin.current, the first argument
//   0x615C addi r3,r1,0xc        ; &(the by-value copy of `begin`)
//   0x6160 stw r5,0x8(r1)
//   0x6164 stw r0,0xc(r1)
//   0x6168 bl fn_14_617C
//
// That is `rstl::destroy(It begin, It end) { destroy_impl(begin, end); }`
// (`include/rstl/construct.hpp:111-114`), which is the out-of-line shape MWCC gives every
// instantiation of it whose `destroy_impl` is not inlined into the caller.  The class is unnamed
// in retail - nothing in these bytes names it and nothing here is evidence about a declaration
// that does not exist - so the iterator is a local stand-in with the one field and the
// one-argument constructor the shape needs, exactly as `src/MetroidPrime/ScriptObjects/Carve801FD52C.cpp`
// does for the DOL copy of this template (its `fn_801FD5B0` is these 14 instructions with a
// different call target, and it is `Matching` at 100.00%).
//
// **The twin, measured this run**: DOL 0x800067A8,
// `destroy<Q24rstl116pointer_iterator<11CTweakValue,Q24rstl48vector<...>>>__4rstl...` of size
// 0x38 (`config/G2ME01/symbols.txt:128`), `Matching` in `main/MetroidPrime/main` - its bytes read
// out of the disc with `python3 tools/dol_read.py 0x800067A8 0x38` are these 14 instructions with
// only the `bl` displacement different.  The twin is a different function with the same shape, so
// nothing here reuses its name: retail calls this copy `fn_14_6144` and the definition below is
// `extern "C"` under exactly that spelling, so `symbols.txt` needs no rename.
//
// **Spellings measured this run against retail's 14 instructions** (mwcceppc, the module's own
// command line, 0x38-byte object with one defined symbol in every case unless noted):
//
//   `void fn_14_6144(SIt begin, SIt end) { fn_14_617C(begin, end); }` with a one-pointer
//   `SIt` carrying a one-argument constructor                              byte-exact (this file)
//   the same with a POD `SIt` (no constructor)                            byte-exact
//   named locals (`SIt b = begin; SIt e = end; callee(b, e);`)            0x40 bytes, four
//                                                                        `stw`s, wrong
//   a `void**` or `void*` parameter pair (no class type)                  the two `addi`s and
//                                                                        the two `stw`s vanish
//
// **This object is compiled with GC/2.7, not the REL default GC/1.3.2, and the version is the
// whole of what it needed.**  GC/1.3.2 schedules the saved-LR store *above* the two loads:
//
//   1.3.2  stwu / mflr / **stw r0,0x14(r1)** / lwz r5,0x0(r4) / lwz r0,0x0(r3) / ...   wrong
//   2.7    stwu / mflr / lwz r5,0x0(r4) / **stw r0,0x14(r1)** / ...                    retail
//
// which is the same difference `src/MetroidPrime/ScriptObjects/CLumiteRelTail.cpp` records for
// `fn_39_738`, reached the same way: a per-object `mw_version="GC/2.7"` in the module's `Rel(...)`
// block.  No spelling moves that store; it is the compiler version.
//
// **Listed in `files.cmake` behind an `#ifdef __MWERKS__`, and the guard is load-bearing.**
// `fn_14_617C` is unclaimed retail code that no port object defines, so an unguarded host build
// would add exactly one name to the port link's undefined set.  The port reads
// `DigitalGuardian.rel` off the disc and never calls into this module, and the MWCC branch is the
// retail source token for token, so the matching build cannot see the difference.  `CLumiteRelTail.cpp`
// measures the same arrangement.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim); there is one function here, so
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/DigitalGuardianDestroy.cpp`
// has nothing to reorder.

extern "C" {

#ifdef __MWERKS__

// One pointer of the iterator retail passes by value, copied into the frame at the call.  The
// element type never appears in these bytes - only the eight bytes of the two iterators do - so
// nothing is asserted about the class `fn_14_617C` walks.
struct SDigitalGuardianIterator {
  void* current;
  SDigitalGuardianIterator(void* p) : current(p) {}
};

// .text 0x617C, 0x60 bytes, unclaimed: the module's own `destroy_impl(It, It)` for the element
// this instantiation walks.  Declared, never defined here - dtk supplies the unclaimed bytes.
void fn_14_617C(SDigitalGuardianIterator begin, SDigitalGuardianIterator end);

// .text 0x6144, 0x38 bytes.  Both iterators by value; the call passes the addresses of the two
// frame copies.  The receiver of neither is read: this is a free function, not a member.
void fn_14_6144(SDigitalGuardianIterator begin, SDigitalGuardianIterator end) {
  fn_14_617C(begin, end);
}

#endif

} // extern "C"
