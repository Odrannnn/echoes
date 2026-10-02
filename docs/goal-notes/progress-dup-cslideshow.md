# progress-dup-cslideshow (lane 11, 2026-10-02) - DONE

`MetroidPrime/CSlideShow` matched_functions **18 -> 39 / 76** (fuzzy 22.56% -> 33.54%).
Global matched **13133 -> 13154**; linked 6225 unchanged. Unit stays `NonMatching`; not flipped.
`./tools/goal_check.sh build/goal/item.json` -> **PASS** (gate clean, `All:` 37.12% -> 37.15%,
`check_symbol_names.py` clean, no asm added, target rose).

21 functions went 0.00% -> 100.00%, all in the target unit, all of them compiler-generated members
(`rstl::vector` / `rstl::pair` instantiations) that this object already emitted or now emits, whose
retail symbols were still dtk's `fn_` placeholders. The item's `reason` was right: the first match
of a shape turns its copies into copy-work - here it turned 20 more, because the copies were in the
same unit under different names.

## 1. The item's first target: `fn_8018FE6C` (0x8018FE6C, 0x148 = 328 B)

**It is `rstl::vector< rstl::pair< uint, uint > >::vector(const vector&)`.**

How it was identified (measurements, not guesses):

- Its five byte-shape copies across `main` (same instruction words apart from call targets) include
  a **named** one: `__ct__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,Q24rstl17rmemory_allocator>FRC...`
  at 0x80004F5C, `size:0x148`, in unit `auto_03_80004D84_text` (found by hashing each function's
  instruction words across `build/G2ME01/obj/**/*.o`; the five hits were this one, the named
  instantiation at 0x80004F5C, `fn_8000387C`, `fn_80005158` and `fn_801EF8B8`).
- The element is 8 bytes and is copied as a block, unrolled eight elements per iteration, with **no
  per-element null test** - the shape `include/rstl/construct.hpp:31-43` documents as "the type is
  declared trivially constructible".
- `include/rstl/pair.hpp:38-44` declares exactly `pair< uint, uint >` trivially destructible and
  gives it the assignment `construct_impl`. **Measured both spellings** with this unit's own
  `build.ninja` flags: `pair< int, int >` emits **292 B with a per-element `cmplwi`**, `pair< uint,
  uint >` emits **328 B, 82 words, identical to retail's `fn_8018FE6C` word for word**.

What produced the match (two edits):

1. `include/MetroidPrime/CSlideShow.hpp:38-45` - `SGalleryData::mSlides` was
   `rstl::vector< rstl::pair< int, int > >`; it is now `rstl::vector< rstl::pair< uint, uint > >`.
   Same size, same offsets, so no layout moves; the only change is which `vector` helpers the
   compiler instantiates (the trivially-(de)structible ones). Corroborated by retail's *destructor*
   for the same member: `fn_80190118` (0x54 = 84 B, `Free` twice and nothing else) is the
   trivially-destructible form; the old spelling gave 132 B with the element loop.
2. `src/MetroidPrime/CSlideShow.cpp:402-415` - `extern "C" void
   force_slide_range_vector_copy(TSlideRangeVec*, const TSlideRangeVec*)` with a placement copy.
   Retail emits the copy out of line because `BuildGalleryLists` pushes an `SGalleryData` through
   `mGalleries.push_back_unsafe` (the chain is `fn_8018FD94` push_back_unsafe -> `fn_8018FDCC`
   construct -> `fn_8018FDEC` construct_impl -> `fn_8018FE14` the struct's copy constructor ->
   `fn_8018FE6C` + `fn_8018FFB4`); `BuildGalleryLists` is not reconstructed here, so nothing
   constructs one and mwcceppc never emits the COMDAT. MWCC 2.7 rejects explicit instantiation of a
   member (`template V::vector(const V&);` is a syntax error; `template class rstl::vector<...>;`
   instantiates only the non-`inline` members, so it does **not** emit the copy constructor), so the
   instantiation is forced from a real function - the same arrangement as the two `reserve` thunks
   in `src/MetroidPrime/main.cpp:249-250`. The thunk's own name is not in `symbols.txt`, so objdiff
   ignores it.

## 2. The other 20 - each was already byte-identical, only unnamed

`config/G2ME01/symbols.txt`: 21 `fn_...` placeholder lines were replaced by the mangled name of the
function that object already defines (`nm` on `build/G2ME01/src/MetroidPrime/CSlideShow.o`). Every
one of them is confirmed by *both* identical instruction words *and* a call-target chain that closes
inside the set (the table's last column). `tools/fnmap.py MetroidPrime/CSlideShow` pairs them by
words; the reloc targets in the last column come from `powerpc-eabi-objdump -dr` on the retail
object (pair by the call target, not by the words - see section 4).

| address | new name (symbols.txt) | size | before | after | reloc-target evidence |
|---|---|---|---|---|---|
| 0x8018C578 | `__as__...vector<STexture>...FRC...` (operator=) | 0x98 | 0.00% | 100% | calls clear/reserve/uninitialized_copy<STexture> |
| 0x8018C610 | `uninitialized_copy<STexture,P STexture>__4rstlF...` | 0x64 | 0.00% | 100% | -> construct<STexture> |
| 0x8018C674 | `construct<STexture>__4rstlFPvRC...` | 0x20 | 0.00% | 100% | -> construct_impl<STexture> |
| 0x8018C694 | `construct_impl<STexture>__4rstlFPvRC...` | 0x4C | 0.00% | 100% | no relocs |
| 0x8018C6E0 | `clear__...vector<STexture>...Fv` | 0x60 | 0.00% | 100% | -> destroy<iterator<STexture>> |
| 0x8018C740 | `destroy<...iterator<STexture>...>__4rstlF...` | 0x38 | 0.00% | 100% | -> destroy_impl<iterator<STexture>> |
| 0x8018C778 | `destroy_impl<...iterator<STexture>...>__4rstlF...` | 0x88 | 0.00% | 100% | `__dt__6CTokenFv`, `Free` |
| 0x8018C800 | `__dt__...vector<STexture>...Fv` | 0x84 | 0.00% | 100% | -> destroy<iterator<STexture>> |
| **0x8018FE6C** | `__ct__...vector<pair<Ui,Ui>>...FRC...` **scope:local** | 0x148 | 0.00% | 100% | `allocate` (see section 1) |
| 0x801900B4 | `__dt__Q210CSlideShow12SGalleryDataFv` | 0x64 | 0.00% | 100% | -> both vector destructors, `Free` |
| 0x80190118 | `__dt__...vector<pair<Ui,Ui>>...Fv` **scope:local** | 0x54 | 0.00% | 100% | `Free` twice; first call in 0x801900B4 |
| 0x8019016C | `__dt__...vector<PC10SObjectTag>...Fv` | 0x54 | 0.00% | 100% | `Free` twice; second call in 0x801900B4 |
| 0x80190658 | `__dt__...vector<SGalleryData>...Fv` | 0x84 | 0.00% | 100% | -> destroy<iterator<SGalleryData>> |
| 0x801906DC | `destroy<...iterator<SGalleryData>...>__4rstlF...` | 0x38 | 0.00% | 100% | -> destroy_impl<iterator<SGalleryData>> |
| 0x80190714 | `destroy_impl<...iterator<SGalleryData>...>__4rstlF...` | 0x50 | 0.00% | 100% | -> destroy<SGalleryData> |
| 0x80190764 | `destroy<SGalleryData>__4rstlFP...` | 0x20 | 0.00% | 100% | -> destroy_impl<SGalleryData> |
| 0x80190784 | `destroy_impl<SGalleryData>__4rstlFP...` | 0x24 | 0.00% | 100% | -> `__dt__SGalleryData` |
| 0x801907A8 | `__dt__Q210CSlideShow10SSlideDataFv` | 0x58 | 0.00% | 100% | -> `__dt__vector<STexture>`, `Free` |
| 0x801911D0 | `reserve__...vector<STexture>...Fi` | 0xE0 | 0.00% | 100% | allocate, uninitialized_copy, `__dt__6CTokenFv`, `Free` x2 |
| 0x801912B0 | `uninitialized_copy<...iterator<STexture>...,P STexture>__4rstlF...` | 0x68 | 0.00% | 100% | -> construct<STexture> |
| 0x80191468 | `reserve__...vector<TToken<CDependencyGroup>>...Fi` | 0xD4 | 0.00% | 100% | allocate, `__ct__6CToken`, `__dt__6CTokenFv`, `Free` |

No source change was needed for those 20: the bodies were already there (`vector<STexture>` is the
member `SSlideData::Reset` assigns, `SGalleryData`'s helpers come from the destructor path, etc.),
only the retail-side name was missing, so objdiff had nothing to pair them with.

## 3. The one thing that broke the gate, and the rule it taught

First attempt used `scope:weak` on the two renames whose name already exists elsewhere in
`symbols.txt` (0x8018FE6C is also 0x80004F5C, 0x80190118 is also 0x80004E30). The build linked,
the report was right, and **the DOL sha1 broke**: `build/G2ME01/main.dol` came out
`4e59cd38...` instead of `6ef9b491...`. Measured cause: the two weak duplicates were folded away -
every data section's file offset and address in the DOL header moved by exactly **-416 bytes**
(`0x148 + 0x54` + 4 of alignment, i.e. the two discarded bodies), so retail's own copies at
0x8018FE6C and 0x80190118 disappeared from `.text`. `scope:local` keeps them (a local symbol cannot
be folded against the global one) and the sha1 is back to `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
That is also what `symbols.txt` already does for its 250 duplicate names: one copy `scope:weak`, the
other `scope:local` (e.g. `destroy<14CSaveableState>...` at 0x80025C7C / 0x802B47F0).

**Rule: a rename that duplicates an existing name needs `scope:local`, not `scope:weak`** - the
sha1 is the only check that sees it.

## 4. Left over, measured

- **19 unnamed functions still at 0.00%**: `fn_8018E5C4` (0x38), `fn_8018F8FC` (0xBC),
  `fn_8018FD94` (0x38), `fn_8018FDCC` (0x20), `fn_8018FDEC` (0x28), `fn_8018FE14` (0x58),
  `fn_8018FFB4` (0x100), `fn_801901C0` (0x4C), `fn_8019020C` (0xE0), `fn_801902EC` (0x4C),
  `fn_80190338` (0xE8), `fn_80191150` (0x80), `fn_80191318` (0xA4), `fn_801913BC` (0xAC),
  `fn_8019153C` (0xAC), `fn_801915E8` (0x20), `fn_80191608` (0x4C), `fn_80191654` (0x68),
  `fn_801916BC` (0xD0). The 0x8018FD94..0x8018FE14 group is the `SGalleryData` chain (section 1);
  0x801901C0..0x80190338 are two `insert`/`insert_into`-shaped pairs for the slide-range vector and
  are reachable the same way (force the instantiation, then rename) - not attempted here.
- **`SGalleryData` is 36 bytes in retail, 28 in this header.** Measured: `fn_8018FD94`
  (push_back_unsafe) strides `mulli r0,r5,36`, `fn_80191608` (destroy<iterator>) strides
  `addi r31,r31,36`, while `fn_8018FE14` (its copy constructor) copies `+0` (word), a vector at
  `+4` and a vector at `+20` and nothing else. So retail has members at `+16..20` and `+32..36`
  that its own copy constructor does not copy. Fixing the layout would let 0x8018FD94, 0x8018FDCC,
  0x8018FDEC and 0x8018FE14 match as well; the member identities were not determined.
- **Do not pair by instruction words alone.** `fn_8018FDEC` (0x28, `construct_impl<T36>`) has the
  same 10 words as the forcing thunk added here, and `fn_8018FDCC`/`fn_80190764`/`fn_801915E8`
  (all 8 words) each have the same words as `construct<STexture>`; only the call target says which
  is which (`fn_8018FDCC -> fn_8018FDEC`, `fn_80190764 -> fn_80190784`, `fn_8018C674 ->
  fn_8018C694`). Name by the reloc target, never by the words.

## 5. Reproduce

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
./tools/fast_try.sh MetroidPrime/CSlideShow       # per-function percentages, no DOL relink
python3 tools/check_decl_order.py --unit MetroidPrime/CSlideShow
./tools/decomp_build.sh                           # needed after a symbols.txt change (retail side)
sha1sum build/G2ME01/main.dol                     # 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/goal_check.sh build/goal/item.json        # the judge; -> PASS
```

Files touched: `src/MetroidPrime/CSlideShow.cpp` (16 lines added),
`include/MetroidPrime/CSlideShow.hpp` (8 lines), `config/G2ME01/symbols.txt` (21 renamed lines,
2 of them `scope:local`). No `tools/`, no `configure.py`, no `build/goal/` (other than this file),
no asm, no unit carved.

NEW: cslideshow-sgallerydata-layout | progress | MetroidPrime/CSlideShow | `SGalleryData` is 36 B in retail (`mulli r0,r5,36` in `fn_8018FD94`, `addi r31,r31,36` in `fn_80191608`) but 28 B in `include/MetroidPrime/CSlideShow.hpp`; retail's copy constructor `fn_8018FE14` copies `+0` (word), a vector at `+4` and a vector at `+20`, so members at `+16..20` and `+32..36` are missing - determining them should match `fn_8018FD94`, `fn_8018FDCC`, `fn_8018FDEC` and `fn_8018FE14` (4 functions, same force-the-instantiation recipe as this item).
