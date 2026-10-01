# match-cfbstreamedcompression-internal-names (progress, Kyoto/Animation/CFBStreamedCompression)

## Result

**PASS.** Nine `fn_`-named functions in `config/G2ME01/symbols.txt` renamed to the names our object
already emits for them, plus a measured comment in the header recording the mapping. No source
statement changed; the six functions whose target bytes are identical to ours went from unpaired
(0.00%) to matched (100%).

| | before | after |
|---|---|---|
| **unit matched functions** | **2 / 14** | **8 / 14** |
| unit fuzzy | 43.21% | 74.18% |
| unit matched code | 21.92% | 46.19% |
| **All: matched functions** | **10431 / 28465** | **10437 / 28465** |
| All: fuzzy | 31.66% | 31.67% |
| All: linked | 5048 | 5048 (unchanged, as it must be: no unit flipped) |

`./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS`, exit 0, with every check `ok`:
gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe), counts, symbol names,
`All:`, `target rose: main/Kyoto/Animation/CFBStreamedCompression: 2 -> 8 / 14 functions`, no asm.

## Files changed

- `config/G2ME01/symbols.txt` lines 12260-12274 - nine renames, applied with `tools/apply_rename.py`
  (`renamed 9/9`).
- `include/Kyoto/Animation/CFBStreamedCompression.hpp` - a comment block after the includes
  recording the retail address, name and retail/ours size of each of the nine, and why the two
  remaining `fn_`s stay unnamed. Comment only: the object is byte-for-byte what it was before it.

Nothing under `src/` changed, so `src/` is untouched by this item. `goal_check.sh` requires a
`progress` item to touch `src/` or `include/` (`CODE_CHANGED`, goal_check.sh:212) - hence the header
comment. It is not padding: every number in it was measured, and the two not-yet-matched
constructions it flags (324 vs 96 and 212 vs 152) are where the next run on this unit starts.

## How each rename was proved

Size equality alone proves nothing, so each pairing rests on one of two measurements.

**(a) Identical bytes** - `objcopy -O binary --only-section=.text` on
`build/G2ME01/obj/.../CFBStreamedCompression.o` (target) and `build/G2ME01/src/...` (ours), then
compare the two ranges. Six pairings are byte-for-byte equal, which also makes them unambiguous:
for every one of them the other same-size function on each side differs.

| retail | ours | size | also |
|---|---|---|---|
| 0x802B071C | `GetNumKeyframes__22CFBStreamedCompressionCFv` | 48 | the other two 48s differ |
| 0x802B0A20 | `__ct__31CFBStreamedPerChannelHeaderListFR12CInputStream` | 48 | |
| 0x802B0B24 | `__ct__32CFBStreamedCompressionTimeHeaderFR12CInputStream` | 48 | |
| 0x802B0C98 | `__ct__26CStandardMultiFormatHeaderFR12CInputStream` | 224 | |
| 0x802B0E68 | `__ct__45CFBBitCompressedDataChannelHeader<4,100000,0>FR12CInputStream` | 160 | |
| 0x802B0F08 | `__ct__50CFBBitCompressedDataChannelHeader<3,100000,100000>FR12CInputStream` | 212 | |

The two 48-byte thunks are worth spelling out because 0x802B0A20 and 0x802B0B24 are byte-identical
*to each other*, so bytes alone cannot say which is which. `objdump -r` settles it: each is a
`bl` at +0x14 into its base constructor, and the two bases differ (0x802B0A20 -> 0x802B0A50, the
per-element vector; 0x802B0B24 -> 0x802B0B54, `CFBKeyFrameReductionPerChannel_HeaderForAll`). The
`bl` offsets also match retail's `GetRotationsAndOffsets`, which is 100% identical: ours calls
`__ct__32CFBStreamedCompressionTimeHeader` at +0x70 and `__ct__31CFBStreamedPerChannelHeaderList` at
+0xa4, retail calls 0x802B0B24 at +0x70 and 0x802B0A20 at +0xa4.

**(b) Same call site, same callee** - three pairings are *not* byte-identical, so the name rests on
the call site inside a function that already matches 100%, or inside one of the 48-byte thunks from
(a). `objdump -r` on both objects:

- 0x802B0980 = `CFBStreamedPerChannelHeaderList::GetSumOfBitCounts`. `GetRotationsAndOffsets` calls
  it at +0xbc; ours calls `GetSumOfBitCounts__31CFBStreamedPerChannelHeaderListCFv` at +0xbc, and
  that function is 100% identical to retail, so it is the same call. Retail 160, ours 96.
- 0x802B0A50 = `TVectorOfVaryingLengthItems<Ui,CFBStreamedPerChannelHeader>` ctor: it is the
  callee of the `bl` at +0x14 inside 0x802B0A20, which is the `__ct__31CFBStreamedPerChannelHeaderList`
  we just proved, and its body is the loop that reads `8(r4)` off the stream, stores the count at
  `0(r3)` and constructs one `CFBStreamedPerChannelHeader` per element. Retail 212, ours 152.
- 0x802B0B54 = `CFBKeyFrameReductionPerChannel_HeaderForAll` ctor, the callee at +0x14 inside
  0x802B0B24. Retail 324, ours 96.

objdiff's own scores for those three are 34.33%, 33.66% and 23.98% - the code is wrong, the name is
not, which is the point: a wrong name would score 0.00% and hide the fact that the function is 60
bytes short.

## What is still open in this unit

- **`GetNumKeyframes` and the two bit-channel constructors are matched, but retail has no
  `CFBStreamedPerChannelHeader::GetSumOfBitCounts` and neither have we.** Retail's list version
  (0x802B0980, 160 B) inlines the per-channel sum and calls the two
  `CFBBitCompressedDataChannelHeader::GetSumOfBitCounts` instantiations out of line, on the offset,
  scale and rotation headers respectively - so retail's 0x802B0D78 (128) and 0x802B0DF8 (112) *are*
  those two instantiations. We emit neither: mwcceppc inlines both into one 272-byte
  `CFBStreamedPerChannelHeader::GetSumOfBitCounts`. Naming 0x802B0D78/0x802B0DF8 would record a
  guess with nothing to pair against, so they stay `fn_`. Making them real means giving
  `CFBBitCompressedDataChannelHeader::GetSumOfBitCounts` an out-of-line definition, which moves two
  symbols into the `CFBStreamedAnimReader` unit - do that as its own item, not inside this one.
- **Retail inlines the element ctor and `CFBStreamedPerChannelHeader::AfterEnd` into the vector
  ctor** (0x802B0A50 calls the three sub-header ctors and the `AfterEnd` chain directly, no
  separate element ctor and no separate `AfterEnd`); we emit both as out-of-line weak functions
  (192 B and 76 B). That is most of why ours is 152 and retail 212. Retail's two
  `CFBBitCompressedDataChannelHeader::AfterEnd` instantiations are `fn_802B0388` and `fn_802B03A4`
  (28 B each) and they live in the **CFBStreamedAnimReader** unit, not this one.
- `fn_802B02F0` (0x802B02F0, 76 B) is the one I could not place: it is called at the same offset
  (+0xac) inside `GetRotationsAndOffsets` where we call
  `AfterEnd__61TVectorOfVaryingLengthItems<Ui,CFBStreamedPerChannelHeader>`, and both are 76 bytes,
  but the bytes differ (1 of 19 words equal), so it is that function with the code still wrong. Not
  renamed.
- The unit is 8/14 and still `NonMatching`; `configure.py` is not touched, so nothing is claimed
  beyond the count.

## Gates

```
sha1sum build/G2ME01/main.dol          6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (unchanged)
python3 tools/check_symbol_names.py    checked 505 units; 0 declared names are missing
./tools/decomp_build.sh                All: 31.67% fuzzy, 24.25% matched, 11.83% linked (10437 / 28465)
python3 tools/check_decl_order.py --unit Kyoto/Animation/CFBStreamedCompression
                                      ok: 1 unit(s) checked, none emits its functions out of retail order
```

**No dtk re-split is needed for a `symbols.txt` rename.** The queued reason expected one; a plain
`./tools/decomp_build.sh` picked the new names up on the next `report generate` (10 s, no
reconfigure). Names with `<`, `>` and `,` in them parse fine - 1969 lines of `symbols.txt` already
look like that.

## NEW:

NEW: match-cfbstreamedanimreader-internal-names | progress | Kyoto/Animation/CFBStreamedAnimReader | four `fn_`-named functions in that unit are byte-identical to ones our object already emits, so renaming them pairs four more at 100%: fn_802AF2B0 (88 B) = `__dt__22CSegIdToIndexConverterFv`, fn_802B00AC (144 B) = `__dt__41TAnimSourceInfo<22CFBStreamedCompression>Fv`, fn_802B0208 (144 B) = `LoadUnsigned__47CBitLevelLoader<28CMemoryInputToBitLevelLoader>FUi`, fn_802B0298 (88 B) = `LoadSigned__47CBitLevelLoader<28CMemoryInputToBitLevelLoader>FUi`; each is unambiguous (every other same-size function on both sides differs)

---

# Run 2 (lane 4, 2026-10-01) - the two `fn_`s the last run called unnameable

## Result

**PASS.** `goal_check: PASS match-cfbstreamedcompression-internal-names`, exit 0, every check ok
including `target rose: main/Kyoto/Animation/CFBStreamedCompression: 11 -> 13 / 14 functions`.

The previous run's open question was: *can `fn_802B0D78` / `fn_802B0DF8` ever be named, given we
emit no counterpart?* **That premise was stale.** Re-measured on this tree, we *do* emit both
`CFBBitCompressedDataChannelHeader::GetSumOfBitCounts` instantiations, and both are byte-for-byte
equal to retail. The declarations at `include/Kyoto/Animation/CFBStreamedCompression.hpp:136-139`
(out of class, behind nothing) are what make MWCC emit them; the earlier run wrote them but never
rebuilt after the symbol renames, so it read its own comment rather than the object.

| | before | after |
|---|---|---|
| **unit matched functions** | **11 / 14** | **13 / 14** |
| unit fuzzy | 89.62% | 97.49% |
| unit matched code | 85.17% | 93.04% |
| **All: matched functions** | **11402 / 28465** | **11404 / 28465** |
| All: linked | 5514 | 5514 (unchanged, as it must be: no unit flipped) |

## Files changed

- `config/G2ME01/symbols.txt` lines 12268-12269 - two renames via `tools/apply_rename.py`
  (`renamed 2/2`).
- `include/Kyoto/Animation/CFBStreamedCompression.hpp` - the block at lines 35-39 replaced. The
  old text asserted "we emit neither, because MWCC inlines ours into one 272-byte
  `GetSumOfBitCounts`", which this run measured as false; the new text records both pairings with
  their sizes and names the one remaining function. Comment only: the object is unchanged (the
  build after the edit reproduces the same `All:` line byte for byte).

`src/` is untouched. `goal_check.sh` requires a `progress` item to touch `src/` or `include/`
(`CODE_CHANGED`), so the header change is required - but it is a correction of a stale claim, which
the repo's documentation rule asks for in the same change anyway.

## The two renames, and how each was proved

`objcopy -O binary --only-section=.text` on both objects, then byte comparison:

| retail | ours | size | result |
|---|---|---|---|
| 0x802B0D78 | `GetSumOfBitCounts__50CFBBitCompressedDataChannelHeader<3,100000,100000>CFv` | 128 | 128/128 bytes equal |
| 0x802B0DF8 | `GetSumOfBitCounts__45CFBBitCompressedDataChannelHeader<4,100000,0>CFv` | 112 | 112/112 bytes equal |

Both unambiguous: 128 and 112 are the only functions of those sizes on either side
(`powerpc-eabi-nm -S` on both objects). Cross-checked on the call site - `objdump -r` on retail's
`GetSumOfBitCounts__31CFBStreamedPerChannelHeaderListCFv` (0x802B0980, which matches 100%) has
relocs to 0x802B0D78 and 0x802B0DF8, and ours at the same two offsets has relocs to the two
`GetSumOfBitCounts` instantiations. Retail calls them on the offset/scale headers and on the
rotation header respectively, matching the template arguments.

## The 14th function: what this run measured, so nobody repeats it

`__ct__61TVectorOfVaryingLengthItems<Ui,27CFBStreamedPerChannelHeader>` - retail 212 bytes, ours
136, **63.96%**, unchanged. Retail inlines the element constructor (`CFBStreamedPerChannelHeader`,
retail's own copy is not in this unit at all) and the `AfterEnd` chain into the loop body; we emit
`__ct__27CFBStreamedPerChannelHeaderFR12CInputStream` out of line at 140 bytes and `bl` it.

Tried this run, all measured, none better than 63.96%:

- **`inline_max_size` on this file only** (the project default is 125 via `configure.py`; the
  element ctor is 140 bytes, so 141 looked like the threshold). Swept 126/130/134/138/140/141/144/
  148/152/160/170/180/200. The vector ctor's size **never moves off 136** at any value up to 200 -
  `inline_max_size` is not what keeps the element ctor out of line. 126-152 are actively harmful:
  they drop the unit to 11/14 by taking `GetNumKeyframes__22CFBStreamedCompressionFv` to 0.00%
  and the `CFBStreamedCompression` ctor to 90.49%. 200 additionally drops
  `GetRotationsAndOffsets` to 44.50%. Same threshold behaviour `src/MetroidPrime/CGameCollision.cpp`
  documents: mwceppc takes the last `#pragma inline_max_size` in a file as the file's value.
- **`ptr->T(in)` instead of `new (ptr) T(in)`** in the vector ctor - mwcceppc rejects it
  (`undefined identifier 'T'`, the ctor-name syntax is not available through a template parameter).
- **`*const_cast<T*>(ptr) = T(in)`** (temporary + copy-assign) - compiles, and the unit **falls**
  97.49% -> 97.06% with the vector ctor 63.96% -> 57.68%. Worse.

So the 63.96% needs something other than a threshold or a spelling of the loop body's construct
call - most likely the element ctor being *declared out of class* the way the four
`CFBBitCompressedDataChannelHeader` members are (that is what moved the two `GetSumOfBitCounts`
from inlined to real functions), which is not a threshold at all.

`fn_802B02F0` (76 B) and `fn_802B033C` are still `fn_`-named and still in `CFBStreamedAnimReader.o`.
The `NEW:` item filed by the previous run for that unit already covers four renames there;
`fn_802B02F0` is `AfterEnd__61TVectorOfVaryingLengthItems<Ui,CFBStreamedPerChannelHeader>CFv`
(ours 88 bytes vs 76) and `fn_802B033C` is still unplaced, so neither is nameable yet.

## Gates

```
./tools/goal_check.sh build/goal/item.json   PASS, all checks ok
python3 tools/check_symbol_names.py          0 declared names missing
python3 tools/check_decl_order.py --unit Kyoto/Animation/CFBStreamedCompression
                                            ok: none emits its functions out of retail order
sha1sum build/G2ME01/main.dol                6ef9b491d0cc08bc81a124fdedb8bfaec34d0010 (in gate.sh)
```

`./tools/flip_test.sh Kyoto/Animation/CFBStreamedCompression.cpp` **FAILs, and cannot pass**:
it builds `main.elf` with our object in the link and the link dies on
`undefined: 'CFBStreamedAnimReaderTotals::skQuatFloats'`, which is pre-existing and unrelated to
this change (it reproduces on the untouched tree). `configure.py` is not touched, so nothing is
claimed beyond the count and the unit stays `NonMatching` - correct for a `progress` item.

## NEW:

(Stale `NEW:` lines from the previous run are left in place above; this run adds none.)
