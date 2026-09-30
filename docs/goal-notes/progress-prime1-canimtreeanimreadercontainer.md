# progress-prime1-canimtreeanimreadercontainer

`kind: progress`, target `main/Kyoto/Animation/CAnimTreeAnimReaderContainer` (DOL unit, stays
`NonMatching`; `configure.py` untouched). Prime 1 was used only as a reading of retail's source
shape - no header, class layout or name from it was copied.

## Result (measured, `build/report.json` vs `build/goal/judge/report.base.json`)

| | base | now |
|---|---|---|
| unit `matched_functions` | 24 / 28 | **26 / 28** |
| unit `fuzzy_match_percent` | 87.85% | 90.02% |
| unit `matched_code_percent` | 67.70% | 88.12% |
| whole build `matched_functions` | 10007 | 10009 |
| whole build `fuzzy_match_percent` | 30.82864% | 30.829203% |

`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:
`matched 10007 -> 10009  linked 4896 -> 4896  (+2 functions at 100%, 0 units newly linked)`,
`no regression`.

## The diff (3 lines, one file)

`src/Kyoto/Animation/CAnimTreeAnimReaderContainer.cpp` only:

1. `VClone`: `mReader->VClone()` -> `mReader->Clone()`.
2. `VGetContributionOfHighestInfluence`: `mReader->VGetSteadyStateAnimInfo()` /
   `mReader->VGetTimeRemaining()` -> `mReader->GetSteadyStateAnimInfo()` /
   `mReader->GetTimeRemaining()` (the non-virtual `IAnimReader` wrappers, which this repo's
   `IAnimReader` already declares).

Nothing else was edited. No header change survived; see the negative results below.

## Per function (item's three names, plus the unnamed one objdiff still reports)

### 1. `VClone__28CAnimTreeAnimReaderContainerCFv` - 90.54% -> **100.00%** (216 B)

**Prime 1's source matched unchanged.** Prime 1 writes `mReader->Clone()`, not `mReader->VClone()`,
and this repo's `IAnimReader` has the same non-virtual
`rstl::ownership_transfer<IAnimReader> Clone() const { return VClone(); }`, so the Prime 1 line
was usable as-is. That is the whole difference, and it is a codegen difference, not a semantic
one: retail's object *calls* the out-of-line weak `Clone__11IAnimReaderCFv` (0x8028F244, 0x38
bytes), while writing `VClone()` makes MWCC inline the virtual dispatch inline
(`lwz r12,0(r4); lwz r12,84(r12); mtctr; bctrl`, +3 instructions). Writing `Clone()` makes MWCC
emit `bl Clone__11IAnimReaderCFv`, and this unit then also carries its own weak copy of it - which
is byte-identical to retail's (verified by disassembly) and is the same COMDAT-weak kind retail
itself carries in a different unit.

### 2. `VGetContributionOfHighestInfluence__28CAnimTreeAnimReaderContainerCFv` - 87.41% -> **100.00%** (128 B)

**Prime 1's source did not match unchanged - and, unusually, Prime 1 is right and we were wrong.**
Prime 1 (and Echoes' retail bytes) call the *non-virtual wrappers*; our line had the `V`-prefixed
virtuals. Same semantics - the wrappers forward to the virtuals - but MWCC's register allocation
differs:

* with `V...`: it caches `*(&mReader)` in `r31` across the first virtual call, so it keeps three
  callee-saved registers (`r29`=return slot, `r30`=this, `r31`=reader), allocates a 64-byte frame,
  and shuffles `mr r31,r4` / `mr r4,r31`. 16 bytes differ, 87.41%.
* with the wrappers: it keeps `this` in `r31`, reloads `&mReader` from `this+20` after the first
  call, keeps two callee-saved registers and a 48-byte frame - retail's exact shape, 100%.

Retail's call order is `VGetTimeRemaining()` (vtable+20) first, then `VGetSteadyStateAnimInfo()`
(vtable+24), returning into `r1+8` and `r1+16`; our compiler already scheduled it that way with
either spelling, so no reordering was needed.

Spellings tried and measured for this function (all others held constant):
`return CAnimTreeEffectiveContribution(...)` with the virtuals 87.41%; with `(*mReader).V...`
87.41%; with local `CCharAnimTime rem` / `CSteadyStateAnimInfo info` (either declaration order)
45.4% / 45.2% - the compiler copies the 24-byte return struct instead of leaving it in the
argument slot; a named result variable then `return result;` 66.47%; `IAnimReader& reader = *mReader`
does not compile with this repo's `object_owner`; **`mReader->GetSteadyStateAnimInfo()` /
`GetTimeRemaining()` 100.00%** - the only one that matches.

### 3. `VGetWeightedReaders__...reserved_vector<pair<float,IAnimReader*>,16> const` - 79.80% -> **79.80%** (40 B). Not fixed - see below.

### 4. `fn_802A5F74` - 0.00%, 160 B (unnamed in retail; the item did not list it)

Identified, and it is reachable but not matchable today. It is
`CAnimTreeEffectiveContribution::CAnimTreeEffectiveContribution(float, const rstl::string&,
const CSteadyStateAnimInfo&, const CCharAnimTime&, u32)` - `0x802A5F74`, called from
`VGetContributionOfHighestInfluence` at `0x802A5F58` and nowhere else in the unit. Register
signature confirms it: `stfs f1,0(r3)` (weight), `__ct__rstl::basic_string` at `r3+4` from `r4`,
20 bytes of `CSteadyStateAnimInfo` copied field-by-field from `r5`, 8 bytes of `CCharAnimTime` from
`r6`, `r7` -> `+52` (`mDbIdx`). Our object emits it as the weak
`__ct__30CAnimTreeEffectiveContributionFfRC...` at the same 160 bytes, so the body is right; it sits
at 0x1E8 where retail has it at 0x1E0, because `VGetWeightedReaders` is 8 bytes too long (item 3).
objdiff does not match an unnamed target function to a differently-named base function, so it stays
at 0% until item 3 is fixed. Fixing item 3 would most likely take this to 100% as a side effect.

## Why `VGetWeightedReaders` was not fixed, and the spellings already tried

Retail's body is 10 instructions with **no branch**; ours carries two extra ones:

```
retail  9c: lwz r0,0(r4) / a0: lwz r5,20(r3) / a4: slwi r0,r0,3 / a8: add r3,r4,r0
        ac: stfs f1,4(r3) / b0: stw r5,8(r3) / b4: lwz r3,0(r4) / ... ++mCount / blr
ours    9c..a8 identical
        ac: addic r3,r3,4
        b0: beq  +0xc            <- BO=12, BI=2: tests CR0.EQ, which nothing in the function sets
        b4: stfs f1,0(r3) / b8: stw r5,4(r3)
```

The same addresses are written either way; the only difference is the spurious `addic`/`beq`,
which comes from the `new (dest) T(src)` inside `rstl::construct_impl` reached by
`reserved_vector::push_back`.

**The one spelling that reproduces retail's bytes is changing the shared header**
`rstl::reserved_vector::push_back` from `construct(data() + mCount, in); ++mCount;` to
`data()[mCount] = in; ++mCount;` - measured, that makes the function byte-identical to retail
(0x28 bytes, 100%). I did **not** make that change here: 83 files include `reserved_vector.hpp` and
`reserved_vector::push_back` is called on non-trivially-copyable element types in this repo
(`CDrawable`, `CPlane`, `CLight`, `CAreaOctTree::Node`, ...), so it is a semantic change to a
container used everywhere, not a codegen fix for this unit. That is a separate, bigger decision
than this item should make.

Negative results worth keeping (all measured on this unit, all with the rest of the file held
constant; each one changes nothing at all, byte for byte):

* A `construct_impl` overload declared in `pair.hpp`, i.e. **after** `construct`'s definition in
  `construct.hpp`, is **not** the one MWCC calls. Adding one for `pair<float, IAnimReader*>` had no
  effect; replacing its body with `out->first = 12345.f` also had no effect (the store was still
  `stfs f1,...` from the argument, not the constant). So the same late-declared overloads that
  `pair.hpp` already has for `pair<uint,uint>` and `pair<int,float>` are very likely never called
  either - not measured directly, so worth a `NEW:`-shaped check by someone with a unit that
  depends on them, but it is *not* a route to this function.
* Dispatching inside the generic `construct_impl` on
  `if (is_trivially_destructible<T>::value) *static_cast<T*>(dest) = src; else new (dest) T(src);`
  plus the `is_trivially_destructible<pair<float,IAnimReader*>>` specialisation in `pair.hpp`:
  no effect either (the `addic`/`beq` stays).
* Giving `rstl::pair` a user-provided copy constructor
  (`pair(const pair& other) : first(other.first), second(other.second) {}`): no effect.
* Local variable then `out.push_back(entry)`: no effect (79.80%, same bytes).
* `out.resize(out.size() + 1, pair(...))`: worse - 0.00%, it outlines a `resize` call.

## Gates, all run on this tree (not remembered)

```
./tools/decomp_build.sh
  All:  30.83% fuzzy, 23.10% matched, 11.74% linked (10009 / 28465 functions)   [base 10007]
  main/Kyoto/Animation/CAnimTreeAnimReaderContainer: 90.02% fuzzy, 88.12% matched (26 / 28)
sha1sum build/G2ME01/main.dol                6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                         no regression
python3 tools/check_symbol_names.py         ok
python3 tools/check_decl_order.py          ok: 957 unit(s) checked, 31 permuted, all 31 accounted for
python3 tools/check_module_wiring.py       ok
python3 tools/check_files_cmake.py          ok
python3 tools/gen_module_order.py --check  ok
python3 tools/check_raw_offsets.py         ok
python3 tools/test_dol_read.py             ok
./tools/probe_sources.sh                   probe: 749 files, 0 failed, 0 errors;
                                           link: LINKED (250 undefined, 0 duplicates)
MP_TOOLCHAIN=... ./tools/link_check.sh     unique undefined 250, duplicate definitions 0,
                                           unchanged from baseline (250 / 0)
python3 tools/link_gap.py --rebuild        ok: 246 MISSING, all accounted for in port_link_gap_list.md
```

The new `bl Clone__11IAnimReaderCFv` reference costs the port nothing: the symbol is already
defined (weak) by `CSequenceHelper.o`, `CAnimTreeSequence.o` and `CAnimTreeLoopIn.o`, so the port's
undefined count and duplicate count are both unchanged at 250 / 0.

`python3 tools/check_docs_claims.py` fails, on exactly two lines and for the expected reason - the
derived state-block counts, which the judge rewrites (`MP_GATE_DOCS_WRITE=1 tools/gate.sh`):

```
missing: 'matched    10009 / 28465 functions'  (HANDOFF state block: total matched)
missing: 'DOL units  8598 / 16726 functions'  (HANDOFF state block: DOL matched)
```

I did not edit `docs/HANDOFF.md` (instructed not to) and did not run `tools/goal_check.sh`, so
`build/goal/` is untouched apart from this notes file.

## Not a promotion

`tools/unit_fit.sh Kyoto/Animation/CAnimTreeAnimReaderContainer.cpp`: `.text` claimed 1684, ours
2056, **over by 372**, with 5 COMDAT-weak functions retail's unit object does not carry (524 bytes
total): `__ct__30CAnimTreeEffectiveContribution...` 160, `__dt__rstl::ownership_transfer<IAnimReader>`
120, `__dt__rstl::object_owner<...>` 108, `__dt__rstl::basic_string<...>` 80, and now
`Clone__11IAnimReaderCFv` 56. Four of the five were already there before this change. No flip was
attempted and none is claimed.

## Lesson

For a member function that returns a large struct, whether the two virtual calls inside it are
written as `v->VMethod()` or as `v->Method()` (the non-virtual forwarding wrapper on the base)
changes MWCC's register allocation and frame size, and retail's own source is the one that
disambiguates. When a function is a few bytes off and the extra bytes are a saved-register spill,
read retail's *callee names* in the relocation, not just the instruction stream.
