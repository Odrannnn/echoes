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
---

# Second run (lane-1, 2026-10-02)

Re-measured on a clean tree first: the previous run's work was on the head already, 46/57,
`fuzzy_match_percent` 97.72767. Not stale - ten functions were still short.

## Result, measured

`build/report.json`, unit `main/Kyoto/Graphics/CCubeMoviePlayer`:

| | before | after |
|---|---|---|
| `matched_functions` | 46 / 57 | **47 / 57** |
| `fuzzy_match_percent` | 97.72767 | 98.74029 |
| `matched_code_percent` | 79.14314 | 79.74094 |

`tools/goal_check.sh build/goal/item.json` -> `PASS progress-unit-ccubemovieplayer`
(`matched 12265 -> 12266`, `linked 5863 -> 5863`, no asm added, gate clean: DOL sha1, 86 RELs,
report diff, wiring, docs claims, port probe).

Per function, before -> after (nothing else in the unit moved, and nothing anywhere got worse -
`tools/report_diff.py`, which is part of the gate, flags any drop over 1e-6):

| function | before | after |
|---|---|---|
| `__as__Q24rstl40single_ptr<Q212CMoviePlayer10SIndexLoad>FPQ212CMoviePlayer10SIndexLoad` | unmapped (0.00%) | **100.00%** |
| `ContinueLoading` | 97.84% | 99.49% |
| `reserve<CTHPTextureSet>` | 75.44% | 91.26% |
| `~vector<CTHPTextureSet>` | 90.06% | 90.30% |
| `clear<CTHPTextureSet>` | 78.33% | 78.83% |
| `fn_80317FF8` | 71.00% | 77.00% |
| `__dt__CMoviePlayer` | 80.51% | 80.51% |
| `fn_8031A8F8` / `fn_80318030` / `fn_8031AA24` / `DecodeFromRead` | 92.31 / 90.00 / 97.65 / 98.00% | unchanged |

`tools/unit_fit.sh Kyoto/Graphics/CCubeMoviePlayer.cpp`: byte-for-byte the same output as before
this run (the same 10 pre-existing weak COMMOD/template symbols, 900 bytes; `.sdata2` over by 4).

## The one function taken to 100%: the out-of-line `single_ptr::operator=`

`__as__Q24rstl40single_ptr<Q212CMoviePlayer10SIndexLoad>FPQ212CMoviePlayer10SIndexLoad`, 72 bytes.
This is what the previous run recorded as "not emitted at all"; the header opt-in it says is
needed does exist and works, it is just TU-wide.

`RSTL_SINGLE_PTR_ASSIGN_OUT_OF_LINE` (already in `include/rstl/single_ptr.hpp` and already used
by `src/MetroidPrime/PathFinding/CPathFindArea.cpp`) turns **every** `single_ptr<T>::operator=(T*)`
in the TU into a call. Retail inlines all of them except this one, so the other nine call sites
are written out by hand through a `static inline` template whose body is the member's own:

```cpp
template < typename T >
static inline void mp_assign(rstl::single_ptr< T >& slot, T* const ptr) {
  delete slot.mPtr;
  slot.mPtr = ptr;
}
```

`mIndexLoad = nullptr;` is left as a plain `=`, so the only reference left is the one retail
makes, and the object carries exactly one weak `__as__` symbol - the same one retail's object
carries. That also fixes the call site: retail's `addi r3,r30,180 ; li r4,0 ; bl` replaces our
five inlined instructions, which is most of `ContinueLoading`'s 1.65 points.

**Every** other `operator=(T* const)` site has to be converted, not most of them. Measured: with
five of the nine left as plain `=`, the unit dropped 46 -> 44, because each one emits a second
weak `__as__<...>` copy retail does not have and the caller grows a `bl` -
`PostDVDReadRequestIfNeeded` 100 -> 94.42%, `PrefetchNextFrame` 100 -> 94.60%, `Rewind`
100 -> 84.29%. The sites are the three `mIndexLoad->m*Request` ones, `mPrefetchBuffer` (three),
`mRequest = nullptr` and the two in `CancelReadRequests`; `nullptr` needs
`static_cast<CDvdRequest*>(nullptr)` etc., because the template's second parameter is `T* const`
and a literal `0` will not deduce.

### Dead ends for the same function (mwcceppc GC/2.7, all measured this run)

- `template rstl::single_ptr<SIndexLoad>& rstl::single_ptr<SIndexLoad>::operator=(SIndexLoad* const);`
  - *"illegal explicit template instantiation"*. The explicit *specialisation* the previous run
  tried is different, and equally dead.
- Taking the address of the member
  (`single_ptr<SIndexLoad>& (*const p)(SIndexLoad* const) = &...::operator=;`) - compiles,
  **no symbol is emitted**: `-O4,p` folds the constant away. `nm` before/after is identical.
- An `asm("__as__Q24rstl40single_ptr<...>FPQ212CMoviePlayer10SIndexLoad")` label, on the
  definition or on a separate declaration - *"type cannot be made into a global register
  variable; only scalers, doubles, floats and vectors are supported"*. mwcceppc only accepts the
  label on a `void` function (which is all `src/MetroidPrime/PortReachStubs.cpp` uses it for),
  and this member returns `single_ptr&`, so there is no `void` spelling that emits `mr r3,r30`.
  The TU-wide opt-in is the way.

## The two bounds are one local, not two

`clear`, `~vector` and `reserve` all pass `&last.x4_current` and `&first.x4_current` to
`fn_80317FF8`, and retail holds the two 8-byte iterators at **consecutive frame slots with the
end first** (`r1+8`, `r1+12`, then the first element at `r1+16`, `r1+20`) - which is why its
`addi r4,r1,12` (second argument) comes before `addi r3,r1,20`. Two separate locals get their
slots the other way round, because mwcceppc hands locals out in reverse declaration order, and
that is the whole of the previous run's "the two requirements pull in opposite directions":
swapping the declaration order swaps which one gets the *initialisation* order but not the slots
in the shape retail has. A struct's members are laid out in declaration order, so
`struct CMovieTextureRange { CMovieTextureIterator last, first; }` reproduces it
(`clear` 78.33 -> 78.42, `reserve` 75.44 -> 91.05, `~vector` 90.06 -> **90.00**, i.e. a
regression - see below).

Dropping `volatile` from `CMovieTextureIterator::x0_owner` is the other half: with both bounds in
one object all four stores are emitted without it, and it is worth another
(clear 78.42 -> 78.83, reserve 91.05 -> 91.26, ~vector 90.00 -> 90.30). So the final shape is
clear 78.83, reserve 91.26, ~vector 90.30, all three above where the previous run left them.

`reserve` also had a real structural bug, not just scheduling: the old body reused the two
address-taken locals for the second call, so `fn_8031A88C`'s arguments had to be reloaded from
the frame and the function needed 32 bytes of frame against retail's 48. The second call takes
its bounds **by value**, so pass the expressions: `fn_8031A88C(mItems, mItems + mCount)`. With
that plus the range struct the frame size matches.

### Dead ends for the range (all measured this run)

- Declaration order of two separate `CMovieTextureIterator` locals, both ways: identical bytes
  (the slots do not follow declaration order). This is what the previous run recorded.
- Init-list order reversed (`: first(items), last(items + count)`), the two-element array form
  (`CMovieTextureIterator range[2]; range[0] = ...; range[1] = ...`), and passing the end pointer
  into the constructor - all three produce **byte-identical** code to the struct form. Only
  struct-versus-separate-locals matters, not the order inside it.
- Both members `volatile` (clear/reserve/`~vector` unchanged at 78.42/91.05/90.00), and
  `first(items + 0)` (unchanged).
- `volatile` kept but with the struct: `~vector` 90.00 against 90.06 before the change, so the
  combination is a measured regression and the `volatile` has to go for `~vector` to improve.

## `fn_80317FF8`, 71.00% -> 77.00%

Same story, one level down: retail holds the two bounds as one 8-byte local (`end` at `r1+8`,
`begin` at `r1+12`). An anonymous `struct { CMovieTexture* end; CMovieTexture* begin; }` gets
77.00%; a two-element array gives the same bytes. The remaining two instructions are retail's
`lwz r5,0(r4)` scheduled *before* the `stw r0,20(r1)` that saves LR, which puts `*last` in r5
instead of r0 and moves the first store after both `addi`s. Measured worse: reversed assignment
order 69.79%, `const` array with a `const_cast` to take the addresses 61.29%.

## Walls (spellings tried **this** run, so the next run can skip them)

- **`__dt__CMoviePlayer` (316 B), 80.51%.** Structural, and I now have the exact reason: retail
  calls two member destructors out of line with the MWCC flags argument -
  `addi r3,r30,180 ; li r4,-1 ; bl __dt__single_ptr<SIndexLoad>` and
  `addi r3,r30,136 ; li r4,-1 ; bl __dt__vector<CTHPTextureSet>` - and we inline both (14
  instructions of it, and a 32-byte frame against retail's 16).
  `#pragma inline_max_size` is the only knob mwcceppc gives, and it is TU-global (the trap
  `src/WorldFormat/CAreaOctTree_Tests.cpp` documents): at **100** the vector destructor stops
  being inlined and this function goes 80.51 -> **96.70%**, but `mp_destroy_val` (the
  `static inline` helper that is the only reason `fn_80319CB4` is at 100%) stops being inlined
  too and drops to 37.15%, so the unit goes 47 -> 46. 110 and 120 are identical to the default
  125 for every function in the unit. Making `mp_destroy_val` non-template does not help
  (`inline_max_size` bounds it either way). A per-instantiation "do not inline this member" does
  not exist in mwcceppc 2.7 - the `operator=` case above had to be solved by writing the member's
  body out at the other call sites instead, and a destructor has no such spelling.
- **`fn_8031A8F8` (104 B) 92.31% and `fn_80318030` (80 B) 90.00%**: unchanged, still the
  one-instruction placement the previous run measured - retail loads `*first` into r31 between
  the two register spills, we load it after `mr r29,r4`. Six spellings tried in the previous run,
  none by me; I did not re-try them.
- **`fn_8031AA24` (68 B) 97.65%, `DecodeFromRead` (284 B) 98.00%, `ContinueLoading` 99.49%**:
  unchanged for the same reason (register choice / one instruction's position). Nothing new
  measured on these three.

## Not done / not filed

No `NEW:` line. Everything left is this unit's own remaining functions, which a re-run of this
item already covers, or a mwcceppc inliner behaviour that is not a target any tool can measure.

The `~vector<CTHPTextureSet>` destructor is *not* `Matching`-able in a way that helps while
`clear`/`reserve` still pass the range by address, and `rstl::vector`'s destructor has no
opt-in for the "stay out of line at this one member-destruction site" that `~CMoviePlayer`
needs - that is a header change (`include/rstl/vector.hpp` is included by 146 files under
`include/` and `src/`) and belongs in its own item, not here.
