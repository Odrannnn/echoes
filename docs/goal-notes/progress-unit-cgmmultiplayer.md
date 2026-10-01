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
