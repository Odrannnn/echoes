# match-cgamestateblockcopyctor - `MetroidPrime/Player/CGameStateBlockCopyCtor` is Matching

**Result: the unit flipped.** `tools/flip_test.sh MetroidPrime/Player/CGameStateBlockCopyCtor.cpp`
prints `PASS -> kept as Matching`, and the tree is left with the flip in place in `configure.py`
(only that one `Object(NonMatching, ...)` -> `Object(Matching, ...)` line changed there).

## What the item asked for, re-measured

`item.json` said 0 of 1 functions at 100%, `fn_80004AA0` at 94.05%. Re-measured on the lane head
before touching anything: `build/report.json` gave

    main/MetroidPrime/Player/CGameStateBlockCopyCtor: fuzzy 94.05%, matched 0.00% (0 / 1)
       fn_80004AA0  94.05%  252 bytes

so the reason was accurate. One function, 0xFC = 252 bytes, no carve needed, no asm.

## The fix, and the two things that were actually wrong

Retail (`tools/dis.sh 0x80004AA0 0xFC`) is `rstl::vector<unsigned char>`'s copy constructor, and
the diff against the head's hand-written body was only 4 instructions, all of them in two places.

**1. The two `== 0` tests are signed.** Retail tests them with `cmpwi` (0x80004AC8, 0x80004AD4);
the lane head emitted `cmplwi`, because `SGameStateBlock` declares `x04_count`/`x08_cap` as `u32`.
`rstl/vector`'s `mCount`/`mCapacity` are `int` (`include/rstl/vector.hpp:25-26`), and this function
is that constructor, so the source reads them as `int`. Casting the two reads to `int` where they
are compared is enough; the stores stay `u32` because `SGameStateBlock` is a view onto a block
that is an element count in one overlay and a byte size in another (see the header's own note), and
`== 0` is the only operation done on them here.

**2. The byte copy must be `rstl::uninitialized_copy_n`, not a loop.** This is the real one. A
hand-written `for (int n = self->x04_count; n != 0; --n) { *to++ = *from++; }` is 94.05% and does
not improve: mwcceppc's loop-idiom transform recognises it and emits the *same* unrolled
8-bytes-then-tail body, but two things land differently -

* the hand-written form puts the trip count in `r0`, so the transform has to spill a copy for the
  tail and emits `mr r3,r0` + `srwi. r0,r0,3`; retail has `srwi. r0,r3,3` on a count that stayed
  in `r3` all the way through, and `andi. r3,r3,7` reads it there;
* the hand-written form allocates `r4` to the *source* pointer; retail has `r4` = destination and
  `r5` = source, which flips every `lbz`/`stb` in both loops.

`rstl::uninitialized_copy_n(src, n, dest)` (`include/rstl/construct.hpp:127`) is the call
`vector::vector(const vector&)` itself makes at `include/rstl/vector.hpp:125`, with the count as
its **second** argument. It produces the count in `r3` and the destination in `r4`, i.e. exactly
retail's allocation, and the whole object goes to 100%.

So the body is now literally `vector.hpp`'s copy constructor, spelled on the `SGameStateBlock`
overlay, calling the header's own copy helper. **No work was deleted**: the previous loop is
exactly what `uninitialized_copy_n` expands to (`construct` on a `uchar` is
`RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE`'s `*static_cast<uchar*>(dest) = src`), and the unrolled
loop body in the object is byte-for-byte the same as before the change.

## Spellings tried, with scores (objdiff fuzzy on `fn_80004AA0`)

| spelling | score |
| --- | --- |
| head: hand-written loop, `from` declared before `to`, `u32` compares | 94.05% |
| + `static_cast<int>` on the two compares, `to` declared before `from` | 98.17% |
| + `rstl::uninitialized_copy_n(src, int(count), dest)` | **100.00%** |

Worth recording for the next unit: declaring the **destination** pointer before the source is what
fixes `r4`/`r5` (98.17% on its own), and the remaining 1.83% is not a spelling of the loop at all -
it is the count's register, which only the header's own helper gets right.

## Gates, all measured on the final tree

* `tools/flip_test.sh MetroidPrime/Player/CGameStateBlockCopyCtor.cpp` -> `PASS -> kept as Matching`
  (it also re-`cmp`s all 86 RELs against `orig/G2ME01/files/RelProd/`).
* `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (the pinned hash).
* All 86 REL sha1s re-hashed against `config/G2ME01/config.yml`: 86 checked, 0 mismatches.
* `python3 tools/check_symbol_names.py` -> `checked 503 units; 0 declared names are missing`.
* `./tools/probe_sources.sh` -> `probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined,
  0 duplicates)`.
* `./tools/unit_fit.sh MetroidPrime/Player/CGameStateBlockCopyCtor.cpp` -> `.text claimed 252 ours
  252 retail 252 fits`, no extra functions.
* `python3 tools/check_decl_order.py --unit MetroidPrime/Player/CGameStateBlockCopyCtor` -> ok
  (trivially, one function).
* `./tools/decomp_build.sh` -> `All: 30.61% fuzzy, 22.73% matched, 11.74% linked (9937 / 28465
  functions)`. Against the head measured before the change, `30.61% / 22.72% / (9936 / 28465)`:
  matched +1, linked unchanged, nothing fell.

## Notes for the driver

* `python3 tools/check_docs_claims.py` fails on exactly the three derived HANDOFF counts, stale by
  the +1 this item earns: matched 9936 -> 9937 and DOL matched 8525 -> 8526 (linked is unchanged in
  the tree; the doc's 4896 is a separate pre-existing staleness). Per the goal prompt the judge
  rewrites the state block from the tree, so no `docs/` edit was made here.
* Nothing else needs doing. `src/MetroidPrime/Player/CGameStateBlockReserve.cpp`
  (`fn_801465EC`, `rstl::vector<unsigned char>::reserve`) is in the same file, is not in
  `configure.py` yet, and its retail object contains the same `srwi. r0,r3,3` transform - so
  `rstl::vector`'s `reserve` at `include/rstl/vector.hpp:157-167` is very likely the same
  near-miss waiting for the same helper. That is a real unit with a real flip in reach, so it is
  filed as a `NEW:` line below.
* `NEW: match-cgamestateblockreserve | match | MetroidPrime/Player/CGameStateBlockReserve | the
  source for fn_801465EC already exists and is not in configure.py; its retail object holds the
  same r3-carried memcpy transform, so vector::reserve at include/rstl/vector.hpp:157 spelled with
  rstl::uninitialized_copy is the same near-miss this item just closed.`
