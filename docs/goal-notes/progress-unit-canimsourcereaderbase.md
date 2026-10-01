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