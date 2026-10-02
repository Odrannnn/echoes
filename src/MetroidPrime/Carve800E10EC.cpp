// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_800DFA60_text.s`, and the body below is the source those
// bytes are the compilation of.
//
// .text 0x800E10EC..0x800E122C, 0x140 = 320 bytes, 4 functions:
//
//   fn_800E1188    0x800E1188  0xA4    41 instructions
//   fn_800E1130    0x800E1130  0x58    22 instructions
//   fn_800E110C    0x800E110C  0x24     9 instructions
//   fn_800E10EC    0x800E10EC  0x20     8 instructions
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to keep that name: an ordinary C++
// definition would mangle and objdiff would pair nothing.
//
// **It is a `.cpp` rather than a `.c`, and that is measured, not tidiness.**  The element teardown
// in `fn_800E1188` is retail's out-of-line
// `internal_dereference__Q24rstl66basic_string<w,Q24rstl14char_traits<w>,Q24rstl17rmemory_allocator>Fv`
// (0x802FDF38), and that name is not made of identifier characters, so a `.c` file cannot declare
// it - `extern void internal_dereference__...<w,...>Fv(void*);` fails with `illegal use of 'void'`
// at the first `<`.  A `.c` cannot include `rstl/vector.hpp` either (`namespace rstl {` is a C++
// construct), and a raw-pointer C loop with a placeholder callee measured **37 differing
// instructions** against this range's 0xA4 bytes, because the bytes want the two
// `pointer_iterator` temporaries `rstl::destroy(begin(), end())` passes by value.  `extern "C"`
// keeps the four symbols unmangled, so objdiff pairs them exactly as it would for a `.c`.

#include "rstl/string.hpp"
#include "rstl/vector.hpp"

// The class these are the teardown of is unnamed in retail's symbols; only its layout is
// visible, and only through `fn_800E1130`: 0x20 bytes whose only non-trivial member sits at
// +0x10.  `fn_800E108C` (0x800E108C, 0x60, still in the auto unit below this claim) walks a run
// of those 0x20-byte objects - count at +0, items at +4, `addi r31,r31,0x20` - and calls
// `fn_800E10EC` with each element's address, which is `rstl::destroy(&*cur)`'s call shape for a
// 0x20-byte element type.  Nothing in the range reads the first 0x10 bytes.
struct CWideStringListHolder {
  uchar mUnknown00[0x10];
  rstl::vector< rstl::wstring > mStrings;
};

extern "C" rstl::vector< rstl::wstring >* fn_800E1188(rstl::vector< rstl::wstring >* self,
                                                      int flag);
extern "C" CWideStringListHolder* fn_800E1130(CWideStringListHolder* self, int flag);
extern "C" void fn_800E110C(CWideStringListHolder* in);
extern "C" void fn_800E10EC(CWideStringListHolder* in);

// 0x800E1188, 0xA4: `rstl::vector<rstl::wstring>`'s deleting destructor.  The `destroy(begin(),
// end())` loop, the `mItems` free and the free-through-to-self when the flag is positive are the
// header's own `~vector()` body (`include/rstl/vector.hpp:139`), and the element teardown is
// `rstl::basic_string<wchar_t>::internal_dereference()` - the only `bl` in the range that names
// another unit, retail 0x802FDF38.
extern "C" rstl::vector< rstl::wstring >* fn_800E1188(rstl::vector< rstl::wstring >* self,
                                                      int flag) {
  if (self != nullptr) {
    rstl::destroy(self->begin(), self->end());
    CMemory::Free(self->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// 0x800E1130, 0x58: the holder's own deleting destructor.  It reaches only the member at +0x10,
// whose destructor is the `fn_800E1188` above (the `li r4,-1` is mwcceppc's explicit destructor
// call), and then frees `this` when the flag is positive.
extern "C" CWideStringListHolder* fn_800E1130(CWideStringListHolder* self, int flag) {
  if (self != nullptr) {
    fn_800E1188(&self->mStrings, -1);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// 0x800E110C, 0x24: `rstl::destroy_impl<X>(X*)` - one explicit destructor call with the deleting
// flag, no trait test, because X is not trivially destructible
// (`include/rstl/construct.hpp:85`).
extern "C" void fn_800E110C(CWideStringListHolder* in) { fn_800E1130(in, -1); }

// 0x800E10EC, 0x20: `rstl::destroy<X>(X*)` - the header's `{ destroy_impl(in); }`, one call and
// nothing else (`include/rstl/construct.hpp:93`).
extern "C" void fn_800E10EC(CWideStringListHolder* in) { fn_800E110C(in); }
