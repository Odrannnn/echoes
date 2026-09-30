# progress-cgamestate-elem12-epilogue-order

**Result: GATE PASS, `goal_check` PASS.** `MetroidPrime/Player/CGameState` 94 -> **96** matched
functions (of 116), `build/report.json` `matched_functions` 9937 -> **9939**, `linked` 4896
unchanged, no function anywhere worse, `+2 functions at 100%`, no `asm`. Not committed (the
driver commits).

```
./tools/gate.sh build/goal/judge/report.base.json        # with MP_GATE_DOCS_WRITE=1, as the judge runs it
  configure ok / ninja + build.sha1 ok / hashes vs config.yml ok / report ok
  per-function diff  matched  9937 -> 9939  linked 4896 -> 4896  (+2 at 100%, 0 newly linked)
  module wiring ok / dol_read ok / docs claims ok / gs offsets ok / raw offsets ok
  decl order ok / files.cmake ok / module order ok / port probe ok / port link gap ok
  GATE PASS  a78fdb8+2 changed

./tools/goal_check.sh build/goal/item.json
  ok  no judge-owned path touched
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 9937 -> 9939   linked 4896 -> 4896
  ok  check_symbol_names.py
  ok  target rose: main/MetroidPrime/Player/CGameState: 94 -> 96 / 116 functions
  ok  no asm added
  goal_check: PASS progress-cgamestate-elem12-epilogue-order
```

Files touched: `src/MetroidPrime/Player/CGameState.cpp` only. `docs/HANDOFF.md` shows as
modified but **the judge wrote it**, not me - `gate.sh` runs `check_docs_claims.py --write` under
`MP_GATE_DOCS_WRITE=1`, and the diff is exactly the two derived lines (9937 -> 9939, DOL 8526 ->
8528). Do not hand-edit it.

## The item's reason was stale, and the real blocker was not a spelling at all

`item.json` said `fn_8014601C` is 99.05% and `fn_80145B90` is bodiless behind it, both differing
only in the epilogue's `r0` reload order. **Re-measured at this head (a78fdb8), both are already
100.00%** and `fn_80145B90` is already 100.00% too. That was fixed after the item was queued, so
the item as written had nothing left to do. Its `target` is the unit, so the work became: raise
the unit's matched count by any means.

## What the real blocker was: a *name*, not a body

The unit reported 11 functions with **no `fuzzy_match_percent` at all** - "bodiless". Three of
them were not bodiless. They were **byte-for-byte correct in our object and reported as absent**,
because **objdiff pairs functions by symbol name** and a C++ template instantiation is emitted
under its *mangled* name, which never pairs with the `fn_801XXXXXX` name retail's symbol table
gives an unnamed function.

`fn_80144818` is `rstl::vector< CHintOptions::SHintState >::operator=`. The object already held
`__as__Q24rstl63vector<Q212CHintOptions10SHintState,...>`, **all 50 instructions identical to
retail**, and the report said the function had no body. The fix is the same trick this file
already used for `fn_801447C4` (whose header comment explains why the `friend` must be declared
first): give the function retail's name and call it by that name.

`fn_80142288` is `rstl::vector< rstl::pair< CAssetId, TEditorId > >::erase( iterator )`, the
one-argument wrapper, reached from `CPersistentOptions::SetCinematicState`. Same story: the
mangled `erase__Q24rstl63vector<Q24rstl19pair<Ui,9TEditorId>,...>FQ24rstl146pointer_iterator<...>`
was already **0 differing instructions of 19**.

**This is the lesson, and it is general:** *a function can be fully decompiled and still count
as zero.* Before writing a single spelling variant, check whether a bodiless retail function is
already present in the object under a different symbol. One throwaway script -
`objdump -d` both objects, parse `<sym>:`, normalise branch targets, and for each bodiless
retail function find the same-length function in ours with a zero-instruction diff - found both
in about a minute after hours of the previous lane's spelling search had failed. **Run that
script first on any unit that reports bodiless functions.**

## Spellings, all measured (`tools/try_batch.py`, differing instructions)

`fn_80144818`, retail 50 instructions. Parameters must be **typed pointers**, not `void*`:

| spelling | differing instrs |
|---|---|
| `void* self, const void* src` + local `vector*` / `const vector*` refs | 15 |
| same, locals declared `from` before `to` | 15 |
| `void*` + `if (self == src)` instead of `to == from` | 15 |
| **`rstl::vector< SHintState >* self, const rstl::vector< SHintState >* src`, used directly** | **0** |
| typed params + `&to == &from` through local references | 0 |
| typed params + `from->mCount` instead of `from->size()` | 0 |

`void*` parameters cost 15 instructions: mwcceppc allocates `r30`/`r31` to the *typed* locals
and the comparison ends up `cmplw r31,r30` where retail has `cmplw r30,r31`, which then reverses
the sense of every later `r30`/`r31` reference in the function. Typed pointers in the signature
get the registers assigned from the parameters, which is what retail has.

Call site: `fn_801447C4` now calls `fn_80144818(&to.mHintStates, &from.mHintStates)` instead of
`to.mHintStates = from.mHintStates`. `fn_801447C4` stayed at 100.00% (21 of 21).

`fn_80142288`: the body is `return self->erase(it, it + 1);` and the two-argument `erase` stays a
template (that is retail's `fn_801422D4`, 10 instructions of diff - a separate wall).
`SetCinematicState` stayed at 100.00% (64 of 64).

## Re-measured, so the next lane does not re-derive it

`fn_80142944` (0x80142944, 104 B) is the **third** instance of this bug and is a clean next item:
`rstl::reserved_vector< rstl::vector< uchar >, 3 >::operator=`, already **byte-identical to retail
(26 of 26 instructions)** under
`__as__Q24rstl65reserved_vector<Q24rstl37vector<Uc,Q24rstl17rmemory_allocator>,3>FRC...`, reached
from `SetCompressedGameOptions` (0x80142920, `addi r3,r3,324` then a tail call). **I tried it and
reverted**: wrapping it as `extern "C" SCompressedGameOptions& fn_80142944(self, other) { return
*self = other; }` compiles to an 8-instruction *tail call* to the template, not to the body -
mwcceppc does not inline the assignment when it is the whole body. It has to be spelled out
statement by statement (`reserved_vector`'s `mCount` is `public`, so that is possible), and I ran
out of the right shape to try. **Worth one item.**

Sweep of the remaining bodiless functions, so nobody re-runs it: `fn_801465EC` (264 B) and
`fn_80142760` (124 B) are **already defined under retail's names** but in *other* units
(`CGameStateBlockReserve.cpp`, `CGameStateBlockFill.cpp`), so they are bodiless in *this* unit -
a placement question, not a spelling one. `fn_801422D4` (108 B) is the two-argument `erase`,
10 instructions of diff. `fn_80143CD4` (80 B) has no counterpart in our object at all.

Carried over unchanged from `progress-cgamestate-elem12` and not re-measured this run (this
item's budget went to the naming fix): `fn_80142944` at 91.92% of eleven spellings *as an
`SGameStateSlots::operator=`* - that reading is wrong, it is the `reserved_vector` assignment, see
above; `fn_8014680C` 92.69% of five; `fn_801466F4` 66.63% -> 91.26% of four.

## Verified, not recalled

- `fn_80144818`: `0 instructions (50 retail / 50 ours)`, and objdiff's own `diff` on it reports
  `match_percent: 100.0` against target symbol 119.
- `fn_80142288`: `0 instructions (19 retail / 19 ours)`.
- `fn_801447C4` 100.00%, `SetCinematicState` 100.00%, `SetCompressedGameOptions` 100.00% - all
  three callers unchanged.
- `report.json` after the change: unit 96/116, `fn_80144818` and `fn_80142288` both
  `fuzzy_match_percent: 100.0` (they were `None`).
- Full gate and `goal_check.sh` output quoted at the top of this file.

## Not done

- No `NEW:` line. The `fn_80142944` finding is a **measured wall in this item's own target unit**,
  not new work, and the brief says not to file a restatement of the current item; it is written
  out above instead. The remaining bodiless functions need a placement decision or a body that
  does not exist yet, which is a judgement call for the orchestrator, not a queue entry.
- `fn_801422D4` (10 instructions) and the other walls above were not re-attempted.
