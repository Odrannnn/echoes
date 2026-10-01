# progress-unit-canimationset

`Kyoto/Animation/CAnimationSet`: **51/67 -> 55/67** matched functions, unit still `NonMatching`.
`./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS`.

## Measured

| | before | after |
|---|---|---|
| `matched_functions` | 51 / 67 | **55 / 67** |
| `fuzzy_match_percent` | 76.864265 | **82.34903** |
| `matched_code_percent` | 64.54294 | **70.0277** |
| `All:` (whole DOL) | 11704 / 28465 | **11708 / 28465** |

`linked` is unchanged at 5656, as it must be: the unit stays `NonMatching`, so none of these four
count towards the one rule's linked total. `build/G2ME01/main.dol` sha1 is still
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

The item's `reason` listed ten unmatched functions and said "51/67 (16 left)". Re-measured, the
clean tree had **seven** functions below 100% and **nine** absent from the object entirely
(`fn_8028CBD0`, `fn_8028D524`, `fn_8028D6D4`, `fn_8028DB8C`, `fn_8028DC10`, `fn_8028DD1C`,
`fn_8028DE98`, `fn_8028DF40`, `fn_8028E0E4`). Only seven of the reason's ten were real; the other
three had already been written. `fn_8028DC10`, `fn_8028DF40` and `fn_8028DD1C` were named as
"cheapest and smallest" and all three turned out to be one-rename or one-plain-spelling wins.

## What I did, per function

All four are `rstl` template instantiations in `src/Kyoto/Animation/CAnimationSet.cpp`, written as
`extern "C"` free functions under retail's own address names, which is the technique the file's
own header comment already documents.

### `fn_8028DD1C` - retail 0x8028DD1C, 96 B - **0.00% -> 100%**

The cheapest of the lot and **no new code at all**: this body was already in the file as
`static void DestroyHalfRange(HalfIter, HalfIter)` and was already **byte-identical** to retail
(the two disassemblies are the same 24 instructions, `stwu r1,-16` through `blr`, with the same
`R_PPC_REL24 ReleaseData__Q24rstl20rc_ptr<10IMetaTrans>Fv` at the same offset). It scored 0.00%
purely because a `static` gets a mangled name and objdiff pairs by name. Renamed to
`extern "C" void fn_8028DD1C(...)` and moved to its descending-offset position. The old comment
claimed it "is still 0.00% here because it is a different function" - that was the diagnosis
being right about the cause and wrong about it being unfixable from C++.

### `fn_8028DF40` - retail 0x8028DF40, 88 B - **0.00% -> 100%**

`rstl::vector<CTransition>::push_back_unsafe`, one line, the plain template spelling:

```cpp
rstl::construct(self->mItems + self->mCount++, in);
```

Matched first try. Note the contrast with `fn_8028CC70` (the `CAnimPOIData` equivalent, already
100%): that one has to call the out-of-line `fn_8028CCA8`, because `CAnimPOIData` has a
user-declared copy constructor. `CTransition` has none, so mwceppc inlines the five word stores
and the `rc_ptr` refcount bump, and the `add. r5,r6,r0 / beqlr` pair is the placement-new null
test with the `add.` carrying the zero flag. No hand-rolled `if` needed.

### `fn_8028DC10` - retail 0x8028DC10, 80 B - **0.00% -> 100%**

`rstl::uninitialized_copy_n<CHalfTransition*, CHalfTransition*>`, again one line:

```cpp
return rstl::uninitialized_copy_n(src, n, dest);
```

Matched first try. Same inlining reason as `fn_8028DF40` - `CHalfTransition` has no user-declared
copy constructor either. Retail's shape is `mtctr r4` / `cmpwi r4,0 / beq` / `bdnz`, i.e. the
template's own countdown, and the `cmplwi r5,0` null test is on the **destination**, not the
source, which is what `construct_impl`'s placement-new test leaves behind.

### `fn_8028DB8C` - retail 0x8028DB8C, 132 B - **0.00% -> 100%**

`rstl::vector<CHalfTransition>::vector(const vector&)`, retail's out-of-line copy constructor.
A copy constructor's symbol name is fixed by the mangler, so it is written as a free function
over the same statements - the same reason `fn_8028CCF0` is not a member. The body is
`fn_8028D950`'s (already 100%) with `mulli 12` and `fn_8028DC10` in place of `fn_8028D9D4`:

```cpp
self->mCount = other->mCount;
self->mCapacity = other->mCapacity;
if (other->mCount == 0 && other->mCapacity == 0) {
  self->mItems = nullptr;
} else {
  rstl::rmemory_allocator().allocate(self->mItems, self->mCapacity);
  fn_8028DC10(other->mItems, self->mCount, self->mItems);
}
```

Note the ordering matters: `fn_8028DC10` had to exist and be named before this one's `bl` paired,
so write the callee first.

## What is left, and what I tried on it

### The five stream constructors - the documented `.sbss` wall, unchanged

`fn_8028CBD0` (160 B), `fn_8028D524` (224 B), `fn_8028D6D4` (184 B), `fn_8028E0E4` (236 B) and
`fn_8028DE98` (168 B) are all still 0.00%. Each loop opens with

```
lbz  r0,lbl_804198xx    ; a byte out of .sbss
mr   r5,r31             ; its address, as a third argument
stb  r0,8(r1)
bl   fn_8028D4BC         ; construct(&tmp, in, &flag)
```

which is mwceppc initialising the empty `TType<T>` that `CInputStream::Get<T>` takes by hidden
pointer. The file's trailing note already characterised this; I did not re-derive it and did not
try to break it, since it needs a `static` byte reloaded out of `.sbss` per iteration in a
header shared by every vector in the tree. **`fn_8028DE98` is a fifth function in this class that
the existing note did not list** - worth recording for the next run.

### `fn_8028E2C0` 95.42% and `fn_8028E754` 92.69% - one reloaded `lwz`, not reachable

Both are `uninitialized_copy<pointer_iterator<...>, T*>`. objdiff's instruction diff is identical
instruction-for-instruction **except at the loop bound**: retail keeps the *end iterator pointer*
in a register and reloads it every iteration, ours hoists the loaded value and compares against it
directly. Two instructions out of 36 and out of 27.

Three spellings tried for `fn_8028E2C0`, all measured:

| spelling | score |
|---|---|
| `for (const CAnimation* last = *end; it != last; ...)` (kept) | **95.42%** |
| `for (; it != *end; ...)` - reload the bound each iteration | 93.19% - **worse**, it moves the reload out of the loop and loses retail's `mr r29,r4` |
| non-const local copy of the end pointer, to force the reload | does not compile |

The third is not expressible: the parameters are `CAnimation* const*`, so both `CAnimation**` and
`const CAnimation**` are an *illegal implicit conversion* from `CAnimation* const*`, and dropping
the inner `const` is an *illegal function overloading* against the `extern "C"` declaration. A
load through a `const` pointer is hoisted by mwceppc; retail did not hoist it. There is no C++
spelling of this that I found, and I did not reach for a hand-rolled reload to fake it.

This is recorded in the source at `fn_8028E2C0`, replacing a comment that gave no reason for the
missing 4.58%.

### `fn_8028E63C` 82.70%, `fn_8028E588` 73.89%, `fn_8028E1F0` 65.38%, `fn_8028E474`/`fn_8028E350` 63.69%

The four `reserve(int)`s plus `fn_8028E63C`. The existing file note attributes all five to
`rstl::pointer_iterator` being **one word** in this tree where retail's is **two**, which shows up
as four stack words at 8(r1)..20(r1) where we build two. I did not re-measure this one and did not
try to change `include/rstl/pointer_iterator.hpp` - it is shared by every vector in the tree, and
the brief forbids changing a class layout for one unit. It is the single largest remaining
blocker in this unit and the one most likely to be worth a lane of its own, because five functions
turn on it.

I also updated the file's closing note, which claimed the four stream constructors were "the only
thing standing between the functions above and 55/67" - 55/67 is where the unit now measures, and
the six partials are a separate matter.

## Verification

```
./tools/decomp_build.sh Kyoto/Animation/CAnimationSet   # All: 11708/28465, unit 55/67
./tools/unit_fit.sh Kyoto/Animation/CAnimationSet.cpp   # 83 extras, all pre-existing COMDAT weak
                                                         # template copies (unchanged by this diff)
python3 tools/check_symbol_names.py                      # 0 missing
python3 tools/check_decl_order.py                        # 978 units, 32 permuted, all accounted for
./tools/goal_check.sh build/goal/item.json               # PASS
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, and the gate's own
86-REL comparison is inside the `ok gate.sh` line. Each new function is declared in descending
retail offset order (0xDD1C after 0xDC60, 0xDF40 before 0xDF98, 0xDC10 between 0xDC60 and 0xDD1C,
0xDB8C between 0xDA3C and 0xD9D4), so the unit is no further from flippable than it was. No `asm`
added, no judge-owned path touched, no other worktree touched, not committed.

The Prime 1 donor at `prime-ref/src/Kyoto/Animation/CAnimationSet.cpp` was **not** needed: all four
functions are `rstl` template instantiations, and this tree's own `include/rstl/vector.hpp` and
`include/rstl/construct.hpp` already spell every one of them correctly. Prime 1's `CAnimationSet`
does not decompile these at all.

## NEW

NEW: match-unit-canimationset | match | Kyoto/Animation/CAnimationSet.cpp | 55/67 and 12 functions short; the 5 `vector(CInputStream&)` ctors need a `.sbss` byte in CInputStream::Get, and the 5 `reserve`/`uninitialized_copy` partials all turn on rstl::pointer_iterator being 1 word where retail's is 2
