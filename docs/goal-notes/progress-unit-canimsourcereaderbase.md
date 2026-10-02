# progress-unit-canimsourcereaderbase

**Result: `Kyoto/Animation/CAnimSourceReaderBase` 12/28 -> 19/28 functions matched. Project
`matched` 11704 -> 11711 (+7), `linked` 5656 -> 5656 (unchanged, as expected - the unit stays
`NonMatching`). `tools/goal_check.sh build/goal/item.json` -> `PASS`. Unit fuzzy 38.68% -> 60.36%.**

## What actually blocked the 16 "0.0%" functions

Not decompilation. **Seven of them were already compiled and byte-identical in our object** - the
only thing wrong was the *name* retail's symbol table has for them.

`objdiff` pairs functions **by name**. `dtk` cannot name a TU-local weak template instantiation, so
`config/G2ME01/symbols.txt` labels them `fn_802A4860`, `fn_802A5958`, ... Our compiler emits the same
code under the correct mangled name (`resize__Q24rstl85vector<...EParentedMode...>FiRC...`), and an
unnamed-vs-named pair scores **0%** no matter that the bytes are equal. This is the mechanism
`docs/RUNNING_THE_DECOMP.md` documents as "Pairing a function the retail symbol table has no name
for"; the fix is the rename, not new code.

Measured, per function: dumped `.text` of `build/G2ME01/src/Kyoto/Animation/CAnimSourceReaderBase.o`
and of `build/G2ME01/main.elf`, and counted differing bytes over each retail function's range
(aligning our `PostConstruct` at object offset 0x284 to retail 0x802A4538 to derive the base).
**Seven pairs came out at 0 differing bytes**, i.e. genuinely identical, not merely similar:

| retail (was `fn_...`) | size | our name, byte-identical |
| --- | --- | --- |
| `fn_802A4860` | 208 | `resize__Q24rstl85vector<Q24rstl41pair<Ui,Q213CParticleData13EParentedMode>,Q24rstl17rmemory_allocator>FiRC...` |
| `fn_802A5958` | 212 | `reserve__Q24rstl85vector<Q24rstl41pair<Ui,Q213CParticleData13EParentedMode>,...>Fi` |
| `fn_802A4B08` | 48 | `__ct__Q24rstl86set<Q24rstl10pair<Ui,i>,...>FRCQ24rstl86set<...>` |
| `fn_802A4BE0` | 116 | `__dt__Q24rstl86set<Q24rstl10pair<Ui,i>,...>Fv` |
| `fn_802A5A2C` | 184 | `copy_from__Q24rstl158red_black_tree<Q24rstl10pair<Ui,i>,...>FP...4node` |
| `fn_802A5B4C` | 96 | `free_node_and_sub_nodes__Q24rstl158red_black_tree<...>FP...4node` |
| `fn_802A5BAC` | 488 | `insert_into__Q24rstl158red_black_tree<...>FP...4nodeRCQ24rstl10pair<Ui,i>` |

Applied with `tools/apply_rename.py` (renamed 7/7) - the rename replaces each line in place, never
inserts beside it. Each then went 0.00% -> 100.00%.

Names were **not guessed**: each was read out of our own object with
`build/binutils/powerpc-eabi-nm --defined-only`, and only the one whose prefix was unique was used.
All three `resize` instantiations in the object are byte-identical to each other (same `stw/stw` pair
copy), so prefix `resize__Q24rstl85vector` - the `EParentedMode` one - is the only unambiguous pick;
`fn_802A4860` is the one `PostConstruct` calls as `resize(this+0x48, ...)`, which is
`mParticleStates`, confirming the assignment.

## The source change (independent of the renames)

`src/Kyoto/Animation/CAnimSourceReaderBase.cpp:29`, in `_getPOIList`:

```c
const CCharAnimTime duration = sourceInfo.GetAnimationDuration();   // was
const CCharAnimTime& duration = sourceInfo.GetAnimationDuration();  // now
```

Prime 1's counterpart has the reference; retail's disassembly of the four `_getPOIList`
instantiations (`fn_802A5298`/`541C`/`55B0`/`5734`) confirms it - retail passes an **address** to
`__lt__`/`__pl__` rather than materialising a second stack copy. Measured effect on our
`_getPOIList` bodies, **-16 bytes each** (frame `-176(r1)` -> the retail-sized `-144(r1)`):

| | ours before | ours after | retail |
| --- | --- | --- | --- |
| `<CBoolPOINode>` | 420 | 404 | 388 (`fn_802A5298`) |
| `<CInt32POINode>` | 444 | 428 | 404 (`fn_802A541C`) |
| `<CParticlePOINode>` | 420 | 404 | 388 (`fn_802A55B0`) |
| `<CSoundPOINode>` | 408 | 392 | 376 (`fn_802A5734`) |

This did **not** change the matched count by itself (still 0.00%, and the unit total is the same 19
with and without it) - it is a real 16-byte-per-function improvement toward those four, not a
cosmetic edit. Both spellings were built and measured; the object `.text` md5 differs between them,
so it is not inert. **16 bytes still to go on each** - see the wall below.

## What is left, and why each is blocked (9 functions, all still 0.00%)

`fn_802A5298`, `fn_802A541C`, `fn_802A55B0`, `fn_802A5734` - the four `_getPOIList` instantiations.
Retail 16 bytes shorter each after the fix above. The opcode diff is **not** register allocation: it
is a different `min_val` lowering. Retail evaluates the `rstl::min_val(duration, totalTime)` guard as
`__pl__` then `__lt__` on stack-copied `CCharAnimTime`s at `r1+8`/`r1+40` (12 instructions), where
ours keeps the sum in a temp and copies it into four separate stack words at `r1+8/12/16/20`
(10 instructions, but 32 bytes of frame in total). Getting the frame to `-144(r1)` needs retail's
exact `min_val`/`CCharAnimTime` argument-passing shape, not a source-level rewrite of the loop.

`fn_802A4930`, `fn_802A4A1C` (236 B each) - retail's `resize` for the `pair<Ui,i>` and `pair<Ui,b>`
vectors, the 8x-unrolled fill variants (`srwi r0,r6,3` / 8 `stw` pairs / `andi. r6,r6,7`). Our
compiler emits the plain loop for both. Only the `EParentedMode` instantiation unrolls in our
object, and it is the one that pairs with `fn_802A4860`. This is codegen dependent on the element
type's copy-assignment, not on `_getPOIList`; changing `rstl::vector::resize` is a shared header and
would move every other unit that instantiates it - **not attempted, it is another unit's blast radius.**

`fn_802A4B38` (168 B) - `rstl::set`'s copy constructor body, called from `fn_802A4B08`. Ours is
`__ct__Q24rstl158red_black_tree<...>` at **164 bytes**, 4 short. Not byte-identical, so a rename
would not help (the docs are explicit: a rename "enables pairing, not matching").

`fn_802A58AC` (172 B) and `fn_802A5AE4` (104 B) - the last two unnamed retail helpers; neither has a
same-size counterpart in our object, so the identity is still open.

## Measurement notes / traps hit

- **`objdiff -s` addresses in the retail `.elf` are absolute but in our `.o` they are
  section-relative.** Comparing them directly makes every function look 100% different. I lost three
  rounds to this: my first diff said "208 of 208 bytes differ", which was pure misalignment. The
  base must be derived from a known pair (`PostConstruct` at object 0x284 <-> retail 0x802A4538),
  giving `+0x802A42B4`. With that, seven functions diff at exactly 0.
- **`powerpc-eabi-nm -S` does not print sizes for the weak symbols in this object** (they come out
  as `000005ac 0 resize__...`), so a size-keyed comparison finds nothing. `objdump -t` prints the
  real sizes and is what I used.
- `tools/unit_fit.sh` already reports this unit as **1984 bytes over** the claimed range with 31
  extra functions, so it cannot flip regardless of these wins - this item was correctly scoped as
  `progress`.

WALL: _getPOIList<T> (all four instantiations) 0.00% - retail's frame is 144 bytes and its
min_val guard lowers to __pl__/__lt__ on two stack-copied CCharAnimTimes; the reference-binding fix
above closes 16 of the 32 bytes per instantiation and the rest is argument-passing shape in
rstl::min_val, not loop logic.

NEW: progress-unit-canimsourcereaderbase | match | Kyoto/Animation/CAnimSourceReaderBase | 9
functions remain at 0.00%; four _getPOIList need 16 bytes each of exact rstl::min_val
argument-passing shape, two unrolled resize instantiations would need a change to the shared
rstl/vector.hpp resize (blast radius across other units), and two helpers have no identified
counterpart yet.
---

# Run 2 (lane L7, 2026-10-02) - the unit is finished

**Result: `Kyoto/Animation/CAnimSourceReaderBase` 19/28 -> 28/28 functions matched, unit
100.00% fuzzy and 100.00% matched. Project `matched` 12232 -> 12241 (+9), `linked` 5860 ->
5860 (unchanged, as expected - the unit stays `NonMatching`). `tools/goal_check.sh
build/goal/item.json` -> `PASS`. DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
`probe: 752 files, 0 failed`, `check_symbol_names.py`: 0 missing, `check_decl_order.py`:
ok. `tools/unit_fit.sh`: .text 7960 vs 6236 claimed, over by 1724, 15 extra functions.**

**The WALL above is superseded - all four `_getPOIList` instantiations are now at 100.00%, and
the two "no identified counterpart" helpers turned out to be renames like the other seven. The
four bytes the old note said were left were not register allocation.**

## What the remaining nine actually were: five more renames

The old note measured sizes and stopped at "no same-size counterpart in our object". Sizes were
misleading because of two things the note did not know yet: two of our object functions change
size when `construct_impl` is added for `pair<uint,int>` (below), and **the retail-vs-our raw byte
diff is dominated by relocation fields**, which must be excluded before a function is called
unidentified. Measured with `objdump -r` + "every differing byte offset falls inside a
relocation's 4-byte field", ten retail `fn_...` symbols turned out to be byte-identical to code we
already emit under the right mangled name:

| retail (was `fn_...`) | size | our name |
| --- | --- | --- |
| `fn_802A58AC` | 172 | `reserve__Q24rstl54vector<Q24rstl10pair<Ui,b>,...>Fi` (6 diff bytes, 2 relocs) |
| `fn_802A4A1C` | 236 | `resize__Q24rstl54vector<Q24rstl10pair<Ui,b>,...>FiRCQ24rstl10pair<Ui,b>` (2 diff bytes, 1 reloc) |
| `fn_802A4930` | 236 | `resize__Q24rstl54vector<Q24rstl10pair<Ui,i>,...>FiRCQ24rstl10pair<Ui,i>` |
| `fn_802A5AE4` | 104 | `create_node__Q24rstl158red_black_tree<Q24rstl10pair<Ui,i>,...>` (3 diff bytes, 1 reloc) |
| `fn_802A5298/541C/55B0/5734` | 388/404/388/376 | `_getPOIList<CBoolPOINode>/<CInt32POINode>/<CParticlePOINode>/<CSoundPOINode>` |
| `fn_802A4B38` | 168 | `__ct__Q24rstl158red_black_tree<Q24rstl10pair<Ui,i>,...>FRCQ24rstl158red_black_tree<...>` |

Names were read out of our own object with `powerpc-eabi-objdump -t`, never guessed, and applied
with `tools/apply_rename.py` (10/10, each replacing its line in place). One is **outside the
claimed range**: `fn_801EF3A4` (0x801EF3A4, 0xAC) -> `reserve<vector<pair<uint,int>>>::reserve`,
which retail put in an unclaimed gap between `MetroidPrime/ScriptObjects/Carve801E8AEC.c` and
`MetroidPrime/CActorField25.cpp`. It is `vector<pair<uint,int>>::reserve` beyond doubt - `slwi
r3,r30,3` and the `lwz/stw` pair copy at +0/+4 - and byte-identical to our 172-byte copy outside
the two `bl`s, so renaming it changed no other unit and made `fn_802A4930`'s `bl` resolve.

Per function: `fn_802A58AC` 0.00 -> 100.00; `fn_802A4A1C` 0.00 -> 100.00; `fn_802A4930`
0.00 -> 100.00; `fn_802A5AE4` 0.00 -> 100.00; `fn_802A5298` 0.00 -> 100.00; `fn_802A541C` 0.00 ->
100.00; `fn_802A55B0` 0.00 -> 100.00; `fn_802A5734` 0.00 -> 100.00; `fn_802A4B38` 0.00 -> 100.00.

## The three source changes that made the code match (not just the names)

1. **`include/rstl/pair.hpp`: `construct_impl` for `pair<uint,int>`.** Retail's
   `fn_802A4930`, `vector<pair<uint,int>>::resize`'s fill loop, is unrolled eight times
   instruction for instruction like the bool pair's `fn_802A4A1C`, `stw` at +4 where the bool pair
   has `stb`. A placement-new of `pair`'s copy constructor gives the 0x98-byte unrolled-free loop,
   so this pair is copied by assignment here too - the same lever and the same conclusion the
   `fn_802A3B80` note above already recorded for the out-of-line copy. Measured blast radius:
   `vector<pair<uint,int>>` exists only as `CAnimSourceReaderBase::mInt32States`, and **no unit
   anywhere got worse** (`All:` 12232 -> 12241, gate's report diff clean). It also shrank
   `create_node` from 0x70 to 0x68, which is what made `fn_802A5AE4` a 104-byte match at all - so
   the old note's "neither has a same-size counterpart" was only true before this change.

2. **`src/Kyoto/Animation/CAnimSourceReaderBase.cpp`: inline `rstl::min_val`.** `min_val`
   takes its arguments **by value** and returns by value (`include/rstl/math.hpp:8`), so the
   library form materialises two extra 8-byte stack copies at `r1+8` and `r1+16` before the
   compare, on top of the one retail makes. Written as the conditional `min_val` itself expands to
   (`totalTime < duration ? totalTime : duration`), the region is instruction for instruction
   retail's, frame `-144(r1)` included. This is the whole of the old note's "16 bytes still to
   go": the argument-passing shape was real, but `min_val`'s by-value signature, not `CCharAnimTime`.
   All four: 404/428/404/392 -> 384/408/384/372, byte diff 261/297/263/267 -> 177/208/179/169.
   A rename was still needed afterwards - the code was right and the pair still scored 0.00%.

3. **`src/.../CAnimSourceReaderBase.cpp`: read the first element through a second name.**
   Retail's pre-loop `nodeTime` load is `mulli r29,r21,48` / `mr r27,r21` / `addi r3,r29,16` /
   `add r3,r31,r3`; ours folded the `mTime` offset into the load (`add r3,r31,r29` / `lfs
   f0,16(r3)`) - one instruction short, 384 against 388. Reading the first element through
   `const int start = passedCount` instead of the `index` the loop increments stops the fold and
   gives 388/404/388/376, all four byte-identical outside relocations. **The reload inside the
   loop already used the three-instruction form on both sides**, which is what identifies this as
   a scheduling choice on one expression and not a different source shape.

4. **`include/rstl/red_black_tree.hpp`: `node* const root` in the copy constructor.** Retail's
   `fn_802A4B38` has `cmplwi r3,0` then a plain `mr r4,r3` then `beq`; ours fused the move into
   the test as `mr. r4,r3` and came out 0xA4 against retail's 0xA8. `const` keeps them apart. The
   first spelling tried - naming `first` and `last` as well - reached the right size but needed a
   second live register (`r5`) where retail reuses `r3`, so it does not match; `const` on `root`
   alone is what does. No other unit moved.

## Spellings measured and rejected (so the next run does not retry them)

`_getPOIList` min region, each built, all four instantiations measured: `const` on `endTime`;
`const` on `totalTime`; `rstl::min_val(totalTime, duration)` (swapped - 1 byte *worse* on the diff,
so retail really is `min_val(duration, totalTime)`); `endTime` bound to `const CCharAnimTime&`;
`duration` as a plain non-`const` value (+4 bytes each, worse); `duration`/`totalTime`/`endTime`
all `const` (worse); `const`/`=` spellings of the `nodeTime` declaration; `nodeTime` as a
`const&`; `stream[passedCount]` in the pre-loop load instead of `stream[index]` (**this alone is
not enough** - still 384, the `const int start` is doing the work); `const T& firstNode =
stream[passedCount]` (drops the unit back to 23/28). `leftmost` with an early `return nullptr`
instead of the `if (n != nullptr) { ... }` form: no change at all.

## State of the unit, and what is left

All 28 functions are at 100.00% and the unit is 100.00% fuzzy / 100.00% matched. It still cannot
flip on `tools/unit_fit.sh`: `.text` 7960 against 6236 claimed, **over by 1724 with 15 extra
functions** (down from the 1984/31 the old note measured - renames and the `pair<uint,int>`
change removed some). The extras are the usual out-of-line weak template copies
(`__dt__21CAnimSourceReaderBaseFv` +176, `reserve<vector<pair<uint,int>>>Fi` +172, the four
`__as__<POINode>FRC<POINode>`, the four vector/red_black_tree `__dt__`, ...) plus `.data` 168 and
`.sdata` 75 that `splits.txt` does not claim. Whether that is harmless COMDAT the retail linker
discarded is only `flip_test.sh`'s verdict, and this item was scoped `progress`.

NEW: progress-unit-canimsourcereaderbase | match | Kyoto/Animation/CAnimSourceReaderBase | the unit
is now 28/28 at 100.00% fuzzy/matched; only unit_fit's 15 extra out-of-line weak copies (1724 bytes
over the claimed range, .data 168 and .sdata 75 unclaimed) stand between it and Matching, and only
flip_test decides whether those are harmless.
