# progress-unit-cgmmultiplayer

`kind: progress`, `target: MetroidPrime/Player/CGMMultiplayer`. Unit stays `NonMatching`;
`flip_test.sh` was deliberately not used to decide anything.

## Result

**18 -> 29 of 34 functions matched.** `build/report.json`, unit
`main/MetroidPrime/Player/CGMMultiplayer`: 57.77% -> 91.13% fuzzy, 31.62% -> 64.69% matched code.
Project-wide `matched_functions` 11847 -> 11858 (`+11 functions at 100%, 0 units newly linked`,
`tools/report_diff.py` prints `no regression`).

`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS progress-unit-cgmmultiplayer`**,
all seven checks ok.

## The item's own hint was misleading, and the reason is worth carrying forward

`item.json` ranked the targets "the closest and smallest unmatched": `RespawnPlayer` 98.1%,
`NotifyListeners` 93.8%, `ChooseSpawnPoint` 85.3%, then a list of `fn_*` at 0.0%. **The three
named functions are the expensive ones and the `fn_*` list is where all the value is.**

The 13 `fn_*` functions were never "unmatched code". They are the unit's *unnamed retail weak
COMDAT copies*: dtk could not name them, so `config/G2ME01/symbols.txt` carries `fn_8019....`
placeholders, while our object has been emitting the real template instantiation under its proper
mangled name all along. objdiff pairs **by name**, so a byte-perfect function scored 0.00%. This is
exactly the case `docs/RUNNING_THE_DECOMP.md` records under "The same holds for weak *functions*
named `fn_`" (the CABSIdle note, 2026-09-29) - name them and the bytes already match.

Measured before touching anything (`.tmp/opencode/rename.py`, which disassembles the retail object
`build/G2ME01/obj/...` and ours and compares **raw instruction words**, not mnemonics, so a
relocation is not mistaken for a difference):

| retail `fn_` | bytes | our symbol with identical words |
|---|---|---|
| `fn_801969DC` | 48 | `__as__19CStaticInterferenceFRC19CStaticInterference` |
| `fn_80196A0C` | 192 | `__as__vector<CStaticInterferenceSource>::operator=` |
| `fn_80196ACC` | 12 | `clear__vector<CStaticInterferenceSource>` |
| `fn_80196AD8` | 88 | `__as__reserved_vector<CPlayerState::CPowerUp,109>` |
| `fn_80196E54` | 292 | `erase__red_black_tree<pair<Ui,CGameModeListener*>>` (const&) |
| `fn_80196F78` | 136 | `erase__red_black_tree` (iterator) |
| `fn_80197000` | 120 | `equal_range__red_black_tree` |
| `fn_80197078` | 104 | `upper_bound__red_black_tree` |
| `fn_801970E0` | 104 | `lower_bound__red_black_tree` |
| `fn_801976A0` | 136 | `__distance<red_black_tree::iterator>` |
| `fn_80197728` | 488 | `insert_into__red_black_tree` |
| `fn_801968D4` | 264 | **no exact match** - real difference, left alone |
| `fn_80197910` | 104 | **no exact match** - real difference, left alone |

`fn_80196ACC` was ambiguous on size alone (three 12-byte symbols); the disassembly settled it:
`clear__vector<CStaticInterferenceSource>` is `li r0,0 / stw r0,4(r3) / blr` and it is what
`fn_80196A0C` (`operator=`) calls on the empty-source path, which the call graph confirms.

Two size-ambiguous pairs were resolved by the same disassembly rather than guessed: `fn_80196F78` /
`fn_801976A0` (both 136 B) are the *iterator* `erase` and `__distance` respectively, and
`fn_80197078` / `fn_801970E0` (both 104 B) are `upper_bound` and `lower_bound`. Getting these four
backwards would have cost 480 bytes of false 100%s.

## Files changed

**`config/G2ME01/symbols.txt`** - 11 lines: the `fn_8019....` placeholder replaced by the symbol our
object already emits, at the same address and size. Nothing else in the file was touched, and
`scope:weak` was **not** added: it is not needed here (measured - the renamed unit scores 29/34 and
the DOL sha1 holds with and without it), and adding it would have edited 11 more lines for nothing.
`tools/report_diff.py` reports each as `RENAMED ... (0.00% -> 100.00%)`, which is the tool's
designed handling of a rename, not a deletion.

**`src/MetroidPrime/Player/CGMMultiplayer.cpp`** - two lines, both spelling changes, no behaviour
change:

- `NotifyListeners`, line 96: `iterator` -> `const_iterator` on the loop. 93.76% -> 95.00%. The
  loop only reads, and this is the spelling retail's codegen agrees with.
- `RespawnPlayer`, line 138: bind `const uint idx = playerIndex;` once and use it for both
  `mSpawnPoints[idx]` and the `ChooseSpawnPoint` argument. 98.15% -> 99.89%. Without it the
  compiler keeps `playerIndex` live in two registers and emits one extra `mr` in the prologue
  (+4 bytes, 716 vs 712). `idx` is a rename of the same value, not a new one.

`docs/HANDOFF.md` is the **judge's** rewrite (`MP_GATE_DOCS_WRITE=1`), not mine; the driver
discards it.

## Per-function, before -> after

| function | before | after | what was tried |
|---|---|---|---|
| `RespawnPlayer` | 98.15% | **99.89%** | `const uint idx` local: 98.15->99.89. Also tried and **worse**: temp for the requested id (99.17), `const TUniqueId req` (99.17), `&mSpawnPoints[idx]` (99.89), `idx` for the later uses (99.30 / 99.27 / 97.78), `GetPlayer` (98.15), swapping the state/player fetches (98.15), `requested` local only (97.26). |
| `NotifyListeners` | 93.76% | **95.00%** | `const_iterator`: 93.76->95.00. Tried and **worse**: `rstl::set& listeners = mListeners` (87.63, and 90.98 with const_iterator), range-`for` (61.35), `&*begin()` pointer walk (61.35), explicit `end` variable (85.69), `while` loop (85.69), duplicating the early-return guard (91.71), `targetIndex != uint(-1)` wrapping the loop (27.12), `0xFFFFu` (21.22), `targetIndex == it->first` (95.00, no change), `continue`-shaped body (95.00, no change), `(*it).first` (95.00, no change). |
| 11 weak COMDAT copies | 0.00% | **100.00%** | renamed in `symbols.txt`; bytes already identical. |
| `ChooseSpawnPoint` | 85.29% | 85.29% | **not attempted this run** - see below. |
| `fn_801968D4` | 0.00% | 0.00% | left `fn_`; a real content difference (see below). |
| `fn_80197910` | 0.00% | 0.00% | left `fn_`; no same-size candidate is byte-identical. |

`unit_fit.sh` still reports `.text` over by 1300 B and 28 extra weak functions. That is expected
and unchanged: they are COMDAT copies both linkers drop once the unit is `Matching`, and the unit
is not close to flipping.

## What is left, for the next run

**`ChooseSpawnPoint`, 560 B at 85.29%, is untouched and is now the biggest single item in the
unit.** Its instruction stream differs structurally, not by register allocation. From the
side-by-side, retail:

- loads `mgr->x810` (`lwz r31,2064(r26)`) and the object-list head `lha r30,8200(r31)` **before**
  the `candidates.reserve(16)`, where ours loads it after;
- indexes players through `addi r0,r23,5372 ; lwzx r4,r26,r0` (an indexed load off a running
  counter in `r23`) where ours keeps a pointer in `r23` and does `lwz r4,5372(r23)` - i.e. retail's
  loop counter and our base pointer are the same register with a different induction;
- tests the distance with `fcmpo cr0,f28,f0 ; cror eq,gt,eq ; bne` (a `>=` written as a fused
  compare plus a `cror`) where ours emits `fcmpo ; bge`. Same predicate, different spelling - this
  is the single cheapest thing to try first;
- ends the loop with `cmpw r4,r0 ; cmpwi r4,0 ; bne` (a capacity test) where ours has
  `cmpw r4,r0 ; bne`, and takes a `bl <vector dtor>` on the empty path where retail's tail
  differs. Ours calls `Random()->Range` and the vector destructor on both exits; retail only on
  one.

The `lha r30,8200(r31)` and `lwz r31,2064(r26)` are `CStateManager`'s object list and its count -
worth reading `CObjectList::GetFirstObjectIndex` / `GetNextObjectIndex` against before trying
spellings, because the loop shape follows from those.

**`fn_801968D4` (264 B) is `CPlayerState::operator=`, and our version is genuinely different** -
it is not a naming problem and must not be renamed. Retail copies fields in 4-byte pairs
(`lwz r5,0(r31) ; stw r5,0(r30)`, offsets 0/8/16/... in strides of 8) and loads a `lbz r0,4(r31)`
for a byte at +4. Ours reads `lwz r5,0(r4) ; lwz r0,4(r4) ; lbz r0,4(r4)` - it loads the word at
+4 and then immediately **overwrites `r0` with the byte at +4**, storing the byte. So ours stores
`r4` (a *word*, the second dword) where retail stores the *first dword*. Retail also copies
`0x48..0x54` (72..84) as **floats** (`lfs`/`stfs`, four of them) where ours copies 72/76/80 as
floats but 84 as a **word** (`lwz r0,84(r31) ; stw r0,84(r30)`). Two separate field-layout
disagreements in one 264-byte function: something at `+4` is a byte, and something in `0x48..0x57`
is four floats. That is a header-layout question in `CPlayerState`, not a `CGMMultiplayer` one, so
it belongs in a `CPlayerState` item.

**`fn_80197910`, 104 B**, has no byte-identical same-size symbol in our object; it is genuinely
unmatched. Not characterised beyond that.

## Nothing filed as `NEW:`

No `NEW:` line. The three items that would be worth a lane's hour are all *inside* this unit
(`ChooseSpawnPoint`, `fn_801968D4`, `fn_80197910`) and are written up above rather than requeued as
placeholders, which is what the brief asks for. The one genuinely different target is
`fn_801968D4`'s root cause, a `CPlayerState` field-layout disagreement - but `CPlayerState` is
already the subject of a different, better-scoped item, and filing it here would duplicate it with
weaker information.

No `WALL:` line. The two functions I pushed (`RespawnPlayer`, `NotifyListeners`) are *not* at a
wall: each improved in this run, and the residue is 1-2 register-allocation `mr`s I did not
exhaust. `RespawnPlayer`'s last difference is exactly two swapped `mr`s at +0x2C/+0x30
(`mr r6,r29 ; mr r5,r28` where retail has `mr r5,r28 ; mr r6,r29`); `NotifyListeners` now emits an
**instruction-for-instruction identical stream** (49 vs 49) differing only in which callee-saved
register each of the five arguments landed in. Both are one scheduling decision from 100% and are
worth a short run, not a wall.

## Gates (all re-measured on this tree)

- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` - unchanged.
- All 86 RELs `sha1`-equal to `config/G2ME01/config.yml`; the independent re-hash in `gate.sh`
  step 3 reports no mismatch.
- `./tools/probe_sources.sh` -> `744 files, 0 failed, 0 errors; link: LINKED (324 undefined,
  0 duplicates)`.
- `python3 tools/check_symbol_names.py` -> `checked 515 units; 0 declared names are missing`.
- `./tools/decomp_build.sh` -> `All: 33.60% fuzzy, 26.71% matched, 12.64% linked (11858 / 28465)`.
- `total_functions` still **28465**.
- `tools/report_diff.py` vs `build/goal/judge/report.base.json` -> `no regression`;
  `linked 5727 -> 5727` (unchanged, as expected for a `progress` item).
- Port undefined count re-measured **both ways**: 324 with the change stashed and 324 with it
  applied, `link_check: unchanged from baseline`. (A stale `build/goal/check-link.log` from an
  earlier run reads 249; it predates this work and is not what this tree produces.)
- `./tools/goal_check.sh build/goal/item.json` -> **PASS**, all checks ok, `target rose:
  main/MetroidPrime/Player/CGMMultiplayer: 18 -> 29 / 34 functions`, `no asm added`.

## A note on the reviewer's "count gamed" rule

`docs/goal-review-prompt.md` rule 7 rejects a `progress` count that is bought by emptying a
function. Nothing here does that: the 11 renames change **no bytes at all** - the renamed symbols
were already emitted, already byte-identical, and already in our object; only the *name objdiff
pairs on* was a placeholder. The two source edits are spelling changes on values that already
existed (`const_iterator` on a read-only loop; `idx` bound from `playerIndex`). No initialisation
was dropped, no call removed, no stub introduced, no `asm` added.

---

# Run 2 (lane 4, 2026-10-02) — `RespawnPlayer` taken to 100%

## Result

**29 -> 30 of 34 functions matched.** `build/report.json`, unit
`main/MetroidPrime/Player/CGMMultiplayer`: `matched_code` 64.69% -> **78.38%**, fuzzy
91.13% -> 91.15%. Project-wide `matched` **12353 -> 12354**, `linked` 5863 -> 5863
(unchanged, as expected for a `progress` item).

`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS progress-unit-cgmmultiplayer`**,
all seven checks ok (`target rose: main/MetroidPrime/Player/CGMMultiplayer: 29 -> 30 / 34`).

## The whole change is one line, and it is a spelling change

`src/MetroidPrime/Player/CGMMultiplayer.cpp:139`

```diff
-  const TUniqueId spawnId = ChooseSpawnPoint(mgr, idx, mSpawnPoints[idx]);
+  const TUniqueId spawnId = ChooseSpawnPoint(mgr, playerIndex, mSpawnPoints[idx]);
```

`idx` is still used, for the `mSpawnPoints[idx]` index; the call's second argument now names the
*parameter* `playerIndex` instead of the local that was bound from it. Both are the same value on
every path. This is the fix for the residue the previous run left: two swapped argument
`mr`s at `+0x2C`/`+0x30`.

**`RespawnPlayer` is not just fuzzy-100% — all 178 instruction words are byte-identical to
retail's.** Measured by extracting `.text` from `build/G2ME01/obj/MetroidPrime/Player/
CGMMultiplayer.o` (retail) and `build/G2ME01/src/MetroidPrime/Player/CGMMultiplayer.o` (ours) and
comparing the 0x2C8 bytes raw: **0 of 178 words differ.** objdiff's json still lists 7 textual
differences, all pure relabels, and none of them is an instruction difference:

- 5 branch targets, every one of them a constant `+0xFC` (our `.text` base is 0xFC past retail's,
  because our object carries 28 extra COMDATs);
- `bl fn_801968D4` vs `bl __as__12CPlayerStateFRC12CPlayerState` — one relocation, and
  `fn_801968D4` is dtk's placeholder for that same `CPlayerState::operator=`;
- `lfs f0, lbl_8041CB30@sda21` vs `lfs f0, @1045@sda21` — one `.rodata` address, two labels.

That also means objdiff's strict `match_percent` stays at 99.86 forever while report.json's
`fuzzy_match_percent` reads 100.00; the judge counts the latter.

## The finding worth keeping: what moved `RespawnPlayer`, and why

The argument registers for the virtual `ChooseSpawnPoint` call are `r4 = this`, `r5 = mgr`,
`r6 = playerIndex`, and the loop-invariant values already sit in `r28 = mgr`, `r29 = playerIndex`.
Both sides emitted the same three `mr`s in the same two groups; only the order *inside* the
argument-setup group differed (`mr r5,r28 ; mr r6,r29` against `mr r6,r29 ; mr r5,r28`). Nothing
about register *assignment* was wrong, so every variant that only renamed or rebound the local
(`CStateManager& smgr`, `const TUniqueId req`, `self.`, `this->`, `uint` vs `const uint`, casts)
reproduced the identical instruction stream — the mapping from incoming register to
callee-saved register is what has to change, and a local whose only use is the argument does not
change it.

Using the parameter directly at the call site does change it: `idx` stays mapped to the `mSpawnPoints`
load and `playerIndex` gets its own pseudo-register for the argument, and the two incoming registers
then reach the argument registers in the order retail emits them.

### Spellings measured this run for `RespawnPlayer` (all on this tree, base = 99.89%)

| spelling | % |
|---|---|
| **`ChooseSpawnPoint(mgr, playerIndex, mSpawnPoints[idx])` with `idx` bound above** | **100.00** |
| `idx` bound, used for both the index and the argument (previous run's best) | 99.89 |
| `CGMMultiplayer& self = *this; ... self.ChooseSpawnPoint(mgr, idx, ...)` | 99.89 |
| `this->ChooseSpawnPoint(mgr, idx, mSpawnPoints[idx])` | 99.89 |
| `this->mSpawnPoints[idx]` | 99.89 |
| `CStateManager& smgr = mgr;` then `ChooseSpawnPoint(smgr, idx, ...)` | 99.89 |
| `CStateManager* const pmgr = &mgr;` then `ChooseSpawnPoint(*pmgr, idx, ...)` | 99.89 |
| `const_cast<CStateManager&>(mgr)` | 99.89 |
| `static_cast<TUniqueId>(mSpawnPoints[idx])` | 99.89 |
| `TUniqueId spawnId = ...` (non-const) | 99.89 |
| `const TUniqueId req = mSpawnPoints[idx];` then pass `req` | 99.17 |
| `mgr` local first, then `idx`, then `req` | 99.17 |
| `const int idx` cast to `uint` at the call | 99.30 |
| `uint idx` (non-const) | 98.15 |
| `TUniqueId spawnId` non-const *and* `uint idx` non-const | 98.15 |
| `CStateManager& smgr = mgr;` with no `idx` at all | 98.15 |
| `ChooseSpawnPoint(mgr, playerIndex, mSpawnPoints[playerIndex])`, no `idx` | 98.15 |
| `static_cast<CGameMode*>(this)->ChooseSpawnPoint(...)` | build error (not in `CGameMode`) |
| `const CStateManager& smgr = mgr;` | build error (`const CStateManager` -> `CStateManager&`) |

**Do not repeat these.** The one that works is the only one in which the local and the parameter
are both live and used for different things.

## `NotifyListeners` is a register-allocation wall (measured this run, 20 spellings)

The two streams are 50 instructions each and **structurally identical**; four instructions differ
only in which callee-saved register each of three values landed in.

| | r28 | r29 | r30 |
|---|---|---|---|
| retail | `event` (from `r7`) | `value` (from `r8`) | loop base (`addi rX, r3, 0x18`) |
| ours | `value` (from `r8`) | loop base | `event` (from `r7`) |

So ours allocates incoming registers in the order `r6, r4, r5, r8, base, r7, root` and retail in
`r6, r4, r5, r7, r8, base, root`. Note this is a *permutation of the operands*, not of the
assignment: swapping the two parameters in the header, or reversing them at the call site, would
swap which incoming register feeds `r28` but would not produce retail's `mr r28,r7 ; mr r29,r8`, so
those two are not a way in. Tried this run, all 95.00% or worse and all producing the identical
prologue: local for `event`; local for `value` (91.02); locals for both (89.49); locals declared
before the early-return guard (95.00); local for `mgr`; local for the listener pointer; local for
`sourceIndex` (90.31); local for `targetIndex` used only in the loop test; `targetIndex == it->first`;
`(*it).second->...`; `static_cast<const void*>(value)`; `const rstl::set<TListener>&` (81.31) and
non-const `rstl::set<TListener>&` (90.98) for the loop bound; `const CGMMultiplayer& self = *this`
(85.69). Previous run also tried and rejected: `rstl::set& listeners`, range-`for`, `&*begin()`
pointer walk, explicit `end` variable, `while` loop, duplicated guard, `targetIndex != uint(-1)`
wrapping the loop, `0xFFFFu`.

WALL: NotifyListeners__14CGMMultiplayerFR13CStateManagerUiUiQ214CGMMultiplayer10EGameEventPCv 95.00% - 50/50 instructions identical, the four that differ are a 3-way permutation of r28/r29/r30 among {event, value, loop base}; 20 spellings this run all reproduced the same prologue, and the parameter order is fixed by five callers that are already at 100%

## `ChooseSpawnPoint` (560 B, 85.29%) — untouched in the diff, but re-measured

The previous run's four structural claims, re-measured on this tree. Two were wrong or inverted,
one turned out not to be actionable.

**Hoisting the object-list load is a red herring.** Retail loads `lwz r31, 0x810(r5)` in the
prologue, before the three `mr`s that set up `candidates.reserve(16)`; ours loads
`lwz r31, 0x810(r26)` immediately after the `bl reserve`. Moving
`CObjectList& objects = mgr.ObjectListById(kOL_All);` above `candidates.reserve(16)` in the source
changed **nothing**: 85.29% either way. It is a scheduling decision, not a statement-order one.

**The player-index loop was described backwards.** Retail uses `r23` as a *pointer* induction
variable and `r22` as the counter - `cmplw r22, r27`, `lwz r4, 0x14fc(r23)`, `addi r23, r23, 4`,
`addi r22, r22, 1`. Ours uses `r23` as a byte offset off `r26` - `addi r0, r23, 0x14fc`,
`lwzx r4, r26, r0`, with `r22` the counter. So it is **retail** that strength-reduces the address
into a pointer and **ours** that keeps an indexed load; the previous note read it the other way
round.

**The `>=` spelling is real and is cheap to match.** Retail ends the distance test with
`fcmpo cr0, f28, f0 ; blt <skip>`; ours emits `fcmpo cr0, f28, f0 ; cror eq,gt,eq ; bne <skip>` for
`nearestDistance >= 7.f`. Writing `!(nearestDistance < 7.f)` instead gives retail's instruction
sequence: **85.29% -> 86.04%**, the only ChooseSpawnPoint change measured this run that moves the
number. **Not kept in the diff** - it is not needed by this item, and `!(a < b)` and `a >= b` differ
for NaN (`blt` is taken for an unordered compare, `cror eq,gt,eq` is not), so it is a real semantic
change bought for 4 bytes. Whoever matches this function needs it and should decide it then.

**The inlined `push_back` is a shared-header difference.** Retail's is
`lwz size ; lwz capacity ; cmpw ; bne grow ; slwi ; bl reserve`. Ours has a second path retail does
not: `cmpw size, capacity ; blt grow ; cmpwi capacity, 0 ; ... li r4, 4 ; beq <grow-with-4>`. That
is `rstl::vector<TUniqueId>::push_back` in a header, so this function cannot be finished from
`CGMMultiplayer.cpp` alone.

STALE: "retail indexes players through `addi r0,r23,5372 ; lwzx r4,r26,r0` where ours keeps a pointer in r23" (previous run, ChooseSpawnPoint) - measured on this tree it is the other way round: retail emits `lwz r4, 0x14fc(r23)` with r23 a pointer and r22 the counter, ours emits the `addi`+`lwzx` off r26; and hoisting `ObjectListById` above `reserve` is worth exactly 0.00%

## `fn_80197910` is `create_node`, characterised (the previous run stopped at "no match")

`build/G2ME01/src/.../CGMMultiplayer.o` (ours) does **not** define `fn_80197910` at all. It defines
`create_node__Q24rstl234red_black_tree<Q24rstl29pair<Ui,P17CGameModeListener>,...>4node...`, 112
bytes, weak COMDAT. Retail's `fn_80197910`, 104 bytes, is the same function: it `allocate(24)`s and
stores the same six words (four incoming arguments plus `*(r8)` and `*(r8+4)`), and its instruction
sequence was printed verbatim in the earlier run's objdiff listing. Two real differences:

- ours has a null-ish guard retail does not: `addic. r5, r3, 0x10 ; beq <epilogue>` (i.e. "if
  `n + 0x10 == 0`, skip the value copy"), and it uses `r5` for the `+0x10` address;
- ours defers both `lwz` from `r31` until after all four argument stores; retail interleaves them.

Both come from `include/rstl/red_black_tree.hpp`'s `create_node` / `rstl/construct.hpp`'s
`construct_impl`, which every unit instantiating `red_black_tree` shares. Even once the bytes are
right, objdiff pairs **by name**, so retail's `fn_80197910` would still need renaming to the mangled
name (as run 1 did for 11 others). Not a `CGMMultiplayer` change.

**Trap for the next run:** `build/G2ME01/obj/MetroidPrime/Player/CGMMultiplayer.o` is the **retail**
object extracted from the DOL; `build/G2ME01/src/MetroidPrime/Player/CGMMultiplayer.o` is **ours**.
Reading the former's symbol table makes it look as though our object defines `fn_80197910` and
`fn_801968D4` under those names - it does not, and that misreading sends you looking for a rename
that is not there.

`fn_801968D4` is unchanged at 0.00% and still the `CPlayerState::operator=` layout question run 1
described; renaming it to `__as__12CPlayerStateFRC12CPlayerState` would only pair it, and the bytes
differ, so it would not become a match.

## Gates (all re-measured on this tree)

- `./tools/decomp_build.sh` -> `All: 34.87% fuzzy, 28.52% matched, 12.90% linked (12354 / 28465)`.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` - unchanged.
- `./tools/goal_check.sh build/goal/item.json` -> **PASS**, seven checks ok, including
  `gate.sh` (DOL sha1, all 86 RELs, report diff, module wiring, docs claims, port probe),
  `check_symbol_names.py`, `counts: matched 12353 -> 12354  linked 5863 -> 5863`,
  `no asm added`, and `no judge-owned path touched`.
- No `config/`, `configure.py`, `splits.txt`, `files.cmake` or `tools/` change; `git status` shows
  only `src/MetroidPrime/Player/CGMMultiplayer.cpp`. (`docs/HANDOFF.md` is rewritten by the judge;
  I reverted my tree's copy so the diff stays one line.)

## Nothing filed as `NEW:`

The two remaining blockers in this unit are both rooted in shared headers
(`rstl/red_black_tree.hpp` for `fn_80197910`, `CPlayerState` for `fn_801968D4`) and would need a
`symbols.txt` rename in *this* unit on top, which makes them awkward to queue as another item's
target - and `CPlayerState` is already the subject of a better-scoped item. They are characterised
above instead, which is what the brief asks for in place of a placeholder `NEW:`.

## A note on the reviewer's "count gamed" rule

No initialisation was dropped, no call removed, no stub introduced, no `asm` added, and no shared
header touched. The single edit replaces one spelling of a value with another spelling of the same
value at one call site; `idx` is still computed and still used for the index. The function went
from 712 bytes of near-retail to 712 bytes of *exactly* retail.

---

# Run 3 (lane 1, 2026-10-02) — `create_node` taken to 100%, `ChooseSpawnPoint` 85.29% -> 91.59%

## Result

**30 -> 31 of 34 functions matched.** Unit `main/MetroidPrime/Player/CGMMultiplayer`:
fuzzy 91.15% -> **93.83%**, matched code 78.38% -> **80.38%**. Project-wide `matched`
**12372 -> 12373** (+1 at 100%), `linked` **5863 -> 5863** (unchanged, as expected for a
`progress` item). `./tools/goal_check.sh build/goal/item.json` -> **PASS**, all seven checks ok
(`target rose: main/MetroidPrime/Player/CGMMultiplayer: 30 -> 31 / 34 functions`).

## The matched function: `fn_80197910` (104 B) is `create_node`, and the fix was already half-written

Runs 1 and 2 both stopped at "`fn_80197910` is `create_node`, our version has a null-ish guard
retail does not (`addic. r5,r3,0x10 ; beq`), so it is `rstl/construct.hpp`'s `construct_impl` -
a shared header, not a `CGMMultiplayer` change". That is right about the mechanism and wrong
about where the fix belongs. `include/rstl/pair.hpp:136` **already** carries

```cpp
template < typename T >
inline void construct_impl(void* dest, const pair< int, T* >& src) {
  *static_cast< pair< int, T* >* >(dest) = src;
}
```

with a comment naming retail's `map<int, CFactoryFnReturn (*)(...)>` `create_node` as the
evidence. The trap is that `CGMMultiplayer`'s listener pair is `rstl::pair< **uint**,
CGameModeListener* >`, so the `int` overload never instantiates. One eight-line overload for the
unsigned first member - the same lever, the same two-word copy - removes the guard.

## Files changed

**`include/rstl/pair.hpp`** (+10, after the `pair<int, T*>` overload): a
`construct_impl(void*, const pair< uint, T* >&)` overload that copies by assignment instead of
through a placement `new`, with the evidence in the comment. This is the same idiom the file
already uses for `pair<uint,uint>`, `pair<uint,bool>`, `pair<uint,int>`, `pair<int,float>`,
`pair<float,float>` and `pair<int,T*>`; nothing is dropped - the copy is the same two words.

**`config/G2ME01/symbols.txt`** (line 6772, one line): `fn_80197910` renamed to the mangled
`create_node__Q24rstl234red_black_tree<pair<Ui,P17CGameModeListener>,...>` our object already
emits, at the same address and size. Same treatment run 1 gave 11 other weak COMDATs.

**`src/MetroidPrime/Player/CGMMultiplayer.cpp`** (3 lines, all spelling/order, all inside
`ChooseSpawnPoint`): see the ladder below. No behaviour change: the hoisted
`ObjectListById(kOL_All)` is a side-effect-free read of `mgr.mObjectLists[0]` that cannot be
affected by `candidates.reserve(16)`'s allocation, and the loop counter change only compares
values in `[0,4]` with a different signedness.

`docs/HANDOFF.md` is the judge's rewrite (`MP_GATE_DOCS_WRITE=1`); I reverted my copy so the
diff is the three files above.

## Per-function, before -> after

| function | before | after | what was tried |
|---|---|---|---|
| `fn_80197910` / `create_node` | 0.00% | **100.00%** | `construct_impl(pair<uint,T*>)` overload + `symbols.txt` rename. |
| `ChooseSpawnPoint` | 85.29% | **91.59%** | ladder below. Still short of 100% - see the blocker. |
| `NotifyListeners` | 95.00% | 95.00% | **not attempted** - run 2 measured 20 spellings against a register-allocation difference. |
| `fn_801968D4` | 0.00% | 0.00% | re-measured; see the STALE line - there is no source in this unit that can move it. |

`create_node` is **instruction-for-instruction identical** to retail's, not merely fuzzy-100%:
26 instructions on each side, every mnemonic equal, compared with `objdump -d` on
`build/G2ME01/src/.../CGMMultiplayer.o` and `build/G2ME01/obj/.../CGMMultiplayer.o`
(note: with the rename in `symbols.txt`, dtk now labels retail's copy `create_node...` too, so
the two symbol tables agree).

## `ChooseSpawnPoint`: the spelling ladder, measured on this tree (base 85.29%)

Each row is cumulative - the next row adds one change to the row above.

| change | % |
|---|---|
| base | 85.29 |
| `if (!(nearestDistance < 7.f) && ...)` instead of `>=` | 86.04 |
| ... + `for (int player = 0; player < static_cast<int>(GetNumPlayers()); ++player)` | 88.45 |
| ... + `CObjectList& objects` hoisted above the **`rstl::vector candidates;` declaration** (not above `reserve`) | **91.59** |

Measured individually as well: the predicate alone is 86.04, the `int` counter alone is 88.13,
and *with the counter change in* putting `objects` back above `reserve` gives 88.45 - so the
declaration order is worth +3.14 and run 2's "hoisting `ObjectListById` is worth 0.00%" was
measured against a weaker base, not a null result.

Two of the three are pure wins of a kind worth writing down:

- **`int player` instead of `uint player`.** Retail strength-reduces the players array into a
  *pointer* induction (`mr r23,r26 ; lwz r4,5372(r23) ; addi r23,r23,4`); we emitted a *byte
  offset* induction plus an extra address form (`li r23,0 ; addi r0,r23,5372 ; lwzx r4,r26,r0`).
  `CStateManager::GetPlayer` already takes `int`, so the counter's declared type is free to
  choose, and `int` is what flips GCC's `ivopts` choice. The prologue (`lwz r31,2064(r5)` first,
  then the three vector stores interleaved with the five `mr`s, then `bl reserve`,
  `lha r30,8200(r31)`) and the whole inner player loop now match retail instruction for
  instruction.
- **`objects` above the declaration.** `mgr.ObjectListById(kOL_All)` is `lwz r31,2064(r5)`. We
  emitted it *after* `bl reserve`; retail emits it as the first instruction of the body, still
  reading the incoming `r5`. A load cannot be sunk past a call, so this is not a scheduling
  choice - the expression has to be written before the statement that calls.

The predicate change is the one with a semantic footnote: `!(a < b)` and `a >= b` differ only
for an unordered compare, i.e. if `nearestDistance` is NaN. Retail takes the `blt` branch form,
so retail accepts such a spawn point and `>=` rejects it; writing it as `!(nearestDistance < 7.f)`
reproduces retail's *behaviour*, not just its bytes. `nearestDistance` is a `Magnitude()` of a
position difference seeded at 1000000, so NaN needs a NaN coordinate. It does not complete the
function; it is kept because it is a measured move toward the one unmatched function that is
not a wall.

## What still blocks `ChooseSpawnPoint`, precisely

The inlined `rstl::vector<TUniqueId>::push_back`. Retail emits 15 instructions and we emit 18:

```
retail: lwz cap ; lwz size ; cmpw cap,size ; bne skip ; addi r3,&vec ; slwi r4,cap,1 ; bl reserve ; ...
ours:   lwz size ; lwz cap ; lhz value ; cmpw ; blt store ; cmpwi cap,0 ; addi r3,&vec ; li r4,4 ; beq grow4 ; slwi r4,cap,1 ; bl reserve ; ...
```

The `cmpwi cap,0 / li r4,4 / beq` is `include/rstl/vector.hpp:80`'s
`reserve(mCapacity != 0 ? mCapacity * 2 : 4)` - a **zero-capacity fallback retail does not
have**. Retail's `push_back` is `if (mCount == mCapacity) reserve(mCapacity << 1);`.

Getting retail's bytes therefore means deleting that fallback from a shared header, which makes
`push_back` on a default-constructed vector write through a null pointer. That is a deliberate
safety divergence in the port's own `rstl`, not a decompilation fix, and it is not this item's
call to make: I did not do it and it is not filed as a `NEW:` (it names no unit, and the target
would be a port-semantics decision rather than something a lane can verify by matching).
Whoever finishes `ChooseSpawnPoint` should decide it explicitly and expect the header's blast
radius to be measured with `report_diff.py`.

## STALE / corrected claims from the earlier runs of this item

STALE: "`fn_801968D4` is a `CPlayerState` field-layout question - something at `+4` is a byte and
something in `0x48..0x57` is four floats" (run 1, repeated by run 2) - measured on this tree the
layout is **identical** and only the scheduling differs. Both sides are 66 instructions copying
the same offsets: 0 word, 4 byte, 8, 12, then word pairs 16/20, 24/28, 32/36, 40/44, then 48,
52; out-of-line member copies at 56, 88, 1408, 1424; floats 72/76/80; **84 as a word in both**.
Retail pairs its loads and stores (`lwz r5,16 ; lwz r0,20 ; stw r5,16 ; stw r0,20`) where we
interleave (`lwz r5,16 ; stw <prev> ; lwz r0,20 ; stw r5,16`), and retail stores the frame in
the prologue before its first load where we hoist `lwz r5,0(r4)` and `lbz r0,4(r4)` up into it.
The "four floats at 0x48..0x54" reading came from aligning two streams that are the same length;
there is no extra field. `include/MetroidPrime/Player/CPlayerState.hpp` declares no
`CPlayerState::operator=` (only `SPersistentState::operator=`, line 192), so the copy is
compiler-generated and **no source in this unit can change it**.

STALE: "hoisting `ObjectListById` above `reserve` is worth exactly 0.00%" (run 2, ChooseSpawnPoint)
- superseded. Above `reserve` it is worth 0.00% on its own; above the `rstl::vector candidates;`
**declaration** it is worth +3.14% on top of the other two changes (85.29 -> 91.59 together).
Run 2's variant also left the load reading the `r26` copy of `mgr` instead of the incoming `r5`.

## Gates (all re-measured on this tree)

- `./tools/goal_check.sh build/goal/item.json` -> **PASS**, seven checks ok.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` - unchanged; all
  86 RELs re-hashed by `gate.sh` step 3 with no mismatch.
- `./tools/decomp_build.sh` -> `All: 34.93% fuzzy, 28.60% matched, 12.90% linked (12373 / 28465)`.
  The `All:` line did not fall (baseline 34.93% / 28.60% / 12.90%).
- `python3 tools/check_symbol_names.py` -> `checked 525 units; 0 declared names are missing`.
- `tools/report_diff.py build/goal/judge/report.base.json build/report.json` -> `no regression`;
  `matched 12372 -> 12373   linked 5863 -> 5863   (+1 functions at 100%, 0 units newly linked)`,
  reporting `create_node` as `RENAMED ... (0.00% -> 100.00%)`, which is the tool's designed
  handling of a rename, not a deletion.
- **The `pair<uint, T*>` overload was measured project-wide, not just in this unit**: ninja has
  no header depfiles here (`ls build/G2ME01/src/**/*.o.d` finds none), so I `touch`ed all 1199
  sources and did a full rebuild before diffing. Only `CGMMultiplayer.o` changed. The two other
  places that name a `pair<uint, X*>` at all - `src/MetroidPrime/CGameArea.cpp:335`'s
  `SRelLocation` (assigned, never `construct`ed) and `include/MetroidPrime/CParticleDatabase.hpp:32`'s
  `map<uint, auto_ptr<...>>` (not a pointer second member, so the overload does not apply) -
  cannot instantiate it.
- No `configure.py`, `splits.txt`, `files.cmake` or `tools/` change; `total_functions` still
  28465.

## Nothing filed as `NEW:`

The one thing left in this unit that a lane could verify is `ChooseSpawnPoint`, and it is a
restatement of the current item. The `rstl::vector::push_back` fallback is a port-semantics
decision in a shared header, not a unit or a symbol, and the brief bars filing a `NEW:` that
names neither. Both are written up above instead.

## A note on the reviewer's "count gamed" rule

The +1 is a real byte match, not a rename onto a function that already matched: before this
change our `create_node` was **112 bytes** with a `addic. r5,r3,16 / beq` guard retail does not
have, and the `construct_impl` overload is what makes it 104. No initialisation was dropped, no
call removed, no stub introduced, no `asm` added. The three `CGMMultiplayer.cpp` lines are a
statement hoist, a loop-counter type, and a predicate spelling, all inside the item's own
unmatched function, and none of them raises `matched_functions` - only `create_node` does.
