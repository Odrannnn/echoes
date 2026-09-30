# progress-rstl-istring-carve - `rstl::istring`, `istring_l` and the 0x802FF3AC carve

`kind: match`, target `rstl/rstl_string_l`. **PASS**, committed as a complete match:
`tools/flip_test.sh rstl/rstl_string_l.cpp` -> `PASS -> kept as Matching`, and
`tools/goal_check.sh` exits 0.

## What I measured first (nothing here was recalled)

    python3 tools/range_owner.py .text 0x802FF3AC 0x802FF3DC   -> UNCLAIMED
    python3 tools/range_owner.py .text 0x8016BEA8 0x8016BF00   -> UNCLAIMED
    build/report.json, main/rstl/rstl_string_l  -> 108 bytes, 2/2 functions, complete
    report.json totals                        -> 10029 / 28465 matched, 4896 linked

`./tools/dis.sh 0x802FF3AC 0x30` is byte-identical to `rstl::string_l`:
`stw r4,0(r3) ; li r0,0 ; mr r5,r4 ; stw r0,4(r3) ; loop: addi r5,r5,1 ; lbz ; extsb. ; bne ;
subf r0,r4,r5 ; stw r0,8(r3) ; blr` - 48 bytes. So it is the `basic_string(literal_t, const char*)`
constructor again, on `basic_string<char, case_insensitive_char_traits<char> >`.

`0x8016BEA8` is a separate 40-byte function (a `stwu`/`mflr`/`bl 8016BED0`/`cntlzw`/`srwi r3,r0,5`
tail) that calls `fn_8016BED0`; it is not part of this unit and is **not** claimed here - see
"Left out" below.

## The change (three files)

* `include/rstl/string.hpp` - `case_insensitive_char_traits<_CharTp>` (primary inherits
  `char_traits`, `char` specialisation with `lower`/`eq`/`compare`), the
  `typedef basic_string<char, case_insensitive_char_traits<char> > istring;`, and the declaration
  `extern "C" istring fn_802FF3AC(const char*)` plus an inline `istring_l` spelling of it.
* `src/rstl/rstl_string_l.cpp` - the third definition, `extern "C" rstl::istring rstl::fn_802FF3AC`,
  declared last (reverse order, below).
* `config/G2ME01/splits.txt` - `rstl/rstl_string_l.cpp`'s `.text` claim `0x802FF3DC..0x802FF448` ->
  `0x802FF3AC..0x802FF448`. 156 bytes. `fn_802FF448` (the four `mNull` initialisers) stays
  unclaimed, as before.

The claim now matches the retail object exactly, which I checked directly:

    build/binutils/powerpc-eabi-nm -n build/G2ME01/obj/rstl/rstl_string_l.o
      00000000 T fn_802FF3AC
      00000030 T wstring_l__4rstlFPCw
      0000006c T string_l__4rstlFPCc

and ours is identical, so objdiff can pair all three and the unit is `3/3`.

## The one non-obvious thing: the name has to be `fn_802FF3AC`, not `istring_l`

A C++ definition of `rstl::istring_l` mangles to `Z<len>istring_l...`, and that **breaks the
link**: retail's `CTextParser.o` references the placeholder `fn_802FF3AC` by name, and while the
range was unclaimed `dtk` supplied it from `auto_*`. Claiming the address removes that definition.
First attempt failed with:

    ### mwldeppc.exe Linker Error:
    #   undefined: 'fn_802FF3AC'
    #   Referenced from 'CTextParser::GetImage(...)' in CTextParser.o

So the definition is `extern "C"` and reproduces `fn_802FF3AC` verbatim; `istring_l` is an inline
forwarder for readable source. This is the same trap `src/rstl/Carve800239F4.c` documents for its
own anonymous functions ("the definitions have to stay C: a C++ one would mangle"), solved from
C++ instead of by dropping to a `.c` file - which was not available here because the return type is
a class template instantiation.

`tools/check_symbol_names.py` -> `503 units; 0 declared names are missing`, so the C++ spelling
cost nothing at the symbol-name gate.

## Verification

    sha1sum build/G2ME01/main.dol                  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    ./tools/decomp_build.sh                        All: 30.86% fuzzy, 23.15% matched, 11.74% linked (10030 / 28465)
    ./tools/flip_test.sh rstl/rstl_string_l.cpp    PASS -> kept as Matching
    ./tools/unit_fit.sh rstl/rstl_string_l.cpp     .text claimed 156 ours 156 retail 156 fits
                                                   no extra functions
    python3 tools/check_decl_order.py --unit rstl/rstl_string_l    ok (no permutation)
    python3 tools/check_symbol_names.py            0 missing names
    ./tools/probe_sources.sh                       749 files, 0 failed, 250 undefined (unchanged)
    python3 tools/check_docs_claims.py             clean after the state block was re-derived
    ./tools/goal_check.sh build/goal/item.json     PASS

`total_functions` is still **28465** after the `splits.txt` edit, and all 86 RELs are `cmp`-equal
(`goal_check`'s `gate.sh` runs them).

## What is still open (measured, not guessed)

**`fn_8016BEA8` / `fn_8016BED0` are not in this diff.** `operator==(istring, istring)` needs its own
unit: `0x8016BEA8..0x8016C230` is an unclaimed `.text` gap about 24 KB away from anything in
`rstl/`, between `MetroidPrime/Player/Carve8016BDE4.c` (ends 0x8016BDEC) and
`MetroidPrime/CInGameTweakManagerCtor.cpp` (starts 0x8016C230), so it cannot extend
`rstl/rstl_string_l.cpp`'s single `.text` claim. Its body is one instruction sequence -
`bl fn_8016BED0 ; cntlzw r0,r3 ; srwi r3,r0,5 ; blr` - so it is the natural next carve once
`compare` is written.

This does not block `CTextParser::GetImage`'s *type* work: `istring` now exists, and `GetImage`
calls `fn_802FF3AC` by the same name retail does, so the only thing missing for a real
`GetImage` is the case-insensitive comparison itself.

NEW: rstl-istring-compare | progress | rstl/rstl_istring_compare | carve the unclaimed .text gap 0x8016BEA8..0x8016C230 for istring's operator== (fn_8016BEA8) and compare (fn_8016BED0); it cannot extend rstl/rstl_string_l.cpp's claim because the range is 24 KB away