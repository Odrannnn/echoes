# Handoff

Orientation for whoever picks this up next, human or agent. Read this first, then
`docs/RUNNING_THE_DECOMP.md` for how the work is run and `PORT_NOTES.md` for how the port
itself works. This file is the map and the current position; those two are the detail.

## The state, measured

```
matched    12227 / 28465 functions        (34.54% fuzzy, 27.87% of code, 12.89% fully linked)
linked     5860 / 28465 functions        (the one rule's count: the unit is Matching and has a source.)
DOL units  10679 / 16726 functions        (main/*, including the SDK's)
port link  291 undefined, 0 duplicates   (291 since the eighth upstream sync, 2026-10-01: upstream's
                                   CFrontEndGameMode, CRelFile and CDamageVulnerability units define what
                                   38 listed names asked for, and six new names opened -
                                   docs/research/port_link_gap.md, "The eighth upstream sync". Before that:
                                   324 since 2026-10-01: the port now links CMemoryCard.cpp,
                                   Player/CGameState.cpp, CWorld.cpp and CGameArea.cpp whole, which
                                   closed 23 names and opened 97 that nothing implements yet -
                                   docs/research/port_link_gap.md, "The four whole units". Before that:
                                   250 again since 2026-09-30: lane commits 33b784fb
REL units   1548 / 11739 functions        (the 86 modules, counted as the complement of main/*. A REL unit only counts when its sha1 matches config/G2ME01/config.yml *and* the .rel is cmp-equal to orig/G2ME01/files/RelProd/, so this number is the module count, not an objdiff percentage.)
```

How these numbers got here - the first seven upstream syncs and the unit flips, each with what it
traded - is in `docs/history/handoff-to-2026-10-01.md`.

**The eighth sync (`PrimeDecomp/echoes` 8bb7bd0f, 2026-10-01, merge base a6538995)** took matched
11959 -> 12087 and linked 5728 -> 5795, measured against the judge's baseline of the pre-merge head.
The decision for it was to **follow upstream's units, classes and names** and re-fit our bodies to
them, since upstream is what later syncs are based on:

- `CGameGlobalObjects` now holds upstream's `CMemoryCardSys mMemoryCardSys` and
  `CRELFileManager mRelFileManager` where we had `pad0` and `x150_tail`, and `CInGameTweakManager`
  is upstream's class (`rstl::vector<CTweakValue> mValues`). `CGMFrontEnd` is `CFrontEndGameMode`,
  `SPlayerConfig` is `CFrontEndPlayerData`, and `CEntityInfo`'s members are `mActive`,
  `mUpdateWhileOccluded`, `mUpdateDuringCinematicSkip`.
- Eight `Matching` carves were absorbed and deleted: `CDamageVulnerabilityStatics.cpp`,
  `CGameGlobalObjectsTailCtor.cpp` and the six `ScriptLoader/Carve80226*.c` (now upstream's
  `Player/CFrontEndGameMode.cpp`, 33/33). The port-only `PortModuleManager.cpp` is deleted too:
  upstream's `CRelFile.cpp` and `Kyoto/CRelFileDebugInfo.cpp` are that code, linked on the host with
  the `port::modules::Prolog/Epilog` calls under `TARGET_PC`.
- Nothing that was at 100% is lower. `~CGameGlobalObjects` (0x80006518) needed its hand-written
  teardown back, now as an `extern "C"` function spelled `__dt__18CGameGlobalObjectsFv`
  (`main.cpp`). One function is worse and is upstream's change: `LoadPickup` 94.39% -> 93.78%,
  where upstream deleted `SLdrPlayerItem` and reads the item through `ReadPlayerItem(int&, ...)`.
- On the host, `Kyoto/CDvdRequestManager.cpp` and `Player/CFrontEndGameMode.cpp` are listed in
  `files.cmake`; `Startup.cpp`, `CMapArea.cpp` and `CMappableObject.cpp` are EXCLUDED in
  `tools/check_files_cmake.py` with the measured reason each.
- `TARGET_PC` blocks lost in the merge: only those in structures upstream replaced
  (`CGameGlobalObjects.hpp`, `CInGameTweakManager.hpp`, `CMain.hpp`, the deleted tail-ctor carve).

That block must appear **exactly once**, and `tools/check_docs_claims.py` now fails if it
does not. Three copies were fused together inside one fence by successive lane merges,
carrying three different sets of numbers (`3241/1831`, `3240/1830`, `3240/1830`) - and the
checker passed the whole time, because it looked for the correct figure and *found it among
the contradictions*. A check that cannot fail on the most obvious way this file goes wrong is
not a check; see `docs/PROCESS_LESSONS.md` #1.

Verify all of that yourself; do not trust this file's numbers over the report:

```sh
./tools/decomp_build.sh            # ninja + objdiff + the All: line
python3 - <<'PY'
import re, hashlib, os
cfg = open('config/G2ME01/config.yml').read()
ok = 0
for name, exp in re.findall(r'object: files/RelProd/(\S+)\n\s+hash: ([0-9a-f]{40})', cfg):
    mod = name[:-4]; p = f'build/G2ME01/{mod}/{mod}.rel'
    a = hashlib.sha1(open(p,'rb').read()).hexdigest() if os.path.exists(p) else 'missing'
    ok += a == exp
print(f'{ok}/86 modules match config.yml')
PY
```

Last known good: the commit that last touched this file (`git log -1 --format=%h -- docs/HANDOFF.md`).
As of the numbers above: DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs
byte-identical to `orig/G2ME01/files/RelProd/`, probe 751 files 0 failures, symbol check 0 missing.
(The old form of this line pinned a commit hash, which cannot be written down in the commit thatcreates it.)

## What is not in git (check these before blaming the tree)

A fresh checkout is **not** self-sufficient. Three things live outside version control, and every
tool fails with a confusing error if one is missing:

1. **The retail data at `orig/G2ME01`** (7.5 MB, untracked and not ignored). `dtk` needs it to split
   the DOL and the 86 RELs, and `config/G2ME01/build.sha1` hashes what it produces. On a copy of this
   directory it is already there; on a `git clone` it is not, and it comes from an owned disc via
   `python3 tools/extract_disc_file.py`. `sha1sum orig/G2ME01/sys/main.dol` should be
   `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` - the same hash everything else is measured against.
2. **The sibling toolchain tree `../MetroidPrimePort`** - MWCC compilers, `dtk`, `wibo`, and a ninja
   under `build/review-tools/bin/`. Nothing here builds it; `MP_TOOLCHAIN_DIR` points at it, and
   `tools/gate.sh`, `tools/decomp_build.sh` and `tools/flip_test.sh` all default to that sibling path
   and fail loudly if it is wrong. The lane briefing has the export line.
3. **`build/`** is ignored and regenerable - `tools/gate.sh` configures and builds it from nothing in
   about two seconds, given 1 and 2.

Also worth knowing, though nothing breaks without it: the upstream reference clone the sync
workstream reads (`git clone --depth 1 https://github.com/PrimeDecomp/echoes`), and the lane
worktrees under `/tmp/opencode`, which are disposable and can be removed (`git worktree list` currently
shows 61, holding ~11 GB of a tmpfs).

**The first command to run in a fresh session is `python3 tools/gate.sh`.** It configures, builds,
checks the DOL and all 86 RELs against `config.yml`, diffs the report per function, and checks the
module wiring, the docs' claims and the port probe - about two seconds, one verdict line, non-zero
exit on any failure. If it passes, the tree is sound and the documentation in this file is current;
if it fails, read the failing step before anything else.

## What this repository is

Two things at once, and it is easy to confuse them:

- **A port** of Metroid Prime 2: Echoes to PC, built on Aurora (the MIT GameCube SDK/GX
  replacement). The port layer is essentially complete: REL runtime, entry point, SDK shims,
  disc tools, tests. It cannot run because the decompilation is far from complete: see the
  state block above for the measured share that is matched and really linked.
- **A contribution to the decompilation** (`PrimeDecomp/echoes`), which is what the remaining
  work actually is. Every rule about completion in `RUNNING_THE_DECOMP.md` comes from this half.
  The public upstream tree is reachable and **ahead of us in units we have not written** (and behind
  in others); from 2026-09-25 the rule is to port from it only where the gates pass, attributed in
  the commit. See "Upstream, and what we take from it" in `RUNNING_THE_DECOMP.md` for the measured
  cost of doing that and the units it has been done for.

## Where the research lives

24 files carry what a later session would otherwise have to re-derive, and each answers one
question that used to cost a session. (Digits above twenty on purpose:
`check_docs_claims.py` matches `(\w+) files carry` and has no spelled-out word past
twenty, so a hyphenated "twenty-two" or a spelled "Twenty-Two" both fail the check.)
`docs/research/README.md` is the index: one row per file with the question it answers. Start
with `boot_path.md` for port work and `port_link_gap.md` for what the port's link still lacks.

## Tools, in the order you will want them

| | |
| `./tools/decomp_build.sh [unit]` | ninja, then objdiff, then that unit's unmatched functions |
| `tools/flip_test.sh <unit>` | **the acceptance test** - flip to `Matching`, rebuild, keep only if the DOL and all 86 RELs still reproduce retail |
| `tools/compare_unit.sh <unit>` | diagnostic: how our object differs from the retail-derived one |
| `tools/fast_try.sh <unit>` | rebuild one object, print only that unit's scores - the loop to use while trying source variants |
| `tools/lanediff.sh <unit> [sym]` | one function, retail against ours, addresses and branch targets stripped so only real differences show |
| `tools/collect.sh <lane>` | three-way apply a lane's diff onto a fresh HEAD worktree, baseline the report from unmodified HEAD, then run the whole gate on the merged result - collection in one command, ~7 s |
| `tools/try_batch.py <src> <unit> <sym> <variants.py>` | try N bodies for one function in one run, ranked by **differing instructions** rather than objdiff's byte percentage; always restores the source |
| `tools/check_raw_offsets.py` | every raw-offset field access, against the policy in `docs/research/raw_offsets.md` - in `gate.sh` |
| `tools/check_files_cmake.py` | **every configured, on-disk unit is in the port build or excluded with a reason.** 96 were in neither, invisibly, because `link_gap.py` derives the gap from the objects `files.cmake` produces; in `gate.sh` |
| `tools/link_gap.py` | what the port's game library still needs to link, by `nm` arithmetic, against `docs/research/port_link_gap.md`; in `gate.sh`. It cannot see a vtable before its key function exists |
| `tools/link_check.sh` | **the real link.** Configures with `MP_SDK_HEADERS_ONLY=OFF`, builds the executable, and reports the linker's own undefined and duplicate counts against `docs/research/port_link_baseline.txt`. The slow gate — run it before committing anything touching `CMakeLists.txt`, `files.cmake`, `platform/`, or the port side of a unit, which is exactly what the fast gates cannot see. `--rebuild` to force a clean configure, `--record` to move the baseline |
| `tools/check_decl_order.py` | which units emit their functions out of retail order, against the work list in `docs/research/decl_order.md` - in `gate.sh` |
| `tools/check_symbol_names.py` | every name `symbols.txt` declares vs what the retail object defines |
| `tools/find_trivial_functions.py` | unmatched functions classified by machine-code shape - the cheap-work queue |
| `tools/scaffold_rel_module.py` | the three artifacts for starting a REL module |
| `tools/wire_rel_setup.py` | claims a module's `REL_Setup` tail and names `RELMain`/`RELExit`/`Module*structors`; check the hash after |
| `docs/research/CPatterned_layout.txt` | the constructor's 2904 bytes, every byte in exactly one row |
| `tools/probe_sources.sh` | the port build's **compile and link** sweep (751 files). As of 2026-09-27 it runs the real link and reports the verdict beside the compile count; it used to compile only, which is how a broken link passed the gate || `build/binutils/powerpc-eabi-objdump`, `powerpc-eabi-nm` | disassemble / list symbols |
There is **no system cmake or ninja**. Use
`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort/build/review-tools/bin/`
for cmake/ctest/ninja, and that port's `build/compilers` and `build/tools/{dtk,wibo}` for the
matching build.

## The one rule that decides whether work counts

A unit is done when the build still reproduces retail **with that unit's own object in the
link**. `configure.py` decides this: `NonMatching` links bytes `dtk` split out of the retail
binary, `Matching` links our compile. So objdiff percentages on a `NonMatching` unit are a
signal, not a result, and `87 files OK` proves nothing about a unit that is not `Matching` -
it validates the untouched parts of the binary. Two sessions were spent on this; see
"the one rule" in `RUNNING_THE_DECOMP.md`.

## Two independent workstreams

**1. The DOL** - the `DOL units` line above. Work is per unit: write it, measure with objdiff, flip
to `Matching` when `tools/flip_test.sh` passes. The two units the whole port was
waiting on are in: `CAi` 11/11 `Matching`; `CPatterned` 29/103 is `NonMatching` since the upstream
merge widened it. Others: `TypesMatch` 506/511, `CStateManager` 101/239, `CPlayerGun` 68/136,
`CPlayerState` 67/72. (`check_docs_claims.py --write` keeps these six counts current.)

**2. The REL modules** - the `REL units` line above, 86 modules. A module counts only when its
sha1 matches `config/G2ME01/config.yml`. **Measure which modules link our own code, never recall
it**: `python3 tools/check_module_wiring.py`. As of the last commit it reports
**99 units of our own code in 77 modules** - `AIMannedTurret`, `AtomicAlpha`, `AtomicBeta`, `BacteriaSwarm`, `Blogg`, `ChozoGhost`, `DarkCommando`, `DarkSamus`, `DarkSamusBattleStage`, `DarkTrooper`, `DestructibleBarrier`, `DigitalGuardian`, `ElitePirate`, `EmperorIngStage1`, `EmperorIngStage2Tentacle`, `EmperorIngStage3`, `EyeBall`, `FishCloud`, `FlyerSwarm`, `FlyingPirate`, `FogOverlay`, `GeomBlobV2`, `Glowbug`, `Grenchler`, `GunTurret`, `Ing`, `IngBlobSwarm`, `IngBoostBallGuardian`, `IngPuddle`, `IngSnatchingSwarm`, `IngSpaceJumpGuardian`, `IngSpiderballGuardian`, `Kralee`, `Krocuss`, `MediumIng`, `Metaree`, `MetareeSwarm`, `Metroid`, `MinorIng`, `MysteryFlyer`, `OctapedeSegment`, `Parasite`, `PillBug`, `PirateRagDoll`, `PlantScarabSwarm`, `PuddleSpore`, `Puffer`, `Rezbit`, `Ripper`, `RubiksPuzzle`, `SandBoss`, `Sandworm`, `ScriptCoin`, `ScriptFrontEndDataNetwork`, `ScriptGui`, `ScriptPlayerActor`, `ScriptPlayerProxy`, `ScriptPlayerTurret`, `ScriptRiftPortal`, `ScriptRsfAudio`, `ScriptSafeZone`, `ScriptStreamedMovie`, `Shredder`, `SnakeWeedSwarm`, `SpacePirate`, `SpankWeed`, `Splinter`, `Splitter`, `Sporb`, `StoneToad`, `SwampBossStage1`, `SwampBossStage2`, `SwarmBasics`, `Tryclops`, `WallCrawler`, `WallWalker`, `WispTentacle`.

The dated account of both (which modules joined when, the `TypesMatch` naming work, the `Rel(...)`
blocks that were lost and restored) is in `docs/history/handoff-to-2026-10-01.md`.

## The blocker: CPatterned, and the base classes below it

**`CAi` is done** (2026-09-25): 11 of 11 functions, `Matching`, DOL sha1 and all 86 RELs still
reproducing retail. The "cyclic link-order dependency" that this section used to describe **was
never real** - the split is accepted, and what blocked it was ordinary symbol renaming. What looked
like a cycle was CodeWarrior COMDAT weak symbols that both linkers discard. The full account, the
rename list and the two traps (claim *all* four sections; a `.sdata2` split may not end inside a dtk
`lbl_`) are in `RUNNING_THE_DECOMP.md`, section "CAi: landed, and the cyclic link-order dependency
was never real". Read that before touching anything in `include/MetroidPrime/Enemies/`.

**`CPatterned` is landed as well** (2026-09-25): a `Matching` unit with 10 of 10 functions at 100%,
claimed as the 92 bytes of the small accessor cluster at `0x80073C58..0x80073CB4` rather than the
0xB58-byte constructor. The class exists, its vtable relocations resolve, and the creature modules
are no longer blocked on the hierarchy existing - only on their own code.

**The constructor has now been measured, mapped and started** (2026-09-25, later the same day).
It is no longer an untouched 2904 bytes:

- **`docs/research/CPatterned_vtable.txt` - all 82 slots**, read from the relocations of
  `auto_07_803B1C40_data.o` and cross-checked against the linked DOL. Slots 0 and 1 are `0x00000000`
  with no relocation, so the two RTTI words were stripped and no vtable dumper can name the type.
  20 of the 82 point at another class's code (8 at `CActor`, 5 at `CPhysicsActor`, 6 at `CAi`), which
  is what fixes the numbering: `CPatterned` owns 2-13, 16-24, 28-29, 33, 36, 38-39, 41, 46-81.
- **`docs/research/CPatterned_layout.txt` - the 2904 bytes, every byte in exactly one row.**
  47.8% is member initialisation with no call in it, 19.4% is three copies of the same anim-token
  boilerplate, 10.1% is the caller's materialisation of the aggregate `CAi`'s constructor takes, and
  11.0% is calls into unwritten bodies. Two things are genuinely unknown, and both are named: the
  `CAi` argument aggregate (288 bytes, and it caps the score) and the access width of the 26-field
  block at `0x420` (312 bytes).
- **`include/MetroidPrime/Enemies/CPatterned.hpp` is decoded**, not stubbed. The
  `x4a0_undecoded[0x2b4]` blob is now six named sub-objects plus real members out to `0x7c0`;
  `CHECK_SIZEOF(CPatterned, 0x7c0)` still holds and the unit is still 10/10 at 100%. Four header
  members were **wrong** and are corrected: `x39c_` is a heap `CToken*`, not an `int`; `x470_` is
  one `CToken`, not a 12-byte token plus 12 bytes of padding; `x34c_28_` is fed from
  `kInvalidUniqueId`, not from `moveType`; and `x448_` starts at -1.0f, not 0.
- **`src/MetroidPrime/Enemies/CPatternedCtor.cpp` compiles** as its own `NonMatching` unit, 1464
  bytes, paired by renaming `fn_80079BE4` to the mangled name MWCC emits. It measures **7.68%** -
  low, and reported as such. The unit is `NonMatching`, so **none of it is in the link and neither
  `matched` nor `linked` moved**; the value is the vtable map, the layout and the next step.

**The next step, measured rather than guessed:** find the aggregate `CAi`'s constructor takes as
its second argument. Retail passes it in a register *and* spills six words to the stack, so it is a
struct big enough that MWCC split it - read `CAi::__ct__` in the `Matching` `CAi.o` and look at how
it reads its own incoming stack words. That is 288 of the 2904 bytes and the only thing standing
between 7.68% and a comparable score. Second, and independent: the 26-field block at `0x420` is
**word**-granular in retail and no declaration tried makes MWCC 2.7 choose word granularity for a
run of one-bit fields (see "MWCC's bit-field granularity" in `RUNNING_THE_DECOMP.md`); it is 312
bytes and worth the same lane's attention.

The other hierarchy gaps, unchanged: `include/MetroidPrime/Enemies/` holds the `SwarmBasics` layer
and now `CAi`/`CPatterned` headers; there is still no `CPatterned.cpp`, no GUI hierarchy
(`src/GuiSys/` and `include/GuiSys/` are empty and unlisted), and `UnkVtable20` is resolved.

## Running lanes

The work is done by many agents in parallel, one per module or unit, each in its own git
worktree with its own real `build/` directory (not a symlink - see the rig defect in
`RUNNING_THE_DECOMP.md`). The full spawn sequence, the non-negotiable details, how to collect a
lane, and the failures that recur are all in that file's "Parallel lanes" section. The two
things that cost the most time:

- **A lane's `config/` is its own view, never a patch.** Copying a lane's `symbols.txt` or
  `splits.txt` can silently revert a later rename, or delete an existing split block. Merge by
  hand against current `HEAD` and diff. This cost a lost `UnkVtable20` rename and two lost
  `configure.py` entries.
- **Verify a lane's report independently.** They have been wrong in both directions - one
  understated its own result by ten functions, several cited verification that proved nothing
  because their unit was `NonMatching`, and one reported its completed work as missing.

The goal loop's queue was triaged on 2026-09-29, after a measured pass rate of match 9/46 and progress
32/53. It now runs progress items first, then match items by their worst remaining function. The
eleven wall and link-level items are in the review queue, with reasons. The judge now gives an agent
one round to fix a bookkeeping-only `gate.sh` failure and, since 2026-09-30, one round to fix a
change that no longer builds or links (about a third of 88 judged failures on 2026-09-28..30). It
parks a match or progress item whose notes gained a `WALL:` line *in that run* (it used to read the
whole file, so any item with an old `WALL:` was parked after its first failure), sets aside an item
the agent marks `STALE:` (already done at the head) without counting a fail, and closes a match item
whose unit is already Matching without an agent run. Agents run `goal_check.sh` themselves before
stopping, and a retry is told the notes are hypotheses to go past, not verdicts. A lane never
claims an item whose `target` unit another lane holds (2026-09-30: 16 of 51 items targeted
`MetroidPrime/main`, eight lanes were in main.cpp at once, and 12 finished attempts in three hours
were discarded on rebase); keep the queue spread over units, `goal_seed.py` does that. Agents no longer write the big docs. The judge re-derives their counts
(`check_docs_claims.py --write`), each item's notes land as `docs/goal-notes/<id>.md`, judged
failures do not back off, and an empty queue is refilled by `tools/goal_seed.py`. Nine lanes run
since 2026-09-30, all on the same goal/decomp tip as master: `mp2-goal@1..8` on space-bunny take
never-failed items; with none free they seed (once per head) and then take a failed-once item,
highest yield first (the hard lane was stopped that evening and 7 of 8 free lanes idled for hours
beside 58 failed-once items, since only an empty queue was seeded). And `mp2-goal@9` is the hard lane (`tools/goal_lanes.sh hard-lane 9
openai/gpt-6-luna`), which takes only items the free model failed once, highest measured yield
first (`_yield_rank` in `goal_queue.py`: Prime 1 donor items gained ~23 functions per agent-hour,
plain `match-` items 0.8; its fails>=1 backlog was 44 items on 2026-09-30). With `MAX_FAILS` at 2, every
item's second and last attempt is GPT-6 Luna's, reviewed on the free model. The 20 review-queue
items without a wall reason were requeued at fails 1 for it (backups `*.bak-luna-*` in the goal
dir); an item Luna fails goes back to review. The drop-ins are in `~/.config/systemd/user/mp2-goal@*.d/`;
the lanes are started by hand, never enabled at boot. The details are in `RUNNING_THE_DECOMP.md`'s goal-loop section. Re-measure the pass
rate from the lanes' logs before changing the loop again.

**Prime 1 as a source donor (2026-09-29).** Echoes' engine is a fork of Metroid Prime 1's, and
PrimeDecomp/prime has most of those classes Matching; a read-only clone sits at `../prime-ref`. Two
trial items paid: `progress-prime1-cactormodelparticles` gained 14 functions and
`progress-prime1-csortedlists` passed the judge at 11 to 19 of 20 CSortedLists functions (still in
review when written), against about 2 for a typical
progress pass. `tools/goal_seed.py` now seeds these (`--only prime1 --prime1-min-same N`): a
NonMatching DOL unit whose Prime 1 counterpart is Matching, listing its unmatched functions that
share a Prime 1 symbol name, same size first. 52 of them (at least one same-size function each) were
queued that day. Their notes record per function whether the Prime 1 source matched unchanged;
read a batch of them before seeding the remaining 26 zero-same-size units.

## Keeping this documentation true

The rules are in the repo `AGENTS.md`; `python3 tools/check_docs_claims.py` enforces the derivable
ones. The longer form that used to be here is in `docs/history/handoff-to-2026-10-01.md`.

**Queue triage, 2026-10-01.** `goal_seed.py` has run dry ("nothing to seed"): its three kinds (REL
heads, Prime 1 donors, near-done `match` units) do not cover a DOL unit that has source but is far
from done. 94 such units (2,080 unmatched functions, 77 with a Prime 1 counterpart) were queued by
hand as `progress-unit-*` items, each listing its closest unmatched functions, and `goal_seed.py`
now has a fourth kind, `unit`, that proposes the same items (and re-proposes a unit once its item
is done, unless it was set aside). Its pass rate is unmeasured: check it before trusting it. Most set-aside
review items are measured walls with notes, not rescuable; `match-ctweakautomapper`, `-cquaternion`
and `-clight` (all functions 100%, unit not linked, never attempted) were re-queued. Still unjudgeable
in review for want of a verify script: `port-cgamestate-fn-80145acc`, `port-rel-loader-fn-31-d8`,
`port-rel-loader-fn-33-a8`, `port-cbodycontroller-hassbodystate`.

## The port's boot position

Measured 2026-10-01 at 200c8dd0 (`tools/goal_verify/boot-progress.sh`, two runs): the boot loop
runs to marker `frame: 300`, tears down and **exits with code 0**. `MP_PORT_FRAMES=300` is the
judge's own frame budget, so no run can print a later marker and no stack is left to name.

**What moves the boot now is the stubs, and after that, something to draw.** Until 2026-10-01
every frame called only seven `[auto-stub]` no-ops: `SetClearColor` (twice), `SetCullMode`,
`SetDepthWriteMode`, `SetBlendMode`, `SetPerspective`, `TickRenderTimings` and
`CStreamAudioManager::Update`, all under their old `fn_` address names, which the
`Carve8026*.cpp` units and `Carve80003858.c` call and which on the host are different symbols
from the `CGraphics::` bodies. `CGraphicsHostStartup.cpp` now has the three bodies the port
lacked (upstream's) and forwards the six `fn_` names; `PortGlobals.cpp` forwards `fn_8032194C`.
Measured after (`tools/boot_probe.sh`, `build-boot-probe/run.log`): 300 frames, exit 0, **no
stub hit inside the frame loop**, 18 distinct stubs per run where there were 25 - eleven reach
stubs in the pre-renderer boot, step 17 and the shutdown (`fn_8032F6EC` x3, `fn_80145C98` x2,
`fn_802BE51C`, `mp_cswarmbasics`, `fn_61_70`, `fn_66_70`, `fn_80193E08`, `fn_8033CEE8`,
`CMain::ResetGameState`, `mp_cswarmbasics_exit`, `CEntityInfo::~CEntityInfo`) and seven one-off
auto-stubs in `CCubeRenderer`'s construction (`fn_80272624`, `fn_802711A4`,
`fn_80271104/0EC8/0D44/0BB4/0A64`). The frames set state and still draw nothing.

**Why nothing is drawn: the boot is parked in `CPreFrontEnd`.** (This replaces an earlier guess
here that `PortBoot.cpp`'s loop skips something between `BeginScene` and `EndScene`; it does not -
the loop is retail's 0x80006034-0x80006460 in full, and its only two `PORT_FRAME_STOP`s are the
terminate and reset paths.) Measured under gdb over 300 frames: `CMainFlow::SetGameState` is called
once (state 7, restart mode `kRM_Default`, so `CPreFrontEnd` is created), and
`CPreFrontEnd::OnMessage` then runs lines 48-49 (`AsyncIdle`, `MemoryCardInitializePump`) on 299
frames and never removes itself, because `gpMemoryCard` stays null. The cause is in the source:
the port links `mainMid.cpp`, whose `CMain::MemoryCardInitializePump` (line 340) is an **empty
body**; the real one is in `main.cpp:928`, which the port does not link. Its body needs, on the
host: `CMemoryCard`'s constructor and `InitializePump` (`CMemoryCard.cpp`, unlisted -
`check_files_cmake.py` measured it at +3 undefined: `CDummyWorld`'s ctor/dtor and
`CResFactory::GetResourceIdToNameList`), `CGameState::InitializeMemoryStates` and
`CPersistentOptions::InitializeMemoryState` (`Player/CGameState.cpp`, which does not compile on
the host - written against upstream's layout - so these want a carve like the other
`CGameState*.cpp` ones). Until that lands `CMainFlow` cannot reach `kCFS_FrontEnd`, and no stub
item changes what the frames draw.

**The pump is real since 2026-10-01, and the boot is still in `CPreFrontEnd`.** `mainMid.cpp`'s
`MemoryCardInitializePump` now has retail's body, and the port links `CMemoryCard.cpp`,
`Player/CGameState.cpp`, `CWorld.cpp` and `CGameArea.cpp` whole (the host uses upstream's
`CGameState`/`CPersistentOptions` layouts; the `TARGET_PC` variants and sixteen carves are gone -
`PORT_NOTES.md`). Measured (`tools/boot_probe.sh`, `build-boot-probe/run.log`): 300 frames, exit 0,
22 distinct stubs. Under gdb `SetGameState(7)` is still the only state change and `~CPreFrontEnd`
never runs: `CMemoryCard::InitializePump` never reports done, because
`CSaveWorldIntermediate::InitializePump` waits on `mSaveWorld->IsLoaded()` and the SAVW factory
(`fn_80182830`, `PORT_FACTORY` in `src/Kyoto/CFactoryFunctionsPort.cpp`) returns an empty object.
The dummy world itself completes on frame 4 and the SAVW read from `FrontEnd.pak` is issued.  (Superseded below: the SAVW factory is now real.)

**After the eighth upstream sync (2026-10-01)** the probe is unchanged in position:
`MP_PORT_FRAMES=300 tools/boot_probe.sh` runs 300 frames and exits 0 with the same stubs as before
it, less `CGMSinglePlayer`'s constructor and `fn_8032F6EC` (both real now): eleven distinct reach
stubs and the seven `CCubeRenderer` auto-stubs. The compiled modules are now initialised through
upstream's `CRELFileToken`/`CRelFile` rather than `PortModuleManager.cpp`.

**The SAVW factory is real since 2026-10-01, and the next wait is the HINT asset.**
`src/MetroidPrime/CWorldSaveGameInfo.cpp` (port-only: `files.cmake`, not `configure.py`, because
retail's `fn_80182830`/`fn_80182EC8` sit in the unsplit `auto_03_80182830_text`) has the real
`CWorldSaveGameInfo(CInputStream&)` and `fn_80182830`. Measured under gdb on `FrontEnd.pak`'s SAVW
(64 bytes, version 5): `areaCount=1`, one cinematic `0x9`, every other list empty.
`CSaveWorldIntermediate::InitializePump` now completes and `CMemoryCard::mWorldInter` is null, so
`CMemoryCard::InitializePump` takes its second branch and waits on `mHints.IsLoaded()`, which is
false for all 300 frames because `'HINT'` (`fn_8017F988`) still returns an empty object.
`SetGameState(7)` is still the only state change and `~CPreFrontEnd` never runs.
Notes: `docs/goal-notes/port-boot-savw-factory.md`.

**`CGMSinglePlayer` is real since 2026-10-01; the `'HINT'` wiring is blocked on the CMFGame
IOWin.** `src/MetroidPrime/PortCGMSinglePlayer.cpp` (port-only, retail 0x80193BD4-0x80193E08) replaces
the reach stub, whose uninitialised object crashed `SetGameState`. Measured
(`boot-progress.sh`): PASS, 2 runs further than 2 head runs, both exit 0 at `frame: 300`; link 322
undefined (323 before), 0 duplicates. The HINT forward itself works (44 hints parsed, plausible
values) but is **not committed**: with it `~CPreFrontEnd` runs, `SetGameState` gets its second call
(`kCFS_FrontEnd`) and then `kCFS_Game` builds `fn_801F47F4` (retail's CMFGame ctor, a reach stub),
and `CIOWinManager::Draw` (`CIOWinManager.cpp:200`) crashes on the null IOWin at frame 5-7;
`boot-progress.sh` says BEHIND. Patch and backtrace: `docs/goal-notes/port-boot-hint-factory.md`.
Next: a real CMFGame (`fn_801F47F4`, 0x2D4 bytes), then apply the HINT forward; (3) a real
`FStringTableFactory`. Weak spots, all reach stubs leaving their object uninitialised: the
constructors of `CWorldTransManager`, `CRelayTracker`, `CMapWorldInfo` and `CGMFrontEnd`,
`CGameStateEnvVarManager::LoadFields` (twice), `~CWorldState`, `CMain::ResetGameState`.

`Carve8026E7F0.cpp` calls `fn_802C162C` with two arguments, as retail does; the forwarder's
third (`write`) is then whatever the register holds. Not on the boot path today.

So `boot_progress.py` has a third rule: head and candidate both exit with code 0 with the same
markers, and the candidate no longer hits at least one stub the head hit = further (new stubs are
allowed and reported; a removed call or an empty body passes it, and the reviewer rejects those).
`blocker` names the most-hit stub of a cleanly exiting head (`port-boot-stub-<sym>-<head7>`, with
the `symbols.txt` name for the address in the reason), so `queue_boot_blocker` seeds these one at
a time as the head moves. Not yet exercised by a lane.

The shutdown hang that was here (`aurora::gx::fifo::drain` under `CGraphics::Shutdown` <-
`~CGraphicsSys`) was a teardown-order bug in `platform/main.cpp`: `graphicsSys` was a plain local of
`main`, so it was destroyed after `aurora_shutdown` had joined Aurora's FIFO worker, and the GPU
stall waited for a thread that was gone. It now lives in an inner scope that closes before
`aurora_dvd_close`. The judge could not see that fix (same markers, no stack = undecidable), so
`boot_progress.py` now scores "the head crashed or hung, the candidate printed the same markers and
exited with code 0" as further; a non-zero exit, or an exit against a head that also exited, is
still undecidable. Notes: `docs/goal-notes/port-boot-aurora-gx-fifo-drain-a62a945.md`.

A boot item left in the review queue stops `queue_boot_blocker` from seeding any new one
(`has-verify` counts review items). Measure the stop with `tools/boot_probe.sh` rather than
trusting this date; earlier stops are in `docs/history/handoff-to-2026-10-01.md` and
`docs/goal-notes/port-boot-*`.

## History

Earlier narratives, verbatim, with figures that are not current: `docs/history/handoff-to-2026-10-01.md`
(moved 2026-10-01; it also holds the index of headings) and `docs/history/handoff-to-2026-09-29.md`.
