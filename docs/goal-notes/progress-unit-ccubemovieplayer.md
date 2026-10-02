# progress-unit-ccubemovieplayer

`Kyoto/Graphics/CCubeMoviePlayer`, `src/Kyoto/Graphics/CCubeMoviePlayer.cpp` only. The unit stays
`NonMatching`; nothing under `configure.py`, `config/`, `tools/` or `docs/` was touched.

## Result, measured

`build/report.json`, unit `main/Kyoto/Graphics/CCubeMoviePlayer`:

| | before | after |
|---|---|---|
| `matched_functions` | 45 / 57 | **46 / 57** |
| `fuzzy_match_percent` | 97.061775 | 97.72767 |
| `matched_code_percent` | 78.27965 | 79.14314 |

`tools/goal_check.sh build/goal/item.json` -> `PASS progress-unit-ccubemovieplayer`
(`matched 12231 -> 12232`, `linked 5860 -> 5860`, no asm added, gate clean).

## The one function taken to 100%

**`fn_80319CB4`, 42.69% -> 100.00%** (104 bytes). This is retail's out-of-line `rstl::destroy` over
`vector<rstl::auto_ptr<uchar>>`, and our object already emitted a *byte-identical* copy of it - under
the weak template name
`destroy<rstl::pointer_iterator<rstl::auto_ptr<uchar>,...>>__4rstlFIt_it` (104 bytes, verified with
`objdump`; `~vector<auto_ptr<uchar>>` calls that symbol). So the bytes were reachable; what was
missing was a symbol named `fn_80319CB4` carrying them.

The old hand-rolled body was `rstl::destroy(*first, *last)` taking the iterators **by pointer**, which
just forwarded to the weak instantiation (12 instructions, 42.69%). Two changes make it match:

1. take the iterators **by value** - the MWCC ABI hands a non-POD class parameter by address, so
   the callee's copies land in its frame (the two dead `stw r31,8(r1)` / `stw r30,12(r1)` that
   retail has and that no source-level C++ can otherwise produce), and
2. run the loop through a one-line `static inline` template rather than calling `rstl::destroy`
   directly - mwccppc will not auto-inline the range `destroy` (`-inline deferred,noauto`, and
   `construct.hpp` declares it before its inline definition so a weak out-of-line copy exists), so
   the direct call stays a `bl`.

With the bound left in memory instead of a register the loop re-reads `*end` every iteration
(`lwz r0,0(r30); cmplw`); with it hoisted into a register the comparison is a bare `cmplw r30,r31`.
That single difference is the whole remaining 8% in the intermediate spellings.

### Spellings and scores for `fn_80319CB4`

| spelling | score |
|---|---|
| `rstl::destroy(*first, *last)`, params by pointer (the old body) | 42.69% |
| params by pointer, `rstl::destroy(*first, *last)` | 84.35% |
| params by pointer, locals `begin`/`end` then `rstl::destroy(begin, end)` | 82.62% |
| params by pointer, `for (it = *first; it != *last; ++it) rstl::destroy(it)` | 78.92% |
| params by pointer, hand-written `if (it->mHas) CMemory::Free(it->mItem)` loop | 81.42% |
| params **by value**, `rstl::destroy(&*cur)` loop, bound read from the parameter | 84.31% |
| ... + pointer-based `cur`/`last` locals | 90.27% |
| ... + `#pragma inline_max_size(400)` / `(2000)` / `inline expanding` to force the inline | no change (37.15%) |
| ... + a local `static inline` helper taking `It*` params | 30.42% |
| params by value, `It cur = begin; for (; cur != last; ++cur) rstl::destroy(&*cur);` | 92.00% |
| **params by value, via `static inline mp_destroy_val(It begin, It end)`** | **100.00%** |

## Improvements measured but not yet at 100%

- **`__dt__vector<CMoviePlayer::CTHPTextureSet>`, 78.06% -> 90.06%** and
  **`clear<...CTHPTextureSet>`, 78.17% -> 78.33%**: build the `mItems + mCount` iterator before the
  `mItems` one in `clear()` and `~vector()`. Retail computes the end address first (`lwz r0,4(r31)`
  = mCount, then `lwz r5,12(r31)` = mItems) and we computed it last. `~vector` is now 3 instructions
  off; `clear` is unchanged in practice because the declaration order also decides which stack slot
  each iterator lands in, and retail needs `first` at `r1+16` (i.e. declared first) with the end
  computed first - the two requirements pull in opposite directions and neither ordering satisfies
  both.
- **`DecodeFromRead`, 96.31% -> 98.00%**: keep `dataStart` and re-form `data = dataStart + offset` at
  the bottom of the loop instead of using `data + offset` in the two calls. That is what leaves the
  loop with the **two** induction variables retail has (`add r25,r25,r0` for the offset and
  `add r26,r26,r0` for the byte cursor). Prime 1's `DecodeFromRead` has the same shape with the
  `data` local declared at the top of the loop; that spelling puts the address formation at the loop
  head instead (71 instructions, right count, wrong position, 96.73%). Remaining diff: one extra
  `mr r5,r26` in the prologue, because our `data` and `dataStart` are two variables where retail's
  is one register. Inlining the base expression instead of naming `dataStart` makes it worse (91.17%).

## Measured walls (spellings tried this run, so the next run can skip them)

All of these are pure register allocation / instruction scheduling - the instruction streams are
identical apart from which register holds what, or one instruction's position.

- **`fn_8031A8F8` (104 B), 92.31%, 6 spellings** - identical instruction stream; retail loads
  `*first` into r31 immediately after `stw r31,28(r1)` and we load it after `mr r29,r4`. Tried:
  `output` local vs incrementing the parameter; `for(...; ++source, ++destination)`; `const`
  parameters; a `bound` local; a cached `*last`. All 92.31% except the `const` parameter (91.73%) and
  the cached `*last` (79.62%).
- **`fn_80318030` (80 B), 90.00%, 5 spellings** - the same one-instruction placement as
  `fn_8031A8F8`: retail hoists `lwz r31,0(r3)` between the two register spills, we emit it after
  `mr r30,r4`. Tried: `end`/`current` declaration order both ways, a `for` init clause, an explicit
  `It` loop variable, `end` without `const`. All 90.00%.
- **`fn_8031AA24` (68 B), 97.65%, 6 spellings** - identical 17 instructions; retail holds the `false`
  constant in r3 (the just-freed incoming `first` register) and `*last` in r0, we hold `false` in r0
  and `*last` in r4. Tried: `for` header vs `while` with trailing `++`, declaration order both ways,
  a `for` init clause, and hand-writing the auto_ptr copy instead of `rstl::construct` (73.24%, far
  worse - do not write it out).
- **`fn_80317FF8` (56 B), 71.00%, 4 spellings** - identical 14 instructions; retail loads `*last`
  into r5 *before* saving LR and we reuse r0 after. Tried: separated declarations vs initialised
  locals vs `const` locals, both orders. 69.79-71.00%.
- **`__as__rstl::single_ptr<CMoviePlayer::SIndexLoad>::operator=(SIndexLoad*)` (72 B), 0.00%** - not
  emitted at all. Retail calls this instantiation out of line from `ContinueLoading`
  (`addi r3,r30,180; li r4,0; bl` at 0x8031CD0) while inlining the same member for every other
  `single_ptr` in the TU. `RSTL_SINGLE_PTR_ASSIGN_OUT_OF_LINE` moves *all* of them (the CDvdRequest
  assignments in `ContinueLoading` would become calls and that function would get worse), and
  mwcceppc accepts but **ignores** an explicit `template <>` specialisation of the member
  (`template <> SIndexPtr& SIndexPtr::operator=(...)` compiles with no diagnostic and is never
  instantiated - the call site still inlines the primary). Needs a per-`T` opt-in in
  `include/rstl/single_ptr.hpp`; not filed as `NEW:` because it is the same unit this item already
  covers.
- **`__dt__CMoviePlayer` (316 B), 80.51%** - structural, not scheduling: retail calls
  `__dt__vector<CTHPTextureSet>(mTextures, -1)` out of line (`addi r3,r30,136; li r4,-1; bl` at
  0x8031FA8) and we inline the whole vector destruction, which is also why retail's frame is
  `-0x10` and ours is `-0x20`. 14 of the 14 extra instructions are the inlined body. Needs
  `rstl::vector`'s destructor to stay out of line at this one call site; the header has no opt-in for
  it today (`~vector` is declared in-class and defined out-of-class, so `-inline auto` still folds
  it in).
- **`ContinueLoading` (1132 B), 97.84%** - same root cause as the `__as__` entry above (the
  `mIndexLoad = nullptr` assignment) plus a r28/r29 swap in the component loop.
- **`reserve<CTHPTextureSet>` (172 B), 75.44%** - retail keeps `this` in r27, we keep it in r29.

## Not done / not filed

No `NEW:` line: everything above is either this unit's own remaining functions (already covered by a
re-run of this item) or a header-level change that would need its own gate run.

`tools/unit_fit.sh Kyoto/Graphics/CCubeMoviePlayer.cpp` reports the pre-existing 10 weak COMMOD/
template symbols in our object that the retail unit object does not carry (900 bytes, including the
`destroy<pointer_iterator<...>>` copy that retail calls `fn_80319CB4`). That count is unchanged by
this diff.