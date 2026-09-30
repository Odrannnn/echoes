# progress-cgamestate-near-miss-retry

Lane 1, `wt-mp2-goal-L1`, head `fa982e0`. **The item did not pass.** `goal_check: FAIL` on
exactly one check — `target did not rise: main/MetroidPrime/Player/CGameState: 100 -> 100 / 116
functions`. Everything else in the judge is green. One function was moved a long way (87.78% ->
98.78%, and the object is now byte-for-byte retail's size) but not to 100%, so the unit's matched
count is unchanged. No commit. The diff is one line in one file.

## Measured, in the order the judge reads it

    ./tools/goal_check.sh build/goal/item.json
      ok    no judge-owned path touched
      ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
      ok    counts: matched 10026 -> 10026   linked 4896 -> 4896
      ok    check_symbol_names.py
      ok    All:  30.85% fuzzy, 23.14% matched, 11.74% linked (10026 / 28465 functions)
      FAIL  target did not rise: main/MetroidPrime/Player/CGameState: 100 -> 100 / 116 functions
      ok    no asm added
      goal_check: FAIL progress-cgamestate-near-miss-retry - 1 failing check(s)

    ./tools/gate.sh build/goal/judge/report.base.json
      GATE PASS  fa982e0+1 changed    (all 17 sub-checks ok, incl. docs claims)
      per-function diff  matched 10026 -> 10026  linked 4896 -> 4896
                          (+0 functions at 100%, 0 units newly linked)  no regression

    ./tools/decomp_build.sh MetroidPrime/Player/CGameState
      main/MetroidPrime/Player/CGameState: 87.40% fuzzy, 59.09% matched (100 / 116 functions)
      AddVariable  87.78% -> 98.78%   188 bytes -> 196 bytes (retail 196)

    sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    python3 tools/check_symbol_names.py   checked 503 units; 0 declared names are missing

**The unit's own fuzzy went 87.28% -> 87.40% and its matched count did not move.** That is the
honest summary: a real improvement to one function, short of the bar the judge sets.

## The probe (the item asked for this first, and it is the main reusable finding)

Built at `.tmp/opencode/probe_gs.sh` (gitignored, so it is not in the diff). It is
`build.ninja`'s exact `mwcc_sjis` cflags for this TU — copied out of
`ninja -t commands build/G2ME01/src/MetroidPrime/Player/CGameState.o`, **including
`-i extern/musyx/include` and the four `-DMUSY_*` defines that `tools/probe_cc.sh` omits** —
through wibo + sjiswrap + mwcceppc, then `tools/bytescmp.py` / a side-by-side differ.

    $ time ./.tmp/opencode/probe_gs.sh src/MetroidPrime/Player/CGameState.cpp .tmp/opencode/probe_gs.o
    ok
    real 0m0.31s
    $ cmp .tmp/opencode/probe_gs.o build/G2ME01/src/MetroidPrime/Player/CGameState.o
    (identical)

**0.31 s and byte-identical to the object's ninja produces.** That is the whole point: it turns
"a wall I can assert" into "a wall I can test". Every number below came from it. `.tmp/` is in
`.gitignore`, so the harness leaves no trace in the diff — but that also means **it does not
survive the driver's `git clean`**, which is the one real defect of building it there. The
`NEW:` line at the end asks for it to become a `tools/` script instead.

A sweep driver ran N candidate bodies through the probe and scored each by *differing
instructions* (`tools/bytescmp.py`), not by objdiff's percentage, because the percentage is
size-dominated and cannot separate a 17-diff attempt from an 18-diff one.

## What moved: `AddVariable`, and the general form

`AddVariable` was at **87.78% / 188 bytes** against retail's 196. The object was **8 bytes short**
and the whole tail was shifted. The cause, read off the two disassemblies side by side:

Retail (0x80145B00..0x80145B18) materialises the end iterator **into the frame** and compares
against the loaded values:

    lwz  r4,16(r1) / li r3,0 / addi r0,r29,12 / stw r3,8(r1) / cmplw r4,r3 / lwz r4,20(r1) / stw r0,12(r1)

Ours kept the pair in registers and **elided both stores** — 4 instructions, 16 bytes, and the
find's sret landed at `r1+8` instead of `r1+16` so everything after it moved too.

**The fix is one character: bind `end` to a `const &`, not to a value.**

    - rstl::map<...>::iterator end = mVariables.end();
    + const rstl::map<...>::iterator& end = mVariables.end();

A by-value local lets mwcceppc prove the aggregate is dead after the compare and drop the stores.
A reference gives the temporary an **address**, so the stores are required. Result: **87.78% ->
98.78%, 188 -> 196 bytes = retail's exact size, differing instructions 36/47 -> 14/49.**

This is the same family as the `ConfigureGameModeLayers` finding from run 2 (a two-step form
beats a direct one because it keeps a local alive), and it generalises: **when retail stores an
aggregate to the frame, a C++ temporary that is only *read* will be optimised out — give it an
address and the stores come back.** It is cheap to test and worth trying before any register
argument.

## What did not move, and why (all measured on this head, all with the probe)

| function | now | what the probe says |
|---|---|---|
| `AddVariable` | **98.78%** (was 87.78%) | exact size, exact instruction sequence; **10 register numbers + 4 relocations** differ. Retail `{found r4, end.mNode r3, end.mHeader r0, bool r3}`, ours `{r0, r4, r3, r4}`. |
| `PutTo__11CWorldState` | 97.30% | 144 vs 148 bytes: still exactly one `mr r5,r31` (0x80145044) missing. |
| `__ct__11CWorldState` | 97.38% | same dead `mr r5,r31`, at 0x80145238. |
| `__ct__18CPersistentOptions` | 95.52% | 776 vs 776 bytes — **size already exact**; the frame is 160 vs retail's 176 and the save is `stmw r22,120` vs `stmw r21,132`, i.e. one callee-saved register short. |
| `PutTo__18CPersistentOptions` | 94.35% | 600 vs 600 bytes, size exact, register numbering only. |
| `PutTo__10CGameState` | 91.90% | 872 vs 876, register numbering only. |

## The `r5` wall, re-tested with a probe instead of by exhaustion

Runs 1-2 tried eight call-site spellings for the missing `mr r5,r31` before
`PutTo__16CWorldLayerStateCFR16CBitStreamWriter` (a one-parameter function) and filed a wall.
With 0.31 s per try I did **eleven more, all byte-identical to each other** (144 bytes, 12/36
differing) and none produces the `mr`:

`mLayerState->PutTo(out)` (head) · `(*mLayerState).PutTo(out)` · `mLayerState.operator->()->PutTo(out)`
· `const CWorldLayerState* pLS = …operator->(); pLS->PutTo(out)` · the same with a non-const
`CWorldLayerState*` · `rstl::ncrc_ptr<CWorldLayerState>::pointer` · `mLayerState->PutTo(out), saveWorld`
(comma) · `mLayerState->PutTo(out); (void)saveWorld;` · `mLayerState->PutTo(out); mLayerState->PutTo(out)`
(two calls) · a `const CWorldSaveGameInfo& sw` alias of the parameter used by the *previous* call ·
`CWorldLayerState& ls = *mLayerState.operator->(); ls.PutTo(out)`.

**This is now a measured wall, not an exhausted one**, and the dead-store reading stays refuted:
retail's own mangled name `PutTo__16CWorldLayerStateCFR16CBitStreamWriter` encodes one parameter,
and `CWorldLayerState`'s unit is `Matching` 15/15 with a one-parameter `PutTo`. The one
explanation I could not test is arity — declaring `PutTo` with a second parameter would change
the mangled name and break that Matching unit, so it is not available.

## One lead the next run should take, measured but not finished

`fn_80143CD4` (0x80143CD4, 80 bytes, currently **0.00%**) is retail's out-of-line
`reserved_vector< SPlayerConfig, 4 >::operator=`. It is **a leaf with no frame and no calls** —
20 instructions, `stw` the count, reload it, `mtctr`/`cmpwi`/`beqlr`, then a `bdnz` loop whose
body is `lwz`+`lbz`+`stw`+`lbz`+`stb`+`stb` (**field-wise, 6 instructions, not a merged 8-byte
move**) with a `cmplwi r5,0`/`beq` null guard on the destination.

It currently has a body, but one built on `resize()` plus a field loop: 140-148 bytes with two
out-of-line calls, 0/36 matching. Sweeping it with the probe against a local `Shadow` struct
(int count + 4 elements) and `rstl::uninitialized_copy_n` over a plain
`{uint; bool; bool}` element type took it **32 bytes -> 72 bytes, 8 -> 18 instructions, 0 -> 6
matching** — the best of nine spellings, and it still misses on:

* the copy is **2 word moves** where retail does **6 field-wise moves** (mwcceppc merges the
  8-byte aggregate; retail's `rstl` does not), and
* the cursors are `r5`/`r4` where retail has `r6`/`r5`.

The `Shadow` + `uninitialized_copy_n` + field-wise `Elem` shape is the thing to carry forward.
I stopped here rather than finish it: `fn_80143CD4` is **owned by the `bodiless-remaining` item
in this chain** (its 0x80143B94 run), and the brief says to leave another item's functions alone.

## Files touched

- `src/MetroidPrime/Player/CGameState.cpp` — `AddVariable`, one line (`iterator&` for `end`),
  plus the comment above it rewritten to record the retail addresses that justify it.
  Nothing else in the tree changed; the probe and sweep drivers live under gitignored `.tmp/`.

## Re-measure before trusting any of this

`AddVariable`'s 98.78% is a real measurement on this head, but the *judge* is what counts and it
did not rise. Two earlier runs in this chain each had their claimed numbers silently invalidated
by a rebase (run 2's note: the tree measured 94.64% and 82.88% where the table said 99.89% and
98.78%). Re-run `decomp_build.sh MetroidPrime/Player/CGameState` and read `AddVariable` out of
`build/report.json` before building on it.

## New queue items

NEW: progress-cgamestate-probe-tool | progress | MetroidPrime/Player/CGameState | the per-unit 0.31 s compile probe - build.ninja's exact mwcc_sjis cflags (including -i extern/musyx/include and the four -DMUSY_* defines tools/probe_cc.sh omits) through wibo+sjiswrap+mwcceppc - reproduces the ninja object byte-for-byte and is what found the `const &` fix below; it is currently a throwaway under gitignored .tmp/ so `git clean` destroys it, and it should be a tools/ script plus a body-sweep driver that scores candidates by differing instructions

NEW: progress-cgamestate-fndef-80143cd4 | progress | MetroidPrime/Player/CGameState | fn_80143CD4 is retail's out-of-line reserved_vector<SPlayerConfig,4>::operator= - a frameless leaf that stores the count, reloads it, and runs a bdnz loop copying 8-byte elements **field-wise** (lwz/lbz/stw/lbz/stb/stb, 6 instructions) with a null-destination guard; a local `Shadow {int; Elem[4]}` plus `uninitialized_copy_n` over a `{uint;bool;bool}` element took it 32->72 bytes and 0->6 matching of 18, and the two remaining differences are that mwcceppc merges the copy into 2 word moves where retail does not, and the cursors are r5/r4 where retail has r6/r5

NEW: progress-cgamestate-addvariable-regs | progress | MetroidPrime/Player/CGameState | AddVariable is now 98.78% with retail's exact 196-byte size and instruction sequence; the whole remainder is 10 register numbers plus 4 relocations, retail `{found r4, end.mNode r3, end.mHeader r0, bool r3}` against ours `{r0, r4, r3, r4}`, so it needs a lever on mwcceppc's allocator and not another body spelling - 24 spellings tried (const&/value/const_iterator&, ==/!=/ternary/field-wise, insert spelled three ways) all give the identical 196-byte 14-diff result

## Retry on L9 (2026-09-30): `AddVariable` now matches exactly

On `wt-mp2-goal-L9` at `1dc5f9a`, re-measurement started at `100/116` CGameState functions and
`10040` global matches. `./tools/gate.sh` passed before editing. I did not repeat the already
recorded `const &`/insert/branch variants for `AddVariable`, nor revisit the measured `r5` dead-move
walls.

Re-created `.tmp/opencode/probe_gs.sh` with the TU's exact `build.ninja` MWCC flags, including
`-i extern/musyx/include`, the four `-DMUSY_*` defines, and `-pragma "inline_max_size(125)"`.
The baseline compile took 0.322s and `cmp` against Ninja's `CGameState.o` was byte-identical.

New measured local-type spellings for `AddVariable` (instruction sequence compared after normalizing
branch targets): the plain-`iterator` baseline was not separately counted; top-level-const
`iterator` had 17 differences; `const_iterator` for `end` alone had 10; and using `const_iterator` for **both**
`it` and `end` had **0 differences (49/49 instructions)**. Direct-initializing `end` as a
`const_iterator` also had 10. The exact form kept is at `src/MetroidPrime/Player/CGameState.cpp:358-359`.
Both values are only compared, so the const-iterator conversion preserves behavior.

Verification after the edit:

```
./tools/fast_try.sh MetroidPrime/Player/CGameState
  main/MetroidPrime/Player/CGameState: 87.41% fuzzy, 60.17% matched code, 101/116 functions
./.tmp/opencode/probe_gs.sh src/MetroidPrime/Player/CGameState.cpp .tmp/opencode/probe_gs.o
cmp .tmp/opencode/probe_gs.o build/G2ME01/src/MetroidPrime/Player/CGameState.o
  identical
./tools/goal_check.sh build/goal/item.json
  PASS; matched 10040 -> 10041, linked 4903 -> 4903
  target 100 -> 101 / 116; no regressions; no asm
./tools/decomp_build.sh MetroidPrime/Player/CGameState
  All: 30.96% fuzzy, 23.22% matched, 11.75% linked (10041 / 28465)
  CGameState: 87.41% fuzzy, 60.17% matched (101 / 116); AddVariable 100.00%
sha1sum build/G2ME01/main.dol
  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
```

`configure.py:533` still marks CGameState `NonMatching`; no flip was attempted, as required for a
progress item. The judge's goal check rewrote derived HANDOFF counts; I restored that generated
diff. Final tracked diff in L9 is only `src/MetroidPrime/Player/CGameState.cpp`; no commit. No new
`NEW:` item.

