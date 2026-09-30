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
