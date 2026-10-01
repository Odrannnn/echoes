# progress-unit-canimtreetweenbase

`Kyoto/Animation/CAnimTreeTweenBase`, `kind: progress`. Unit stays `NonMatching`; no `flip_test`.

## Result

**`main/Kyoto/Animation/CAnimTreeTweenBase`: 15 -> 17 / 20 functions matched** (measured from
`build/report.json` after the change; `tools/goal_check.sh build/goal/item.json` -> `PASS`, `matched
11810 -> 11812`, `linked 5727 -> 5727` unchanged, DOL sha1 and all 86 RELs held, no `asm` added).

Unit fuzzy 61.64% -> 65.15%, matched code 33.78% -> 37.29%.

## What I did

Two functions this unit never had bodies for, `fn_802AB084` (84 B) and `fn_802AB560` (84 B), both
**0.00% -> 100.00%**.

They are one shape twice over: a free function that hands the work to one child and picks the timed
or the current-pose `IAnimReader` virtual from the `rstl::optional_object<CCharAnimTime>`. `VGetSegData`
is at vtable offset `0x4C` and `VGetSegData(..., const CCharAnimTime&)` at `0x50`; `VGetSegStatementSet`
is at `0x44` and its timed overload at `0x48` (read off `__vt__18CAnimTreeTweenBase` at
`0x803B9A48` - the vptr points *at* the first virtual, so slot `n` is at `4*n`, with
`0x48`/`0x4C` for the two `VGetSegStatementSet`/`VGetSegData` pairs). Retail tests the optional's
valid byte at `+8` with `lbz` + `cmplwi` + `beq`, loads the child's raw pointer with
`lwz r3,0(r3)` (so the parameter is a `rstl::rc_ptr`/reference-to-pointer, not the node itself),
and `bctrl`s through `mtctr`. Both bodies are 84 bytes and reproduce retail byte for byte.

- `fn_802AB084` -> `child->VGetSegData(layout, data, *time)` / `child->VGetSegData(layout, data)`
- `fn_802AB560` -> `child->VGetSegStatementSet(list, setOut, *time)` / `child->VGetSegStatementSet(list, setOut)`

**The name is load-bearing, and that is the whole trick here.** objdiff pairs functions by symbol
name, and retail has no name for either of these, so dtk calls them `fn_802AB084` / `fn_802AB560`.
Written as `CAnimTreeTweenBase::GetSegDataFromChild` the body is byte-identical and objdiff still
reads **0.00%** - I measured exactly that first (see "Tried" below). `extern "C"` keeps the
identifier out of the mangler, so our object carries the same symbol dtk's does. This is
`RUNNING_THE_DECOMP.md`'s existing "name the function what the retail symbol says"; the note there
also warns that `friend extern "C"` is not accepted by mwcceppc, which is why these are free
functions at file scope rather than private statics on the class - retail's own `r3` is the child
pointer, not `this`, so a member function could not have produced these bytes anyway.

Files touched: `src/Kyoto/Animation/CAnimTreeTweenBase.cpp` only (+30 lines, two definitions in the
two slots their retail offsets require). `include/Kyoto/Animation/CAnimTreeTweenBase.hpp` was tried
with private static declarations and **reverted** - see below.

## Tried and measured

1. Private statics `CAnimTreeTweenBase::GetSegDataFromChild` / `GetSegStatementSetFromChild`, declared
   in the header. Compiles, both functions **byte-identical to retail** (verified with an objdump byte
   compare against `main.elf`), unit still reports `fn_802AB084` / `fn_802AB560` at **0.00%**,
   `matched_functions` stays 15, and `goal_check` reports `FAIL target did not rise: 15 -> 15`. The
   mangled name is the only difference from the passing version.
2. `extern "C"` free functions with dtk's names - same bodies, `17 / 20`, PASS.

## Notes for the next run

- **Declaration order.** mwcceppc emits in reverse source order, so each new definition must go in
  the slot its retail offset needs. `python3 tools/check_decl_order.py --unit
  main/Kyoto/Animation/CAnimTreeTweenBase` -> `ok` for the passing tree. Retail's order in this unit
  is `BlendSegData` (0x6D0 in our object) then `fn_802AB084` then `VGetSegStatementSet` (timed, untimed)
  then `BlendSegStatementSet` then `fn_802AB560` then `VGetRotation`, i.e. the helper sits
  **immediately after** its `Blend*` caller in the file, not next to its `VGet*` wrappers.
- **Still unmatched, with the reason, so no one re-derives it:**
  - `VSimplified` **99.46%**, 1332 B, the closest. The whole prologue through the two
    `Simplified()` calls and both `take_ownership()` blocks is instruction-identical to retail. What
    differs is the *order* of the two child-validity tests and the `simplifyA`/`simplifyB` merge:
    retail emits `cmplwi r31,0` (a plain compare) where we emit `clrlwi. r0,r31,24` (clear the low 24
    bits) for the `if (!best)` test on the `rc_ptr`, and retail's two branches at `+0x448` / `+0x4b0`
    hoist `li r0,1 / stb r0,8(r29)` (the `optional_object` valid flag) *above* the `cmplwi r29,0`
    test where we put it after. That is register allocation plus block ordering in a 1332-byte
    function, not one spelling. Left alone deliberately: it is one instruction class from 100% but
    rewriting the body risks the 17 that now hold, for at most +1.
  - `BlendSegData` 0.65% and `BlendSegStatementSet` 0.38% are empty `TODO` stubs. Prime 1's
    counterpart (`prime-ref/src/Kyoto/Animation/CAnimTreeTweenBase.cpp`) has the logic **inlined into
    its two `VGetSegStatementSet` overloads**; Echoes factored it out into these two `Blend*` helpers,
    so the donor has to be restructured, not copied. `BlendSegData`'s 612 bytes need
    `CJointData_LinearStorage` and `CCharLayoutInfo` accessors this repo does not have yet.
- `unit_fit.sh` reports 14 "extra" functions in our object (1192 B) that the retail unit object does
  not define - `Depth`, four `rc_ptr`/`ncrc_ptr`/`optional_object` template destructors,
  `ReleaseData<rc_ptr<CAnimTreeNode>>`, `__dt__CBoolPOINode`, `__dt__CPOINode`, and the two helpers
  this item added. These are COMDAT/template copies the retail linker discards, the same class
  `CAi` carries, so the unit still cannot flip; that is expected for a `progress` item and not
  something this change introduced. `.text` is 636 bytes short of the claim and `.data` 24 over.
