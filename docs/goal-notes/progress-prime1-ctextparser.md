# progress-prime1-ctextparser

`kind: progress`, target `Kyoto/Text/CTextParser` (main DOL unit `main/Kyoto/Text/CTextParser`).
Only `src/Kyoto/Text/CTextParser.cpp` changed. The unit stays `NonMatching` (8/15 -> **12/15**
matched functions; `All:` 9804 -> 9808 / 28465).

## Result per function (objdiff `build/report.json`, from `main/Kyoto/Text/CTextParser`)

| function | before | after | Prime 1's source |
|---|---|---|---|
| `GetColorValue__11CTextParserFPCw` (68 B) | 57.76% | **100%** | matched **unchanged** (one-operand order) |
| `ParseInt__11CTextParserFPCwib` (272 B) | 49.35% | **100%** | matched **unchanged** (only the parameter name differs) |
| `GetAssetIdFromString__11CTextParserF...` (352 B) | 92.69% | **100%** | matched **unchanged** except `int id` instead of `const CAssetId id` + `static_cast<CAssetId>(id)` |
| `ParseTag__11CTextParserFR18CTextExecuteBufferPCwiPC...` (1812 B) | 0.22% | **100%** | matched **unchanged** plus Echoes' extra `character-extra-space=` tag |
| `GetImage__11CTextParserFPCwiPC...` (1460 B) | 5.96% | 5.96% (unchanged) | **blocked**, see below |
| `fn_802BA70C` (212 B) | 0.00% | 0.00% | not retail-named; see below |
| `fn_802BA68C` (128 B) | 0.00% | 0.00% | not retail-named; see below |

Every one of the four matches is exact: `tools/try_batch.py` reports `*** MATCH ***` (zero
differing instructions against `build/G2ME01/obj/Kyoto/Text/CTextParser.o`), not just a
percentage.

### The three edits that mattered

1. **`GetColorValue`** - this repo had `(FromHex(str[0]) << 4) + FromHex(str[1])`; Prime 1 has
   `FromHex(str[1]) + (FromHex(str[0]) << 4)`. mwcceppc 2.7 evaluates the right operand of `+`
   first, so only Prime 1's operand order makes it call `FromHex(str[0])` first, which is what
   retail does. Spellings tried: Prime 1's order (**MATCH**), the current order, `|`, explicit
   `int lo`/`int hi` locals in both orders, `hi + (FromHex(str[0]) << 4)`. All the others leave
   6 differing instructions.
2. **`ParseInt`** - Prime 1's `while (len > procCur) { val *= 10; wchar_t ch = str[procCur];
   val += ch - L'0'; ++procCur; }` is what makes the compiler unroll 8x (`srwi. r0,r3,3` /
   `mtctr` / `bdnz` at 0x802b94c8). A `for` loop with the identical body also matches, but a
   `do/while` does not (59 differing instructions - it cannot be trip-counted). Only the
   parameter name differs from this repo's header (`signVal` -> `allowSign`).
3. **`GetAssetIdFromString`** - the 23 differing instructions were entirely register allocation
   (`r30`/`r31` swapped) caused by declaring the accumulator `const CAssetId id` with
   `static_cast<uint>(GetColorValue(...))` on the first term. Prime 1's plain `int id` with
   `static_cast<CAssetId>(id)` at the `binary_find` call is byte-identical. This is a real
   finding: **`int` is not the same codegen as `CAssetId` here even though the return type is
   `CAssetId`.**
4. **`ParseTag`** - Prime 1's body verbatim, with two Echoes deltas read out of the retail
   disassembly, not guessed:
   - Echoes has a tag Prime 1 does not: `character-extra-space=` at string-pool offset 214
     (base 0x803b9dc8), `ParseInt(str + 22, len - 22, true)` then
     `CTextExecuteBuffer::AddCharacterExtraSpace(int)` (0x802b99d0-0x802b99e8). It sits
     **between** `line-extra-space=` and `just=`. Dropping it costs 33 differing instructions.
   - `geometry-color=` really does parse at `+11`/`-11` (0x802b97e8) although the tag is 14
     characters. That is a **retail bug**, and Prime 1's source has the identical bug, so
     Prime 1's spelling is the correct one to keep. Worth a comment in the source.
   - `line-spacing=` is Prime 1's `v / 100.f`. `-(float)ParseInt(...) / 100.f` is 1 instruction
     away (an extra `fneg`) and `ParseInt(...) / -100.f` also matches; keep `v / 100.f`.
   - `EColorType` constants come out as `3` (fg), `0` (main), `1` (outline) - this repo's
     `TextCommon.hpp` enumerates them exactly as Prime 1's, so no edit was needed there. The
     new include of `Kyoto/Text/TextCommon.hpp` is the only header-side change; without it
     `kCT_Foreground` / `kJustification_*` do not resolve.
   - `AddColor(EColorType, float, float, float, float)` (Prime 1's convenience overload) is
     **not** in Echoes's `CTextExecuteBuffer` and is not referenced by retail - the 2-argument
     `AddColor(EColorType, const CTextColor&)` is.

## GetImage is blocked, and the blocker is measured

Prime 1's body is structurally right (I checked every block of 0x802b9d58-0x802ba30c against
it), but it cannot be spelled in this tree. Prime 1 writes the tag test as

```cpp
rstl::operator==(rstl::istring(tokens[0].c_str()), rstl::istring_l("A"))
```

**`rstl::istring` does not exist in this repo.** `include/rstl/string.hpp` has `char_traits`,
`literal_t`, `string_l` and `wstring_l` and nothing else; `grep -rn case_insensitive include/
src/` returns nothing. Retail's three callees are all **UNCLAIMED** DOL ranges:

- `fn_802FDC4C` (0x802FDC4C) - `basic_string` ctor from `(const char*, int = -1, allocator)`
  with the case-insensitive traits
- `fn_802FF3AC` (0x802FF3AC) - `istring_l`; `src/rstl/rstl_string_l.cpp`'s own header comment
  already records that these 48 bytes are **byte-identical to `string_l`**, and that unit's claim
  (0x802FF3DC..0x802FF448) starts 48 bytes later
- `fn_8016BEA8` (0x8016BEA8) - `operator==(const istring&, const istring&)`; it tail-calls
  `fn_8016BED0`, the real case-insensitive compare

Measured with the substitute that *is* available here,
`CStringExtras::CompareCaseInsensitive(tokens[0].c_str(), "A") == 0`: **391 differing
instructions** out of ~365, versus 0 for the four functions above. The difference is not the
body - it is that the whole prologue moves (frame `-432` -> `-448`, `r29/r30/r31` renumbered)
because the call shapes are different, and every literal comparison becomes a different
`bl`. So the percentage is not the interesting number; the missing type is.

`rstl::istring` is the general prerequisite, and it lands outside this unit, so it is a
separate carve (see `NEW:` below) rather than something to force into this diff.

### `fn_802BA70C` / `fn_802BA68C` are not a blocker

They read as two dead functions, but `tools/unit_fit.sh Kyoto/Text/CTextParser.cpp` shows what
retail's unnamed pair actually is - same sizes to the byte:

- `+212  reserve__Q24rstl54vector<17TToken<8CTexture>,...Fi`  <- `fn_802BA70C` is 212 bytes
- `+128  lower_bound<...pair_sorter_finder...>`                <- `fn_802BA68C` is 128 bytes

They are template instantiations dtk left unnamed. `unit_fit` lists them under "extra" only
because it pairs symbols by name, so their 0.00% is a naming artefact, not missing work. A run
that tries to "add" them will waste time.

## Gate

`./tools/gate.sh build/goal/judge/report.base.json` (after a full `./tools/decomp_build.sh`):

```
ninja + build.sha1          ok      main.dol = 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
hashes vs config.yml        ok      (all 86 RELs)
per-function diff           matched 9804 -> 9808  linked 4895 -> 4895
                                   (+4 functions at 100%, 0 units newly linked)
module wiring / dol_read / gs offsets / raw offsets / decl order /
files.cmake / module order / port probe / port link gap   ok
```

`python3 tools/check_symbol_names.py` -> `checked 503 units; 0 declared names are missing`.
`python3 tools/check_decl_order.py --unit Kyoto/Text/CTextParser` -> ok.

The only red line is `docs claims`, and it is the derived-count check alone:

```
missing: 'matched    9808 / 28465 functions'  (HANDOFF state block: total matched)
missing: 'DOL units  8397 / 16726 functions'  (HANDOFF state block: DOL matched)
```

Those two figures moved because of this change; per the brief the judge rewrites them from the
tree, and `docs/HANDOFF.md` is not mine to edit.

## Method note for the next run

`tools/try_batch.py` is what made this item cheap: each variant is a body-only replacement, one
object rebuild, and a count of **differing instructions** rather than a byte percentage. Three
of the four fixes were one attempt each, and the three that did not match showed up as 6, 23 and
33 instructions - small enough to read the diff and see the cause. A percentage would have said
92.69% for a function one register swap from 100%. The parameter-name mismatch
(`signVal`/`allowSign`, `string`/`str`, `vec`/`textureMap`) shows up only as a bare
`BUILD FAIL` with no message, because `try_batch` prints only the last 4 lines of ninja's
**stderr** while mwcceppc writes to stdout; that cost one round trip and is worth fixing in the
tool rather than rediscovering.

NEW: progress-rstl-istring-carve | match | rstl/rstl_string_l | define rstl::istring (basic_string<char, case_insensitive_char_traits<char>>), istring_l at 0x802FF3AC and operator==(istring,istring) at 0x8016BEA8, all three currently UNCLAIMED; unlocks CTextParser::GetImage (1460 B, 5.96%), whose only remaining blocker is this type
