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
