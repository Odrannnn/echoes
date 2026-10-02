# progress-unit-cgmdeathmatch

`kind: progress`, target `MetroidPrime/Player/CGMDeathMatch`, worktree `../wt-mp2-goal-L5`, head
`52504f3d`. One file touched: `src/MetroidPrime/Player/CGMDeathMatch.cpp`.

## Measured result

`tools/goal_check.sh build/goal/item.json` → **PASS** (run in the worktree, same baselines the
driver uses):

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11851 -> 11852   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.58% fuzzy, 26.70% matched, 12.64% linked (11852 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CGMDeathMatch: 10 -> 11 / 19 functions
  ok    no asm added
goal_check: PASS progress-unit-cgmdeathmatch
```

Unit, from `build/report.json` before and after:

| | matched_functions | fuzzy | matched_code |
|---|---|---|---|
| baseline `build/goal/judge/report.base.json` | 10 / 19 | 68.01108 % | 848 / 2528 |
| after | **11 / 19** | 69.582275 % | 948 / 2528 |

`main.dol` sha1 is unchanged (`6ef9b491…`); this unit stays `NonMatching`, so `flip_test.sh` was
not run and is not the acceptance test for a `progress` item.

## Per function

**`IsNearScoreLimit__13CGMDeathMatchCFRC13CStateManagerUi` — 99.76 % → 100.0 % (the counted one).**

The whole diff was two instructions. Retail tests the remaining score with `cmpwi r0,1; bgt`,
ours compiled `… < 2` to `cmpwi r0,2; bge`. Both encode "not below 2"; writing the bound as
`<= 1` makes mwcceppc emit retail's pair:

```cpp
-  return mFragLimit - GetItemAmount(mgr, playerIndex) < 2 && mFragLimit > 1;
+  return mFragLimit - GetItemAmount(mgr, playerIndex) <= 1 && mFragLimit > 1;
```

Same predicate for `int`, so nothing else changed. `< 2` / `<= 1` are the only two spellings to
try here; MWCC picks `cmpwi a,C; b<op>` for `<=` and `cmpwi a,C+1; b<other-op>` for `<`, so which
one retail wanted is visible in the immediates.

**`EndGame__13CGMDeathMatchFiR13CStateManager` — 70.13 % → 96.11 % (real decompilation, not counted).**

Retail keeps `&mPlayers[i]` in a register and uses it for all three stores; ours reloaded
`mPlayers.begin` and used `stwx`. Taking the element by reference gets it:

```cpp
  for (int i = 0; i < mPlayerCount; ++i) {
    SPlayerState& player = mPlayers[uint(i)];
    const CPlayerState& state = *mgr.GetPlayerState(uint(i));
    player.mScore = state.GetItemAmount(CPlayerState::kIT_FragCount);
    ...
```

Spellings measured, all with the `SPlayerState&` reference present:

| spelling | score |
|---|---|
| `int i`, `mPlayers[i]`, `*mgr.GetPlayerState(i)`, three `mPlayers[i].field =` | 70.13 % |
| + `SPlayerState& player = mPlayers[i]` | 82.11 % |
| + `for (uint i = 0; i < uint(mPlayerCount); ++i)` | 94.53 % |
| state lookup before the element reference | 94.42 % |
| + `for (int i …)` with `uint(i)` on both index expressions | **96.11 %** |
| same, state lookup first | 96.00 % |

`uint i` alone gives MWCC three induction variables instead of folding `mgr + 4*i` into one
pointer (94.53 %), but the bound comparison then has to be `cmplw`; retail's is `cmpw`, so the
counter has to stay signed and only the *index expressions* are cast — that combination is the
96.11 % row.

Residual at 96.11 %: **only register allocation, no instruction is missing**. Same 38 instructions
as retail, same order, same frame (`stwu r1,-48`, `stmw r25,20(r1)`, `lmw r25`). What differs is
which register holds what — ours `(4*i, state, 20*i) = (r31, r28, r30)`, retail
`(r31, r30, r28)` — plus the preheader zeroing (`li r27,0; li r31,0; li r30,0` vs retail's
`li r31,0; li r27,0; mr r28,r31`) and the loop-tail order. The scratch for the array address is
r0 in ours and r3 in retail. I could not move it; five spellings are in the table above.

**`IsGameOver__13CGMDeathMatchFv` — 77.78 %, unchanged. Wall.**

The only difference is one instruction's position: retail loads the bitfield *before* saving r31
(`lbz r0,64(r3); stw r31,12(r1); li r31,0`), mwcceppc always emits the callee-save first. Tried:

| spelling | score |
|---|---|
| `return x40_24_ \|\| CGMMultiplayer::IsGameOver();` (as it was) | 77.78 % |
| `const bool b = x40_24_; return b \|\| …;` | 77.78 % (identical bytes) |
| `if (x40_24_) { return true; } return CGMMultiplayer::IsGameOver();` | 61.33 % |
| `return x40_24_ ? true : CGMMultiplayer::IsGameOver();` | 61.33 % (13 insns, no r31 result) |

The `||` form is right — it is the only one that uses r31 as the result register at all — and the
local-variable spelling compiles to the same bytes. What is left is one instruction moved past
the prologue; that is a scheduler difference, not something source can reach from here.

**`__ct__…` 77.75 % and `Update…` 69.18 % — untouched.** Both are register allocation plus
scheduling over long stretches (see the diff below), not a wrong expression; I did not spend a run
on them.

## The four `fn_8019xxxx` functions are unreachable from C++ (measured)

`fn_801932DC` (116 B), `fn_80193350` (132 B), `fn_801939CC` (164 B) and `fn_80193B74` (96 B) are
reported with no `fuzzy_match_percent` at all: **our object emits no symbol with those names.**
`powerpc-eabi-nm` on the retail object shows all four as global `T` symbols whose names are dtk's
placeholders for addresses the symbol map has no name for; `fn_80193xxx` is not a legal C++
identifier and mwcceppc mangles every function it emits, so no spelling of our source can produce
one of them, and objdiff matches functions by name. Our object *does* contain the corresponding
code as weak template instantiations — `__dt__Q24rstl66vector<…SPlayerState…>Fv` (0x16c) is
retail's `fn_80193350`, `__ct__Q24rstl66vector<…>FiRC…` (0x8e4) is retail's `fn_801939CC` — under
their real mangled names, which is why they do not match.

**So this unit's ceiling from source is 15 / 19, not 19 / 19**, and a future run should not burn a
lane trying to match those four. (An `objdiff.json` `symbol_mappings` entry would map them, but
that is the judge's configuration, not decompilation.)

## Two things that cost time here, for the next run

1. **`build/G2ME01/obj/<unit>.o` is retail, `build/G2ME01/src/<unit>.o` is ours.** `objdiff.json`
   gives the unit `target_path: build/G2ME01/obj/…` and `base_path: build/G2ME01/src/…`, and
   `build.ninja` links `src/` for `Matching` units and `obj/` for `NonMatching` ones (checked:
   `CGameStateBlockCopyCtor` links `src/`, `CGMDeathMatch` links `obj/`). `obj/` is not a ninja
   output — `ninja -t query build/G2ME01/obj/MetroidPrime/Player/CGMDeathMatch.o` answers
   "outputs: build/G2ME01/main.elf", i.e. it is an input. It is the object dtk extracted from the
   DOL. Consequence: **while the unit is `NonMatching` the DOL sha1 gate is satisfied by retail
   bytes no matter what our source compiles to** — changing codegen here cannot break the gate,
   and objdiff is the only thing that sees the difference. Disassemble `obj/` for retail and
   `src/` for us; `build/G2ME01/asm/…/<unit>.s` is also retail and matches the DOL bytes.
2. **`build/report.json`'s per-function `fuzzy_match_percent` is what the judge reads**
   (`goal_check.sh`'s `target_rose`), so objdiff's `diff -u <unit>` one-shot output is a good
   second opinion, but the two differ: `diff` showed the two destructors at 99.82 % / 99.5 %
   while the report has both at 100.0 %. Read the report.

## For the next run on this unit

Order by cost, not by the item's `reason` (which listed `IsGameOver` second): `EndGame` at 96.11 %
is the cheapest of the four reachable ones and only needs its register allocation to fall out —
try giving MWCC the values in a different order, e.g. materialising the state pointer into a local
before the element reference, or swapping which of the three stores comes first. Then `IsGameOver`
(one instruction, scheduler), then `__ct__` and `Update`.

## Reproduce

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
./tools/decomp_build.sh MetroidPrime/Player/CGMDeathMatch.cpp   # prints the per-function scores
./tools/goal_check.sh build/goal/item.json                     # PASS
```

---

# Run 2 (worktree `../wt-mp2-goal-L7`, head `ec8d759`, 2026-10-02)

Same item re-queued, baseline re-measured on this tree: **11 / 19 matched, 69.582275 % fuzzy,
948 / 2528 matched code** — identical to the L5 run's "after", so nothing had landed elsewhere.
One file touched: `src/MetroidPrime/Player/CGMDeathMatch.cpp`.

## Measured result

`tools/goal_check.sh build/goal/item.json` → **PASS**:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12341 -> 12342   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    All:  34.86% fuzzy, 28.46% matched, 12.90% linked (12342 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CGMDeathMatch: 11 -> 12 / 19 functions
  ok    no asm added
goal_check: PASS progress-unit-cgmdeathmatch
```

| | matched_functions | fuzzy | matched_code |
|---|---|---|---|
| baseline `build/goal/judge/report.base.json` | 11 / 19 | 69.582275 % | 948 / 2528 |
| after | **12 / 19** | 76.58544 % | 1100 / 2528 |

`main.dol` sha1 still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; the unit stays `NonMatching`, so
`flip_test.sh` is not the acceptance test for a `progress` item and was not run.

## Per function

**`EndGame__13CGMDeathMatchFiR13CStateManager` — 96.11 % → 100.0 % (the counted one).**

Two independent one-character changes on top of what L5 had:

```cpp
-    SPlayerState& player = mPlayers[uint(i)];
     const CPlayerState& state = *mgr.GetPlayerState(uint(i));
+    SPlayerState& player = mPlayers[i];
```

Neither alone does it; together they reach retail's bytes. Measured, each with the other in place:

| spelling | score |
|---|---|
| `mPlayers[uint(i)]` then `state` (L5's committed shape) | 96.11 % |
| `mPlayers[i]` (int index) then `state` | 98.42 % |
| `state` then `mPlayers[uint(i)]` | 96.00 % |
| `state` then `mPlayers[i]` | **100.00 %** |
| `state` then `mPlayers.at(i)` | 100.00 % |
| `const CPlayerState* state` (pointer) then `mPlayers[i]` | 100.00 % |

**Lesson worth keeping (this is the general rule, not just this function): on this target the
index expression's *spelling* and the *order of the two reference bindings* each move the register
allocation on their own.** `mPlayers[i]` vs `mPlayers[uint(i)]` differ only by a zero-extension that
is a no-op, yet `i` vs `uint(i)` changes which register holds the induction variable; `operator[]`
takes `int idx` (`include/rstl/vector.hpp:108`), so the two spellings build the address differently
and MWCC picks a different IV. Binding order decides which of {state pointer, element pointer} is
allocated first. The same two levers are worth trying on any loop that indexes both a C++ object and
a raw array.

**`Update__13CGMDeathMatchFfR13CStateManager` — 69.18 % → 98.29 % (real decompilation, not counted).**

Three changes, all needed:

1. **The two `if` branches were the wrong way round in our source.** Retail tests
   `lbz mDead; cmplwi r0,0; beq` — branch to the *first-death* block — and lays the respawn block
   out first, so the source is `if (player.mDead) { respawn } else { first death }`, not the
   `if (!player.mDead) … else …` we had. Alone this is 93.74 %.
2. **`CPlayer* obj = mgr.GetPlayer(i);` hoisted to the top of the loop body.** Retail loads
   `mgr.mPlayers[i]` (`lwz r3,5372(r31)`) at the *top* of the loop and keeps it live; ours sank the
   load to its use and recomputed `mgr + 4*i` each iteration (`add r3,r30,r27`). 95.68 %.
   `CStateManager::GetPlayer(int)` is `return mPlayers[index];` — an array read with no side
   effects, so calling it unconditionally is behaviour-identical.
3. **`int i` instead of `uint i`** (with `uint(i)` at the `RespawnPlayer` argument). This is what
   makes MWCC strength-reduce `mgr + 4*i` into a single induction register seeded from `mgr`
   (`mr r31,r29; …; addi r31,r31,4`) exactly as retail does. 98.29 %.

Residual at 98.29 %: **only register numbering.** Same 147 instructions in the same order; retail
`{20*i, this, mgr, i, mgr+4*i} = {r27, r28, r29, r30, r31}`, ours
`{this, mgr, i, 20*i, mgr+4*i} = {r27, r28, r29, r30, r31}`. The preheader even creates the three IVs
in the same order (`mgr+4*i`, `i`, `20*i`), so the allocator's *priority* differs, not the IR.
Twelve spellings measured, all 98.29 % (identical bytes) or worse:

| variant | score |
|---|---|
| `obj` / `state` / `player` bound in that order | 98.29 % |
| `obj` last, `obj` between the other two, `state` as a pointer, `player` as a pointer, `const CPlayer*`, nested `if` instead of `&&` | 98.29 % |
| `uint i` with hoisted `obj` | 95.68 % |
| `n = mgr.GetNumPlayers()` hoisted in the `for`-init | 96.13 % |
| `player`, `obj`, `state` / `player`, `state`, `obj` orderings | 97.67 % |
| `obj`, `player`, `state` | 97.78 % |
| moving `EndGame` inside the respawn branch | 90.12 % |

**`IsGameOver__13CGMDeathMatchFv` — 77.78 %, unchanged. Wall.**

Still exactly one instruction out of place: retail emits `lbz r0,64(r3)` *before* the callee-save
`stw r31,12(r1)`, we emit it after. Nine further spellings measured this run, all worse or
byte-identical to the committed `||` form: bitwise `|` 36.56 %, `const bool` local 77.78 % (identical
bytes), `bool` local with an `if (!over)` 59.94 %, two named bools then `a || b` 60.00 %,
`if/else` both returning 61.33 %, `static_cast<bool>(x40_24_)` 77.78 % (identical), `!!x40_24_`
77.78 % (identical), ternary 61.33 %. `||` is the only spelling that uses r31 as the result register
at all; the phi materialisation `li r31,0` is correct, only the scheduler moves the load. This is
one instruction past the prologue and nothing in the source reaches it.

**`__ct__13CGMDeathMatchFiifbb` — 77.75 %, untouched.** Ours emits two instructions retail does not
(`srwi r3,r0,31` and `rlwinm r3,r0,2,31,31`, a bool→int round trip around the
`mHasFragLimit(fragLimit > 0)` value) and hoists `neg/andc` on `mFragLimit` into the prologue instead
of reloading `lwz 60(r31)` where it is used; the bitfield stores for bits 26/27 then use shift 5/6
where retail uses 5/7 because of the extra normalisation. 66 instructions to retail's 65. Untouched:
I had no lever for the extra normalisation and did not spend the run on it.

## On the four `fn_8019xxxx` names — re-measured, and L5's conclusion holds but for a weaker reason

L5 said the ceiling is 15/19 because "no spelling of our source can produce the name". That is right
about the *name*, but it also matters that **the code is already there and byte-identical**, which
L5 asserted only for two of the four. Measured on this tree, by address:

| retail | our symbol | identical? |
|---|---|---|
| `fn_801932DC` 0xe8, 116 B | `__dt__rstl::set<pair<Ui, CGameModeListener*>, …>Fv` 0xf8, 116 B | yes (33+29 insns, same text) |
| `fn_80193350` 0x15c, 132 B | `__dt__rstl::vector<CGMDeathMatch::SPlayerState, …>Fv` 0x16c, 132 B | yes (diffed instruction by instruction) |
| `fn_801939CC` 0x7d8, 164 B | `__ct__rstl::vector<SPlayerState,…>FiRC…` 0x8e4, 164 B | yes |
| `fn_80193B74` 0x980, 96 B | `free_node_and_sub_nodes__rstl::red_black_tree<…>4node` 0x9d0, 96 B | yes |

So the decompilation of those four is *done*; only the symbol name differs, and objdiff matches by
name. The only two ways to close that gap are `objdiff.json` `symbol_mappings` or renaming the
entries in `config/G2ME01/symbols.txt`, and both are judge/retail-side configuration, not
decompilation — `tools/check_symbol_names.py` explicitly documents that renaming a `fn_`/`lbl_`
placeholder breaks the 86 REL links. I did not touch either, and I would not: a reviewer reads that
as moving the measurement rather than decompiling. **Treat 15/19 as this unit's practical ceiling
from source.** This confirms L5's measurement rather than extending it.

## Method note for the next run

Per-function objdiff percentages are readable in ~3 s per variant by rebuilding only the one object
and regenerating the report, which makes a spelling sweep cheap:

```sh
NINJA=$MP_TOOLCHAIN_DIR/build/review-tools/bin/ninja
$NINJA build/G2ME01/src/MetroidPrime/Player/CGMDeathMatch.o
./build/tools/objdiff-cli report generate -o build/report.json
# then read build/report.json → units[].functions[].fuzzy_match_percent
```

Sweeps run this run: EndGame 8+8, IsGameOver 9, Update 7+6+6+6, all spellings listed above. The
remaining candidate is `Update` at 98.29 %, which needs one register-priority flip, and `__ct__` at
77.75 %, which needs a real expression change in the constructor initialiser list.

## Reproduce

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
./tools/decomp_build.sh MetroidPrime/Player/CGMDeathMatch   # prints the per-function scores
./tools/goal_check.sh build/goal/item.json                   # PASS
```
