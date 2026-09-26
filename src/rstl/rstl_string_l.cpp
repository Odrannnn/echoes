/**
 * `rstl::string_l` and `rstl::wstring_l` - retail's two "from a literal" string constructors.
 *
 * **0x802FF3DC, 0x6C = 108 bytes, `Matching`.** Both are declared in `include/rstl/string.hpp`
 * and *used* from fifteen places (`src/MetroidPrime/main.cpp` alone calls `string_l` eight times),
 * but nothing in this tree defines them: the DOL links today only because `dtk` still supplies
 * retail's own copies out of an `auto_*` object. `tools/range_owner.py` said
 * `.text 0x802FF3DC..0x802FF448` was UNCLAIMED - it is the 108 bytes between the end of
 * `rstl/rstl_strings.cpp` (0x802FF3AC) and the start of `rstl/RstlExtras.cpp` (0x802FF4BC) - so
 * this unit claims exactly that range and the `auto_*` object that used to hold it is shortened
 * at both ends.
 *
 * ## The body is `basic_string`'s `literal_t` constructor, expanded
 *
 * Neither function copies: it points `x0_ptr` straight at the caller's literal, sets `x4_cow` to
 * null and measures the length with a scan, which is what the `literal_t` constructor in
 * `include/rstl/string.hpp` does. The two are 48 and 60 bytes and one instruction sequence each:
 *
 * ```
 *   string_l   stw r4,0(r3) ; li r0,0 ; mr r5,r4 ; stw r0,4(r3)
 *              loop: addi r5,r5,1 ; lbz r0,0(r5) ; extsb. r0,r0 ; bne loop
 *              subf r0,r4,r5 ; stw r0,8(r3) ; blr
 *
 *   wstring_l  ... lhz r0,0(r5) ; cmplwi r0,0 ; bne loop
 *              subf r4,r4,r5 ; srwi r0,r4,31 ; add r0,r0,r4 ; srawi r0,r0,1 ; stw r0,8(r3)
 * ```
 *
 * `rstl::string` is returned **in memory** (twelve bytes, so above the Itanium ABI's eight-byte
 * threshold), which is why r3 is the caller's return slot and r4 the literal - the same registers a
 * copy constructor would take, and the reason `basic_string(literal_t, ...)` expands in place here
 * instead of being called.
 *
 * The `wchar_t` version's `(len + 1) >> 1 << 1` is not written by hand: it is MWCC's own narrowing
 * of `x8_size` to the element count of the *byte* difference, and it comes out of
 * `static_cast< uint >(it - data)` on a `wchar_t*` with the same source as the `char` version.
 *
 * `fn_802FF3AC`, the 48 bytes immediately below `wstring_l`, is **byte-identical** to `string_l`,
 * and `fn_802FF448`, the 116 bytes above it, is `basic_string`'s four `mNull` initialisers. Neither
 * is claimed here: a unit may hold one discontiguous range per section, and leaving the two
 * unclaimed keeps this unit's object to exactly the 108 bytes retail has at 0x802FF3DC.
 */

#include "rstl/string.hpp"

rstl::string rstl::string_l(const char* data) {
  return rstl::string(rstl::string::literal_t(), data);
}

rstl::wstring rstl::wstring_l(const wchar_t* data) {
  return rstl::wstring(rstl::wstring::literal_t(), data);
}
