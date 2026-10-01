# progress-unit-canimation

`kind: progress`, `target: Kyoto/Animation/CAnimation`. **PASS** — the unit went from 1/4 matched
functions to **4/4**, and its `.text` is now 448/448 bytes (100.00% fuzzy). All three functions the
item named are at 100.00%.

`./tools/goal_check.sh build/goal/item.json` → `goal_check: PASS progress-unit-canimation`, with
`target rose: main/Kyoto/Animation/CAnimation: 1 -> 4 / 4 functions` and
`counts: matched 11838 -> 11841   linked 5727 -> 5727`. The unit stays `NonMatching`; `flip_test.sh`
was **not** run, per the item.

## What was measured, before and after

`build/report.json`, `main/Kyoto/Animation/CAnimation`, per function:

| function | before | after | bytes |
|---|---|---|---|
| `fn_8028CAE4` | no score (0.0%) | **100.00%** | 164 |
| `fn_8028CA5C` | no score (0.0%) | **100.00%** | 136 |
| `fn_8028CA3C` | no score (0.0%) | **100.00%** | 32 |
| `__ct__10CAnimationFR12CInputStream` | 100.00% | 100.00% | 116 |

Unit `matched_functions` 1 → 4, `matched_code` 116 → 448 of 448, `fuzzy_match_percent`
25.892857 → 100.0. Tree-wide `matched_functions` 11838 → 11841.

Full-report diff against a rebuild of the stashed tree (`.tmp/report_before.json` vs
`.tmp/report_after.json`, both from `./tools/decomp_build.sh`): **3 better, 0 worse, 0 new, 0 lost**,
across all 2071 units. Nothing else moved.

## What the three functions are

Read out of the linked retail ELF with `tools/dis.sh`; the notes in the source carry the
instruction-by-instruction reading. All three are members of `CAnimationManager` in retail's source
even though `splits.txt` puts them in `CAnimation.cpp`, and all three are the kind of function this
repo writes by hand: **dtk has no name for them** (it prints `fn_<address>`), while the code they
contain is what `mwcceppc` already emits under a *mangled* template name — and objdiff pairs by
name, so no C++ declaration can make them match. Same technique and same reason as the block at the
top of `src/Kyoto/Animation/CAnimationSet.cpp`.

- **`fn_8028CA3C`** (0x20 = 32 B) is the out-of-line copy of `TToken<T>::NonConstCopy() const`.
  That same instantiation is already matched **under its mangled name** in two other units —
  `NonConstCopy__29TToken<19CTransitionDatabase>CFv` (`src/Kyoto/Animation/CTreeUtils.cpp`, 100%)
  and `NonConstCopy__32TToken<22CAnimationDatabaseGame>CFv`
  (`src/MetroidPrime/Factories/CCharacterFactory.cpp`, 100%) — which is the confirmation that this
  is the function and not something else. Body: a frame and one `bl __ct__6CTokenFRC6CToken`.
- **`fn_8028CA5C`** (0x88 = 136 B) is `CAnimationManager::GetMetaAnim(uint) const`: copy the
  manager's `TToken<CAnimationDatabase>` out, load the database through `CToken::GetObj()` (the
  `+4` is `CObjOwnerDerivedFromIObjUntyped::m_objPtr`), call `GetMetaAnim` at `vtable[0]`, and
  return the `rc_ptr` **by value** so its copy constructor and `++*mRefCount` are inlined here.
- **`fn_8028CAE4`** (0xA4 = 164 B) is the same lookup followed by
  `IMetaAnim::GetAnimationTree(sysCtx, orders)` at `vtable[1]`, returning `ncrc_ptr<CAnimTreeNode>`.

Ten retail call sites confirm the signature and the caller-side shape, all in
`src/MetroidPrime/CAnimData.cpp` (`GetAnimationPrimitives`, `CollectAnimationResources`,
`AddAdditiveAnimation`, `GetAdditiveAnimationTree`, …): every one loads the manager via
`GetAnimationManager()`, then an animation index out of `this->232`, then calls with `r3` = a
stack slot, `r4` = the manager, `r5` = the index. `fn_8028CAE4` also takes `r6` = a
`CMetaAnimTreeBuildOrders` built by `CMetaAnimTreeBuildOrders::NoSpecialOrders()` immediately
before the call at 0x80026CF4-0x80026D08.

The vtable slots are the header's own declarations, so nothing outside this change moves: an
object's vtable pointer addresses the first *function*, which is why the loads are `vtable+0` and
`vtable+4` and not `+8`/`+12`.

## Files touched

- `src/Kyoto/Animation/CAnimation.cpp` — the three `extern "C"` definitions, in **descending retail
  offset** (0x8028CAE4, 0x8028CA5C, 0x8028CA3C) above the existing constructor, with the
  reasoning for each body in comments. No `asm`, no transcribed disassembly.
- `include/Kyoto/Animation/CAnimationManager.hpp` — two inline accessors,
  `GetAnimationDatabase()` and `GetSysContext()` (marked guessed names), needed to spell the two
  calls from outside the class. Members, layout (`CHECK_SIZEOF` 0x20) and the vtable are unchanged.

`docs/HANDOFF.md`'s state block was rewritten by the tooling, not by hand, and the driver
regenerates it anyway.

## What was tried, and what the spellings cost

Measured with `./tools/decomp_build.sh Kyoto/Animation/CAnimation.cpp`, reading the result out of
`build/G2ME01/src/Kyoto/Animation/CAnimation.o`:

1. **`fn_8028CA3C` taking `const CToken*`, body `return TToken<CAnimationDatabase>(*src);`** —
   44 bytes, 62.5%. Extra `stw r31,12(r1)` / `mr r31,r3` / `lwz r31,12(r1)`: with the parameter
   typed `CToken`, `mwceppc` keeps the destination live across the call and spills `r31`. Retail has
   no spill, so the 32-byte body is unreachable that way. **Taking `const TToken<CAnimationDatabase>*`
   and returning `*src` is 32 bytes and 100.00%** — the extra base-class conversion is what stops the
   spill. That spelling is what is committed.
2. **`fn_8028CA5C` with the database in a named local** — 148 bytes, 70.91%, because the named
   pointer adds a third callee-saved register (`stw r29,20(r1)` / `mr r29,r3` / `lwz r29,20(r1)`)
   that retail does not have. Inlining the token temporary into the `return` statement removes all
   three: 136 bytes, **100.00%**, and the token's destructor lands at 0x8028CAC8, where retail has it.
3. **`fn_8028CAE4` with the database in a named local** — 144 bytes, 58.68%, and the token died one
   statement too early (retail destroys it *after* the `GetMetaAnim` `bctrl`, before the tree is
   built). Taking the address of the returned `rc_ptr` in one statement instead
   (`&(*token)->GetMetaAnim(animId)`) puts the destructor at 0x8028CB44 and makes the body 164
   bytes: **100.00%**.

`unit_fit.sh` reports the five extra functions as WEAK COMDAT copies (inline virtuals and template
destructors — `__dt__Q24rstl28TToken<18CAnimationDatabase>Fv`, `__dt__9IMetaAnimFv`,
`ReleaseData__Q24rstl18rc_ptr<9IMetaAnim>Fv`, `__dt__Q24rstl18rc_ptr<9IMetaAnim>Fv`,
`__dt__Q24rstl66basic_string<...>`), which is the "harmless cause" it names: the retail linker and
mwldeppc both discard them. Not a flip blocker on this evidence, but **`flip_test.sh` has not been
run, so the flip is unmeasured.**

## Gates

- `sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` ✓ (retail)
- `./tools/decomp_build.sh` → `87 files OK`, all 86 RELs `cmp`-equal, `All: 33.56% fuzzy,
  26.65% matched, 12.64% linked (11841 / 28465 functions)` — the `All:` line did not fall
- `python3 tools/check_symbol_names.py` → `checked 515 units; 0 declared names are missing`
- `python3 tools/check_decl_order.py --list` → this unit is **not** in the permuted list. (The
  neighbouring `main/Kyoto/Animation/CAnimationSet` is, and was before this change: 54 fns, not
  touched here.)
- `./tools/goal_check.sh build/goal/item.json` → PASS, quoted above

## For the next run

The unit is complete but `NonMatching`, and the flip is unmeasured. `NEW:` is deliberately **not**
filed for it: it is a restatement of the current item, and this item's judge already passes.
Whoever takes it next should run `./tools/flip_test.sh Kyoto/Animation/CAnimation.cpp` — the
obstruction to check first is `unit_fit.sh`'s five extra COMDAT functions, not the source.