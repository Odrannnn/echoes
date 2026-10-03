// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:6005-6006`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8016BDEC_text.s` before the claim existed (the same range
// is now `build/G2ME01/asm/MetroidPrime/Carve8016BEA8.s`), and the bodies below are the C++ those
// bytes are the compilation of.
//
// .text 0x8016BEA8..0x8016BF4C, 0xA4 = 164 bytes, 2 functions:
//
//   fn_8016BED0    0x8016BED0  0x7C  124 bytes  31 instructions  `istring::compare`
//   fn_8016BEA8    0x8016BEA8  0x28   40 bytes  10 instructions  `istring::operator==`
//
// **What the two are: `rstl::istring`'s `compare` and `operator==`, and the header already says so.**
// `include/rstl/string.hpp:397-408` documents that `typedef basic_string<char,
// case_insensitive_char_traits<char>> istring` is retail's third `rstl` string, that
// `CTextParser::GetImage` is its only caller, and that retail emits both members out of line for
// this instantiation - "`fn_8016BEA8` is `operator==` and it calls `fn_8016BED0`, which is
// `compare`", in an unclaimed `.text` gap "far from `rstl/rstl_string_l.cpp`, so they need a unit of
// their own".  This is that unit.  The two bodies are `string.hpp:379-382` and `string.hpp:384-387`
// verbatim - `internal_compare(begin(), end(), other.begin(), other.end())` and
// `compare(other) == 0` - and the `cntlzw`/`srwi` pair at 0x8016BEB8 is the compiler's own code
// for `== 0` (the same two instructions as the twin below).
//
// **Both twins are exact, and they are already `Matching` units in this tree** - measured
// instruction by instruction, not by percentage:
//
//   fn_8016BEA8 = `__eq__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>CFRCQ24rstl66basic_string<c,...>`
//                 (`symbols.txt:1393`, 0x8004985C, 0x28) in `MetroidPrime/CIOWinManager.cpp`
//                 (`Matching`, 19/19): these ten instructions word for word, with
//                 `bl compare__Q24rstl...` where this `bl` is.  The twin's source is the same
//                 `string.hpp:382-385` body, instantiated with `char_traits<char>`.
//   fn_8016BED0 = `compare__Q24rstl66basic_string<c,...>CFRCQ24rstl66basic_string<c,...>`
//                 (`symbols.txt:596`, 0x80021074, 0x7C) in `MetroidPrime/CCredits.cpp`
//                 (`MatchingFor("G2ME01")`, 49/49): these 31 instructions word for word - the same
//                 operand schedule, the same `li r7,0`, the same three `addi` into r5/r6/r4/r3 and
//                 all sixteen `stw` displacements - with
//                 `bl internal_compare<rstl::const_linear_iterator<...>>` where this `bl` is.
//                 `string.hpp:379-382` is that twin's source too.
//
// **`fn_8016BF4C` is `istring`'s own `internal_compare`**, 0x8016BF4C, 0x228 (`symbols.txt:6007`),
// and it is *not* the twin's callee: the twin's is `internal_compare<...>` at 0x800210F0 and is
// 0x114 = 276 bytes, because `char_traits<char>::compare` is a plain subtraction
// (`string.hpp:55-58`) - measured, that body has two `extsb` and no range tests at all, where
// `fn_8016BF4C` has eight `extsb`, twelve `cmpwi` and six `subi`, folding both sides of every
// comparison through `['a','z']` / `[0xE0,0xFE]` / `[0x30A0,0x30FF]` before subtracting.  That is
// `case_insensitive_char_traits<char>::compare` = `lower(lhs) - lower(rhs)` (`string.hpp:86-96`),
// i.e. the template parameter of the same `internal_compare` is instantiated twice and retail
// emitted both out of line.  Declared, never defined here; see the stand-in in
// `src/MetroidPrime/PortLinkStubs.cpp`.
//
// **The eight stack words at +0x08..+0x47 are four arguments each stored twice, and both copies
// are load-bearing.**  `internal_compare` takes its four iterators **by value**, and MWCC passes a
// class-type by-value argument as a pointer to a caller-side temporary - so each argument needs a
// temporary for the `begin()`/`end()` result *and* one for the parameter, 16 bytes apart, and the
// four argument registers are the second of each pair (`addi r3,r1,0x40`, `addi r4,r1,0x30`,
// `addi r5,r1,0x20`, `addi r6,r1,0x10`).  Measured: naming the four iterators in locals and
// passing them (`It f = self->begin(); ... fn(f, l, of, ol);`) is byte-exact for the *length* but
// gives a different frame - the parameter copies land at +0x08..+0x27 and the argument registers
// become `+0x20/+0x18/+0x10/+0x08` - so the call is written the way `string.hpp:379-382` writes
// it, as the call itself.  Same measurement and same spelling as
// `src/MetroidPrime/Carve8000432C.cpp:52-61` records for its own two arguments.
//
// **`extern "C"` is what keeps the two `fn_` names unmangled** - `symbols.txt:6005-6006` carries
// those placeholders verbatim and a C++ definition would mangle to `_Z<len>fn_<addr>...` so
// objdiff would pair nothing - so the unit is a `.cpp` rather than a `.c`.  It cannot be a `.c`:
// the pass-by-value class argument above is what allocates those eight words, and `-lang=c` gives
// POD structs a different frame (measured with `mwcceppc.exe` and the DOL's own flags: 86 of 124
// bytes differ).  `extern "C"` is the same trade `MetroidPrime/Carve8000432C.cpp` makes for its
// three `fn_` names, and its `pointer_iterator` is the same local-template device this file's
// `const_linear_iterator` is.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `tools/check_decl_order.py --unit
// main/MetroidPrime/Carve8016BEA8` is the cheap check.
//
// Its own unit because a claim may not span an unclaimed gap and may not sit on a unit boundary
// that a neighbouring `Matching` unit's `.text` ends on: the range is two of the six functions in
// the unclaimed run 0x8016BDEC..0x8016C230, and the claim below it is
// `MetroidPrime/CInGameTweakManagerReadFromMemoryCard.cpp` (0x8016BDE4..0x8016BDEC,
// `splits.txt:1008`) while the one above is `MetroidPrime/CInGameTweakManagerCtor.cpp`
// (0x8016C230..0x8016C244, `splits.txt:1011`); the claim starts 0xBC bytes into the `auto_*` unit,
// which is what keeps `dtk dol split` from reporting a link-order cycle.  The two functions left
// unclaimed are `GetTweakValue` and `HasTweakValue` (both named, so each needs its own `.cpp`) and
// `fn_8016BF4C`, the callee documented above.  The directory is retail's own, taken from those
// neighbours.

struct S8016BEA8;

namespace rstl {

/** `include/rstl/linear_iterator.hpp`'s `const_linear_iterator`, cut to the two members this
 *  file's bytes read.  `rstl::basic_string`'s `const_iterator` is this with `T = char`,
 *  `Container = basic_string` and `Alloc = rmemory_allocator`; in the callee at 0x8016BF4C,
 *  `lwz r9,0x0(r5)` then `lbzx r12,r12,r5` is `(*mOwner)[mIndex]` with `mOwner` at +0x00 and
 *  `mIndex` at +0x04, and that same function's `cmplw r7,r0` / `lwz r3,0x4(r4)` / `cmpw r8,r3`
 *  tail is `mOwner != other.mOwner || mIndex != other.mIndex`.  The constructor is kept because
 *  the pass-by-value class argument above is what allocates the eight stack words. */
template < class T, class Container, class Alloc >
class const_linear_iterator {
public:
  const_linear_iterator(const Container* owner, int index) : mOwner(owner), mIndex(index) {}

private:
  const Container* mOwner;
  int mIndex;
};

} // namespace rstl

/** Retail's `rstl::istring` as far as these two functions read it: `mPtr` at +0x00 and `mSize` at
 *  +0x08 are the only two words either body or its callee touches, and +0x04 (`mCow` in
 *  `include/rstl/string.hpp:107-109`) is left as the word between them, as
 *  `src/MetroidPrime/Carve8000432C.cpp` leaves the words its bytes skip.  `begin()` and `end()`
 *  are `string.hpp:212-213` verbatim. */
struct S8016BEA8 {
  typedef rstl::const_linear_iterator< char, S8016BEA8, int > const_iterator;

  const char* mPtr;
  int x04;
  unsigned int mSize;

  unsigned int size() const { return mSize; }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, static_cast< int >(size())); }
};

typedef S8016BEA8::const_iterator It8016BEA8;

/** 0x8016BF4C, `symbols.txt:6007`, 0x228: `istring`'s out-of-line `internal_compare`, the callee
 *  both functions below reach.  It sits in the unclaimed gap above this claim, so no unit owns it
 *  and the DOL link resolves it from dtk's `auto_03_8016BDEC_text` object; declared here with
 *  retail's own name, never defined here. */
extern "C" int fn_8016BF4C(It8016BEA8 first, It8016BEA8 last, It8016BEA8 otherFirst,
                           It8016BEA8 otherLast);

/** `fn_8016BED0` - retail `.text:0x8016BED0`, 0x7C = 124 bytes: `istring::compare`, the body of
 *  `include/rstl/string.hpp:379-382`.  The call is written as the call; see the header for the
 *  measurement behind that and for why the callee is named here and not defined here. */
extern "C" int fn_8016BED0(const S8016BEA8* self, const S8016BEA8* other) {
  return fn_8016BF4C(self->begin(), self->end(), other->begin(), other->end());
}

/** `fn_8016BEA8` - retail `.text:0x8016BEA8`, 0x28 = 40 bytes: `istring::operator==`, the body of
 *  `include/rstl/string.hpp:384-387`, whose `cntlzw`/`srwi` is the compiler's `== 0`. */
extern "C" bool fn_8016BEA8(const S8016BEA8* self, const S8016BEA8* other) {
  return fn_8016BED0(self, other) == 0;
}