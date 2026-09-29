# Decomp findings, per item

Dated findings moved verbatim out of `docs/RUNNING_THE_DECOMP.md` on 2026-09-29: one section per goal item or lane. The techniques still hold unless a later section says otherwise; the counts in them were right when written and are not current. New items' notes are committed as `docs/goal-notes/<id>.md` instead.

## A defect found in the rig (2026-09-25)

`config/G2ME01/build.sha1` names its files with paths relative to `build/` - literally
`build/G2ME01/<Module>/<Module>.rel` - and `dtk shasum` reads those paths. A lane that
configures into `build-clone/` therefore has its integrity check read the **master tree's**
`build/`, not its own output. In practice `ninja` printed `87 files OK` while the lane's own
`build-clone/G2ME01/ScriptCoin/ScriptCoin.rel` had a different sha1 from the original. Every
REL lane's "cmp silent, 87 files OK" report was unproven, and one of them held a module whose
own code does not reproduce it.

Rules that follow, and they are not optional:

- **A lane must not symlink the master `build/`.** Give it its own directory, so the check's
  relative paths resolve to the lane's artifacts.
- **Verify against `config/G2ME01/config.yml`**, which records each module's expected hash -
  not against a copy of the file in another tree, and not against "the check passed".
- **`87 files OK` from a *lane's clone* is not evidence.** That rule dates from the rig defect above,
  where the check read the master tree's files. In a real `build/`, ninja's `CHECK` edge runs
  `dtk shasum -c config/G2ME01/build.sha1`, which hashes the DOL and all 86 RELs and is the same
  check as `config.yml` - so **ninja's exit status is the acceptance test**. Reading hashes off disk
  afterwards is the unreliable part: a failed ninja leaves the previous `main.dol` in place, which is
  how an early commit here claimed a green DOL after a build that had failed.

### A second rig defect, found the same day: `flip_test.sh` never ran configure

`tools/flip_test.sh` needs the arguments `configure.py` was last run with, which `build.ninja`
records as a ninja variable spanning several lines, each continuation ending in ` $`:

```
configure_args = --version G2ME01 --compilers $
    /path/to/compilers $
    ...
```

The old extraction (`sed -n 's/^configure_args = //p' build.ninja | tr -d '\\\n'`) read only the
first line, so it ran `python3 configure.py --version G2ME01 --compilers` - an argparse error -
and every single flip reported `configure.py failed` followed by `FAIL -> reverted`. **A lane that
trusted it would have concluded, wrongly, that nothing could be promoted.** It was found by
running the tool by hand on `CScriptCannonBall` and reading the output instead of the exit code.

The fix joins the continuation block first (see `CONFIGURE_ARGS` in the script) and prints the last
15 lines of the ninja log when the build genuinely fails, so a real failure is diagnosable from the
tool's own output. Two lessons worth keeping: a verification tool that fails *closed* is
indistinguishable from "nothing passes" unless you read its output, and any tool reading
`build.ninja` must handle multi-line values.

`tools/decomp_build.sh` had a smaller version of the same disease: its report tail indexed
`fuzzy_match_percent` directly, and objdiff omits that key for the data-only units in the report
(any `auto_*` region with no code), so the plain invocation died with `KeyError` before printing a
single unit. It now skips units with no `total_functions` and defaults the missing percentages, so
the worklist it prints is usable again.

### A third rig defect: the acceptance test could pass on nothing

`tools/flip_test.sh` reported `PASS` in two situations where it had tested nothing at all:

- **The unit is in no `splits.txt`.** Nothing claims its range, so our object is compiled and never
  linked; the flip is a no-op. It passed on `CScriptIngSwarm.cpp` in 0.6 s. (Two commits wired
  `IngSwarm` and `WallCrawlerSwarm` this way, into nothing.)
- **The unit is `Matching` with no source file.** `configure.py` prints `Missing source file` and
  links the **retail** object; the check then passes trivially. `flip_test` hid that line with
  `>/dev/null`.

Both are now refused with an explanation, and the script exits non-zero if anything failed or was
skipped, so a caller cannot read success off a partial run. It also keeps its `configure.py` backup in
`mktemp` rather than one shared `/tmp/opencode/cfg.before` - two lanes flipping at once used to
restore each other's file, which is a plausible cause of the lost `Rel(...)` blocks described below.

The general lesson, and it is the same one three times over today: **a verification tool must be
proven able to fail.** `gate.sh` exists so that the acceptance test cannot be run partially, and
`report_diff.py` exists because losing a function was invisible to every gate.

### A fourth rig defect: `flip_test.sh` could not verify any MusyX unit (fixed 2026-09-29)

`unit_info()` built the source path as `'src/' + source`, but `configure.py`'s `MusyX()` helper sets
`"src_dir": "extern/musyx/src"`, so every MusyX unit failed check (a) with "no source file" - and the
goal loop's `match` items on MusyX could not pass however right the code was. `match-snd3d` hit it:
the lane matched `CalcEmitter` (17/17, `& 0x100` for 2.0.3 where upstream's ternary gave `0x180`) and
was set aside. `unit_info()` now takes the root from the enclosing `MusyX(` call, and the
`Missing source file` guard matches the full path as well. `snd3d.c` then flipped: `PASS -> kept`.

### The unattended goal loop, and five ways it passed changes nobody had checked (2026-09-27)

`tools/run_goal.sh` (run by `mp2-goal.service`) takes items from `build/goal/queue.json` in the
`../wt-mp2-goal` worktree, runs one agent per item using `docs/goal-unit-prompt.md`, judges the result
with `tools/goal_check.sh` and commits on `goal/decomp` only when the judge says PASS. After any
pass that leaves `goal/decomp` at least `MP_GOAL_FF_EVERY` commits (default 10) ahead of master, it
fast-forwards master to `goal/decomp` if master's tree is clean, and logs the skip if it is not.
It first merges master in, because master gains tooling commits the branch lacks. A conflict
aborts that merge and skips the fast-forward. (Until 2026-09-29 the trigger was every tenth pass of
one process; lanes restart often, so it fired once in 40 passes, silently, and the branch drifted
78 commits ahead.) The first version ran for hours and never produced
a result anyone could trust:

- **Ghost agents.** `opencode run` without `--standalone` is a client of the shared server. When the
  timeout killed the client, the session kept running on the server, and several agents ended up
  editing the one worktree at once. Agents now run with `--standalone` under `timeout -k`, so they die
  with the run.
- **Sessions piled up.** Every run left its session in `opencode.db` (149 in the two lane worktrees
  by 2026-09-29). After each agent, review and fix run, `prune_sessions` now deletes the lane's
  earlier sessions and keeps the newest one: opencode registers the lane worktrees, and deleting
  a worktree's last session was followed by that worktree disappearing (2026-09-28). The
  transcripts in `build/goal/agent/` are the record. `MP_GOAL_KEEP_SESSIONS=1` turns it off.
- **It judged the wrong tree.** `goal_check.sh` measured the main checkout, so every item was judged
  on master. `MP_GOAL_TREE` is now exported, and the judge's baselines (`build/goal/judge/`) are
  recorded per HEAD in the worktree and protected by checksums. An agent that edits them fails the
  item, and the baselines are recorded again.
- **A port item could not fail.** "Undefined count did not rise" passed on no change at all, and a
  port that *did not compile* reported zero undefined symbols, which read as a perfect link. Now a
  port item needs a `src/`/`include/` change. If `link_check.sh` prints `LINKER NEVER RAN` or compile
  errors, the counts are treated as vacuous. The target must be in the baseline undefined list and
  gone afterwards, or the item must name a host test in `tools/goal_verify/` (`goal_queue.py add
  --verify`). An item the judge cannot see goes straight to review, without an agent run.
  A verify script can boot the port: `port-pak-pump.sh` runs `boot_probe.sh` against the disc
  and passes only if every admitted pak reaches `kAP_Loaded` and the boot leaves the pump for
  the renderer. It was measured failing on `goal/decomp` `eac0c3e` and passing with the fix
  before it was queued. A verify script that has never been seen to fail proves nothing.
- **Agents could edit the judge.** Any change under `tools/`, to the port baseline file or in
  `build/goal/` fails the item. A `NEW:` line whose target is under `tools/` therefore goes
  straight to review: three such items sat in the queue and could only fail.
- **A timeout threw finished work away.** `port-streamnewgamestate` attempt 1 reached
  `goal_check: PASS` at ~3486 s, was killed at 3601 s and reset. An agent that times out
  (124/137) with changes under `src/`/`include/` is now judged like any other, not reset.

- **The loop was not running the model it named.** `opencode run` ignores an agent file's
  `model:` and uses opencode.json's top-level model, so every worker and reviewer session up to
  2026-09-28 ran on `opencode-go/mimo-v2.6-flash` (read back from `session_message` in
  `~/.local/share/opencode/opencode.db`). `run_goal.sh` now passes `-m` per agent (`model_for`:
  `worker`/`spacebunny` → `opencode-go/space-bunny-free#max`) and logs the model on each run.

- **Nobody read the diff.** The judge proves a change breaks nothing it measures, and nothing else.
  The first real pass (`6973386`, `port-pak-byteorder`) carried about 200 lines of `src/` changes
  beyond its item that no check covers. A change the judge passes now goes to a reviewer agent
  (`MP_GOAL_REVIEWER`, default `worker` - space-bunny at reasoning max, in a fresh session; it was
  `ornith` until 2026-09-28), with the brief in
  `docs/goal-review-prompt.md`: scope, faked targets, bypassed walls, host correctness, and doc
  claims. A REJECT appends its reason to the item's notes and then gets `MP_GOAL_FIX_ROUNDS`
  (default 1) fix rounds: the same agent, in a fresh session, is handed the reviewer's paragraph
  and corrects the staged change in place, and the judge and the reviewer run again as on a
  first pass. Only a change still rejected after that fails the attempt (2026-09-29: 10 of the
  11 rejections so far said the code was right and named doc claims to restate, and each one
  threw a judged change away). The judge has the same kind of round (2026-09-29, `gate_fix_round`,
  `MP_GOAL_GATE_FIX=0` turns it off): when `gate.sh` is goal_check's only failing check and every
  failing step is bookkeeping (`MP_GOAL_GATE_FIXABLE`, default `docs raw-offsets files-cmake
  decl-order module-order`), the agent gets the `GATE FAIL` line and the gate logs for one round and
  the judge runs again. The Parasite, ElitePirate and Splitter heads each matched 100%, failed only
  `raw-offsets`/`files-cmake` twice, and were landed by hand with only those lines added. A failed
  `match` item whose notes carry a `WALL:` line (the prompt asks for one when every spelling sits at
  the same sub-100% score) goes to review after one run instead of two, and does not count toward the
  consecutive-failure stop. On 2026-09-29 the queue was re-sorted by hand: progress first, then
  match items by their worst remaining function, lowest first. Match items whose remaining functions
  were all >=97% were parked for review, and so were five units whose every function is 100% but
  which `flip_test.sh` failed at link time (CTweakAutoMapper, CQuaternion, CLight,
  CStateMachineFactory, CParticleGen). Measured before the change: match 9/46, progress 32/53.
  Docs are the driver's, not the agent's (2026-09-29). Before judging, `run_goal.sh` restores
  `HANDOFF.md`, `RUNNING_THE_DECOMP.md` and `LANE_BRIEFING.md` from the base, and `goal_check.sh`
  runs the gate with `MP_GATE_DOCS_WRITE=1`, so `check_docs_claims.py --write` re-derives the state
  block, the per-unit counts, the module-wiring sentence and the probe count from the build. The
  item's notes file is committed as `docs/goal-notes/<id>.md`, and the reviewer judges code and
  config only. Only an agent error backs off now; a judged failure goes straight to the next item
  (the old sleep was 2.8h of 51h lane time). When nothing is ready, a lane runs
  `tools/goal_seed.py` once per branch head (`MP_GOAL_SEED_MAX`, default 10, 0 turns it off). It
  seeds REL modules with no own code first, then DOL units with 1-3 functions left, the worst below
  97%, and the unit at >=90% fuzzy. No verdict means
  no commit: the item goes to review with its patch kept in `build/goal/review/`. A reviewer that
  changes the tree has its verdict voided. The reviewer can only block a commit, never rescue one
  the judge failed. It reads only the kinds in `MP_GOAL_REVIEW_KINDS` (default `port`). A match
  item is decided by `flip_test` and the sha1s, which prove the bytes. A port item's checks can
  pass on an empty stub.

Every path was then exercised with a stub agent (`MP_GOAL_OPENCODE`): good, broken build, agent
error, tamper, malformed and duplicate `NEW:` lines, second instance, and the disk guard. Each one
failed or passed as intended. The reviewer paths got the same treatment. A PASS committed with the
verdict in the message. A REJECT failed three times, recorded three reasons, then went to review.
A reviewer that edited a source file had its verdict voided and its edit kept out of the commit,
and the next try passed. A reviewer with no verdict sent the item to review and stopped the loop.
ornith also reviewed `6973386` itself. It flagged stale line references and an understated doc
claim, judged the extra diagnostics to be in scope, and passed it without touching the tree.

#### Parallel lanes (2026-09-28)

Agent time dominates an item (5-30 min against ~40 s of judging), so several items run at once.
`MP_GOAL_LANE=k` runs `run_goal.sh` as lane k (`mp2-goal@k.service`, set up by
`tools/goal_lanes.sh setup N` and `install-unit`):

- Lane k works in `../wt-mp2-goal-L<k>` on `goal/lane-<k>`, reset to `goal/decomp` before every
  item. Its lock, log, judge baselines and `item.json` are its own. The queue, notes, agent
  transcripts and review patches stay shared in `../wt-mp2-goal/build/goal`
  (`MP_GOAL_QUEUE_DIR`).
- `goal_queue.py next --lane k` claims the item, under an flock on `queue.lock` that every
  queue command takes. Other lanes skip it. done/fail/review drop the claim, and a lane that
  restarts releases its own stale claims. `has-next` exits 3 when every ready item is another
  lane's. The lane then waits rather than stopping.
- Judging and review run in parallel. Landing does not: under `publish.lock` a passed change is
  carried onto the current `goal/decomp` (`git apply --3way`). If the tip moved, the judge's
  baselines are recorded again and the change is **judged again** there. It is then committed
  on the lane branch and published by `update-ref` with the old tip as the expected value. If
  the change does not apply, or fails on the moved tip, the item is *released* for a fresh
  attempt, not failed, and the reason goes into its notes. **A carry whose conflicts are all in
  `docs/*.md` is not released (2026-09-29):** `tools/union_docs_conflicts.sh` union-merges them,
  the worktree is rebuilt and `tools/sync_state_block.py --dedupe` re-derives the state block, and
  the re-judge decides it. All 11 carries that had failed conflicted only in `HANDOFF.md` and
  `RUNNING_THE_DECOMP.md` (two lanes appending to one table, both moving the state block). A union
  keeps both sides of a line both lanes rewrote; the counted lines are re-derived and
  `check_docs_claims.py` judges the rest, and the commit message says the docs were merged after
  review. The module-wiring sentence in HANDOFF (`**N units of our own code in M
  modules**` and its list) is re-derived the same way, from the judged tree's
  `tools/check_module_wiring.py`: before that, two lanes that each wired a module left two stale
  copies and the docs gate rejected a passing change (`progress-rel-head-darktrooper`, 2026-09-29).
- Nothing checks out `goal/decomp` while lanes run; `setup` detaches `../wt-mp2-goal`. Lanes hold
  `mode.lock` shared and the single loop holds it exclusively, so the two never run together.
- Only lane 1 runs the boot-blocker scan.

**A match item passes only if its unit is `Object(Matching, ...)` in `configure.py` after the
change and `flip_test.sh` says PASS (2026-09-28).** Before this, `goal_check.sh` passed flip_test
the queue target as written (`Kyoto/Audio/CStaticAudioPlayer`, with no `.cpp`). flip_test found no
entry and printed `SKIP ... not listed`, and the judge read SKIP as "already Matching". Three
items were marked done with their unit still NonMatching: `match-cpvsvisoctree` (`82f8f51`) and
`match-cstaticaudioplayer` both landed docs-only walls, which are honest negative results kept
as docs, and `match-cunknown90` landed nothing and is re-queued. The judge now resolves the
target to its configure.py entry (`.cpp`/`.cp`/`.c`), fails an absent one, and treats SKIP as a
failure. flip_test also stops if `mktemp` fails. A lane had its `.tmp` vanish mid-run, and the
empty backup path would have left a failed flip in configure.py.

**Feed the loop near-misses, not whole units (2026-09-28).** Before queueing match items, run
`tools/flip_test.sh $(tools/flip_candidates.py)` yourself. It takes minutes, and any unit that holds
is free. For each one that fails, write the flip output and `tools/compare_unit.sh` into the
item's notes file, because the agent reads that first. Then queue
`tools/flip_candidates.py --missing 1 --min-missing 1 --json` in gain order, with the one short
function named in the reason. `goal_queue.py add --update [--first]` re-briefs or re-orders an
item that is already queued, under the queue lock, so the lanes can keep running. Proven walls
go to `review`, not back to an agent.

**A `match` item on a unit too big to flip can only be reset, so use `progress` (2026-09-28).**
Measured in the lanes' notes: the CStateManager layout fix (+34 functions) was done correctly four
times and the CGameState helpers (+5) twice. Each time it was thrown away because flip_test failed
on a unit sitting at 69/239 or 70/116. A `progress` item passes `goal_check.sh` when the full gate
is clean (report_diff fails any worse function), the target unit's `matched_functions` rises
strictly over the judge's baseline, the diff touches `src/` or `include/`, and no added line
contains `asm`/`__asm`. The target resolves in `report.json` by unit name or `/`-suffix and must
name exactly one unit - except `module:<Module>`, which sums over every unit under `<Module>/`,
because carving a REL module replaces its `auto_*` units with new names the baseline lacks. The
module's sha1 is held by the gate like any other. The reviewer reads progress items (default `REVIEW_KINDS="port progress"`)
for counts bought by gutting a body. Queue the big units' remaining work as `progress`, and keep
`match` for units one item can flip.

**A `match` item whose flip fails is judged as `progress` (2026-09-29).** `match-ccharlayoutinfo`
took its unit 27/28 -> 28/28 with every other check green and was discarded twice: the unit cannot
flip for an object-layout reason no C++ edit reaches (rescued by hand as `5ce5610`). Now
`goal_check.sh` holds a flip `FAIL` back until the verdict; if nothing else failed, it runs the
`progress` test above (target rose, `src/`/`include/` touched, no asm), and on a pass exits **3,
PARTIAL**. `run_goal.sh` treats 3 as a pass with `PARTIAL=1`: the change is reviewed as a
`progress` change, committed as `progress: <id>`, and `goal_queue.py partial` puts the item at the
back of the queue with `fails` reset and `partials` counted. A flip with no verdict line, a SKIP,
or a PASS that left the unit `NonMatching` still fails outright. A PARTIAL on a rebased tip that
now flips becomes a full pass.

### The boot-progress judge: boot blockers that judge themselves (2026-09-27)

`tools/goal_verify/boot-progress.sh` is the one verify script that fits any item: it passes a
port item when the boot gets **further** than at the branch head. `boot_probe.sh` runs the port
under `boot_gdb_run.sh` (via `MP_PROBE_RUNNER`), twice. A fault stops the run. A run still
alive after `MP_BOOT_HANG_SECS` (45 s) is interrupted five times, one second apart, and gdb
prints the main thread's stack each time. `boot_progress.py` reads the log. It uses the boot
markers seen (`boot: step`, `Initializing renderer`) and the repo-source frames of the stop. A
hang is placed at the frames all five samples share. Head lines are mapped through
`git diff -U0 HEAD` so edited code does not shift the comparison. More markers is further,
whatever kind of stop follows. The first version checked the kind first and scored a clean exit
as undecidable. That failed a real fix: `port-boot-cpakfile-sresinfo-getsize-4dfc8ed`
attempt 1 took the boot from the `GetSize` fault through steps 12-20 to a normal exit, printing
10 new markers.
Otherwise the first differing frame decides, by a later line in the same function. **Every
candidate sample must beat every head sample**, and the verdict is `BOOT_PROGRESS PASS|FAIL`.
Verify mode also fails a change that edits a marker line in code or adds a file that prints one
(comments may quote them).

The sampling is not optional. At `9f119c5` the head's boot is not deterministic: one run faults at
`CResLoaderPakPump.cpp:98`, another hangs in the allocator's list insert (`fn_802FC378`) at a
different line each time. With one sample per run, an unchanged tree once scored "further"
(a hang at `AsyncIdlePakLoading:103` against the fault at `:98`).

Measured both ways in a throwaway worktree at `9f119c5`, before any use:

- the unchanged tree → `BOOT_PROGRESS FAIL` (one pair "further", the others behind or no further);
- the tree with the `port-pak-pump` fix (`1f2701c`) → `BOOT_PROGRESS PASS`, with new markers
  "Initializing renderer..." and step 21c;
- a code line with a marker in the diff → FAIL. A new file printing a marker → FAIL. A new
  file quoting one in a comment → no guard failure.

In the loop (`run_goal.sh`): before an item whose `verify` is this script, the head's position is
recorded into `build/goal/judge/boot.base.json` (`record_boot`, once per head). Its sha256 is
held in the driver's memory, because the agent can write under `build/`; a changed baseline fails
the attempt. At the loop top, when no boot-progress item is queued or in review and the head
moved, `queue_boot_blocker` boots the head and queues where it stops, **at the front**, as
`port-boot-<func>-<sha7>`. An agent's `NEW:` line may end in `| verify: boot-progress.sh`; no
other script can be named there. `MP_GOAL_BOOT_BLOCKERS=0` turns the scan off.

Limits:

- A skip, stub or early return moves the stop point just as a fix does. Only the reviewer
  catches that (`docs/goal-review-prompt.md`, point 3).
- ~~Once the boot reaches the game loop, a "hang" is the game running. Turn the scan off by
  then.~~ Superseded by the frame loop below: the loop ends itself after `MP_PORT_FRAMES` frames.
- A head that exits cleanly still records a baseline (its markers), but the scan has no stack to
  name and queues nothing.
- A run takes a port build plus up to 2×(45+5+60)+30 s of boots, inside `goal_check.sh`'s
  600 s verify limit.

After `1f2701c`, the head's boot passes step 21c and faults in `CEnvFxManager::Initialize` →
`fn_802FC63C` (`CResLoaderLoadNewResourceSync.cpp:86/96`, in `CPakFile::SResInfo::GetSize` /
`CDvdFile::StallForARAMFile`). That is the first blocker the scan will queue.

### The frame loop under the same judge (2026-09-27)

Retail's frame loop (0x80006034-0x80006460) is written as step 21 of `PortBoot.cpp`, one statement
per retail call. A callee with no body is a `PORT_FRAME_STOP(name, "addr, size")` on the line
where retail calls it. The macro prints `frame loop stopped: ...` and calls `abort()`, so the
run stops there with a stack. Every frame prints `frame: N`, which is a boot marker. More
frames means further. `MP_PORT_FRAMES=N` returns from the loop after N frames. The judge
exports `MP_PORT_FRAMES=300` (`boot-progress.sh`), so a tree that runs the full budget is
"still running" and gets no stack, not a hang. `frame: `, `MP_PORT_FRAMES` and
`frame loop stopped` are in the protected marker list, so an item cannot edit them.

One rule applies only to these stops. When the head stops at a `frame loop stopped` line that
the candidate rewrote, and the candidate's stack is deeper from that line, that is further
("the call now happens"). A callee that faults is progress over a stop that never called it.
The scan's reason for such a stop tells the agent to write the callee, replace the
`PORT_FRAME_STOP` with retail's call, keep the undefined count down, and leave the macro, the
frame print and the budget alone.

Measured in a throwaway worktree at `goal/decomp` `4dfc8ed`, plus the `GetSize` fixes. The
head stops at frame 1, `fn_801F05D0` (superseded 2026-09-28: that callee has a host body, and
the head now stops at `fn_8030172C`; the rule below is unchanged):

- unchanged → FAIL;
- the stop replaced by a call that faults inside the callee → PASS (declared-stop rule);
- the stop replaced by the real call, which returns through a reach stub → PASS (a later line);
- a fault inserted before the stop → FAIL (behind);
- the `frame:` print removed, or `abort()` removed from the macro → FAIL (marker-line guard);
- synthetic: 5 frames against 3 → further. 300 against 300 → undecided. 3 against 5 → behind.

The next known wall comes after the stops before it are written. Step 18 builds a *local*
`CIOWinManager`, so `IsEmpty()` is true and frame 1 takes the reset path
(`docs/research/boot_path.md`, row 21).

## A 20-84 byte function in an `auto_*` unit is a whole unit, and it closes both gaps (2026-09-25)

The five smallest entries on the port's link-gap list all landed in one lane as five
`Matching` DOL units, byte-exact, **+5 matched, +5 linked, and the link gap 41 -> 36 with
no symbol added**. That is the cheapest thing in this repository right now and the recipe
is short, so it is written out in full.

**The arrangement.** The port's game sources and the decompilation are the *same* `src/`
tree, so one file can be both: a definition that closes a link-gap entry *and* a unit that
`dtk dol split` carves a range for. Per function, four edits:

1. a new `.cpp` under `src/` (not appended to an existing unit - a new file cannot move any
   other unit's percentages, which is the risk LANE.md warns about),
2. a `.text` block in `config/G2ME01/splits.txt` claiming **exactly** the function, inserted
   in address order, checked with `tools/range_bounds.py` and `tools/range_owner.py`,
3. `Object(Matching, "<that path>")` in `configure.py`,
4. the path in `files.cmake`, which is the manifest `tools/probe_sources.sh` and `mp_game`
   both read.

The claim is one function's exact byte range and **no** data section, so the object emits
no `initializer`-order problem and no second `.text` range. `tools/unit_fit.sh` on each of
the five said "no extra functions"; two of them emit 4 bytes of unclaimed `.sdata`
(`CGunWeaponTouch.cpp` picks up `SolidMaterial` from `CGunWeapon.hpp`, harmless under
`-strip_partial`) and the DOL still hashed correctly.

**The `fn_` name was kept on all five, deliberately.** The briefing said to rename each to the
mangled name MWCC emits. That is right when retail names the function, and wrong here: all
five are unnamed in retail, every one of them is already *called* by the port under its
`fn_` name (`CActor.cpp:677`, `CPlayerGun.cpp:697`, `CStateManager.cpp:489`,
`main.cpp:39`, `CGameOptions.cpp:14`), and an `extern "C"` definition of the same name
serves both builds with no edit to the caller. Renaming would mean editing two files to move
a symbol whose correct name is unknown. What each function *is* went in the source as a
comment with the measurement that identified it.

**Three codegen facts, each one a wrong guess first.** All three are MWCC behaviour, not
decompilation difficulty, and all three are reproducible in a minute with
`tools/fast_try.sh` + `tools/lanediff.sh`.

- **`rstl::pair`'s `const L&` constructor puts bool literals in `.sdata`.** Writing
  `return rstl::pair<bool,bool>(true, false);` gives `lbz r0,@105` / `lbz r0,@106` where
  retail has `li r4,1` / `li r0,0`. Default-constructing and then assigning the two fields
  gives retail's bytes exactly. **Do not "fix" it by taking the constructor's parameters by
  value** - that is the obvious reading of the evidence and it is a trap: `rstl/string.hpp`'s
  `position_iterator` goes through the same constructor, the by-value form moves it 0x30
  bytes, and the DOL sha1 goes to `bcfaca08334a11f278abd0f32079ca6a83a8aa8d`. That is the
  third rig defect in this file, and it is a *shared header*.
- **`rlwimi rA, rS, 7, 24, 24` is bit 0 of a `bool : 1`**, and `6, 25, 25` is bit 1; the
  pattern is `sh = 7 - N`, position `24 + N`, for byte bit `N`. Calibrate against a unit
  that already matches rather than decoding the mask: `CGameOptions`'s constructor
  (0x80161B9C) writes a byte at a 4-aligned offset with six `bool : 1` members. Guessing
  "bit 7" from the mask alone put the field 0x58 too high and produced one differing
  instruction out of sixteen.
- **A pointer to the counter reproduces retail's register allocation** for
  `table[i] = 0; count = count + 1;`. Written as absolute subscripts of the global, MWCC keeps
  the *base* in a register and emits 876/880 displacements (4 bytes short of retail);
  written as `int* c = &global[0x36C/4]; c[1 + *c] = 0; *c = *c + 1;` it keeps
  `base + 0x36C` in a register and produces `add r4, r6, r0` + `stw r5, 4(r4)` and
  `lwz r4, 0(r6)` + `stw r0, 0(r6)` - retail's bytes.

**`#ifndef TARGET_PC` around a call to an unwritten callee.** Four of the five bodies call
something nobody has written. If that callee is on the port's link-gap list, calling it
costs nothing (it stays missing) - `CGunWeaponTouch.cpp` calls `fn_800E5D80` for exactly
this reason. If it is **not** on the list, the call *adds* a missing symbol, so the host
branch is a documented no-op instead: `fn_80340F9C` (a 0x1868-byte function) and
`fn_801ECE14` are both called from the retail body and both excluded from the host body by
`#ifndef TARGET_PC`. This is the same judgement as `CWorldState::Update`: the gap must go
down, and a body that trades one missing symbol for another has not moved it.

**What this arrangement cannot do.** The unit has to reproduce the range, so it needs the
class's *layout*, not just its behaviour. `CActorField25.cpp` reproduces offset `+0x5C`
from three measured anchors (`GetPoint__6CAABoxCFi(this + 0x14, i)` puts `CAABox` at `+0x14`,
and `0x801ECE54`'s zeroing puts `int[4]` at `+0x2C` and `float[4]` at `+0x3C`) plus declared
padding - the class's *name* is not recovered and the struct says so. A function of this size
whose class is entirely unknown, or whose only anchor is a raw `.bss` address, is much more
expensive than these five were.

### A printed table is not a measurement - do not read a formatted size back as a number

Two lanes in a row were briefed with wrong function sizes, and the cause was **mine**, not the
tree's. The table I generated to brief them ended every size with a literal `B` as a unit suffix
(`{sz}B`), and I then read the *printed string* `0x28B` as hexadecimal 0x28B - 651 bytes - when
the value was `0x28`, 40 bytes. So:

| symbol | what I briefed | the real size | ratio |
| --- | --- | --- | --- |
| `fn_80038624` | 9,675 | 604 | 16x |
| `fn_8003C054` | 1,803 | 112 | 16x |
| `fn_800C08D4` | 651 | 40 | 16x |
| `fn_800E5C78` | 2,699 | 168 | 16x |

**`symbols.txt` sizes are plain hexadecimal with no suffix** - confirmed against
`fn_80003C44`, whose `size:0xBC` is 188 and which `report.json` also gives as 188. One lane
concluded from its own measurements that dtk appends `B` for bytes; that is **wrong**, and the
error would have been recorded here as a fact about the tooling. The honest lesson is narrower
and more general: **a table you formatted is not a number you measured** - parse the field,
never read the rendered string back, and sanity-check a column against `report.json` before
briefing anyone on it. A 16x error is not a detail; it told two lanes the work was three orders
of magnitude larger than it was, and nearly made both of them plan around the wrong thing.

The reverse check is cheap and worth doing every time: the sizes in `report.json` and in
`build/report.json`'s per-function entries are the same field, and a spot-check against one
function settles the convention for all of them.

### A struct layout measured with a host compiler is not a measurement (2026-09-26)

The port build is **64-bit** and retail is **32-bit**, so an `offsetof`/`sizeof` probe compiled with
the host `g++` answers a question nobody asked. `rstl::string` is `{const char*, control*, uint,
rmemory_allocator}`, which is 8+8+4+1 = **24 bytes on x86-64** and **16 on retail's PowerPC**. Any
generated struct full of strings is therefore measured 0x50 too big per fourteen strings, and the
error compounds silently down the member list.

This was measured, not reasoned: the same headers, the same tree, two compilers.

| | 64-bit host `g++` | **32-bit mwcceppc** | retail |
| `sizeof(rstl::string)` | 0x18 | **0x10** | 0x10 |
| `sizeof(CTweakContents)` | 0x37D0 | **0x3244** | 0x31F4 |
| `offsetof(CTweakContents, TweakPlayer)` | 0x1220 | **0x10E8** | 0x10E8 |
| `sizeof(SLdrTweakPlayer)` | 0x388 | **0x37C** | 0x37C |

The 64-bit column was published in `docs/research/tweak_globals.md` as the reason every member from
`TweakBall` on was mis-placed, and a lane was about to spend itself re-deriving sixteen
`LoadTypedef*` bodies to fix a defect that did not exist. The real error was **one** struct's size,
`SLdrTweakPlayerRes` (0x548 against 0x4F8).

**So: measure a layout with the unit's own compiler.** The target cannot be run, but it can be
read - emit the offsets as a `.data` array and `objdump -s` the object:

```sh
cat > /tmp/ctc.cpp <<'EOF'
#include "MetroidPrime/Tweaks/CTweakContents.hpp"
#include <stddef.h>
extern "C" unsigned int g_probe[] = {
  (unsigned int)sizeof(CTweakContents), (unsigned int)offsetof(CTweakContents, TweakPlayer),
};
EOF
$MP_TOOLCHAIN_DIR/build/tools/wibo build/tools/sjiswrap.exe \
  $MP_TOOLCHAIN_DIR/build/compilers/GC/1.3.2/mwcceppc.exe \
  <the unit's cflags, copied out of build.ninja> -c /tmp/ctc.cpp -o /tmp/probe/
build/binutils/powerpc-eabi-nm -S /tmp/probe/ctc.o | grep g_probe   # the array's offset
build/binutils/powerpc-eabi-objdump -s -j .data /tmp/probe/ctc.o
```

Two free cross-checks that would have caught it in minutes, and that are worth running on any
layout claim: **read the offsets out of a function the compiler already emitted for that struct**
(the `addi r3,r31,0x10e8` in our own `__ct__14CTweakContentsFv` is the number, and it is retail's),
and **compile a consumer and compare its bytes with retail's**. `docs/research/tweak_player.md` does
the second for the five `CTweakPlayer` thunks and gets 12/12 bytes each.

The same caveat in the other direction, and it is the port's problem rather than the decompilation's:
on the **host** the same named-member accessors read the wrong bytes, because the host layout is the
64-bit one above. A tweak header cannot fix that, and it is the same class of defect as
`PORT_NOTES.md`'s first finding about `OSModuleHeader`.

### A header comment that recorded *mwcceppc's own output* is the same defect one level up (2026-09-26)

The section above is about measuring a layout with the wrong compiler. The harder version is
measuring it with the **right** compiler and then reading the number as if it were retail's - which
is what `CHECK_SIZEOF` and every `//!< 0x...` comment invite, because they are all *our* numbers
until something in the retail disassembly pins them. Two headers in the tree were in that state,
and both were worth far more than any single function.

| header | what the comment said | retail | cost |
| `CStateManager.hpp` | `pad2_2[0x34]`, so `x1684` at 0x168C and `sizeof` 0x2958 | `0x2C`, `x1684` at 0x1684, `sizeof` 0x2950 | **17 functions to 100%** in one edit |
| `CAnimData.hpp` | "measured with mwcceppc ... `x178_particleDB` is at 0x188", `x120_unk[0x58]`, `sizeof` 0x630 | particle DB at **0x178**, `sizeof` 0x620 | `CActor::SetModelData` to 100% |

`CStateManager` is the one to generalise from. Its `m_isDarkWorld` was documented at `0x294c` and
`SetIsDarkWorld` sat at **99.79%** for a reason that had nothing to do with code: retail emits
`lbz r0,10572(r3)` and we emitted `lbz r0,10580(r3)`. One pad eight bytes too long, and **every
member from `x1684` to the end of a 0x2950-byte class** was eight bytes high. Nineteen functions
in the unit touch that range; **none of the nineteen was at 100%**, which is what made the fix
free - so the first thing to do with a suspected layout drift is count how many already-100%
functions read the range, because that number is both the regression risk and the upside.

**The signature, and it is cheap to scan for.** A class laid out wrongly by a constant makes every
function that touches the moved members land at 96-99% with a *uniform* operand delta. So: diff
the operand offsets of our object against retail's, per function, per paired symbol, and print the
set of deltas. `tools/offset_shift.py` (new, this lane) does it for every `main/` unit and lists
only the functions that are 80-100% and *not* already delta-0. A codegen difference gives deltas
`{0}` or a spread; a layout drift gives a single non-zero value repeated. That scan found both of
the above and found **no third** - after the fixes, every `main/` function between 40% and 100%
either has delta `{0}` or a spread.

Measuring the layout is `tools/probe_offsets.cpp` (new, this lane), which is the section above's
technique reduced to one member list: emit the offsets as a `.data` array, `objdump -s` it, and
then **check the list against the retail disassembly rather than against the comments**. Two
gotchas: `#define private public` around the include is what lets `offsetof` reach a private
member, and `offsetof` on a *bitfield* is an "illegal operand" under mwcceppc, so leave the flags
out and infer their byte from the member in front of them.

### A flag width is not a pointer width: `kAllocatorPointerBits` cost the port its first crash

The host-layout problem above has a nastier form, and it is worth separating out because the fix
looks local and is not.

`include/Kyoto/Alloc/AllocatorCommon.hpp` had

```cpp
static const int kAllocatorPointerSize = sizeof(void*);
static const int kAllocatorPointerBits = kAllocatorPointerSize * 8;
```

`kAllocatorPointerBits` is the width of the **flag field** in a block pointer - how many low bits
are flags rather than address. Every accessor in `CGameAllocator` uses it as
`x14_next & ~(kAllocatorPointerBits - 1)`. Retail's block pointers are `0x20`-aligned, so on
retail `sizeof(void*) * 8 == 32` and a five-bit mask is exactly right. On a 64-bit host the mask
became six bits, and **the sixth bit is `0x20`, which under a `0x40` block stride is address**:

```
block 2 at 0x7fff9d4c60c0
  x4_len        = 32
  x14_next      = 0x7fff9d4c6100
  expected next = block + sizeof(SGameMemInfo) + x4_len = 0x7fff9d4c6120
```

`x14_next` was **stored** correctly; `GetNext()` stripped the `0x20` on the way out. So the free
list walked 32 bytes short, left the block list, and read payload as headers - the guard words were
absent and a code address sat where the length belonged. The symptom, ten frames away, was
`FindFreeBlock` rejecting a good 180,220-byte block for a 135,168-byte request out of a 24 MB heap.

Three things made it survive a session:

- **The setters are correct.** `SetNext` ORs the old low bits back in, so the stored value is
  always right and only the read is wrong. Nothing looks wrong until a list walk.
- **It reproduces retail exactly on retail's word size**, so every "does it still match?" check
  passes - on the one platform where the question does not arise.
- **No tool objects.** The types are right and `0x3F` is a perfectly legal mask.

The fix pins the count to retail's value and **deliberately stops deriving it from
`kAllocatorPointerSize`** - the derivation *is* the defect, because it makes the mask a function of
the machine being debugged. `kAllocatorPointerSize` stays `sizeof(void*)`: it genuinely is about the
host and it still drives `EXPAND_PATTERN` and the top-nybble mask, which *should* widen with the
pointer. Only the flag count was wrong to widen.

**Generalise it as: when a constant is derived, check that the thing it is derived from is the same
kind of thing.** A field width is a protocol property; a pointer width is a machine property. They
agree at 32 bits and the agreement is the bug's cover. And prefer deriving from what actually
determines the value - where the arena is concerned that is `sizeof(SGameMemInfo)`, not a literal
`64` copied out of it, since a copied literal is a host-specific patch wearing a general fix's
clothes.

**The check that makes this safe to try is worth internalising: a fix that is a no-op under the
reference build cannot have broken what you were asked to preserve.** This one is a no-op for the
decomp build (MWCC pointers are 32-bit), so `GATE PASS` with `matched` and `linked` unmoved was
available *before* deciding to keep it, rather than after.

Two neighbouring defects in the same structure are still open, and both are recorded with
measurements in `docs/research/allocator_flag_mask.md`: `x4_len` is a 64-bit `size_t` carrying a
stale upper half (the `uint` fix costs `FindFreeBlock` its 100% match and was reverted), and
`x10_last` is not stride-aligned. **Both were tried and reverted for perturbing a Matching
function** - which is the rule working, not the rule being inconvenient.

### `CHECK_SIZEOF` is a consistency check, not a measurement - and a ctor's call *order* is not a base offset

Both halves of this cost three sessions over four bytes in
`include/MetroidPrime/CGameGlobalObjects.hpp`, and both are general.

**`CHECK_SIZEOF(T, n)` cannot tell you `n`.** It asserts that the *model in the header* is `n`
bytes; the value it checks is the one the header already declares. `CHECK_SIZEOF(CResFactory,
0xe4)` passed for as long as the header said 0xE4 - and so would `0xd0` or `0x30`. It is worth
writing, because it catches a member list that has drifted from the number written beside it, and
it is worthless as *evidence*: "CHECK_SIZEOF-confirmed" appeared in this repo's docs three times
as support for a size that was wrong. What is evidence, in decreasing order of directness:

1. **retail's own bytes** - the last store in retail's constructor, the argument to
   `operator new`, the displacement in one `addi`/`lwz`;
2. **an arithmetic closure** - two member offsets retail gives you plus a `CHECK_SIZEOF` on one of
   the classes, so `A + sizeof(B) == C` has to hold. `CResFactory` is 0xE0 because `CSimplePool`
   is `CHECK_SIZEOF(..., 0x24)` and retail builds it at `this+0xE4` on a factory at `this+0x04`
   (0x800084AC-0x800084B4), and because the member after that is built at `this+0x108`
   (0x800084B8) - two independent subtractions that agree;
3. `CHECK_SIZEOF`, which is worth nothing on its own.

**A constructor's call order gives you no base offset.**
`CGameGlobalObjects::CGameGlobalObjects` calls `fn_803096C4` on `this+0x00` and then `fn_802FB154`
on `this+0x04`, and three sessions read that as "`CResFactory` is at +0, and the `char pad0[4]` in
the header is spurious". The call order says what is built first. It does not say where the object
starts. **The instruction that settles it is in the same function and is a store to a global**:
`addi r0,r31,4` / `stw r0,-28380(r13)` at 0x80008528/0x80008534 publishes
`gpResourceFactory = this+0x04`, and `gpResourceFactory` is the `CResFactory*` (the 36
registrations address `CFactoryMgr` as `gpResourceFactory`+0x74, and `fn_802FB154` builds `+0x04`
and `+0x74` in itself). So the four bytes are real - they have a constructor at 0x800084A0 and a
destructor at 0x800065F0 - and deleting them cost two functions: `PostInitialize` 99.88 -> 98.37
and the ctor 28.35 -> 22.11, because the fourth argument to `AllocateRenderer` becomes
`mr r6,r29` where retail has `addi r6,r29,4`.

**So whenever a constructor is the evidence, go to its end.** A `CGameGlobalObjects`-shaped ctor
finishes by publishing its members: the tail of `fn_800084A0` at 0x80008528-0x8000855C is nine
instructions that give four absolute offsets, and the destructor at 0x80006518 gives six more.
The ctor's *calls* give the same numbers one field at a time and in the wrong order of certainty.
`tools/sda.py` is the only supported way to read the `disp(r13)` ones - `_SDA_BASE_` for G2ME01 is
0x8041FD80, and the SDA21 field is the **full** signed displacement.

**And the general blast-radius rule, which is why this looked like a tree-wide change:** a class's
*size* matters to its **owner's** layout; its *members' offsets* do not. `CResLoader` at
`CResFactory`+0x04 and `CFactoryMgr` at `CResFactory`+0x74 are offsets from a `CResFactory*`, and
every caller reaches them through `gpResourceFactory`, so nothing about `CGameGlobalObjects` can
move them. `CResFactory`'s extent is what reaches `CSimplePool` and everything after it. Before
promoting a "delete this member" change to a tree-wide event, work out which of the two it is: it
is the difference between 30 moving units and none. The full reports are in
`docs/research/paks.md`'s third correction - **0 units moved** either way, and the two variants
differed in two functions of one `NonMatching` unit, better in one and worse in the other.

### mwcceppc will not name a **private** member outside its class, so those classes can carry no `CHECK_SIZEOF`-style guard

Measured twice, 2026-09-26, and it is why the guard that would have caught the class of defect
above is unavailable exactly where the defect was:

| what | mwcceppc 2.7 says |
| `CHECK_OFFSETOF(CResFactory, x74_factoryMgr, 0x74)` | `illegal access to protected/private member` - the macro is `((size_t)&(((T*)0)->member))` and MWCC access-checks it |
| `NESTED_CHECK_SIZEOF(CGameGlobalObjects, resFactory, 0xe0)` | `declaration syntax error`, from `check_sizeof< CGameGlobalObjects::resFactory, 0xe0 >` |
| the same two against **public** members | fine - `CTweakValue::Audio`, `CPakFile::SResInfo` and `CStringTable::SReloadData` are all live `NESTED_CHECK_SIZEOF`s |

So the rule is: **`CHECK_OFFSETOF` and `NESTED_CHECK_SIZEOF` only work on public members**, and
every live use in the tree is a public one. `MetroidPrime/CStateManager.hpp`'s two
`CHECK_OFFSETOF`s are commented out for the same reason and the comment there does not say so -
worth fixing if you touch it.

The consequence is the useful part: **a class whose members are all `private:` - which is most of
them - has no compile-time layout guard in this project at all.** Its `CHECK_SIZEOF(T, n)` is
self-referential, as above, and no offset can be asserted. For those classes the only instrument
is `tools/report_diff.py` over a recorded baseline, and it does catch this: deleting
`CGameGlobalObjects`'s `pad0` shows up at once as `PostInitialize` 99.88 -> 98.37. **Record the
baseline before touching a header** - `tools/gate.sh --baseline` on a clean tree is what makes
that possible, and it is a step to do before the first edit rather than after the last.

### `r2` is `_SDA2_BASE_` (0x804223C0), not `_SDA_BASE_` - read the small-data base off the startup stub

`config/G2ME01/symbols.txt` has both, and picking the wrong one silently reads the wrong four bytes,
which is how a float constant turns into a byte table. **The startup stub says which, in three
instructions**, at 0x8000345C:

```
8000345c: lis  r1,-32701 ; ori r1,r1,22792   ->  r1  = 0x80435908   (stack top)
80003464: lis  r2,-32702 ; ori r2,r2,9152    ->  r2  = 0x804223C0   (_SDA2_BASE_)
8000346c: lis  r13,-32703; ori r13,r13,64896 ->  r13 = 0x8041FD80   (_SDA_BASE_, the GOT)
```

So `lwz`/`lfs`/`lfd D(r2)` means **`0x804223C0 + D`**, and both `.sdata` and `.sdata2` are inside
its -32KB window (the displacements actually used span -32768..-10944). `r13`, not `r2`, is what
`lwz rX,-28376(r13)`-style GOT access uses.

Worked example, 2026-09-25, and it is the difference between a function that is 97% and one that is
100%: `fn_800E6AD0` (0x800E6AD0) holds `lfs f0,-27796(r2)`. Read against `_SDA_BASE_` that is
0x804190EC, which is `.sbss` - a zero fill - and `CModelData`'s default scale looks like `0.0f`.
Read against `_SDA2_BASE_` it is 0x8041B72C, `.sdata2`, and it is `1.0f`, which is what a unit
scale is. Same for `fn_800CB764`'s `lfs f3,-28856(r2)` = 0x8041B308 = `0.0f`, and the four constants
of `fn_8001D678` at -32188/-32176/-32168/-32160, which are 0x8041A5FC and three doubles at
0x8041A610/0x8041A618/0x8041A620. **Decode a `disp(r2)` with `_SDA2_BASE_` before concluding
anything about the value.**

### mwcceppc materialises `operator new`'s operands differently from retail, so **no allocating function can be `Matching`** (measured 2026-09-26)

**This is the largest single blocker found so far, and it is not specific to any one function.**

`CIOWinManager::AddIOWin` (retail 0x80049BDC, 0x17C = 380 bytes) is written, instruction for
instruction, including the exception-safe `new` shape - and it stops at **95.24%** on two
allocations' worth of one instruction each:

```
retail   lis r3,-32710 ; addi r4,r3,26592 ; li r3,16 ; addi r4,r4,51 ; li r5,0 ; bl __nw__FUlPCcPCc
ours     lis r3,0                  ; addi r4,r3,0               ; li r5,0 ; li r3,16 ; bl __nw__FUlPCcPCc
         R_PPC_ADDR16_HA @stringBase0  R_PPC_ADDR16_LO @stringBase0
```

Retail builds the `operator new(size_t, const char*, const char*)` file-string operand as `lis` plus
**two** `addi`s, with the `lis` hoisted to the top of the block; this compiler emits `lis` plus
**one** `addi` against relocations. Every other instruction in the 380-byte function matches,
including the register allocation, the bottom-tested insertion walk, and the inlined two-word
`rc_ptr` copy with its AddRef through the *second* word.

**No spelling of the source changes this.** It is a property of mwcceppc's constant
materialisation, and it applies to `new T(...)` in every constructor, every `operator new` call,
and every implicit allocation - which is most of what is left on the frame loop and a large part of
the DOL. **Read this before writing a function that allocates**: the body is worth writing (the
port needs it, and the score says how close it is), but do not spend a session trying to reach
100% on the allocation sequence.

It is *not* the same failure as the section below. That one is an operand that cannot be resolved
at all; this one resolves perfectly and is one instruction short. And the fix, if there is one, is
worth finding: name retail's file-string constant (`extern "C" const char lbl_803A6813[];`) so the
object owns no `.rodata` - which a `Matching` unit may not have - and see whether the instruction
count moves. Measured once, negative; not measured with the named constant.

### mwcceppc emits small-data references it cannot resolve (measured 2026-09-26)

**MWCC will put an `extern "C"` object in `.bss` into the small-data area without checking that it is
inside the ±32 KB window around `_SDA_BASE_`, and the failure lands at link time, not compile time.**

Found while writing `CGameArchitectureSupport::UnloadAudio` (retail 0x8029EF20, 0xAC). Declaring the
three globals retail's body uses -

```cpp
extern "C" void* lbl_804152DC;
extern "C" int   lbl_80413EFC;
```

- produces `lwz r3,offset(r13)` with `R_PPC_EMB_SDA21` for **both**, and neither displacement is
  representable:

| object | offset from `_SDA_BASE_` = 0x8041FD80 | fits `int16`? |
| `lbl_80419884` (`.sbss`) | -25852 | yes |
| `lbl_804152DC` (`.bss`) | -43684 | **no** |
| `lbl_80413EFC` (`.bss`) | -48772 | **no** |
| `lbl_80413F00` (`.bss`) | -48768 | **no** |

Retail's own code used `lis r3,0x8041 ; addi r3,r3,0x52DC` for the first and
`lis r3,0x8041 ; addi r31,r3,0x3EFC` for the second - the absolute form, which is what a compiler
that checked would have to emit. **So the shape of retail's instruction is evidence about the
distance of the object from `_SDA_BASE_`, and reading a `disp(r13)` as a resolved address in a
`Matching` candidate is how you find out.**

The practical consequences:

- **A unit that takes the address of a far `.bss`/`.sdata` object cannot be `Matching` as written.**
  There is no source spelling tried here (plain extern, array extern, explicit cast) that makes
  mwcceppc choose `lis`/`addi`. Treat it as blocked until somebody finds the lever, and say so
  rather than shipping a unit that fails at link.
- **Check it before writing the body**, not after: for every `extern "C"` data symbol a unit will
  reference, compute `addr - 0x8041FD80` and see whether it fits a signed 16-bit. One command:
  `python3 tools/sda.py <addr>` (that tool exists for the decoding half of this).
- **The window is asymmetric in practice.** `.sdata2` sits just *below* `_SDA2_BASE_`
  (0x804223C0), and the largest displacement retail actually uses there is -32768, i.e. exactly the
  boundary. `.sbss` (0x80418EA0..0x8041A3A8) and `.sdata` (0x80417D80..0x80418E84) are both inside
  `_SDA_BASE_`'s window, so **a reference to a `.sbss` global is nearly always fine and a reference to
  a large `.bss` global is nearly always not** - `.bss` runs 0x803C5A20..0x80417D64, which is 256 KB
  wide, so most of it is out of range.

Worked example and the full function: `docs/research/frame_loop.md`, "Row 10".

### MWCC 2.7 accepts bit-fields in a mem-init list, and it is the only way to get retail's store order

Measured on `CModelData::CModelData()` (retail 0x800E6AD0), 2026-09-25. C++ forbids initialising a
bit-field in a member-initialiser list. **MWCC 2.7 accepts it**, and the acceptance is load-bearing:

```cpp
CModelData::CModelData()
: x0_scale(CVector3f(1.f, 1.f, 1.f)), xc_animData()
, x14_24_renderSorted(false), x14_25_sortThermal(false), x14_26_(true), x14_27_(false)
, x18_ambientColor(CColor::White())
, x1c_normalModel(), x2c_xrayModel(), x3c_infraModel() {}
```

is **byte-exact, 152 bytes**. The same stores in the constructor *body* are 97.11% and 148 bytes,
because MWCC runs every mem-init first and the body afterwards, while retail writes the four
`rlwimi`/`stb` pairs for the byte at 0x14 **between** `xc_animData` at 0xC and `x18_ambientColor at
0x18 - which is member-*declaration* order. There is no other way to get an interleaving like that
into a constructor: not the declaration order, not the list order, not the body.

**So: when retail's store order is not "mem-inits then body", put the bit-fields in the list.** And
when you do, expect a second difference, because a constructor also materialises `this` in r3
somewhere in the middle of the frame - on this function a dead `mr r3,r31` between the `CColor`
load and the store. A free `extern "C"` function does not get it, which is the whole 4-byte residue
of the 97.11% version, and which is why the two forms are 152 and 148 bytes.

### Closing a link-gap function only counts if its own dependencies are in hand too

Measured 2026-09-25 on the six `fn_`-named symbols a lane took off `docs/research/
port_link_gap.md`. Two of them closed and the gap went **41 -> 39**, and the difference between the
two is the whole lesson.

**A function whose body needs nothing but itself closes for free.** `fn_800C08D4` (0x800C08D4, 40
bytes, the morphball's `state == 4 || 5 || 6` predicate, 31 callers) became
`src/MetroidPrime/Player/CMorphBallC80.cpp`, is byte-exact, and is in `files.cmake`, so the port
links it.

**A function that forwards to another unwritten one is a trade, not a win.** `fn_8001D658`
(0x8001D658, 32 bytes) is a frame whose whole body is `bl fn_8001D678` - the Gekko software square
root, 228 bytes of `frsqrte` plus three Newton steps and the libm edge-case classifier, not
written. Defining the wrapper in `src/Kyoto/Math/CMathSqrtF.cpp` replaces one MISSING symbol with
`fn_8001D678` and the count does not move. **It closes only because the same file carries an
`#ifdef TARGET_PC` definition of `fn_8001D678` (`sqrtf`)** - the shape `Kyoto/Math/RMathUtils.cpp`
already uses for the identical call. So the honest rule is: a new unit counts for the port only if
everything it references is either already defined or given a host-side `#ifdef TARGET_PC` body,
and the two `CGunEffectTouch` units are the counter-example - byte-exact in the DOL, and deliberately
**not** in `files.cmake`, because they reference `fn_800E4E50`, `fn_800E4E9C`, `fn_80027AE8`,
`fn_80027B44` and `CModel::Touch`, and adding them would have grown the gap by four.

**A DOL symbol cannot be renamed to a C++ name if a Matching REL unit already defines that name.**
This is the blocker on `fn_800E6AD0` (0x800E6AD0, `CModelData`'s default constructor), which is
97.11% as an `extern "C"` function and **byte-exact as `CModelData::CModelData()`** - the last four
bytes are MWCC's constructor convention, a dead `mr r3,r31` (see the mem-init-list section above).
The rename is the obvious fix and it is wrong:
`MetroidPrime/ScriptObjects/CScriptScriptStreamedMovie.cpp` is a **`Matching` REL unit** claiming
`.text 0x1054..0x1074` of the `ScriptStreamedMovie` module, and its object *defines*
`__ct__10CModelDataFv` while *importing* `fn_800E6AD0`:

```
$ nm -n build/G2ME01/src/MetroidPrime/ScriptObjects/CScriptScriptStreamedMovie.o
         U fn_800E6AD0
00000000 T __ct__10CModelDataFv
```

Rename the DOL symbol and that wrapper resolves to itself. **Check `nm` on every unit that shares a
name before renaming anything in `symbols.txt`** - the name is the link, and a REL module's
definitions are the DOL's definitions as far as the module is concerned.

### Find a name-to-address table by decoding a static initialiser, not by guessing (2026-09-25)

The port's link gap held 234 symbols the docs called "REL module loaders", of which 159 were
referred to by *port* names (`LoadFlyerSwarm`, `LoadIngSnatchingSwarm`, ...) that retail's
`symbols.txt` does not use. The briefing called 133 of them "not named in `symbols.txt` at
all". **They are named - as `fn_80229F90` and friends.** dtk could not pair them because their
only caller is a static initialiser rather than code, so objdiff had nothing to pair on. The
mapping is one table lookup away and always was:

`__sinit_ScriptLoader_cpp` (0x80242894, 5696 bytes) is retail's own copy of the port's
184-entry `{FourCC, FScriptLoader}` table. A dozen lines of `lis`/`addi`/`stw` dataflow over
its disassembly recovers the table at 0x8045E138, and the port's tags then match retail's
**position for position, 184 for 184, zero mismatches**. That is what makes it evidence: a
positional match over 184 entries that had to be right 184 times cannot be coincidence. The
whole identification took minutes and turned 133 "unknown until looked at" into a table of
addresses, and the answer changed the plan - 73 of the 159 are the same 44-byte thunk, not the
20 the briefing expected, so the cheap work was 3.6x bigger than advertised.

Three details, each of which silently loses data:

- The base register is `r3`, set once by `lis r3,-32706` and never changed, and the table
  starts with a **`stwu`**, not a `stw`. Reading the `stwu`'s displacement as the first table
  offset puts every entry 7880 bytes early and the FourCCs stop matching.
- **Two entries are stored from a register that was spilled to the stack and reloaded**
  (`BLUR`, `DBAR`). A decoder tracking only `lis`/`addi` into registers drops them; handling
  `stw ...,(r1)` / `lwz ...,(r1)` against a simulated `r1` gets all 184. The tell is that the
  FourCC decodes correctly and the *pointer* field holds ASCII.
- `lis`/`addi` pairs are written destination-different from source (`lis r19,16707` then
  `addi r20,r19,21586`), so a decoder that only handles `addi rA,rA,K` misses most of them.

**The general lesson: an unnamed `fn_*` is a naming failure, not a missing name, and the
caller that objdiff cannot see is a static initialiser.** When a whole group of `fn_` symbols
turns out to be one shape reached from one table, the table is the deliverable and the shapes
follow from it.

### A DOL symbol a REL module imports cannot be renamed, and claiming its bytes deletes it

The corollary of the `CModelData`/`fn_800E6AD0` trap above, from the other direction, and it
cost two builds. A loader thunk in the DOL is paired with an 8-byte setter that writes its
`.sbss` slot, and the setter is **imported by name by the REL modules** - `Blogg` imports
`fn_80218B08`, `ChozoGhost` imports `fn_80218D24`, `DigitalGuardian` imports `fn_8021F9B0`, and
so on for 20-odd modules. So:

- The thunk and the slot **may** be renamed in `symbols.txt`: dtk regenerates the DOL's own
  objects from it, so intra-DOL references follow. Measured: none of the 64 slots is imported
  by any module.
- The setter **may not**, and its 8 bytes **may not be claimed**. A range that swallows it
  deletes the symbol, and `dtk rel make` fails with
  `Failed to find symbol fn_80227530 in any module` - which is the failure to expect, because
  the module objects are retail bytes and no `configure.py` edit can reach them.

And the shape of a thunk is not always `(*p)(...)`. Five modules register 2-4 loaders through
one slot, so the body is `p->slotN(mgr, input, info)`; `(*p)(...)` on a struct pointer is
`call of non-function`. `uint` is also not visible through `ScriptLoader.hpp` alone -
`ScriptLoaderRel.cpp` gets it from `TGameTypes.hpp` - and `unsigned int` produces the same
bytes.

### Two tools are weaker than they look, for REL units

Found while flipping `AIMannedTurret`, and both cost real time:

- **`unit_fit.sh` is a weak signal even for a DOL unit, and it cannot see a deficit.** Measured on
  `CPlayerState`, which cannot flip: the tool reports 6 extra emitted functions / 532 bytes, of
  which five are `WEAK` (CodeWarrior COMDAT copies, the harmless `CAi` class) and the sixth is a
  `LOCAL` that **mwldeppc drops anyway** - it is absent from the linked ELF. It also reports
  `.sbss SHORT by 3` and an unclaimed 2-byte `.sdata`, all of which mwldeppc absorbs because the
  section *totals* do not change. The only real signal was `.text over by 528` - and the actual
  blocker was a 4-byte **deficit** the tool has no column for. Treat its output as where to look,
  never as the verdict. **Its `.rodata SHORT by N` column is not a blocker for a REL module:
  mwldeppc pads `.rodata` at link time**, so a unit 5 bytes short of its claimed `.rodata` can
  still hash exactly - `ForgottenObject` is the measurement: with the flip, the linked module's
  `.rodata` is 0x94 = 148 either way, absorbing our 11-vs-16 *and* `REL/REL_Setup.cpp`'s own
  129-vs-132. The DOL's `Kyoto/CToken.cpp` says the same thing. **The exception is padding that belongs to the *next* unit's alignment**: `CTextRenderBuffer`'s `.rodata` is 0x37 bytes where retail claims 0x40, and the flip shifted everything after it by 8 until `CCubeMoviePlayer`'s `.rodata` split got `align:16` (its data starts with 16-byte masks; dtk's split object had guessed 8). When a flip shifts the section right after a unit that is short, check the next unit's alignment before you look for missing data. **The opposite case, `.rodata` over by 8, is usually an unclaimed placement-new string**: every unit calling `__nw__FUlPCcPCc` owns an 8-byte `"??(??)"` `@stringBase0`, and several sit unclaimed as `lbl_` strings (`0x803AEDC0`-`0x803AEE08` is a run of them). Read which one the retail object's `lis`/`addi` relocates against and add that 8-byte `.rodata` range to the unit's split, renaming the label `@stringBase0 scope:local` (`CAnimTreeNode`, `lbl_803AEDE8`). If the pool is longer than 8 bytes, check who else relocates against it: `lbl_803AA230` is `"??(??)\0Default\0"`, and retail `Enemies/CStateMachine.o` reads `"Default"` from it at +7, so `Factories/CStateMachineFactory` and `Enemies/CStateMachine` were one TU. The factory cannot flip alone; it needs the two splits merged, which means finishing `CStateMachine` first. **`multiply-defined: '<Class>::__vt'` on a flip means retail's vtable is inside an unclaimed `auto_*_data` blob**: find `__vt__<mangled>` in `symbols.txt` and give the unit that `.data` range (`CSoundPOINode`, `.data 0x803BBB58..0x803BBB68`). **A float pool that holds the same constants in a different order is not a source problem we have found a fix for**: `CLight`'s code is byte-identical, but retail orders its `.sdata2` pool 1/255, eps, 1.0, 0.0, -1.0, 3e36, ... and ours puts 0.0 after 3e36 and -1.0 last; moving the static definitions changed nothing. `CQuaternion` shows the same (retail 0.0, 1.0 first). Both stay `NonMatching`. **Vtables are emitted where the key function is defined, in reverse source order, and the key function is the first non-inline virtual in the class's *own* declaration order**: `CIntElement`'s `__vt__23CIEParticleCreationTime` sat last instead of first until `GetValue` was declared before the destructor, making the later-defined `GetValue` the key. **`CParticleGen` is not explained yet**: retail's vtable (`0x803B1D9C`) sits with game data between `CArchMsgParmControllerStatus` and `CGameArea`, next to weak copies of its inline virtuals (`0x800534B0`..), yet `AddModifier` (non-inline, in `CParticleGen.cpp`) should make it strong in that object. The destructor slot is 0 in retail, and `virtual ~CParticleGen() = 0;` plus an inline definition reproduces that, but the vtable stays strong in our object, so the flip still fails with multiply-defined `__vt`. **The recipe that actually answers the question, in one build:**

  ```sh
  # flip the unit by hand in configure.py, build, keep main.elf, and read the section sizes
  readelf -SW build/G2ME01/main.elf | grep -E '\.text|\.rodata|\.data|\.bss|\.sdata'
  ```

  Every section matches except the ones the unit owns, so "the flip failed" becomes "the flip is
  N bytes short in `.text`, and here is the function" - which is what `flip_test.sh`'s "DOL
  differs" cannot tell you. On `CPlayerState` that put the blocker on one unconditional `b` in
  `InitializeScanTimes` (0xe0 against retail's 0xe4) and cleared the other 68 functions, including
  all six the tool had flagged. Note also that both objects `unit_fit.sh` compares are dtk's view,
  where every symbol is `GLOBAL`, so their bindings say nothing about what the retail linker did.
- **`tools/unit_fit.sh` is vacuous for a REL unit.** It compares our object against
  `build/G2ME01/<Module>/obj/<unit>.o` as "retail", but that file is a dtk-processed **copy of
  our own compiled object** - dtk produces no retail object for a claimed range. So the `retail`
  column is our own size, the "extra functions" check compares our object with a copy of itself,
  and both `fits` and `no extra functions` are not evidence of anything. It is sound for DOL
  units, where `build/G2ME01/obj/<unit>.o` really is the retail-derived object. It is also not
  refreshed when the source object changes, so it can be stale as well as circular.
- **`tools/compare_unit.sh` does not work for REL modules.** It only looks under
  `build/G2ME01/obj/` and `build/G2ME01/src/` and exits 2 with "build first" for every REL unit,
  although the module recipe sends lanes to it. For a REL unit, diff the link's own inputs:
  `build/G2ME01/src/<unit>.o` against the module's `.rel`.

The lesson is the one this file keeps making: **a check that cannot fail is not a check.** Both
tools still work where they are pointed at the right thing; the trap is that they report
success where they measure nothing.

#### But a literal that only *joins* `.rodata` is much cheaper than that (measured 2026-09-26)

The section above reads as "any string literal in a `NonMatching` unit is dangerous", and that is
wider than the measurement supports. Writing `CGameGlobalObjects::AddPaksAndFactories` and
`CMain::InitializeSubsystems` in `main.cpp` added **13 string literals** to the unit (eleven pak
names and two `printf` formats), `.rodata` grew 0x6C -> 0x11A, and of the 81 functions in `main.o`
**75 instruction streams came out byte-identical** to the build of `4d49561`. Three were the
functions being written. The remaining three - `InfiniteLoopAlarm`, `LoadStringTable`,
`PostInitialize` - each differ in **exactly one instruction**, and it is always the same one: the
`addi` that is the low half of an `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` pair against
`@stringBase0` (34 -> 152, 48 -> 166, 58 -> 176, all three by the same 118 bytes, which is how far
the new literals pushed the existing ones along inside the pool). **That is a relocation addend the
linker overwrites**, so the linked address does not move: the gate's per-function diff listed all
three as unchanged and `main.dol`'s sha1 did not change.

So there are two different failures and they should not be conflated:

- **A literal that merely joins `.rodata`** costs an `addi` addend in functions that address the
  string pool. objdiff pairs by name and the percentage does not move. Verified free.
- **A literal that changes what an unrelated function *computes*** - which is what the recorded
  case was: `__ct__CGameArchitectureSupport` grew 32 bytes and acquired a `__cvt_dbl_usll` call -
  really does cost 100% functions. That is mwcceppc re-optimising, and nothing about being a
  string predicts it.

**The check is the same either way and it takes two seconds**, so do it rather than guessing:

```sh
$MP_TOOLCHAIN_DIR/build/review-tools/bin/ninja -f build.ninja build/G2ME01/src/<unit>.o
build/binutils/powerpc-eabi-objdump -h build/G2ME01/src/<unit>.o | grep ' .text'   # must be unchanged
```

## Run the real linker before you trust any link-gap arithmetic (2026-09-25)

`tools/link_gap.py` derives the port's link gap from `nm` set arithmetic. It is
convenient and it is close — 559 against the linker's 525 — but a single real
`ld.bfd` run over the port executable is better evidence, and the first one ever
attempted found two bugs that no amount of `nm` could have:

- `src/REL/REL_Setup.cpp` walked `_ctors`/`_dtors`, which the **GameCube's linker
  synthesises and an ELF link does not**. Two unresolvable references, in a unit
  that is correct for retail and was always going to be. Fixed with an
  `#ifdef`-guarded host branch over `__init_array_start`; the `__MWERKS__` branch
  is untouched, so the unit still matches byte for byte. **A host-build bug in a
  `Matching` unit is invisible to every gate in this file** — the gates all
  measure retail, and only the port build exercises the other branch.
- `platform/ai_dma.cpp` was fully written, sitting in the tree, compiled by
  nothing, so all five AI DMA entry points were missing from the port. Aurora
  *declares* four of them in `dolphin/ai.h` and implements **none** — check the
  header, do not assume the SDK provides them.

The commands are in `PORT_NOTES.md` under "Two builds exist". Two `cmake`s and a
`ninja`, no more, and it is the only thing in this repository that measures the
port rather than the decompilation.

**`nm` has one structural blind spot worth knowing.** A vtable is only *emitted*
by the translation unit that defines the class's key function. While that key
function is unwritten, no object in the tree contains the vtable at all, so
`nm` has nothing to count and `link_gap.py` cannot list it — yet the linker asks
for `vtable for CPlayer`, `vtable for CCollidableAABox`, `vtable for CSimplePool`
and `typeinfo for CGunWeapon`. Closing one of those means **writing the key
function**. Never hand-emit a vtable to satisfy the linker: it is a symptom, and
the cure produces a binary whose vtable layout nothing else agrees with.

**Fourteen translation units define a `RELMain`**, one per REL module we have
reimplemented, and they collide in a flat link. That collision is not a bug in the
sources: on the cube each is a separate module whose prolog and epilog mwldeppc
builds from `RELMain`/`RELExit`, so the duplication *is* the module system working.
`ld.bfd` names only three of them because it stops at the first collision — **when
the linker under-reports a structural problem, count it yourself before sizing the
fix.** Resolved by giving each module's entry points a distinct name **on the host
only** (`#ifdef __MWERKS__` keeps the retail names, so the units stay `Matching`),
with `platform/compiled_modules.cpp` as the registry that owns them.

**And the part that matters more than the rename:** `platform/rel.cpp` calls a
loaded module's prolog by *guest address inside the image*, never by symbol name,
so **nothing on the host called `RELMain` at all.** Renaming alone would have given
a green link with every module's function-pointer table still null and every
loader behind one unreachable. **A symbol that resolves but is never called is a
worse bug than an unresolved one, because nothing reports it.** The moment a rename
makes a linker error go away, ask who was supposed to be calling it.

`--allow-multiple-definition` was the other option and is worse than either: it
greens the link while running one module's entry point and skipping thirteen. **A
green link that lies is worse than a red one**, which is why this project's one rule
counts `Matching` units rather than a successful link.

## A `Matching` unit may claim one function of a three-function triple (2026-09-26)

The Tweaks module lays its per-struct code out as three contiguous entries -
`LoadTypedef<T>`, `~T`, `T` - and the generated sources put all three in one `.cpp`,
so one split block covers the triple. **That is the wrong granularity whenever the
constructor is not at 100% and the loader is**: `__ct__18SLdrTweakCameraBobFv` is at
57.67% and `LoadTypedefSLdrTweakCameraBob` is at 100%, so the triple can never be
`Matching` and the good function is stuck behind the bad one for nothing.

The fix is four edits and nothing else, and it is the `ScriptRiftPortal` three-way
split applied per function:

1. move the one function into a new file,
   `src/MetroidPrime/ScriptLoader/Structs/<T>_Load.cpp`, whose comment records the
   exact range it claims and why the other two stay behind;
2. **shrink** the old unit's block in `config/G2ME01/rels/Tweaks/splits.txt` to the
   destructor and constructor only, and add a new block for the new unit - both in
   address order, and the two blocks must not overlap or `dtk rel make` fails;
3. `Object(Matching, ".../Structs/<T>_Load.cpp")` in `configure.py`, immediately
   before the `NonMatching` entry it was split out of;
4. the new path in `files.cmake`, or the port does not compile it and
   `tools/probe_sources.sh` does not see it.

`SLdrTweakCameraBob.cpp` keeps its own name and stays `NonMatching`, so the port still
gets the constructor and destructor from the same place it always did, and
`tools/link_gap.py` does not move at all - the three symbols were already defined by
the port build, so this buys linked functions and no gap. Landed: `CameraBob_Load`
(688 bytes), `SlideShow_Load` (892), `Targeting_Load` (4,532), all three flip-tested
PASS and the module sha1 still matches `config.yml`.

**Two of the seven 100% `LoadTypedef` bodies could not be split this way, and the
reason is worth recording.** `PlayerGun` and `Game` compile with an 8-byte `.data`
section holding two anonymous 4-byte items, and `mwldeppc` refuses the module with
`Can not mix BSS section '.bss' with non-BSS section '.data' in linker command file`.
The same five units also emit a **weak `~rstl::basic_string`** (0x50 bytes) that
retail does not have in the claimed range, because the body constructs a temporary
`rstl::string` from the stream; objdiff pairs by name and ignores it, and the module
hash still holds, so that part is harmless. The `.data` is not. Both units were
reverted; they are the next thing to try, and the fix is to find where the two data
items come from rather than to suppress them.

**What did not work on the remaining eight `LoadTypedef` bodies at 99.1-99.6%.** They
differ from retail in the loop header by two register allocations and nothing else -
retail reuses the stream pointer's register for the property tag, this build keeps
both and takes a third:

```
retail: lwz r4,8(r30); addi r0,r4,4; stw r0,8(r30); lwz r3,8(r30); lwz r4,0(r4)
ours:   lwz r3,8(r30); addi r0,r3,4; stw r0,8(r30); lwz r4,8(r30); lwz r6,0(r3)
```

and the same eight differ in 14-20 instructions, all of them register numbers. Tried
with `tools/try_batch.py`, **all with no effect on the count**: `const` on both
locals, `int` vs `uint` for the tag, `unsigned int`/`long` for it, `int` vs `u16` for
the size, `unsigned short` for the loop bound, an extra `const int n = propertyCount`
local, and a `while` form. Seven other `LoadTypedef` bodies in the same module come
out at **100%** from the identical template, so the source is right and this is
MWCC's allocator, not a modelling gap. It is the eighth entry on the known-hard list.

| `Kyoto/CResLoaderGetPakCount.cpp` (DOL unit) | **landed, 2026-09-26 (lane `g1`)** - `CResLoader::GetPakCount` (0x802FBC60, `size:0x10`) at **100.00%**, unit `Matching`, `flip_test.sh` PASS, and it **closed a link-gap symbol** (`_ZNK10CResLoader11GetPakCountEv`). Its own unit rather than a third function in `CResLoaderPakPump.cpp` because the two are 0x1B4 apart and a unit may not claim two discontiguous ranges. Four instructions: the counts of the `+0x18` and `+0x30` lists added together, and **not** the `+0x48` loading list - a pak being loaded is not a pak you can read. |
| `Kyoto/CResLoaderGetPakFile.cpp` (DOL unit) | **attempted, not landed, 2026-09-26 (lane `g1`)** - `CResLoader::GetPakFile` (0x802FBA68, `size:0xFC`) at **80.13%**, unit left `NonMatching` with the range claimed so retail's bytes stay in the link. It is **one shape away, not twenty**: MWCC unrolls the node walk by eight and **peels the first eight iterations**, so retail's chunk count is `((idx-8)+7)>>3` behind a `cmpwi r4,8`, and this build emits `(idx - count18)>>3` with no peel, giving an object 0xE0 = 224 bytes against 0xFC. Getting the peel is a control-flow experiment, not a naming one. It still **closed a link-gap symbol** (`_ZNK10CResLoader10GetPakFileEi`), because a body the port compiles is not a missing symbol whether or not it is retail's - the same distinction `port_link_gap.md`'s "the port already defines `LoadForgottenObject`" section is about. |
| `MetroidPrime/CModelDataCopyCtor.cpp` (DOL unit) | **landed, 2026-09-26 (lane `h4`)** - `CModelData::CModelData(const CModelData&)` (`__ct__10CModelDataFRC10CModelData`, 0x80018FBC, `size:0x13C` = 316 bytes) at **100.00%**, unit `Matching`, `flip_test.sh` PASS, and it **closed a link-gap symbol** (`_ZN10CModelDataC1ERKS_`) with **no new callee**: the two calls in the body, `__ct__6CTokenFRC6CToken` and `Lock__6CTokenFv`, are both already in `src/Kyoto/CToken.cpp`. It needed two header corrections, and both are generalisable - see the next two sections. The object also emits three weak destructor instantiations (288 bytes: `__dt__15TToken<6CModel>Fv` 84, `__dt__Q24rstl20auto_ptr<9CAnimData>Fv` 96, `__dt__Q24rstl40optional_object<21TLockedToken<6CModel>>Fv` 108) that the retail unit object does not have. `unit_fit.sh` lists them and **the flip holds anyway** - the same COMDAT case as `CAi`'s 224 bytes. |
| `MetroidPrime/CStateManagerScriptMsgArray.cpp` (DOL unit) | **partly landed, 2026-09-26 (lane `h4`)** - `CStateManager::ScriptMsgArray::fn_8019E6BC` (0x8019E6BC, `size:0x58` = 88 bytes) at **100.00%**, unit `Matching`, `flip_test.sh` PASS, and it **closed three link-gap symbols** net. `ScriptMsgArray` is a **192-entry ring buffer, not a vector**, and saying so is what makes the pop exact: unsigned cursors (retail wraps them with `lis r5,-21845` / `mulhwu` / `srwi 7` / `mulli 192`, which a signed `%` will not produce), a `mutable` read cursor (retail's pop is a **const** member function that advances it) and a **by-value** return (retail builds the message in the caller's return slot in `r3`). The class's other two methods, `Append` (0x8019E714, 0x58) and `fn_8019E69C` (0x8019E69C, 0x20), are written and correct but **not byte-exact** - register allocation and one algebraic reassociation - so they live in `CStateManagerScriptMsgArrayCursor.cpp`, a **port-only** TU. That is not a stylistic choice: a `Matching` unit that also defined them is **multiply-defined** against the `auto_03_8019B988_text` / `auto_03_8019E714_text` objects dtk fills the ranges either side of the claim with. Measured: 0x58 vs 0x58 and 0x20 vs 0x20, 22 and 8 instructions each, every value right. |

## Two header facts that decide whether a copy constructor can be exact (2026-09-26, lane `h4`)

Both of these were worth more than the 316 bytes they unlocked, because each one is a
*discriminator* - a single instruction in retail that tells you which of two spellings the class
really has, and both spellings compile.

**1. `Lock()` in a copy is `TLockedToken`, not `TCachedToken`.** `CModelData`'s three model slots
were `rstl::optional_object< TCachedToken< CModel > >`. Retail's copy constructor copies each of
them as `__ct__6CTokenFRC6CToken` + `dst.x8 = src.x8` + **`Lock__6CTokenFv`**, and the `Lock()` is
`TLockedToken`'s copy constructor (`include/Kyoto/TToken.hpp:67`, `x0_token(token); x8_item(*token);
x0_token.Lock();`) - `TCachedToken` has no `Lock()` in its implicit copy at all. Changing the
member type compiled to **exactly 0x13C bytes with zero differing instructions**, and both types
are 0x10 bytes with the flag at +0xC, so the layout and `CHECK_SIZEOF(CModelData, 0x4c)` are
untouched. With `TCachedToken` the function is 12 bytes short - once per member - and the
per-function diff has nothing useful to say about it.

**2. A whole-byte bit-field copy needs a *named struct*, not loose bit-fields.** Retail's
`CModelData` default constructor (0x800E6AD0) writes the four flag bits at +0x14 as four separate
`lbz`/`rlwimi`/`stb` triples, and its copy constructor (0x80019010) copies the same byte as **one
`lbz`/`stb` pair**. Those two shapes cannot both come from four loose one-bit bit-fields in a
mem-init list: MWCC 2.7 gives four read-modify-write chains, **28 instructions against retail's
2**, and the object came out 0x294 = 660 bytes against 0x13C. Measured, four spellings
(`tools`-shaped probe, mwcceppc's own flags):

| spelling | emitted for a whole-struct copy |
| four loose `bool : 1` in the mem-init list | 4x `lbz`/`rlwimi`/`stb` - 28 instructions |
| two `uchar : 4` fields in the mem-init list | 2x `lbz`/`rlwimi`/`stb` |
| a **named struct** of four `bool : 1`, copied as a unit | **`lbz`/`stb` - retail's exact pair** |
| a union of an anonymous struct and a `uchar` | `lbz`/`stb` too, but MWCC 2.7 rejects `v.f.a` in the same TU |

The named struct also keeps the default constructor exact, because its inlined default constructor
assigning each bit is precisely the four-`rlwimi` shape retail has. **Loose bit-fields match one
constructor and cannot match the other; the struct matches both.** The bit-fields must be in the
mem-init list either way - retail writes 0x14 *between* `xc_animData` at 0x0C and
`x18_ambientColor` at 0x18, which is declaration order, and a constructor body runs after every
mem-init. C++ forbids bit-fields there; MWCC 2.7 accepts it.

## A `Matching` DOL unit may define functions outside its claimed range - but they must come *after* it (2026-09-26, lane `h4`)

`CStateManagerScriptMsgArray.cpp` claims 0x8019E6BC..0x8019E714 and also defines `Append` and
`fn_8019E69C`, which live in the ranges **either side** of that. That fails the link:

```
multiply-defined: 'CStateManager::ScriptMsgArray::fn_8019E69C()' in CStateManagerScriptMsgArray.o
Previously defined in auto_03_8019B988_text.o
```

`dtk` fills every range **no unit claims** with retail's own bytes, so a function outside the
claim is already defined by an `auto_*` object. Two consequences, and they point in opposite
directions:

* Functions retail's symbol table has **no name for** are safe to define anywhere - the
  `CModelDataCopyCtor` object emits three weak destructor instantiations the retail unit object
  does not have and the flip holds, because nothing else defines them.
* Functions retail **does** name are not: keep them in a TU `configure.py` does not claim (the
  `PortGlobals.cpp` / `PortBoot.cpp` pattern), or in a unit that claims their range.

And within one object, mwcceppc emits in reverse source order, so the claimed function has to be
declared **last** if the file also holds earlier-addressed ones - while
`tools/check_decl_order.py` still has to see ascending retail order, which it does, because the
two unclaimed ones bracket the claimed one.

## Adding two lines to a header can reschedule an unrelated function in a `NonMatching` unit (2026-09-26, lane `h4`)

`include/MetroidPrime/CStateManager.hpp` changed by 19 lines, all of them inside the private
nested `struct ScriptMsgArray` - two cursor types and one return type, with **no layout change
whatsoever**. That moved `CStateManager::AddDrawableActor` from **58.57% to 52.10%**: the same 21
instructions in the same 0x54 bytes, with four of them rescheduled. Bisected: the
`CModelData.hpp` change in the same commit is innocent, the `CStateManager.hpp` change is the
cause, and the baseline and perturbed objects are both reproducible from the same flags.

This is the same failure mode as the string-literal case already in this file, with a different
trigger, and it is a warning rather than a gate failure: `tools/gate.sh` prints it and still says
`GATE PASS`. **The lesson is that a header edit is a code edit.** A change to a private nested
type in a header included by a 239-function `NonMatching` unit is not free, and the cheap check is
the one in `docs/LANE_BRIEFING.md`: after any header change,
`ninja -f build.ninja build/G2ME01/src/<unit>.o` and compare the functions you did not mean to
touch.

## A vtable is a `Matching` unit's `.data` claim, and its layout is not the Itanium one (2026-09-26, lane `j3`)

A class's vtable is only **emitted** by the translation unit that defines its **key function** -
the first non-pure, non-inline virtual. With none defined, the constructor's vptr store references
a symbol nothing provides, and `ld.bfd` reports one undefined `vtable for X` for the whole class.
Defining the destructor alone is therefore a **net loss**: the vtable appears and its slots then
relocate against members that are *also* undefined, so one missing vtable becomes three missing
methods. **The accessors and the destructor have to land in the same commit.**

MWCC's layout, read out of `.data` in `build/G2ME01/main.elf`, and it is the thing that makes a
vtable readable at all:

```
0, 0, <slot 0>, <slot 1>, ...        # offset-to-top, then typeinfo - zero because -RTTI off
```

* **one** slot for the destructor, not two. `CEntity` declares a virtual destructor and five
  methods and has 6 slots. The emitted function takes the deleting flag in `r4` and calls
  `CMemory::Free` itself.
* **a pure virtual gets a NULL slot.** That is how `CIOWin::OnMessage` shows up in
  `vtable for CIOWin` as a `0`, and why a vtable can be *correct* while a method is still missing.
* slots are in **declaration order**, so a header that reorders or adds a virtual changes the
  vtable. `vtable for CMainFlow`'s last slot is `CIOWin::PreDraw` because `CMainFlow` does not
  override it - which is also why adding a `PreDraw` override to that header would break the DOL.
* `symbols.txt`'s object size is rounded up to a multiple of 4, so 7 words reads as `0x20`. That
  padding word is why `unit_fit.sh` says a vtable claim is "SHORT by 4" and why that is not a
  failure - `MetroidPrime/CIOWinDtor.cpp` is the worked example, `PASS` and the DOL sha1 unchanged.

**The claim is what stops the duplicate.** The unit that emits the vtable must claim its `.data`
range in `splits.txt`; otherwise dtk's fill also supplies those bytes and two objects own
`__vt__6CIOWin`. You do not write the vtable in the source - mwcceppc derives it from the class and
it comes out right, which is the check that the header's declaration order is retail's.

**Naming a symbol you did not write is legitimate and is not a stub.** `CMainFlow`'s vtable has an
`OnMessage` slot, and `OnMessage` is not written. `config/G2ME01/symbols.txt` renames retail's
unnamed `fn_8001DF54` to `OnMessage__9CMainFlowFRC20CArchitectureMessageR18CArchitectureQueue`, so
dtk's fill object carries the name the vtable's relocation needs and **retail's own bytes back it**.
The port's link then asks for `CMainFlow::OnMessage` by name instead of for a vtable that named
nothing: MISSING goes 288 -> 289 while the c++ runtime bucket goes 18 -> 16. That is the same
pattern `CMainFlowCtor.cpp` used for `fn_80049E98` -> `__ct__6CIOWin...`, and it is the opposite of
the trap vtable `docs/research/port_link_stubs.md` refuses.

**One `dtk` rule that costs a build cycle:** a claim may not *end inside* a symbol, so claiming
`0x80049E10..0x80049E1C` for three functions fails with `Split ... ends within symbol
'GetIsContinueDraw__6CIOWinCFv' (0x80049E18..0x80049E20)`. The end has to be the symbol's end.

**A local object's two vptr stores are a blocker, not a detail.** `CMainFlow::OnMessage`'s shape
is reproduced byte for byte by a probe - the whole `switch` dispatch, both call sites, the
destructor call and the epilogue - but it stores **two** vtable addresses for an 8-byte stack
object, so a `Matching` unit has to place a base class's vtable and a derived one's at two fixed
addresses, and the class must be *complete* in the header (so its destructor is called out of line)
while its key function lives in another unit. `docs/research/boot_probe.md` has the addresses and
the three consequences.

## A switch jumptable forces the unit to own the vtable next to it (2026-09-26, lane `k2`)

`CMainFlow::AdvanceGameState` (0x8001DE68, 224 bytes) switches on `x14_gameState` and its jumptable
is `.data 0x803B178C`, 17 words, immediately after `vtable for CMainFlow` at `0x803B1770`. Writing
it took three rules, and **the first two are properties of the toolchain, not of the function**, so
they will decide the next switch-jumptable function the same way:

* **A `Matching` unit cannot own a `.data` object at a 4-byte-aligned address.** mwcceppc 2.7 puts
  `.data` in an 8-byte-aligned section whatever is in it - `-align powerpc`, `-align 4` and
  `-align off` all give `2**3`, measured - and mwldeppc then inserts four bytes of padding, which
  moves every address above it and fails the DOL sha1 with all 86 RELs. dtk warns first
  (`Alignment for ... .data expected 8, but starts at 0x803B178C`), and **`align:4` on the split
  line silences the warning without changing the padding** - a build cycle spent on that.
* **A switch's jumptable is emitted as a local `@N` symbol, not as the name `symbols.txt` gives
  it.** So even with the bytes right, `dtk dol diff` reports
  `Expected to find symbol jumptable_803B178C (type Object, size 0x44) at 0x803B178C`. That is not
  in the gate, but it is the honest answer and it is not worth arguing with.
* Together they mean the only 8-aligned `.data` range that can hold this table is the one starting
  with the vtable, and the vtable is emitted by the unit that defines the class's **key function**,
  `~CMainFlow`. So `AdvanceGameState` *and* `SetGameState` (0x8001DB54, 788 bytes, which sits
  between the other two) had to join `CMainFlowDtor`'s unit - one unit cannot claim two
  discontiguous ranges, and splitting the destructor out would leave the vtable unemitted. The
  landed shape is `src/MetroidPrime/CMainFlowDtor.cpp`: `.text 0x8001DAF4..0x8001DF48` (1,108
  bytes, three functions) and `.data 0x803B1770..0x803B17D0` (96 bytes), 3/3 at 100.00%,
  `flip_test` PASS. It also happens to be the arrangement that works: mwcceppc emits the vtable at
  `.data+0` and the jumptable at `.data+0x1C`, which is retail's layout.

**Read the jumptable out of the DOL; the arms will lie to you.** The seventeen words are at
`0x803B178C` and `objdump -s -j .data` prints them, and **only five of the seventeen entries are
cases** - the other twelve are the function's single `default`. That is what pins the source: a
`switch` whose labels are exactly `{kCFS_Unspecified, kCFS_PreFrontEnd, kCFS_FrontEnd, kCFS_Game,
kCFS_GameExit}` makes a compiler build a table spanning min..max of the *labels*, -1..15, with the
gaps defaulting. Reading the arms instead gives the opposite conclusion, because four of the five
bodies are the same two instructions with a different constant. The dispatch itself,
`addi r0,r4,1 ; cmplwi r0,16 ; bgt`, is the `+1` bias a negative lowest label forces.

**Four spellings that are the bytes**, all measured with `tools/probe_cc.sh` and all in the source's
header comment:

* **A switch's bodies come out in source order.** Retail's arms run `kCFS_Game`,
  `kCFS_PreFrontEnd`, `kCFS_FrontEnd`, `kCFS_GameExit`, `kCFS_Unspecified` - not the enum's order -
  and reordering them permutes the bytes while every per-function percentage stays at 100%.
* **A fallthrough is how retail shares a body.** `kCFS_GameExit` falls through into
  `kCFS_Unspecified` and the two share one `SetGameState(kCFS_PreFrontEnd, queue)`; with an explicit
  `break` there is no merge and the function is 4 bytes longer.
* **A `bool` local is what makes mwcceppc materialise a predicate.** `if (a >= x && a <= y)` emits
  the two compares and a direct branch; the same test in a `bool` emits
  `li r0,0 / ... / li r0,1 / clrlwi. r0,r0,24 / beq`, which is retail's shape.
* **`queue.Push(f(x))` is 40 bytes smaller than `CArchitectureMessage m = f(x); queue.Push(m);`**,
  because mwcceppc 2.7 does not elide the copy out of a return value - the same mechanism
  `CInputGeneratorUpdate.cpp` documents, seen from the other side.

**Two constants that are not the ones a reader expects, and a general rule for finding them.**
Retail's `addis r0,r4,-21326 ; cmplwi r0,18252` looks like `== 0x949A` and is not: mwcceppc
canonicalises a 32-bit equality compare whose constant does not fit a `cmplwi` as
`addis rD,rS,-(K>>16)` + `cmplwi rD,K&0xFFFF`, so `K = 0x949A` gives `addis rD,rS,0` and retail's
pair pins **`K = 0x534E474C`**. The same shape in `SetGameState` pins `K = 0x46524E44`, compared
against `CGameMode`'s sixteenth virtual (`lwz r12,68(r12)`, which is `v15()` - the only one of the
twenty-three that returns `int`). Neither constant's *meaning* is recovered. When a `cmplwi`
immediate looks like a mangled number, decompose it with that canonical form before guessing.

**Two dead stores, reproduced rather than fixed.** `gpMain->SetX90_30(true)` emits
`lbz r0,144(r3) ; li r4,1 ; rlwimi r0,r4,1,30,30 ; stb r0,144(r3)`: the mask is word bit 30, which
is byte **0x93**, and the store is to byte **0x90**, so three of the four instructions cannot change
the byte. mwcceppc does this for all nine of `CMain`'s bitfields (measured one at a time), and
`= true` rather than `= false` is what reproduces it - `= false` lets MWCC fold the constant and emit
`li r4,0`. Retail has the same no-op, so "fixing" it would break the hash.

**A parameter that retail never writes, and the two ways to spell a call with too few arguments.**
`SetGameState` calls `StreamNewGameState` with `li r4,0` - a **null** `CInputStream&` - and never
writes r5 at all: the second argument is whatever the virtual call above it left. No C++ source
expresses "pass an uninitialised int", and `int saveIdx;` uninitialised makes mwcceppc allocate a
callee-saved register for it, which costs `stw r30,72(r1)`, `mr r5,r30` and `lwz r30,72(r1)` and
moves every branch displacement in the function. The fix is an
`extern "C"` declaration that **spells retail's mangled name out and drops the parameter** -
`extern "C"` suppresses mangling, so the parameter list does not change the symbol the call refers
to, and r5 is left alone.

**What a `Matching` unit may carry besides its range.** This unit's object also emits
`ReleaseData__Q24rstl34rc_ptr<24IArchitectureMessageParm>Fv`, `__dt__20CArchitectureMessageFv`,
`__dt__24IArchitectureMessageParmFv` and 12 bytes of `__vt__24IArchitectureMessageParm` - 264 bytes
of COMDAT weak template and inline-virtual definitions that the retail unit object does not have,
all because the unit now instantiates `rstl::rc_ptr<IArchitectureMessageParm>` for the first time
in the DOL. `unit_fit.sh` lists them, calls them the harmless cause they are (`CAi` carries 224
bytes of the same and still flips), and says only `flip_test` decides. It does: `PASS`.

**The port's link gap went *up* in the session that made two functions exact**, and that is the
shape to expect: twelve new retail callees (`fn_80020478`, `fn_800214A0`, `fn_80022C74`,
`fn_80048EA4`, `fn_801423A8`, `fn_80143884`, `fn_80143E88`, `fn_80180598`, `fn_80192808`,
`fn_80193E08`, `fn_801F47F4` and `StreamNewGameState__5CMainFR12CInputStreami`) moved from
*not referenced* to *referenced and missing*. All twelve are named now, which is the improvement -
`docs/research/unidentified.md` exists to be emptied. The six `.sdata` words the same unit reads
did **not** reach the list: they are defined with retail's values in `src/MetroidPrime/PortGlobals.cpp`,
the same treatment `lbl_803A60A0` already gets, and a zero fill would be a wrong answer rather than
a missing one. MISSING 289 -> 301; `libc/libm` 29 -> 30, which is `__dt__24IArchitectureMessageParmFv`
landing in the libc bucket because the classifier sends every `__`-prefixed symbol there.

## Recovering functions the upstream merge dropped, and where the wall is (2026-09-28)

Nine functions came back in one wave on the merge worktree, from four causes, all of them
"upstream's version of a TU replaced ours" rather than anything structural:

1. **A body the merge deleted outright.** `CScriptPickup::fn_800B4518` and its declaration were
   gone; the retail body is three instructions of bitfield set and the retail symbol is itself
   `fn_800B4518`, so the name is not the problem - the *declaration* was. Same for
   `__sinit_CScriptPickup_cpp`: upstream had a `static float skDrawInDistance = 30.f;` (referenced
   only from a comment) where retail has `static TUniqueId sUnkPickupId = kInvalidUniqueId;`. A
   static initialiser the compiler cannot fold is worth a whole static-init function, and the
   symbol it writes is the one the retail map names.
2. **A renamed function, and a typedef cannot fix it.** Retail's setter is
   `ScriptGUI_SetPtrs__FP10GUILoaders`; upstream calls it `SetSGuiWidget_FuncPtrs` and types it on
   `SGuiWidget_FuncPtrs`. **The parameter type has to be a class actually named `GUILoaders`** -
   the Itanium/MWCC mangler mangles the underlying class, so `typedef SGuiWidget_FuncPtrs
   GUILoaders;` produces the same symbol as the typedef's target and does not help.
3. **A by-reference parameter that retail passes in memory.** `SnakeWeedAlt_8021BA94` copies the
   three words of a `CVector3f` into a caller-side temporary and passes its address, which is what
   mwcceppc does for a 12-byte aggregate taken **by value**. Changing the pmf's first parameter
   from `const CVector3f&` to `CVector3f` took the function from 56.9% to 100%.
4. **A generated struct with the wrong member count.** See `docs/HANDOFF.md` step 17(b'): five
   map-icon ids the generator put in `SLdrTweakPlayerRes_AutoMapperIcons` made it 0x50 too wide
   and cost five functions across two units at once. **When retail's offsets are all off by one
   constant, count the members against retail's constructor, not against the loader** - the
   constructor's store count is the ground truth and the generator's id list is not.

Two things did *not* work, and both are walls rather than puzzles:

- **A retail symbol map can be internally inconsistent, and that is a zero-sum rename.**
  `TypesMatch` has `TCastToPtr<22CScriptPointOfInterest>__FP7CEntity` next to
  `TypesMatch__10CUnknown90CFi` for what is plainly one class. Renaming `CUnknown90` to
  `CScriptPointOfInterest` wins the `__FP7CEntity` overload and loses the `__FR7CEntity` overload
  and `TypesMatch`: net zero, twice.
- **Past ~99% the residue is register allocation, not meaning.** `CEntity::AcceptScriptMsg` at
  99.78% differs only in which halfword is loaded into `r7` first and in a 4-byte stack-slot
  offset; both sides store `m_originator` from `src+2` and `m_id` from `src+4`. `CActor::
  OnScanStateChange` at 99.79% differs only in whether one `TUniqueId` temporary gets one stack
  slot or two. `CGameOptions::InitSoundMode` at 87.7% differs only in whether `li r0,1` sits
  before or after the `cmpwi`; a named local of the enum type does not move it. Reading these as
  "a wrong expression" wastes a session - check whether the *stores* agree before rewriting.

**A build fact worth knowing before you spend a session on a DOL hash.** `build/G2ME01/obj/`
holds `dtk dol split`'s output - the *retail* objects, one per configured unit - and `main.elf`
links those, not `build/G2ME01/src/*.o`, for every unit the ninja generator did not mark
otherwise (876 of 1416 inputs at the time of writing). objdiff compares our compiled
`src/` object against the split one, so for those units **the `main.dol` sha1 cannot move no
matter what the source says**: a green sha is not evidence that an edit was harmless, and an edit
is not a gate failure either. Check `grep -c '^build build/G2ME01/obj/' build.ninja` and read
the unit's entry in the `main.elf` input list before deciding whether a change is DOL-visible.

### Regaining whole units after the merge: four causes, none in the unit's own code

These took `CAi` and `ScriptLoaderRel` back to `Matching` (DOL sha1 held, `flip_test` PASS), after
their functions had matched again for a while and their flips still failed.

1. **Weak inline copies grow `.text`.** Upstream's headers define `CHealthInfo`'s and
   `CDamageVulnerability`'s copy constructors, `SMoverData`'s constructor and four `CAi` virtuals
   inline. Retail has them out of line, at their own addresses, so with the headers as they were our
   object emitted weak copies: `.text` was 0x9f8 against retail's 0x708.
   - The fix is `#define MP_RETAIL_OUT_OF_LINE_COPIES` at the top of `CAi.cpp`. The headers declare
     those members out of line under that macro, when not building for `TARGET_PC`.
2. **Pooled constants that nothing references still take space.** `CCharAnimTime`'s inline
   factories take `const&` arguments, so each call pools a float in `.sdata`, and mwldeppc keeps the
   pooled words even when nothing references them (0x34 against 0x10).
   - `CCHARANIMTIME_LOCAL_CONSTANTS` switches them to locals.
   - **It has to be opt-in.** Making it global broke four functions in `CCharAnimTime.cpp`, which
     needs the direct form, and broke the DOL.
3. **The merge can drop a split section silently.** `CAi`'s `.sdata2` range and `kCAiSplashDenom`'s
   size were missing from `splits.txt`/`symbols.txt`, so `.sdata2` came out 0x20 too large.
   - Diff the unit's `splits.txt` entry against master before debugging code.
4. **A definition outside the split is a duplicate once the unit links.** Upstream's
   `ScriptLoaderRel.cpp` defined `SetTweaks_FuncPtrs`, which retail has at 0x802187E4, outside that
   unit's range. The auto-split asm defines that symbol too, so the flip failed with mwldeppc
   `multiply-defined`.
   - The fix: the port-only `ModulePublish.cpp` owns it again. The loader globals also went back to
     8-byte slots, matching `symbols.txt`'s `size:0x8`.

**Per-function losses came from shared headers and stubs, not from rewritten bodies.** Triage
`build/gate-diff.log` (what `gate.sh` writes against master's report) by looking at the header
and the mangled name before the body. Each of these was cheaper to fix than the unit it hit:

- **A dropped header shape can move many units.** Upstream's `CModelFlags(ETrans, float)` passes
  `rgba` straight to `CColor`. Master routed it through `AlphaOf`, which forces retail's second
  `lfs`, and restoring that took ForgottenObject's `RenderInternal` from 88% to 95%.
  `CPlane` lost its trivially-constructible trait (`RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE`); putting
  it back took the `CFrustumPlanes` ctor from 70% to 99.6% and `CCubeRenderer`'s dtor to 100%.
  Diff the report as a whole after any header edit, because gains and losses land in other units.
- **Qualifiers and bitfield types are proven by the asm.** Upstream declared `RenderInternal`
  `const`, but retail's mangling has no `C`, so objdiff paired nothing (0%). `TrackedShot`'s
  `bool m_b : 1` makes MWCC normalise to 0/1 (`neg`/`or`) before the `rlwimi`, and retail stores
  the raw bit, so they are `uchar`.
- **Upstream stubs replace real bodies.** `Tweaks.cpp`'s `REL_CreateTweakGlobals` and `main.cpp`'s
  `AddPaksAndFactories` are `{}` upstream, while master had 68% and 59% bodies. The first went back
  under `#ifndef TARGET_PC`. The second is still a loop item, because the port builds master's
  `mainMid.cpp` and keeps the body; only the objdiff unit lost it.
- **A "compiler implicit" claim needs `symbols.txt`.** Retail defines `__dt__14SLdrCannonBallFv`
  in the ScriptCannonBall REL, so it belongs in `CScriptCannonBall.cpp`, not in the port-only
  `SLdrStructMembers.cpp`.

**A wider upstream unit is not a regression.** `CFactoryMgr` (22 functions, 19 matching) and
`CPatterned` (103) cover what master carved as small `Matching` units. The carved functions still
match inside them, but the unit can only flip once all of it matches. Count lost *functions* by
address (`lostcmp`), not lost units.

## The carve vein, and what it taught about `linked` and about `PortLinkStubs`

Carving one retail function out of a dtk `auto_*` range as its own `Matching` unit is the
highest-yield thing in this tree: **11 units / 14 functions** in one batch, and 62 units / 188
functions in another, all at 100.00%, all verified by `flip_test`. Three rules came out of it
that are not obvious.

**1. `linked` counts *functions*, so a contiguous run is one unit.** The four METROTRK stubs at
`0x80003840..0x80003858` are a single 0x18-byte claim worth **four** matched functions. Do not
split them for tidiness: each split is another `splits.txt` range, another `Object`, another
`files.cmake` line and another chance for the range clash below.

**2. mwcceppc keeps a comparison's source operand order, and that order decides register
assignment.** `IsAllocValid` is 20 bytes either way and scores **59%** as
`ptr != (const void*)-1` and **100%** as `(const void*)-1 != ptr`. Eleven other spellings all
give the 59%. This is the same family as the `rc_ptr` r3 finding (see below): the register a
temporary lands in is a function of *how the expression is written*, not of what it computes. When
a function is the right length and the right arithmetic and still mismatches in one or two
registers, re-order the operands before you re-think the body.

**3. A carve that `PortLinkStubs.cpp` also defines is a duplicate the moment it is listed.** Four
of the eleven needed a hand deletion from the stub file. `link_gap.py` counts what is *missing*
and structurally cannot see a symbol that is defined twice, and the boot probe cannot see it
either - it links with the reach stubs, so the duplicate never appears there. `tools/gate.sh`
has a `port link dups` step for exactly this and it is not optional.

**And a carve can create a link-order cycle with a *neighbouring pre-existing* `Matching` unit.**
Carving `0x80302BAC..0x80302BBC` out of `auto_03_803029D8_text` - which starts exactly where
`CFrustumPlanes.cpp`'s `.text` ends - fails `dtk dol split` with
`Cyclic dependency ... CFrustumPlanes.cpp -> auto_03_803029D8_text` **before anything compiles**.
Proximity to another carve is fine (0x80335A14, 0x80335A5C and 0x80335AB0 are 12 and 24 bytes
apart and all three link); proximity to an existing *unit boundary* is not. When a carve fails
with a cycle and the range looks innocuous, check what range precedes it.

**Source order inside a carved unit is descending by address.** mwcceppc emits functions in
reverse source order, so an ascending file is a permuted `.text` - which is 100.00% per function
and still breaks the DOL.

### The mixed-compiler mechanism exists, and no unit in this tree wants it

`Object(..., mw_version=..., cflags=...)` resolves as **per-object overrides** -
`tools/project.py:65` carries `mw_version` in `Object`'s options, line 665 turns it into the
compiler path (`compilers / "$mw_version"`), and line 1025 collects `used_compiler_versions` as a
**set**. So a project can build different units with different mwcceppc versions and **no change to
`project.py` is required**. `dtk`, ninja and objdiff all accept it. For GC 3.0 the only flag
difference is `-enc SJIS` where 2.x wants `-multibyte`, so:

```python
cflags_gc30 = [("-enc SJIS" if f == "-multibyte" else f) for f in cflags_retro]
Object(NonMatching, "some/Unit.cpp", mw_version="GC/3.0a3", cflags=cflags_gc30),
```

**Measured on the one unit it was built for, and it is worse there.** `CErrorOutputWindowCtor` goes
78.56% (2.7) -> 55.44% (3.0a3 `-O4,p`) -> 13.11% (`-O1`). Across all twenty versions on the real
body: every 2.x emits 46 instructions, every 3.0a* emits 34, **retail is 45** - none byte-exact.
3.0a* fixes the `cntlzw` and then coalesces retail's four `lbz`/`rlwimi`/`stb` pairs (at `-O4,p`) or
drops a `li r3,1` the retail code CSEs (at `-O1`). Both are redundant-load/store elimination and no
flag exposes them.

Nothing else wants it either: `CFrustumPlanes::__ct__` 17 -> 293 differing instructions,
`CVector3f::Cross` 13/16 both with 3.0a3 *losing* one, and `CGX.cpp` **will not compile** under
3.0a3 (`illegal reference type 'void &'`, `single_ptr.hpp:35`).

**So: a mechanism, not a policy.** Recorded because the capability is unlocked and currently unused,
and because the next person to suspect "wrong compiler version" should find this rather than repeat
it. `tools/probe_cerror_versions.py` and `tools/probe_cntlzw_versions.py` are the two probes; the
second one ranks compilers on a synthetic function, the first settles them on the real body, **and
they disagree** - which is the point.

### 2c. A split that moves many functions costs fidelity, and the cost is not avoidable by choosing a different cut

Two splits are now measured, and the difference between them is the lesson.

| split | functions moved | cost |
| --- | --- | --- |
| `mainTail.cpp` -> `CMainShutdownSubsystems.cpp` | **1** | **none.** `CMain::ShutdownSubsystems` went 1.47% -> **`Matching` 100.00%** |
| `main.cpp` -> `CMainRsMain.cpp` + `mainMid.cpp` | **38** | `__ct__24CGameArchitectureSupport` **93.10% -> 87.99%**, `AddPaksAndFactories` 57.15% -> 57.04% |

The mechanism is **mwcceppc's `@stringBase0`**. It is a per-object symbol holding the base of the
literal pool, and the placement string `??(??)..` moved from 0 to 0x76 when 38 functions changed
units; two of seven references to it then change shape. **Both cut directions give 87.99%**, and
bisecting by removing one function at a time does not converge, because the pool base depends on the
set rather than on any one member. So this is not a placement mistake to be fixed by moving a
boundary - it is a property of how much moved.

**The rule: budget the split by how many functions it moves, and prefer the narrowest cut that
reaches the function you want.** A one-function split is free here. A 38-function split silently
degrades everything it carries, and `NonMatching` is exactly why the gate does not notice - which is
the same trade this file has refused before.

**And a split is not progress on its own.** The `main.cpp` split was accepted, gate-green, hash-stable,
and moved `matched` and `linked` by **zero**, because `CMain::RsMain` stayed at 0.26% and
`CMain::CheckReset` at 0.47%. A carve is only worth making when the function inside it can actually
be matched, so **check what is blocking the function before you split its unit** - for `RsMain` that
is a `CMain.hpp` layout job plus 308 unwritten bytes, and neither is affected by where the boundary
sits.

## The `@stringBase0` pool is PER TRANSLATION UNIT, and that settles the `main.cpp` split question

**The hypothesis that a split of `main.cpp` could be free is disproven, and the reason generalises.**

The MWCC constant pool (`@stringBase0` and the `@n` literals in it) is emitted **per translation unit**,
in that unit's own emission order. So the question "does `StreamNewGameState` staying in the head keep
`"??"` at offset 0" has the answer **no**, because the string the constructor needs is not in the same
TU as the head's first string user.

Measured on the three-way split (`main` -> `main` + `CMainFillInAssetIDs` + `mainMid`):

| function | retail | before | after |
| --- | --- | --- | --- |
| `__ct__24CGameArchitectureSupport` | 0x80007EC4 | 93.09910% | **87.98649%** (-5.113) |
| `AddPaksAndFactories` | 0x80007168 | 57.14876% | 57.03513% (-0.114) |

28033 functions unchanged, 0 lost, 0 gained. The tail's first string user in emission order is
`AddPaksAndFactories` (13 pak literals), not `"??"`, so `"??"` lands at 0x76.

**No three-way split can avoid this.** With the carve at 0x80006B38 the ranges are forced to
{head, carve, tail} and the constructor at 0x80007EC4 is always in the third. **This is a structural
property of the layout, not a choice to be tuned** - which is why it is worth writing down rather than
re-attempting.

**And the trade was still worth taking**, because the cost lands on a function that counts 0. The
constructor is 1-of-11 in its unit, so it contributes nothing to `matched` or `linked`, and the
regression is a *linkage-context* artifact: identical C++ source, different pool layout, identical
behaviour. Against that, `CMain::FillInAssetIDs` became an isolated **`Matching` 100.00% 1/1** unit and
`linked` rose 2554 -> 2555.

**The rule this gives: judge a split's cost by what it does to the COUNTS, not to the percentages.**
Percentages on functions that contribute 0 are not a currency. `CMain::AsyncIdle` was declined for
exactly the mirror image of this reason - 1-of-11, contributing 0 to both, for no Matching unit.

**Two operational notes from the same lane.** `flip_test.sh` rewrites the literal one-line
`Object(NonMatching, "<unit>"` form; a wrapped `Object(\n  NonMatching,\n  "<unit>"` is silently never
flipped, so entries must stay on one line - `extra_cflags=[...]` after the unit name is fine. And
`inline_max_size` is settable per unit
(`extra_cflags=['-pragma "inline_max_size(125)"']`), which is how the new units hold their pools.

## `sizeof(CMain)` is 0x98, and retail says so directly

`include/MetroidPrime/CMain.hpp` declared 0x94. It is **0x98**, and the proof is not an inference from
a store instruction - it is retail's own symbol:

```
config/G2ME01/symbols.txt:18933  sMainSpace = .bss:0x803C5A20; // type:object size:0x98 scope:global
config/G2ME01/symbols.txt:18934  lbl_803C5AB8 = .bss:0x803C5AB8; // type:object size:0xC
```

`0x803C5AB8 - 0x803C5A20 = 0x98`, so the size is bounded on both sides by retail, and the next object
is only 12 bytes later. `CMain` is that object: `InvokeCMain` at 0x80008818 is
`lis r9,0x803C ; addic. r31,r9,0x5A20`. All 20 probed words now agree with retail, and
`sizeof(SFrameTimeHistory) == 0x14`.

**The missing 4 bytes are at +0x94**, stored by `stw r8,148(r3)` at 0x800089A0 and by `RsMain`'s
`stw r0,148(r31)` at 0x80005E30.

**A premise in the brief was wrong, and the correction matters: the `li r3,356` in `RsMain` is
`CGameArchitectureSupport`'s size (0x164), not `CMain`'s.** So the +0x94 member is a
`CGameArchitectureSupport*`, not a `CMain*`. Note the naming trap this creates: **`0x164` is that
class's size, and it must not become this member's name** - the member is at +0x94. (The first
version of this header called it `x164_`, which by this file's own convention reads as offset 0x164;
corrected to `x94_cGameArchitectureSupport` at collection.)

**What the two 20-byte windows are.** `+0x18` and `+0x2C`, and `+0x10..+0x18` is a `double`
(`stfd f2,16(r3)`, 0.8041A3F0 = 0.0):

| offset | member | evidence |
| --- | --- | --- |
| `+0x18` | `int count` | ctor `stw r8,24(r3)` at 0x800088C4; `fn_800069AC` does `lwz r0,0(r3) ; cmpwi r0,4` |
| `+0x1C..+0x28` | `float values[4]` | `stfs f0,4(r5)` with `r5 = r3 + count*4`; `v[4]` is left uninitialised by the ctor, which is why `RsMain` pushes 4 seeds |
| `+0x2C` | the same struct, second instance | ctor `stw r8,44(r3)` at 0x800088C8 |
| `+0x40`, `+0x44` | each history's **sum** (Superseded 2026-09-28: it is the **mean** — `fn_80008B60`'s tail is `fmuls f1,f3,f0` after `fdivs f0,f2,f0`, with `f2 = 1.0f` and `f0 = count`; the constant read as `200.0` is mwcc's int-to-double bias 2^52+2^31) | `fn_80006954` returns `fn_80008B60(h->v, h->count)`, an unrolled `fadds` accumulator, stored at 0x80006120 / 0x8000623C |

**Three claims in `boot_path.md` row 10 were wrong and are corrected in place.** It **does not sort** -
there is no `fcmpo`/`fcmpu` in its 308 bytes, only a shift and an 8x unrolled accumulation. The two
floats are **sums, not a running minimum** (superseded 2026-09-28: they are **means** —
`sum * (1.0f / count)`; see the `+0x40`, `+0x44` row above and the `fn_80008B60` note at the end of
this file). And **`CMain::DrawDebugMetrics` is 0x6C bytes and reads
neither** - it toggles a global and calls `CMemory::GetMetrics`; the consumer is `fn_800597D8`. Row 10
was also not an unclaimed gap.

**The strongest single consistency check**, and the reason to believe the model: the sample is
`float(tick delta) * mData[0x10] / 0.016666668`, and `RsMain`'s seeds of 0.3f/0.2f match the
`+0x40`/`+0x44` seeds exactly. `x10_unk` (+0x10) is named for its type only - its absolute unit is
**not** derivable, because its factor is written once to 0.0f by `__sinit_CStopwatch_cpp` at
0x8028BCF0. Flagged as a guess rather than dressed up as a name.

### `fn_800069AC` - 304 of 308 bytes, and why the last 4 are not reachable from here

Written in `src/MetroidPrime/Carve800069AC.c`. NonMatching, **not claimed**, and the reason is
measured: 76 instructions / 304 B against retail's 77 / 308, 62 differing, **the first 15
instructions byte-identical**. Retail materialises the destination pointer with one extra
`addi r8,r8,4`, and **all 62 differences follow from that single `+4` placement** (`lfsx` versus
`add`+displacement). 40 spellings and 6 flag sets were tried; 304 B is the plateau.

**A claim is impossible from this lane**: 0x800069AC sits **inside** `MetroidPrime/main.cpp`'s claim
and cutting that claim in two is another lane's file. After the `6a846c2` three-way split the head is
0x800053B8..0x80006B38, so the function is in the head - claiming it means a two-way split of the
head, which is its own decision with its own pool consequences.

**And landing it buys nothing yet, which is worth saying plainly:** `linked` would not rise, and
nothing in the port can call it - `TARGET_PC` compiles a host `RsMain` that returns immediately, and an
unclaimed carve defines no DOL symbol.

## The state block was frozen, and one of its four lines had no value check at all

Found by a lane while checking something else, and every part of it is the same failure class.

**The writer keyed on hardcoded values.** The ad-hoc script that maintains
`docs/HANDOFF.md`'s state block searched for the literal prefix `"linked     2551"` and
replaced it with the current number. **After one successful run the line read `linked     2554`,
no longer matched its own key, and every later run was a no-op for that line - forever, with
no error.** The same held for the other three, so the block froze at whatever each line
happened to hold when its key last matched. `linked` drifted to 2554 while `report.json` said
2555, and nothing said so.

**Its replacement string truncated the line mid-sentence.** The `REL units` template ended at
`"...This line used to add a"`, and because the key matched it kept overwriting the line with
that fragment. The committed state block carried a dangling sentence; it is gone from the last
60 commits of history, so the prose was rewritten rather than recovered. **A presence test
cannot see a sentence that stops in the middle** - so the fix checks the *shape* of each line,
not only that it is there.

**The checker tested the value of three of the four lines.** `matched`, `DOL units` and
`REL units` were value-checked; **`linked` was only ever checked for appearing exactly once.**
That is the same defect the once-only test had two revisions earlier, in the same file, and the
code said so: *"a once-only test on two of the four lines is a check that covers half the thing
it is named after, which lends its reputation to the half it does not cover."* The value check
had the same shape and nobody extended it.

**All three are now fixed, and the fix is in the repo rather than in `/tmp`:**
`tools/sync_state_block.py` keys on the stable **prefix** so the rewrite is idempotent forever,
rewrites only the numbers and carries the prose across verbatim, **fails loudly** if a line is
missing or has no `functions` to anchor on, and warns on a line that ends mid-sentence.
`check_docs_claims.py` now checks `linked`'s **value**. Both were tested against injected drift:
`sync_state_block.py --check` exits 1 and `check_docs_claims.py` exits 1 on a wrong number, and
0 when correct.

**The general lesson, which is a new entry for `docs/PROCESS_LESSONS.md`: a rewrite keyed on the
value it is about to replace is a rewrite that stops working the moment it succeeds.** Key on
something stable - a prefix, an id, a path - and then make the tool's own drift check a gate step,
because a writer that silently stops is indistinguishable from a writer that has nothing to do.

## `mainMid`'s declaration order is fixed; the unit still cannot flip, and the reason is the link

A pure block move - **234 code lines before, 234 after, 0 changed** - putting the file in
descending-by-retail-address order. The lever: `CArchitectureQueue::Push` (0x80007A80) above
`CGameArchitectureSupport::Update` (0x80007A14). Five definitions outside this claim were moved
to the top sorted by their own addresses, read from `symbols.txt` rather than guessed.

**Measured: 2 of the 15 functions our object emits that retail names were misplaced before, 0 of
15 after** - the adjacent `Push`/`Update` transposition at positions 7-8 was displacing all 12
above it. `check_decl_order.py --unit` reports `ok`.

**And the unit did not move: 54.52%, 9/21, identical before and after.** The reason is worth
recording because it is *not* ordering:

- **`flip_test` fails at the link, not on bytes.** `CResFactory::GetResourceIdByName` is the
  36-byte forwarder at **0x80006B80 - the first byte of this claim** - and nothing else defines
  it, so that one function must be written in source.
- The two `CFactoryMgr::RegisterFactory*` undefineds come from `AddPaksAndFactories`'s **36
  registrations at 0x80007504-0x80007864**, which call symbols `symbols.txt` leaves unnamed. So
  that 1936-byte function **cannot be closed inside this file at all.**
- `unit_fit`: `.text` claimed 6412, ours 6272 - short by 140 - with 24 extra functions / 2352
  bytes.
- Three of retail's unnamed functions at 0x80007AA0/0x80007AC8/0x80007B38 are now emitted
  **byte-identical** (modulo two `bl` relocations) as `push_back` / `do_insert_before` /
  `create_node`, 40/112/136 B, in retail's exact slots. **Pairing them needs three renames in
  `config/G2ME01/symbols.txt`**, e.g.
  `push_back__Q24rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>FRC20CArchitectureMessage = .text:0x80007AA0; // type:function size:0x28`.
  **Those renames are not done** - a DOL-wide symbols edit is its own change.

**So this is a write-the-functions problem, not a reorder problem, and the next slice should be
treated as one:** 3,500+ bytes of unwritten bodies, one required forwarder, one `symbols.txt`
rename set, and 5 definitions that have to move to their own units.

## A percentage is not a link result: `Carve8026FBFC` is 100.00% and must stay `NonMatching`

`BeginScene` (retail 0x8026FBFC, 0x180 = 384 B) is **byte-exact in `.text` and objdiff 100.00%**,
and it still cannot be `Matching`. Isolated by applying one manifest edit at a time:

| edit | DOL sha1 | build.sha1 |
| --- | --- | --- |
| splits only | `6ef9b491` | 87/87 |
| `Carve80272958.c` Matching | `6ef9b491` | 87/87 |
| `Carve80270848.cpp` NonMatching | `6ef9b491` | 87/87 |
| **`Carve8026FBFC.cpp` Matching** | **`09afd3be` (+32 B)** | **44/87** |

**The mechanism:** that unit's object has **no `.sdata2` section**, yet mwldeppc attributes **20 bytes
at 0x8041E250** to it (`main.elf.MAP`, `@407..@411`). Those 20 bytes collide with `CStopwatch.o`'s 8,
`.sdata2` grows 0x54C0 -> 0x54E0, and **43 REL hashes break**.

**It is not a source problem** - the relocations already use the named `lbl_8041DFBC` - so there is
nothing to fix in the C++. This is the same class as the `SetViewPointMatrix` wall: **MWCC's constant
pool is placed by the linker, and a unit's `.sdata2` contribution is not a property of its source
alone.** The rule is the one this repo keeps re-learning, in its sharpest form: **objdiff percentage is
a signal; `Matching` with `flip_test` PASS and 87/87 sha1s is the result.**

## `build.ninja` goes to the repository root, and always has

`tools/project.py:1546` opens `"build.ninja"` as a **relative path, hardcoded to cwd** - not to
`--build-dir`. A lane's fresh worktree therefore looks broken: `ninja -C build` reports
`loading 'build.ninja': No such file or directory`, and a `build.ninja` sits at the repo root.

**The root cause is the first `configure.py` in a fresh worktree, not the change under test.** It runs
before `build/G2ME01/config.json` exists, so `build_config` is `None` and ninja is written with **7
edges instead of 1,788**. One `ninja` fixes it. **Reproduced at HEAD with zero edits.**

This cost a full collection once: the symptom was described correctly, the cause was not found in the
time available, and "I could not isolate it" was then treated as "it is unlandable" and the work was
reverted - when the change had been fine and the harness had simply never been run this way. **Before
blaming a change for a build failure in a fresh worktree, run the harness twice.**

## `build/report.base.json` is untracked, and a stale one turns history into regressions

The gate's per-function diff compares against `build/report.base.json`, which **is not in git**. Ours
still read 3973/2550 and so reported `AddPaksAndFactories` and `__ct__24CGameArchitectureSupport` as
**GONE** - they had moved to `mainMid` in the three-way split three commits earlier. A stale baseline
makes every legitimate earlier change look like a regression, **which is its own way of making a gate
meaningless**: the honest response to a gate that suddenly fails is to ask whether the gate's *input*
is current before concluding the *change* is wrong.

`./tools/gate.sh --baseline` records it, and **refuses to run on a dirty tree** - correctly, since the
baseline must come from a verified commit. Rebase it onto the last commit that passed every gate, then
hold the new change to *that*.

## `CInputStream` reads big-endian on a host (2026-09-27, goal item `port-pak-byteorder`)

**The byte-order wall on the pak chain is fixed at the reader, and only there.** Retail's
`ReadInt32` is the CPU's own `lwz` on a big-endian PowerPC, so the value it returns is the
big-endian word in the buffer with no conversion to see; a little-endian host's identical load
returns the four bytes reversed, and `CPakFile::InitialHeaderLoad` read `0x05000300` against its
`version != 0x30005` test and returned **without advancing `x2c_asyncLoadPhase`**.

**What changed, three files:**

- `include/Kyoto/Streams/CInputStream.hpp` - two host-only helpers, `cinput_stream_read_be32` and
  `cinput_stream_read_be16`, under `#ifdef TARGET_PC`, applied by `ReadInt32` and `ReadUint16` in
  an `#ifdef TARGET_PC` / `#else` around the *return only*, so mwcceppc pre-processes the function
  to byte-identical text. `ReadInt16`/`ReadInt8`/`ReadBool`/`ReadFloat` are reached through those
  two, and so is every `Get<T>()` and every `rstl` stream constructor.
- `src/Kyoto/CResLoaderLoadResourceSyncCompressed.cpp` and
  `src/Kyoto/CResLoaderLoadNewResourceSync.cpp` - the four-byte decompressed-size prefix, which
  those two friends read straight off `x8_ptr` because `Get(4)` is in another translation unit,
  goes through the same helper. **This is the one read `ReadInt32` does not perform**, and it is
  why the fix is not a swap inside `CPakFile`: the same stream also supplies the name-list length,
  `x4c_resTableCount`, every 20-byte resource-table entry and `CStringExtras::ReadString`'s string
  lengths, so a pak-local swap would fix the version word and leave all of them reversed.

Two host-only diagnostics that asserted the old cause were corrected with it:
`src/MetroidPrime/mainMid.cpp`'s `[pak] pump:` messages and the wall note in
`src/Kyoto/CResLoaderAddPakFileAsync.cpp`.

**Measured, not recalled.**

- Host runtime, the real `CInputStream` over `00 03 00 05`: `version = 0x00030005`,
  a count field of `8` read as `8`, `GetReadPosition() = 12`, a 16-bit `AB CD` read `0xABCD`, and
  `3F 80 00 00` read as `1.000000`. `BE_TEST PASS`.
- `./tools/decomp_build.sh`: `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465)`,
  unmoved. DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs against `config.yml`.
- `tools/report_diff.py`: `matched 3980 -> 3980  linked 2557 -> 2557  no regression`, and the three
  touched units' fuzzy percentages are identical before and after - `CInputStream` 100.0,
  `CResLoaderLoadNewResourceSync` 98.041664, `CResLoaderLoadResourceSyncCompressed` 97.24719.
- `./tools/goal_check.sh build/goal/item.json`: **`goal_check: PASS port-pak-byteorder`**, all nine
  checks `ok`, exit 0. Re-run 2026-09-27 17:06 on the tree as it stands: `GATE PASS 5d41809+10
  changed`, `8 path(s) changed under src/ or include/`, `port undefined 322 -> 322`, and
  `probe:` **652** files, `0 failed, 0 errors; link: LINKED (322 undefined, 0 duplicates)`. (An earlier
  run of the same diff read `GATE PASS 8c0783d+3 changed`; the HEAD moved, the numbers did not.)

**What is NOT measured, and do not read this as more than it is:** the in-game claim. The queue
reason asked whether the seven admitted paks now reach `kAP_Loaded`; `tools/boot_probe.sh` was run
and **died in the windowing layer before the game's start-up** - `[error] [aurora::window] Error
initializing SDL: x11 not available`, exit 134, no `[pak]` line at all - so the pak chain's
behaviour at boot is still unmeasured. The measurement above is a function-level one. The next run
that gets a display should print the `[pak] pump:` counts, and they are the number that settles it.

**The judge for a `port` item is partly vacuous, and this item is the proof.** `goal_check.sh`
tests `grep -qF "$TARGET" undef_by_obj.txt`, but `CInputStream::ReadInt32` is an in-class inline -
it is never in the port's undefined set, before or after - so that line passed on a clean tree and
would have passed with the fix absent. The item's `ok` came from `gate.sh`, the counts and the
probe, all of which would also pass on a clean tree. **A `port` item whose target is an inline
function can only be failed by the per-kind check that cannot see it**; the honest verdict for
this one is the runtime test and the report diff above, not the PASS line.

**Three follow-ups this item did not take, all measured:**

- `CBitStreamReader::ReadBits` (`src/Kyoto/Streams/CBitStreamReader.cpp:46`) does
  `x0_stream.Get(&x4_bitWord, len)` and then shifts `x4_bitWord` as if it were big-endian. `Get`
  is a raw copy, so on a host the word is still reversed - the same defect, one level up, and it
  is the next byte-order hole after this one.
- `COutputStream`'s writer side is still host-endian: `CBasics::SwapBytes` is identity under
  `#if 0`, and **retail's is identity too** - proved by `CGameStateBlockFill`,
  `CGameStateSysOptsPutTo` and `CGameStateSlotDefaults`, all three `Matching` with that identity
  inlined. Enabling it under `TARGET_PC` would restore retail's stream-order semantics on the host
  and fix `CCubeMoviePlayer`'s THP headers with it; nothing round-trips a `COutputStream` into a
  `CInputStream` on the boot path today, so nothing here is broken by leaving it.
- `CStringTable`'s `uint* entry = reinterpret_cast< uint* >(x10_strings)` (`src/Kyoto/Text/CStringTable.cpp:96`)
  walks the string table's raw words without a swap.

**And a process finding: this worktree was `git reset --hard` twice out from under the lane.** The
change above passed `goal_check` at 15:52:54 with `8c0783d+3 changed`, and `tools/run_goal.sh`
started a new item at 15:55:50 and reset the tree at 15:55:51, discarding it; the driver then
judged the same item against a clean tree and advanced the queue. The edits were re-applied, reset
again before 16:01:55, and re-applied a third time. **A driver that resets the worktree while a
lane is still in flight destroys the work it is about to judge**, and the vacuous `port` check
above is what let the item read as passed anyway. If a `port` item's target is an inline function,
the driver should not treat `PASS` as evidence that the fix exists.

**Re-measured independently, 2026-09-27, on the re-applied tree (goal lane, same item).** `stat`
before and after the build shows no source byte moved during it, so every number below is
attributable to exactly this tree. The reader, tested against the real
`src/Kyoto/Streams/CInputStream.cpp` and the real header: compiled with `-DTARGET_PC` it prints
`version = 0x00030005`, a count of `8`, `0xABCD`, `1.000000` and `BE_TEST PASS`; the **same test
compiled without `TARGET_PC` fails all five checks with `version = 0x05000300`** - the control that
proves the test can fail and that the `#ifdef` is what it is measuring. Then the gates, each run
again rather than quoted: `./tools/decomp_build.sh` `All: 8.52% fuzzy, 7.54% matched, 5.32% linked
(3980 / 28465)`, DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs against `config.yml`,
`tools/report_diff.py` `matched 3980 -> 3980  linked 2557 -> 2557  no regression`, `tools/gate.sh`
`GATE PASS`, `tools/probe_sources.sh` 0 failed / 0 errors with the link at 322 undefined and 0
duplicates, `tools/check_symbol_names.py` 0 missing, `tools/check_docs_claims.py` ok, and
`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS port-pak-byteorder`**. Still not
measured, and not this item's to measure: whether the seven admitted paks reach `kAP_Loaded`, which
needs a display for `tools/boot_probe.sh` and is `port-pak-warmup`'s number.

**Re-applied and re-judged 2026-09-27 17:04-17:07 (goal lane, same item, tree reset again in
between).** `git status --porcelain` was empty when this run started, so the fix had been reset a
fourth time; it was restored from the salvaged diff of the lane that wrote it and judged as above:
`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS port-pak-byteorder`**, nine `ok`
and exit 0, in 51 s - `GATE PASS 5d41809+10 changed`, `counts: matched 3980 -> 3980   linked 2557 ->
2557`, `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465)`, `8 path(s) changed under
src/ or include/`, `verify port-pak-byteorder.sh: BE_TEST PASS`, `port undefined 322 -> 322`. DOL
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs against `config.yml`, `check_docs_claims.py`
ok. The control still fails: the same test compiled **without** `-DTARGET_PC` prints
`version = 0x05000300` and `BE_TEST FAIL (5)`. Two things were deliberately left out of the restored
diff: `src/MetroidPrime/PortReachStubs.cpp`, which `tools/boot_probe.sh` *appends* to itself
(`fn_802C1658`, `lbl_80418AE4` - both in `Carve8026FB80.cpp.o`'s undefined set) and which is not part
of the fix, and nothing else. Still unmeasured, still not this item's to measure: the boot.

## `CResFactory::AsyncIdle` is written, and the thing under it is a `CDvdRequest` (2026-09-27, goal item `port-asyncidle`)

**The port's link had asked for `_ZN11CResFactory9AsyncIdleEjb` ever since the written
`CMain::AsyncIdle` started calling `gpResourceFactory->AsyncIdle(time, flag)` (boot-path step 21e,
`src/MetroidPrime/main.cpp:307`), and `docs/HANDOFF.md` was still saying it needed a member model
past +0x9C that nothing in the tree had.** It does not: lane `m3` measured the whole `CResFactory`
interior (`docs/research/paks.md`, "The `CResFactory` interior, measured") and all four words this
function reads - `+0xA0`, `+0xB0`, `+0xCC`, `+0xD0` - are named members. What was actually missing
was the disassembly read end to end. Three things in it were not obvious and all three are measured:

- **The divisor is not this class's.** Retail's `r31` at 0x802FA3F8 is `0x80411050`, which
  `config/G2ME01/symbols.txt` names `mData__10CStopwatch` (`.bss`, `size:0x18`), and the two words
  loaded from it are `x8_timerFreqO1M` - `CStopwatch::CSWData`'s `s64` at +0x08, ticks per
  microsecond, written by retail's own `CStopwatch::CSWData::Initialize` as `stw r3,8(r31)` /
  `stw r4,12(r31)` at 0x8028C1C0-0x8028C1C4. That is `__div2i`'s divisor with `r5` the high word
  and `r6` the low, which is why the load is two `lwz`s, and it is what fixes `time`'s unit:
  `CMain::AsyncIdle` passes 500, 5000 and 1000000, so the elapsed count has to be microseconds.
  The public route to the same word is the new
  `CStopwatch::GetGlobalTimerFreqO1M()` (`include/Kyoto/Basics/CStopwatch.hpp`), an inline static
  accessor - no unit mwcceppc compiles emits anything it did not emit before.
- **The element's type is `CDvdRequest`, and the slot is `IsComplete`.** `lwz r3,20(r25)` with the
  node in `r25` is `x8_item+0x0C`, and both this function and retail's enqueue (`fn_802FAF1C`) call
  through the pointer held there. The offsets only close if MWCC's vptr points at the **vtable
  symbol's base** rather than past its two header words - which `CResFactory`'s own constructor
  states by storing `0x803BAF08`, the `__vt__` symbol itself - and then `vptr+0x10` is
  `CDvdRequest::IsComplete` and `vptr+0x18` is `CDvdRequest::GetMediaType`. The header's own slot
  comments (`// 10`, `// 18`) say the same thing, `src/MetroidPrime/mainMid.cpp:434` already relies
  on it, and `CDvdFile::AsyncSeekRead` returns `CDvdRequest*`
  (`include/Kyoto/CDvdFile.hpp:53`) which is what `fn_802FC898` - the call whose result
  `fn_802FA140` stores at `item+0x0C` - is built on.
- **The trade is one-for-one, not a win.** Retail's erase is out of line (`fn_802FB2E4`, 0x8C
  bytes: unlink, `fn_802FA070(item, -1)`, `CMemory::Free(node)`, `--x14_count`), and that symbol
  was **not** in the port's undefined set, so calling it adds one. Defining `AsyncIdle` removes
  exactly one. The counts bear it out: **322 undefined before, 322 after**, with
  `_ZN11CResFactory9AsyncIdleEjb` gone and `fn_802FB2E4` in its place - and `fn_802FA070` is not
  referenced by the new object, so the item destructor is not a second new hole.

**What changed:**

- `src/Kyoto/CResFactoryAsyncIdle.cpp` - new, port-only, listed in `files.cmake`. The body is
  retail's two halves in retail's order: the `xc8_active` sweep (advance the iterator *before* the
  possible erase, which frees the node) and the timed `x9c_loading` pump loop with its `stop` byte,
  `time - elapsed` budget and `flag` override. Its header carries the annotated disassembly.
- `include/Kyoto/Basics/CStopwatch.hpp` - `GetGlobalTimerFreqO1M()`, the accessor above.
- `src/Kyoto/CResFactoryPortVirtuals.cpp` - **the port's empty `CResFactory::CResFactory()` now
  initialises both lists.** Retail's `fn_802FB154` writes each list's four pointers to its own
  `xc_empty_prev` and its count to 0; an empty body left all six words of both `SLoadList` members
  indeterminate, which was harmless only while nothing read them. Walking an indeterminate
  `x4_start` is a segfault rather than a wrong answer, so this is a prerequisite of the function,
  not a convenience. `x0_allocator` is left alone - retail stores nothing there either.
- `src/MetroidPrime/PortReachStubs.cpp` - **`reachstub_137` deleted.** The stub and a real
  definition of the same symbol collide the moment `MP_BOOT_STUBS=ON`, which is what
  `tools/boot_probe.sh` passes, and `gate.sh`'s duplicate step cannot see it. This is the rule
  `tools/check_files_cmake.py` states for `CAudioStateWinCtor.cpp`: "Delete that alias."
- `docs/research/port_link_gap_list.md` - regenerated with `tools/link_gap.py --write-list`:
  `_ZN11CResFactory9AsyncIdleEjb` out, `fn_802FB2E4` in, 319 MISSING both before and after.
  The tool's own check is the reason the file is touched at all - a listed symbol that is no longer
  missing fails the gate until its entry is deleted.
- The probe's file count moved **652 -> 653** with the new source, so every current-state quote of
  it in `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` was bumped by
  `tools/check_docs_claims.py`'s rule. Three *historical* incident quotes of the same count were
  **not** rewritten - the figure was right when written - they were re-spelled as `` `652` files ``
  so the checker reads them as a figure of the past rather than a current claim.

**Measured, not recalled.**

- `./tools/decomp_build.sh`: `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465
  functions)`; DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs against
  `config.yml`. Adding a header accessor changed no unit's bytes.
- `tools/report_diff.py` over the judge's baseline: `matched 3980 -> 3980   linked 2557 -> 2557`.
- `tools/link_check.sh`: `compile errors 0`, `unique undefined symbols 322`,
  `duplicate definitions 0`, `unchanged from baseline (322 undefined, 0 duplicates)` -
  and `CResFactory::AsyncIdle(unsigned int, bool)` is no longer in `link_undefined.txt` while
  `fn_802FB2E4` is, at line 309.
- `python3 tools/link_gap.py`: `319 MISSING symbol(s), all accounted for`.
- `python3 tools/check_symbol_names.py`: `checked 322 units; 0 declared names are missing`.
- `python3 tools/check_files_cmake.py`: `647 sources`, `0 on-disk sources are in no manifest`.
- `python3 tools/check_decl_order.py`: `ok: 841 unit(s) checked`.

## `StreamNewGameState` is defined under retail's own name (2026-09-27, goal item `port-streamnewgamestate`)

The target `StreamNewGameState__5CMainFR12CInputStreami` was never missing a *function* - it was
missing a *name*. `main.cpp` has carried `CMain::StreamNewGameState`'s body since the scaffold, but
the host compiles it to `_ZN5CMain18StreamNewGameStateER12CInputStreami`, and the one caller in the
port, `CMainFlowDtor.cpp:315`, reaches it through the `extern "C"` declaration at line 208 spelled
with mwcceppc's mangled name - which no host compiler will ever emit for a member. So the symbol
sat on the undefined list while its body sat in the tree. `CMainFlowDtor.cpp`'s own header comment
(point 4) says why that call site is untouchable: retail passes a **null** `CInputStream&` and
never writes r5, and no C++ spelling of "an uninitialised int" is free.

**What landed.** `src/MetroidPrime/PortStreamNewGameState.cpp`, port-only (`configure.py` does not
declare it, so no `splits.txt` range and no DOL byte moves), holding retail's 532-byte body block
by block from `powerpc-eabi-objdump` of `0x800053B8..0x800055CC`. Its header is the annotated
disassembly: every retail address, what it does, and which line here answers it. Plus
`CMain::GetGameGlobalObjects()` in `include/MetroidPrime/CMain.hpp` - retail reads that pointer as
`lwz r3,84(r28)` and the member is private, and **the offset cannot be spelled instead**, because
`CMain`+0x54 in a 32-bit GameCube object is not `CMain`+0x54 when every pointer is eight bytes
wide; the accessor is `inline` with no caller in any `configure.py` unit, so mwcceppc emits nothing.

**The three helpers, each measured rather than guessed.** `SGameStateSlots` copy, release and
assign are `fn_80004C90`/`fn_80004CD4`, `__dt__80004B9C`/`fn_80004BEC`/`fn_80004C4C` and
`fn_80142944` in retail, and all three reduce to the two primitives the port already defines:
`fn_80004C4C` is `li r4,-1; b fn_80004A4C`, `fn_80004CD4` is a `count`-iteration loop of
`fn_80004D3C` -> `fn_80004D5C` -> `fn_80004AA0`, and `fn_80142944` is `fn_80004BEC(dst)` then a
range copy-construct then `dst->x00_count = src->x00_count` - i.e. `ReleaseSlots` followed by
`CopySlots`. `fn_801427DC` is `addi r3,r3,376; b fn_80142800`, and `fn_80142800` is
`SGameStateBlock`'s `operator=` (`fn_80142914` then free-or-reserve-and-copy), so the file does
free-then-copy instead of reuse: the same bytes in the block, a fresh allocation rather than a
reused one.

**The two blocks that are not reproduced, named in the file's header rather than dropped.**

- The `SGameStateCardOpts` copy at `CGameState+0x54` and its carry-over: `fn_80005108` is
  `fn_800052A0(dst, src)` + `fn_80005158(dst+0x18, src+0x18)` + one word and
  `__dt__PersistentOptions_800050A4` destroys sub-objects at `+0x00` and `+0x18`, so the member
  owns two heap things the header's `u8 x00[0x1C]` does not model - a plain struct copy here would
  be a shallow copy of both and a double free. **The one word that local is actually read for
  survives**: retail loads the save-slot index at 0x800053E8, and that is `gpGameState->x54.x28`,
  read before the release.
- `fn_80142FEC(new)` (0x80142FEC, 0x80), only when the flag is set, unnamed and unwritten.

**Why the rest costs exactly one new symbol.** `tools/goal_check.sh` fails a `port` item whose
unique undefined count rises; resolving the target frees exactly one, and the whole budget is spent
on `fn_80144140`, retail's `CGameState` stream constructor, which the port does not define
(`CGameStateStreamCtor.cpp` is in `check_files_cmake.py`'s `EXCLUDED` list: twenty-one symbols to
close none). Everything else is defined already or written here from `fn_80004AA0`/`fn_80004A4C`.

**A NEW finding, recorded here rather than fixed: retail's `fn_80144140` takes a
`CBitStreamReader&`, not the `CInputStream&` that `CGameState.hpp:94` and
`CGameStateStreamCtor.cpp:343` both declare.** The bytes settle it - `StreamNewGameState` passes
`&r1+8`, the `CBitStreamReader` built at 0x80005498, and `fn_80144140` then calls
`ReadBits__16CBitStreamReaderFUi` on that pointer at 0x80144314 and passes it to
`__ct__12CPlayerStateFiR16CBitStreamReader` at 0x80144420. `CBitStreamReader` stores `x0_stream` at
`+0x00` and has no vtable (`__ct__16CBitStreamReaderFR12CInputStream` is four stores), so it is not
a `CInputStream` and cannot be passed as one. The new file declares it `extern "C"` with the type
retail's bytes show; the two existing declarations are untouched because both are in units this
item may not move.

`reachstub_264` was deleted from `src/MetroidPrime/PortReachStubs.cpp` in the same change: the stub
and a real definition of the same symbol collide the moment `MP_BOOT_STUBS=ON`, which is what
`tools/boot_probe.sh` passes and what `gate.sh`'s duplicate step cannot see. The file's own
"Breakdown" line claimed 317 stubs; the bodies say **298** (244 `_Z...`, 3 `REL_Load*`, 51
unmangled), so it was already stale and is now written from a count rather than from memory.

**Measured, not recalled.**

- `./tools/decomp_build.sh`: `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465
  functions)`; `sha1sum build/G2ME01/main.dol` =
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs against `config.yml`.
- `tools/gate.sh build/goal/judge/report.base.json`: `matched 3980 -> 3980   linked 2557 -> 2557`,
  `no regression`, every step ok - including `docs claims` after the probe count moved.
- `tools/link_check.sh`: `compile errors 0`, `unique undefined symbols 322`,
  `duplicate definitions 0`, `unchanged from baseline (322 undefined, 0 duplicates)`, and
  `StreamNewGameState__5CMainFR12CInputStreami` is no longer in `link_undefined.txt` while
  `fn_80144140` is, at line 272.
- `python3 tools/link_gap.py --rebuild`: `319 MISSING symbol(s), all accounted for`. The list was
  regenerated with `--write-list`: the target out, `fn_80144140` in, both in the unmangled group,
  so `docs/research/port_link_gap.md`'s 173/75/71 table is unchanged.
- `./tools/probe_sources.sh`: `654` files, `0 failed, 0 errors; link: LINKED (322 undefined, 0
  duplicates)`. The count moved 653 -> 654 with the new source, so every *current-state* quote of
  it in `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` was bumped under
  `check_docs_claims.py`'s rule; one *historical* session-end quote was **not** rewritten - it was
  re-spelled as `` `653` files ``, the convention `RUNNING_THE_DECOMP.md` records for `` `652` files ``.
- `python3 tools/check_symbol_names.py`: `checked 322 units; 0 declared names are missing`.
- `python3 tools/check_files_cmake.py`: `648 sources`, `0 on-disk sources are in no manifest`,
  `every configured DOL object is either in files.cmake or excluded with a reason`.
- `python3 tools/check_decl_order.py`: `ok: 841 unit(s) checked, 18 permuted, all 18 accounted for`.

## The pak pump drains: `fn_802FD174` erased from `list + 0x48` (2026-09-27, goal item `port-pak-pump`)

One line: **the port's own copy of `fn_802FD174` was handed `&x48_pakLoadingList` and cast it to
`CResLoader*`, so it erased from `self->x48_pakLoadingList` - `list + 0x48`, 0x90 past the start
of a 0x70 object.** `x48`'s count never moved, `AreAllPaksLoaded()` never became true, and the
`while (!AreAllPaksLoaded())` loop in `AddPaksAndFactories` block 7 kept re-moving the same
already-loaded entry. The fix is four lines inside the existing `#ifdef TARGET_PC` block of
`src/Kyoto/CResLoaderPakPump.cpp`: cast to `rstl::list< SPakLoadEntry >*` and erase from that.
Retail is unaffected - `mwcceppc` does not define `TARGET_PC`, so the matching build never sees
this block and its call still binds to retail's own `fn_802FD174`.

**Measured, not recalled.** The acceptance test is `tools/goal_verify/port-pak-pump.sh`, which
builds `tools/boot_probe.sh` and boots against the disc for up to 120 s:

- pre-fix: `verify: all 7 paks loaded but the boot never left the pump (no "Initializing
  renderer..." after it) - PAK_PUMP FAIL`, with `[pak] pump: 1000 iterations and x18+x30 is still
  1835. The list is not draining.` - 7 of 7 `phase -> kAP_Loaded` lines present, so `CPakFile`'s
  warmup chain was **not** the wall;
- post-fix: `PAK_PUMP PASS: 7/7 paks loaded, the pump drained, the boot reached the renderer`,
  and the run.log carries `Initializing renderer...` then `boot: step 21c returned -
  CCubeRenderer's constructor completed, 8 pool tokens` before the known later fault in
  `CEnvFxManager::Initialize` (past this check).
- `./tools/goal_check.sh` (the driver's invocation, `MetroidPrime2Port/tools/goal_check.sh` from
  inside the worktree): **`goal_check: PASS port-pak-pump`**, nine `ok`, exit 0 - `GATE PASS`,
  `counts: matched 3980 -> 3980   linked 2557 -> 2557`, `All: 8.52% fuzzy, 7.54% matched, 5.32%
  linked (3980 / 28465 functions)`, `1 path(s) changed under src/ or include/`, `port undefined
  322 -> 322`, `probe:` `654` files, `0 failed, 0 errors; link: LINKED (322 undefined, 0 duplicates)`.
- DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and 86/86 RELs (in `gate.sh`), plus
  `check_decl_order.py` `841 unit(s) checked, 18 permuted, all 18 accounted for`,
  `check_symbol_names.py` `0 declared names are missing`, `check_docs_claims.py` ok.

**Process note worth keeping: running `tools/goal_check.sh` by hand from the worktree fails the
item even when it is a pass.** It resolves `tools/goal_verify/` relative to its own location, so
the worktree copy - which has no `port-pak-pump.sh` - prints `verify script
tools/goal_verify/port-pak-pump.sh is missing`; the driver runs the *main repo's* script with
`cwd` set to the worktree (`run_goal.sh:378`), which finds it. The agent may not copy the script
across: it is under `tools/`, and touching that path fails the item outright.

`docs/HANDOFF.md` was corrected in place for the two stale passages this item measured: the
"seven paks reaching `kAP_Loaded` is still open" line and the "`CPakFile::Warmup` /
`CRealDvdRequest::IsComplete` gate the pak chain" line, both marked superseded.

## `fn_8029c7e8` is `CSfxManager::LoadTranslationTable`, and the port now keeps its token (2026-09-27, goal item `port-fillinassetids`)

**The symbol the port's link asks for, `CSimplePool::fn_8029c7e8(SObjectTag const&)`, is audio's
translation-table loader under the name this tree gave the address, and `./tools/dis.sh 0x8029C7E8
0x150` says so directly rather than by inference.** `r3` is used **only** as the object of one
virtual call (`lwz r12,0(r29)` / `lwz r12,12(r12)` / `bctrl`, with `r4=r29, r5=r30` - `GetObj(tag)`
at vtable slot 0xC); `r4` is tested as a **null pointer** and returns 0 when it is; `lbl_80419884`
(`.sbss`, `r13-25852`, resolved with `tools/sda.py`) is deleted through `fn_80255C00` - which frees
the buffer at `+12` and then the object, a deleting destructor - and zeroed; a fresh `CToken` is
stored into `lbl_8041988C` (`r13-25844`, 8 bytes: `x0_has` then `x4_item`), that token is `Lock()`ed,
and the function returns 1. That is `CSfxManager::LoadTranslationTable(CSimplePool* pool, const
SObjectTag* tag)` statement for statement - `../MetroidPrimePort/src/Kyoto/Audio/CSfxManager.cpp:703`,
called the same way from that tree's `main.cpp:558` with `gpSimplePool` and
`gpResourceFactory->GetResourceIdByName("sound_lookup")`. The surrounding object agrees too: a 0x4C
function in front (MP1's `TranslateSFXID` is 0x4C) and the same four between it and `CSfxManager::
PitchBend` in both trees - MP1's names for them are the `auto_ptr<CToken>` destructor, `GetRank`,
`IsHandleValid`, `IsPlaying` - and MP1's Japanese and PAL builds give the function `size:0x150`,
which is this address's size here.

**What was written: one port-only body in `src/Kyoto/CSimplePoolPort.cpp`** - `GetObj(tag)` into a
`CToken` that is *kept* and `Lock()`ed, held in a file-local `rstl::auto_ptr` named as retail's
`CSfxManager::mTranslationTableTok`. Keeping it is the whole of the work: dropping the token runs
`CObjectReference::RemoveReference` -> `CSimplePool::ObjectUnreferenced` on the spot and
`FillInAssetIDs` is a no-op. Retail's parsed `rstl::vector< short >` is **not** represented, stated
rather than faked - its only reader, `CSfxManager::TranslateSFXID`, is still undefined in the port,
so there is no table to drop and none was invented. Retail returns `true`; the declaration stays
`void` because `include/Kyoto/CSimplePool.hpp` is included by `Kyoto/CSimplePoolCtor.cpp`, a
`Matching` unit, and a `bool` there buys nothing the caller reads. `reachstub_150` came out of
`src/MetroidPrime/PortReachStubs.cpp` in the same change - the deletion `tools/boot_probe.sh`'s own
duplicate-definition branch prescribes - and the file's breakdown was recounted with its own grep:
**297 stubs** (243 Itanium, 3 `REL_Load*`, 51 unmangled), 298 before. `docs/research/
port_link_gap_list.md` was regenerated with `tools/link_gap.py --rebuild --write-list` (319 -> 318
entries, `other game methods` 173 -> 172) and `port_link_gap.md`'s table row moved with it, because
a listed symbol that is no longer missing fails the gate as stale.

**Measured, not recalled**: `./tools/probe_sources.sh` `654` files, `0 failed, 0 errors; link: LINKED
(321 undefined, 0 duplicates)`; `./tools/link_check.sh` `compile errors 0`, `unique undefined symbols
321`, `duplicate definitions 0`, with the target absent from the list it was in at the branch head
(`build/goal/judge/undef.base.count` = 322 -> 321); `./tools/gate.sh
build/goal/judge/report.base.json` `GATE PASS 1f2701c+5 changed`, `matched 3980 -> 3980   linked
2557 -> 2557`, `port link gap ok`, `docs claims ok`, `reach stubs not in a real build ok`;
`./tools/decomp_build.sh` `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465 functions)`;
`sha1sum build/G2ME01/main.dol` `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`python3 tools/check_symbol_names.py` `checked 322 units; 0 declared names are missing from their
object`; `python3 tools/check_docs_claims.py` `docs claims agree with the tree`.

**What this does not do.** `CMain::FillInAssetIDs` is still off the host boot ladder - `PortBoot.cpp`
runs steps 12 and 17-20 and stops - so nothing calls the new body yet, and the `sound_lookup` table
it would load needs `Strings.pak`. Both are recorded rather than papered over: the entry answers
"the pool knows the tag and cannot build it", which is the answer `PortPoolStandIns.cpp`'s
`sound_lookup_ATBL` row was written to give.

**One derived input is knowingly left stale.** `docs/research/boot_path_undefined.txt` and
`docs/research/boot_path_reachable.tsv` still name this symbol - both say "Generated by
tools/link_reach.py - do not hand-edit", and that tool was run against a fresh
`build-port-link/build.log` and its output measured (`the linker asked for: 321`,
`referenced by a REACHABLE object: 319`, `referenced only by UNREACHABLE objects: 2`, all three
files rewritten) **but not kept**: the same run also refreshes entries that went stale in earlier
landings - symbols since defined (`AllocateRenderer`, the `CARAMManager` and `CCallStack`
families) and reference moves (`main.cpp` -> `mainMid.cpp`), 59 insertions and 61 deletions across
the three files, none of them this item's. So they belong to their own change. The consequence is
recorded where it bites: `src/MetroidPrime/PortReachStubs.cpp`'s RETIRED comment says re-running
`tools/gen_link_stubs.py --reachable` puts `reachstub_150` back until those two files are
regenerated.

## `GetResourceIdByName` walks the loader first, and an owned buffer frees from the game heap (2026-09-27, goal item `port-boot-cpakfile-sresinfo-getsize`)

Two `src/` fixes moved the boot off `CPakFile::SResInfo::GetSize` (`src/Kyoto/CPakFile.cpp:94`,
the branch head's stop, reproduced twice on the clean tree) and into retail's frame loop.
`src/Kyoto/CResFactoryPortVirtuals.cpp` now does retail's body first - `fn_802FCC44`, the loader's
two-list walk - and keeps the stand-in registry as the fallback: the registry answered
`"DUMB_SnowForces"` with `kStandInIdBase + 10` = `0xF000000A`, an id no pak row carries, so
`fn_802FCDE8` returned null, `x68_curRes` stayed null and `fn_802FC63C:86` dereferenced it.
`src/Kyoto/Streams/CInputStream.cpp` frees an owned buffer with `CMemory::Free` under `TARGET_PC`
- both producers (`CResLoaderLoadNewResourceSync.cpp:93`, `DolphinCLZOInputStream.cpp:9`) allocate
with `CMemory::Alloc`, and on a host the `delete[]` spelling reaches libstdc++'s `free` and aborts
in `munmap_chunk()` at the end of `CEnvFxManager::Initialize` - while mwcceppc still compiles
`delete[]`, so `tools/flip_test.sh Kyoto/Streams/CInputStream.cpp` reports
`PASS  -> kept as Matching`. Measured: `./tools/goal_verify/boot-progress.sh` ->
`BOOT_PROGRESS PASS: all 2 runs got further than all 2 head runs` (both runs reach the frame loop
and stop at its first declared stop, `fn_801F05D0`, retail 0x801F05D0), and
`./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS` (gate ok, `matched 3980 ->
3980   linked 2557 -> 2557`, `port undefined 321 -> 321`, probe `654` files 0 failed).

## `LoadTypedefEditorProperties` reads its four properties, and its one callee came with it (2026-09-28, goal item `port-loadtypedefeditorprops`)

**New, port-only: `src/MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties_Load.cpp`** - listed
in `files.cmake`, absent from `configure.py`, because retail `0x8023EF3C` (size `0x140`) is in an
unclaimed `.text` range: the nearest split blocks are `MetroidPrime/ScriptLoader/RubiksPuzzle.cpp`
ending `0x802399F4` and `MetroidPrime/ScriptLoader.cpp` starting `0x80242894`, so no unit owns
these bytes and there is nothing for `flip_test.sh` to flip. A carve would be four files in one
change and is a different job; this item is `kind: port`.

The body has the shape every generated `LoadTypedef*` has - `u16` property count, then a word id
and a `u16` size per property, then a switch - and every arm was checked against
`tools/dis.sh 0x8023EF3C 0x140` rather than assumed: `0x494E414D` `name` constructs a temporary
`rstl::string` from the stream, assigns it over `name` and destroys it (`0x8023EFDC`, which is
what `sldrThis.name = rstl::string(input);` emits), `0x5846524D` `transform` calls the helper at
`0x8023F00C`, `0x41435456` `active` reads one byte, normalises it to 0/1 with `neg`/`or`/`srwi 31`
and stores it at `+0x34` (`0x8023F014`, which is what `ReadBool()` - `ReadUint8() != 0` - does),
`0x5D298A43` `unknown_0x5d298a43` reads one word into `+0x38` (`0x8023F038`), and the default arm
is `ReadBytes(nullptr, propertySize)` (`0x8023F050`). Retail's struct is 60 bytes - `name` 0x00,
`transform` 0x10, `active` 0x34, `unknown` 0x38 - which is the header's own declaration order.

**`LoadTypedefSLdrTransform` had to be written with it.** The `transform` arm's callee is retail's
`fn_8023F8CC` (`0x8023F8CC`, `0x9C`): three `CVector3f` read from the stream into offsets 0x00,
0x0C, 0x18 - position, rotation, scale. It is unnamed in `symbols.txt` because nothing else in the
DOL calls it; scanning `.text` for `bl 0x8023F8CC` returns exactly one hit, `0x8023F00C`. Nothing
in the port referenced it either, so no stub list carried it: leaving the arm as a call to an
undefined symbol would have closed one gap and opened another (`321 -> 321`) and would have broken
`tools/boot_probe.sh`'s link, which has no reach stub to answer it. Both functions are in the one
new file.

`reachstub_194` left `src/MetroidPrime/PortReachStubs.cpp` in the same change - the deletion
`tools/boot_probe.sh`'s duplicate-definition branch prescribes - and the file's breakdown was
recounted with its own grep: **296 stubs** (242 Itanium, 3 `REL_Load*`, 51 unmangled), 297 before.

**One host conversion, spelled out rather than inherited.** Retail's helper calls
`__ct__9CVector3fFR12CInputStream` three times (`0x8023F8EC`, `0x8023F910`, `0x8023F934`), and the
port's own copy of that constructor is `in.Get(this, sizeof(CVector3f))`
(`src/Kyoto/Math/CVector3f.cpp:24`, a `Matching` unit whose body may not change) over
`CInputStream::Get`'s plain `memcpy` (`src/Kyoto/Streams/CInputStream.cpp:51`, no `TARGET_PC`
arm), so on a little-endian host the twelve bytes come back in stream order and the three floats
are not retail's value. The arm therefore reads them with `ReadFloat()` - `ReadInt32` and
`cinput_stream_read_be32` under `TARGET_PC` - which is how `CColor::CColor(CInputStream&)`
(`src/Kyoto/Graphics/DolphinCColor.cpp:6`) reads its four floats, and why the three reads are a
file-local helper: three `ReadFloat()`s in one argument list are unsequenced.
`CScriptPickup.cpp:289/292/361` uses `CVector3f(input)` and inherits that pre-existing gap in
shared code; it is deliberately not touched here.

**Measured, not recalled**: `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS
port-loadtypedefeditorprops`, nine `ok`, exit 0 - `GATE PASS 2f37441+7 changed`, `matched 3980 ->
3980   linked 2557 -> 2557   (+0 functions at 100%, 0 units newly linked)`, `All: 8.52% fuzzy, 7.54%
matched, 5.32% linked (3980 / 28465 functions)`, `2 path(s) changed under src/ or include/`,
`LoadTypedefEditorProperties(...) was undefined at the branch head and is not now`,
`port undefined 321 -> 320`, `probe: `655` files, 0 failed, 0 errors; link: LINKED (320 undefined,
0 duplicates)`; `./tools/link_check.sh` `compile errors 0`, `unique undefined symbols 320`,
`duplicate definitions 0`; `python3 tools/link_gap.py --list` `16 c++ runtime / linker, 34
libc/libm, 134 aurora source, 0 aurora header only, 317 MISSING` with `--write-list` `wrote 317
entries in 3 groups` (318 -> 317, `REL module loaders` 71 -> 70, so `port_link_gap.md`'s table row
moved with it); `python3 tools/check_files_cmake.py` `649 sources`; `python3
tools/check_symbol_names.py` `checked 322 units; 0 declared names are missing from their object`;
`python3 tools/check_docs_claims.py` `docs claims agree with the tree`; `sha1sum
build/G2ME01/main.dol` `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

**Adding a source moved the probe's file count 654 -> 655**, so every *current-state* quote of it
in `docs/HANDOFF.md` and this file was bumped under `check_docs_claims.py`'s rule, and the
historical session-end quotes were re-spelled as `` `654` files ``, the convention this file
already records for `` `653` files `` and `` `652` files ``.

**Still listing the symbol, not regenerated here:** `docs/research/boot_path_undefined.txt:202`
and `docs/research/boot_path_reachable.tsv:196` are `tools/link_reach.py`'s output and are the
generator input for `tools/gen_link_stubs.py`; re-running the generator over them puts
`reachstub_194` back, which is what the retirement comment in `PortReachStubs.cpp` says.

## Retail has one `LdrToEntityInfo`, so the port's const overload is the forwarder (2026-09-28, goal item `port-ldrtoentityinfo`)

**Which of the pair is the forwarder - the question the item was queued on - is a count, not a
judgement.** `config/G2ME01/symbols.txt` has exactly one `LdrToEntityInfo`,
`LdrToEntityInfo__FR11CEntityInfoRC20SLdrEditorProperties` at `0x80239BD4` (`0x38`), and
`objdump -d build/G2ME01/main.elf` has **91 `bl 80239bd4`** and no second function anywhere in
`.text`. Those 91 include every loader whose `info` parameter is `const CEntityInfo&` and which
binds to the *const* overload in this tree - `LoadPickup` (call at `0x800B3FA4`), `LoadHUDMemo`,
`LoadSequenceTimer`, `LoadStreamedAudio`, `LoadAreaProperties`. So retail's non-const is the
body; the const overload is the port's own (it comes from `include/MetroidPrime/CEntityInfo.hpp`,
which declares only the const one, which is why `build/goal/judge/undef.base.txt:159-160` carries
both) and it is a forwarder over a `const_cast` - the cast those four retail call sites are
already making, and which costs no instruction.

**The body is three bits, not a conversion.** `tools/dis.sh 0x80239BD4 0x38`: `props.active`
(the bool at `SLdrEditorProperties +0x34`) into the flags byte's bit 7,
`props.unknown_0x5d298a43` (`+0x38`) bit 0 into bit 6, and its bit 1 into bit 5 - three
`lbz`/`stb` read-modify-writes in source order, which is how mwcceppc emits three independent
field stores (the rule `real_loaders.md` item 5 used for `LoadAreaProperties`' stores). Bits
7/6/5 are `active`/`scriptingBlocked`/`unk` in declaration order: the constructor at `0x800484D4`
proves the layout rather than the header does - `editorId` is written with `stw r0,20(r29)`, the
flags byte is `lbz r0,24(r29)` (+0x18), and that constructor's own `bool active` argument goes to
bit 7 with the same `rlwimi ...,7,24,24`. There is no area id, no connection list and no call in
the 0x38 bytes: the name overstates the work, and anything that reads only the name will go
looking for a conversion that is not there.

**New, port-only: `src/MetroidPrime/LdrToEntityInfo.cpp`**, both overloads, listed in
`files.cmake` and absent from `configure.py` because `0x80239BD4` sits in the same unclaimed
`.text` range as the previous item's `0x8023EF3C` (nearest splits `RubiksPuzzle.cpp` ending
`0x802399F4`, `ScriptLoader.cpp` starting `0x80242894`) - no unit owns the bytes, so there is
nothing for `flip_test.sh` to flip and a carve (four files in one change) is a different job.
`CEntityInfo.hpp` gained a `friend` for the non-const - retail names no setter for those bits,
only the two constructors and the destructor - and `struct SLdrEditorProperties;` moved above the
class so the friend declaration can see it; the const overload needs no access, only
`const_cast`. `reachstub_187` and `reachstub_188` came out of `src/MetroidPrime/PortReachStubs.cpp`
in the same change, and its header breakdown was recounted with its own grep: **294 stubs**
(240 Itanium, 3 `REL_Load*`, 51 unmangled), 296 before.

**Measured, not recalled**: `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS
port-ldrtoentityinfo`, nine `ok`, exit 0 - `GATE PASS 4d89321+8 changed`, `matched 3980 -> 3980
linked 2557 -> 2557`, `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465 functions)`,
`3 path(s) changed under src/ or include/`, `LdrToEntityInfo(...) was undefined at the branch head
and is not now`, `port undefined 320 -> 318`, `probe: `656` files, 0 failed, 0 errors; link: LINKED
(318 undefined, 0 duplicates)`; `./tools/link_check.sh` `compile errors 0`, `unique undefined
symbols 318`, `duplicate definitions 0`; `powerpc-eabi-nm` on the new object lists both
`_Z15LdrToEntityInfoR11CEntityInfoRK20SLdrEditorProperties` and
`_Z15LdrToEntityInfoRK11CEntityInfoRK20SLdrEditorProperties` as `T`; `python3 tools/link_gap.py`
`measured over 650 object(s)`, `16 c++ runtime / 34 libc / 134 aurora source / 0 header only /
315 MISSING`, `ok: 315 MISSING symbol(s), all accounted for`, `--write-list` `wrote 315 entries in
3 groups` (317 -> 315, `other game methods` 172 -> 170, so `port_link_gap.md`'s table row moved
with it); `python3 tools/check_files_cmake.py` `650 sources`; `python3 tools/check_symbol_names.py`
`checked 322 units; 0 declared names are missing from their object`; `python3
tools/check_docs_claims.py` `docs claims agree with the tree`; `sha1sum build/G2ME01/main.dol`
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

**The boot probe linked with both stubs out, and asked for the body instead.**
`./tools/boot_probe.sh` -> `relink status 0`, `linked 93119976 bytes`, `the log names 0
unresolved symbols` (the `fn_80270xxx` `[auto-stub]` lines are unnamed retail functions that
are in `link_gap.py`'s MISSING list before and after this item), **no** `multiple definition of` anywhere - the one
failure the boot probe can see that `gate.sh`'s duplicate step cannot, because that step links
without `MP_BOOT_STUBS=ON` - and **zero** `[reach-stub ... LdrToEntityInfo ...]` lines in the run
log, so the nine objects resolve to the new file rather than to a stub that logs and returns. The
boot itself went where the branch's last measured boot went: `boot: step 21 - the frame loop`,
`frame: 1`, `frame loop stopped: fn_801F05D0(lbl_80418EC8) (retail 0x801F05D0, 0xF8) is not
written`. This item does not move that wall - it is a script-loader helper, not one of the frame
loop's callees.

**Adding a source moved the probe's file count 655 -> 656**, so the six *current-state* quotes of
it in `docs/HANDOFF.md` and this file were bumped under `check_docs_claims.py`'s rule, and the
two historical session-end quotes were re-spelled as `` `655` files ``, the convention this file
already records for `` `654` files ``.

**Still listing the two symbols, deliberately not regenerated here.** `tools/link_reach.py` was
run and measured - `the linker asked for: 318`, `referenced by a REACHABLE object: 316`,
`referenced only by UNREACHABLE objects: 2` - but it rewrites `docs/research/boot_path_undefined.txt`,
`boot_path_reachable.tsv` and `boot_path_stubbable.tsv` with a 54/66-line diff that also drops
symbols this item never touched (`AllocateRenderer`, `CARAMManager::*`, `CGraphics::*`,
`CCallStack::*`, all closed by earlier items - i.e. those two files were already stale before
this one), so the writes were reverted rather than folded into an item about `LdrToEntityInfo`.
They still list both symbols (`boot_path_undefined.txt:195-196`), so re-running
`tools/gen_link_stubs.py --reachable` here puts `reachstub_187` and `reachstub_188` back, which is
what the retirement comment in `PortReachStubs.cpp` says.

## The empty destructor is retail's, and what it must free is four members (2026-09-28, goal item `port-modeldata-dtor`)

**The item's `reason` named the trap - "a destructor which does nothing is a plausible lie - record
what the real one must free" - and the record is the argument that the empty body is right.**
Retail `__dt__10CModelDataFv`, `0x800E6810`, `0xF0` = 240 bytes (the next symbol is `fn_800E6900`,
so 0xF0 is retail's own size), and `tools/dis.sh 0x800E6810 0xF0` spends every instruction on a
member: four guarded blocks in reverse declaration order, then the deleting tail.

| retail | member | what the call frees |
| --- | --- | --- |
| `lbz r0,72(r30)` … `li r4,0; bl __dt__6CTokenFv` @`0x800E6830` | `x3c_infraModel` (+0x3C) | `optional_object` tests `m_valid`, destroys `TLockedToken<CModel>` → `CToken::~CToken()`, the unlock + `RemoveRef` on the `CObjectReference` |
| `lbz r0,56(r30)` … `bl __dt__6CTokenFv` @`0x800E6864` | `x2c_xrayModel` (+0x2C) | same |
| `lbz r0,40(r30)` … `bl __dt__6CTokenFv` @`0x800E6890` | `x1c_normalModel` (+0x1C) | same |
| `lbz r0,12(r30)`; `lwz r3,16(r30)`; `li r4,1`; `bl fn_8002C340` @`0x800E68BC` | `xc_animData` (+0x0C) | `auto_ptr` tests `x0_has` and `delete`s → **`fn_8002C340` is `CAnimData::~CAnimData`**, identified member by member from its own call sites (the ladder at the bottom of `include/MetroidPrime/CAnimData.hpp`) |
| `extsh. r0,r31`; `ble`; `bl Free__7CMemoryFPCv` @`0x800E68D4` | `this`, deleting flag | `operator delete` → `CMemory::Free` (`CMemory.hpp:46`) |

No flag is written back after any of the four calls, and `x0_scale`, `x14_flags` and
`x18_ambientColor` have trivial destructors - so there is no statement in those 240 bytes that a
body would have produced. An empty body over these members *is* that code, which is why
`src/MetroidPrime/CModelDataDtor.cpp` is `{}` and not a stand-in. The lie to avoid was the other
one: `CModelData.hpp` only forward-declares `CAnimData`, and `delete` on an incomplete type still
compiles while dropping the destructor call, so the file includes `MetroidPrime/CAnimData.hpp` to
make retail's `bl` at `0x800E68D0` come out of the port.

**What that costs, measured rather than assumed.** The include means the port's undefined list
gains `CAnimData::~CAnimData()` - nothing in the tree defines it; `CAnimData.hpp:43` is the only
declaration - in exchange for losing `CModelData::~CModelData()`. `./tools/link_check.sh` ->
`compile errors 0`, `unique undefined symbols 318`, `duplicate definitions 0`, and the undefined
list diff against the branch head is **exactly those two lines**, so the count holds:
`port undefined 318 -> 318`. `--strict` -> `STRICT PASS - ... 318 undefined against a baseline of
322 (no growth)`. The dependency is retail's own, so it is queued rather than papered over: a
`NEW: port-animdata-dtor | port | CAnimData::~CModelData()` line is in
`build/goal/notes/port-modeldata-dtor.md`, with the ladder of what its 0x2F8 bytes must free.

**One reach stub had to come out with it, and one had to go in - both for the same tool.**
`reachstub_92` aliased `_ZN10CModelDataD1Ev`; `PortReachStubs.cpp` is linked only under
`-DMP_BOOT_STUBS=ON`, which only `tools/boot_probe.sh` passes, so the real definition and the stub
would collide there while `gate.sh`'s `port link dups` step - which links without the option -
reports `duplicate definitions 0` either way. Retired with a `RETIRED 2026-09-28` comment.

**And `./tools/boot_probe.sh` then failed, which is the finding worth the lines.** The first link
named 5 unresolved symbols; the script's own pass stubs the ones that are legal C identifiers
(four `fn_`/`lbl_` names) and prints `not declarable as C identifiers (left for a human):
CAnimData::~CAnimData()` for the fifth - ld prints the *demangled* name and a C++ destructor is not
an identifier - so the relink could not fix it: `relink status 1`, `undefined reference to
'CAnimData::~CAnimData()'` from `rstl/auto_ptr.hpp:21`, `BUILD FAILED`. **A link that does not
finish is the one failure mode where the probe reports no symbol at all**, so it had to be closed
rather than noted: `reachstub_318` is hand-added for `_ZN9CAnimDataD1Ev`, with why in its own
comment. Re-run after that - `link named 4 unresolved symbol(s), 4 not stubbed`, `relink status 0`,
`linked 93182792 bytes`, `the log names 0 unresolved symbols`, **zero** `multiple definition of`
lines (the retirement, measured), then `boot: step 21 - the frame loop`, `frame: 1`,
`frame loop stopped: fn_801F05D0(lbl_80418EC8) (retail 0x801F05D0, 0xF8) is not written - frame
1`: the same wall as the branch's last measured boot, so this item did not move it. The header's
own recount went with it - measured with its own grep, **294 stubs** - 240 Itanium, 3
`REL_Load*`, 51 unmangled: 293 after the retirement, 294 once the callee's stub went in, the same
total with a different Itanium symbol in it.

**Measured, not recalled**: `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS
port-modeldata-dtor`, nine `ok`, exit 0 - `GATE PASS 1fa2358+6 changed`, `matched 3980 -> 3980
linked 2557 -> 2557`, `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465 functions)`,
`2 path(s) changed under src/ or include/`, `CModelData::~CModelData() was undefined at the branch
head and is not now`, `port undefined 318 -> 318`, `probe: `657` files, 0 failed, 0 errors; link:
LINKED (318 undefined, 0 duplicates)`; `python3 tools/link_gap.py --write-list` `wrote 315 entries
in 3 groups` with a two-line diff (`- _ZN10CModelDataD1Ev`, `+ _ZN9CAnimDataD1Ev`) and the recheck
`ok: 315 MISSING symbol(s), all accounted for` - both symbols are `other game methods`, 170 in and
170 out, so `port_link_gap.md`'s group table did not move; `check_files_cmake.py` `651 sources`,
`0 on-disk sources are in no manifest at all (dead)`; `check_symbol_names.py` `checked 322 units; 0
declared names are missing`; `check_decl_order.py` `ok: 841 unit(s) checked`;
`check_raw_offsets.py` `ok: 108 raw-offset site(s)`; `sha1sum build/G2ME01/main.dol`
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

**Adding a source moved the probe's file count 656 -> 657**, so the six *current-state* quotes of
it in `docs/HANDOFF.md` and this file were bumped under `check_docs_claims.py`'s rule (it failed
first, on its own `stale:` line, naming `656` against the probe's `657`), and the two historical
session-end quotes were re-spelled as `` `656` files ``, the convention this file already records for
`` `655` files `` and `` `654` files ``.

**No `configure.py` claim, and the research files were not regenerated.** There is no unit for
this function: no split covers `0x800E6810`, so claiming it would be a carve of a range that
belongs to nobody, and the DOL build never sees this file. `docs/research/boot_path_undefined.txt`
(line 100) and `boot_path_reachable.tsv` (line 94) still list the symbol for the same reason the
`LdrToEntityInfo` pass left its two: they are `tools/link_reach.py`'s output, and regenerating
them rewrites three research files with a diff that also drops symbols this item never touched.
Re-running `tools/gen_link_stubs.py --reachable` over them therefore puts `reachstub_92` back,
which is what its retirement comment says.

## `IterateSearch` is a two-instruction FP-allocation wall, and Metroid Prime 1 has it too (2026-09-28, goal item `match-cpvsvisoctree`)

**Not promoted; the unit stays `NonMatching` at 99.921875 % (2 functions, 1 matched), and this is
the second run on it.** `IterateSearch` (492 B, `99.9187 %`) is instruction-for-instruction retail
except at unit offset `0x78` / `0x80`: retail emits `fadds f1,f1,f8` / `stfs f1,16(r1)`, we emit
`fadds f0,f1,f8` / `stfs f0,16(r1)`. `GetNumChildren` is 100 %. The first run proved the bound by
flipping the unit to `Matching` and byte-diffing the DOL: **two differing bytes, both that register
field, nothing else in any section.**

**What this run added (the first run's ~40 source variants, 20 compilers and 26 flag sets were not
repeated):** 26 new source variants, ~45 new flag configurations, a version x flag-set matrix done
with each compiler's own flags, 3 line-number perturbations and 1 build-wrapper experiment. Every
one of them scores 2 differing instructions or worse. Two negatives worth keeping: **dead locals do
not perturb MWCC's allocator** (five different unused locals, all still 2 - they are dropped before
the deciding pass), and **line numbers do not matter either**, so `#line`/whitespace nudges are not
a lever. Anything that restates the `center` computation while keeping the 32-byte frame scores far
worse (24-130), which closes the one search the first note left open.

**The version matrix now pairs each compiler with the flags this tree actually uses for it**, which
the first run did not: `configure.py` builds the Dolphin libs with `GC/1.2.5n` and the RELs with
`GC/1.3.2`, so "1.0-1.2.5 fail on flags, and no unit uses them" was wrong on its second half. With
`cflags_dolphin` those five compile - and produce **157 instructions**, a different code-generation
generation, not a near miss. `1.3 1.3.2 1.3.2r 2.0 2.0p1 2.5 2.6 2.7` x `cflags_retro` all give
exactly 2; `cflags_dolphin` gives 24 and `cflags_rel` 118, so `cflags_retro` is the best of the
three and the tie-break has not moved across eight compiler versions.

**Independent confirmation, and the reason this is filed as a wall rather than unfinished work:**
PrimeDecomp's Metroid Prime 1 tree hit the same place - PR `PrimeDecomp/prime#383`, *"Implement
octree search with two floating-point register differences remaining"*, reports
`IterateSearch 1.14 % -> 99.92 %` and `.sdata2 0.00 % -> 66.67 %` on GM8E01_00/01 and GM8P01_00,
their `configure.py` still has `Object(NonMatching, "Kyoto/PVS/CPVSVisOctree.cpp")`, and their
`CPVSVisOctree.cpp`/`CPVSVisOctree.hpp` are byte-identical to ours (fetched 2026-09-28). Two
projects, two games, the same source, the same two floating-point registers.

Full measurements, the per-variant table and the exact commands are in
`build/goal/notes/match-cpvsvisoctree.md`. `NEW:` lines: none - nothing outside this item was
found broken.

## The frame loop's DMA cleanup is written, so its stop moved one callee along (2026-09-28, goal item `port-boot-cmain-rsmain-0eb92a1`)

**What was written:** retail's `fn_8030172C` (0x8030172C, 0x20) - the call `CMain::RsMain`'s frame
loop makes at 0x800060A8, and the DOL's only caller of it - plus the `fn_8030174C` (0x74) it wraps.
`./tools/dis.sh 0x8030172C 0x20` is `stwu r1,-16; mflr r0; stw r0,20(r1); bl 8030174c; lwz/mtlr/
addi/blr`: no argument of its own, so both are `extern "C"` with no parameters.
`./tools/dis.sh 0x8030174C 0x74` is the walk of `lbl_804175B8` (0x18-byte .bss = one
`rstl::list`, `sActiveDMAs` in the port) - for each node, `r3 = *(node+8)` is the request and
`lbz r0,36(r3)` its `+0x24` byte; if it is set, `CMemory::Free(r3)` frees the request and
`fn_8030215C(&list, node)` relinks, `x14_count--`, frees the node and hands back the next. That
second call is `rstl::list::do_erase` (include/rstl/list.hpp:284, read against retail's own
instructions), so the port writes it as `delete *it; it = sActiveDMAs->erase(it);` - free before
unlink, which is also what `IsDMACompleted` and `WaitForDMACompletion` in the same file do.

Both bodies went into `src/Kyoto/CARAMManagerPort.cpp`, a port-only file, and
`src/MetroidPrime/PortBoot.cpp`'s stop became `fn_8030172C();`. **There is no carve:** retail
0x8030172C..0x8030184C is still unclaimed in `config/G2ME01/splits.txt`, so nothing was added to
`configure.py` or `splits.txt`, and `files.cmake` already listed the file.

**The one host line, and why it is not a deviation from retail:** the pass calls `ARQPoll()` first.
On the cube the `+0x24` byte is written by the ARQ interrupt, which needs nobody's help; on the
host Aurora does the copy at post time and *defers* that callback to `ARQPoll`
(`CARAMManagerPort.cpp`'s own header), so a pass that did not poll would sweep a list whose
completion bytes are never written and could never free anything. `IsDMACompleted`, `CancelDMA`
and `WaitForDMACompletion` in that file each poll first for the same reason, and `ARQInit` has run
by then: `fn_80301CC4`, which `rs_new`s the list the pass starts by testing, is called after it in
`PortInitializeSubsystems`. `CARAMManager::WaitForAllDMAsToComplete` - retail's `fn_8030184C` -
now *calls* the pass instead of carrying a second copy of the walk, which is the relation its own
comment already described: one poll and one sweep per iteration, exactly the sequence it had.

**Measured:** `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS
port-boot-cmain-rsmain-0eb92a1`, `GATE PASS 0eb92a1+2 changed`, `matched 3980 -> 3980 linked 2557
-> 2557`, `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465 functions)`, `port undefined
318 -> 318`, `probe: 658 translation units, 0 failed, 0 errors; link: LINKED (318 undefined, 0 duplicates)` (that run's own count; the source of truth is `./tools/probe_sources.sh`),
`verify boot-progress.sh: BOOT_PROGRESS PASS: all 2 runs got further than all 2 head runs`. The
undefined-symbol list is line-for-line the baseline's - the two new bodies add definitions, and the
one call they introduce is satisfied by the other one. The boot now stops at the loop's next
declared stop, `fn_80006954` (0x58, called at 0x80006114 and 0x80006234), at
`src/MetroidPrime/PortBoot.cpp:410` where the head stopped at `:398`; DOL sha1 unchanged at
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `check_symbol_names.py` 0 missing.

**Next wall:** `fn_80006954(&total, &x18_frameTimeHistory)`, retail 0x80006954, 0x58, declared
twice (0x80006114 and 0x80006234). Its body is measured in `docs/research/boot_path.md` row 10 -
a valid flag at `total+4`, and `fn_80008B60(h->v, h->count)`'s unrolled `fadds` sum over the
history at `CMain`+0x18/+0x2C stored at `total+0` - and while `fn_80006954` itself has no body in
this tree, `fn_80008B60` (0x80008B60, 0xC8) does not either. **Superseded 2026-09-28: that
accumulator's return is the **mean**, `sum * (1.0f / count)`, not the bare sum** - both are written
now, in `src/MetroidPrime/PortFrameTimeHistory.c`.

## `CSfxManager::TranslateSFXID` reads retail's table and answers with it, 0xFFFF (2026-09-28, goal item `port-translatesfxid`)

**The symbol is `fn_8029C79C`, 0x4C bytes, and it was found by its size and its neighbour rather
than by name.** `./tools/dis.sh 0x8029C79C 0x4C` is a load of `-25852(r13)`, a null test, a
`lwz 0x4(r4)` count, a `cmpw` against the zero-extended id, a `lwz 0xC(r4)` buffer, a
`lhax` halfword load and a `clrlwi 16` - and `fn_8029c7e8`, the 0x150-byte function the previous
item established is `CSfxManager::LoadTranslationTable`, starts at exactly 0x8029C79C + 0x4C.
MP1's `TranslateSFXID` is also 0x4C, and `../MetroidPrimePort/src/Kyoto/Audio/
CSfxManager.cpp:716` is statement for statement the body written below.

**The table is retail's own, at retail's own address, and the pointer is retail's field.** `-25852`
is `lbl_80419884` in `.sbss` (`python3 tools/sda.py -25852` -> `0x80419884 lbl_80419884 (in .sbss,
+0x0)`), and the function reads it as `count = *(int*)(p+4)`, `items = *(short**)(p+12)` -
**this tree's `rstl::vector` layout** (`x4_count` at +4, `xc_items` at +12,
`include/rstl/vector.hpp:18-21`), so the declaration is `rstl::vector< short >*` and not a
shape-compatible guess. `fn_8029C7E8` drops it before every load (`li r4,1; bl fn_80255C00` at
`+0x40`, `stw r0,-25852(r13)` at `+0x4C`), and that statement is now in the port's
`fn_8029c7e8` too, so the pointer the reader tests has the lifecycle retail gives it.

**What changed for the boot is the failure mode, and it is the dangerous kind.** The reach stub
returned 0 in `r3`, and 0 is a *valid* runtime sound id, so every sound the game asked for by
per-area id would have become a real-looking wrong sound. The body returns
`kInternalInvalidSfxId` (0xFFFF), which is **retail's own answer for a missing table** - the
first two statements of the body - so the port now answers the same thing retail does on a disc
where `LoadTranslationTable` was never reached.

**No table is built, and that is stated rather than faked.** The bytes are the
`sound_lookup_ATBL` resource in `Strings.pak`, and `Strings.pak` is **not on the ISO** -
`docs/HANDOFF.md` records the measurement (20 `.pak`s, none named that, so retail's own
`CDvdFile::FileExists` probe at 0x800071A8 fails as well). The pool keeps its token over a null
object (`src/Kyoto/CSimplePoolPort.cpp`), and `fn_8029AB80`, the 0x68-byte `ATBL` factory
(`li r3,0x10` / `__nw__FUlPCcPCc` / a `rstl::vector< short >` off the stream), is still
`return CFactoryFnReturn()` in `src/Kyoto/CFactoryFunctionsPort.cpp` because there is no stream
to hand it. **Writing a mapping here would be fabricating the game's sound table, and a
plausible-looking fabricated id is the failure mode `PortPoolStandIns.cpp` calls the most
dangerous possible wrong answer**, so the vector stays null.

**Files.** `include/Kyoto/Audio/CSfxManagerPort.hpp` is new and port-only - the one accessor the
loader needs, so the loader in `src/Kyoto/CSimplePoolPort.cpp` and the reader in
`src/MetroidPrime/PortAudio.cpp` reach one object. It is a separate header because
`include/Kyoto/Audio/CSfxManager.hpp` is included by `Kyoto/CSimplePoolCtor.cpp`, a `Matching`
unit, and adding a member there would be a change to a matching object. `src/MetroidPrime/
PortAudio.cpp` gained the `TranslateSFXID` body, `port::sfx::ClearTranslationTable()` and the
`kInternalInvalidSfxId` definition (0xFFFF, derived in `src/MetroidPrime/PortGlobals.cpp`'s
comment on `kMedPriority` from `.sdata2` 0x8041E2E6); it was put there rather than in a file of
its own because the file is already the port's audio bodies and `files.cmake` already lists it, so
**no manifest moved**. `src/Kyoto/CSimplePoolPort.cpp` gained one call and the paragraph that said
the reader was still undefined. `reachstub_148` came out of `src/MetroidPrime/PortReachStubs.cpp`
- the deletion `tools/boot_probe.sh`'s own duplicate-definition branch prescribes - and the file's
breakdown was recounted with its own grep: **293 stubs** (239 Itanium, 3 `REL_Load*`, 51
unmangled), 294 before. `docs/research/port_link_gap_list.md` was regenerated with
`tools/link_gap.py --rebuild --write-list` (314 entries in 3 groups, `other game methods`
**170 -> 169**) and `port_link_gap.md`'s table row moved with it.

**Measured, not recalled:** `./tools/probe_sources.sh` -> `probe: 658 translation units, 0 failed,
0 errors;
link: LINKED (317 undefined, 0 duplicates)` (that run's own count; the source of truth is
`./tools/probe_sources.sh`); `./tools/link_check.sh` -> `compile errors 0`,
`unique undefined symbols 317`, `duplicate definitions 0`, and the target is absent from
`build-port-link/link_undefined.txt` where the judge's recorded base had it - that base was
**318** (`build/goal/judge/undef.base.count`, the driver's, not the agent's) and the tree now
measures **317**, with no symbol added to the gap; `./tools/decomp_build.sh` -> `All: 8.52%
fuzzy, 7.54% matched, 5.32% linked (3980 / 28465 functions)`, unchanged; `sha1sum
build/G2ME01/main.dol` `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `python3
tools/check_symbol_names.py` -> `checked 322 units; 0 declared names are missing from their
object`; `check_decl_order.py` -> `841 unit(s) checked, 18 permuted, all 18 accounted for`;
`check_files_cmake.py` -> every configured DOL object is either listed or excluded;
`check_boot_stubs.py` ok; `python3 tools/check_docs_claims.py` -> `docs claims agree with the
tree`. **`docs/HANDOFF.md`'s state block moved with it**: it said `port link 322 undefined`, which
was already stale (the tree measured 318 before this change), and it now says **317** and says
that `docs/research/port_link_baseline.txt` is the recorded *floor* at 322 rather than a mirror -
`link_check.sh --strict` fails only on growth, so a tree below 322 is a win. That file is the
judge's and is not touched.

**What this does not do.** Nothing calls the new body yet: `CActor::ProcessSoundEvent` is reached
only once an actor is spawned, and `CMain::FillInAssetIDs` - the one caller of the loader - is
still off the host boot ladder (`PortBoot.cpp` runs steps 12 and 17-20 and stops). The boot path
is therefore unchanged and no `verify` script was needed for this item. What would make the table
real is the loader's missing half, not this function: `AddPaksAndFactories` (boot step 13) and
`AsyncIdlePakLoading` filling `CResLoader`'s lists, plus a `Strings.pak` on the disc, plus
`fn_8029AB80` written to build the vector off the resource stream.

## The frame-time pair is written, so the loop's stop moved on to `fn_80049244` (2026-09-28, goal item `port-boot-frame0-fn80049244`)

The head at `23075fb` stopped on the first of two `PORT_FRAME_STOP`s, at
`src/MetroidPrime/PortBoot.cpp:410`: `fn_80006954(&total, &x18_frameTimeHistory)`. Both are written
now, in **`src/MetroidPrime/PortFrameTimeHistory.c`** - a `.c` file in `files.cmake`, port-only,
claiming nothing in the DOL - and both stops are replaced by retail's call on retail's line.

**Why `files.cmake` and not a carve.** Neither address is in an unclaimed gap, and they are not in
the *same* claim: `config/G2ME01/splits.txt` gives 0x80006954 to `MetroidPrime/main.cpp`
(`.text` 0x800053B8-0x80006B38) and 0x80008B60 to `MetroidPrime/mainTail.cpp`
(`.text` 0x80008680-0x80009880), which the objects confirm -
`build/binutils/powerpc-eabi-nm build/G2ME01/obj/MetroidPrime/mainTail.o | grep 80008B60` ->
`000004e0 T fn_80008B60`, and `main.o` carries it as `U`. A carve here is therefore **two** claim
cuts, each in another lane's file (`mainsplit` for the first, mainTail's own cut for the second),
so this is the `PortGlobals.cpp`/`PortModuleManager.cpp` shape instead: host link only, and delete
it when a cut lands. `src/` **or** `include/` is satisfied by `PortBoot.cpp` and by the new file.

**Measured against retail, per function, not as a percentage** (flags of `tools/probe_cc.sh` plus
`-lang=c`, the language the `.c` rule gets):

    fn_80008B60  0x80008B60  0xC8  retail 50 insn / 200 B   ours 50 / 200
                 7 lines differ, every one a relocation: 5 branch displacements and the two
                 constant loads `lfd f1,-32664(r2)` / `lfs f2,-32740(r2)`
    fn_80006954  0x80006954  0x58  retail 22 insn /  88 B   ours 22 /  88
                 4 lines differ: the `bl` displacement (a relocation) and `stfs/li/stb` against
                 retail's `li/stb/stfs` - the flag store after the value store, which is why
                 `out->value = ...` is written before `out->valid = 1`

**`fn_80008B60` is a mean and the constant is not `200.0`.** The tail reads
`xoris r3,r4,32768 ; lis r0,17200 ; lfd f0,8(r1) ; lfd f1,-32664(r2) ; fsubs f0,f0,f1 ;
fdivs f0,f2,f0 ; fmuls f1,f3,f0`, and `0x8041A428` is `43300000 80000000` =
**2^52 + 2^31 = 4503601774854144.0** (`objdump -s -j .sdata2 build/G2ME01/main.elf`), not an
int-to-double encoding of a small integer: the pair is mwcc's integer-to-double bias, and the
`fsubs` cancels it and leaves exactly `count`. So the body is `sum * (1.0f / count)` - 50
instructions in 200 bytes, byte-exact bar relocations. **Written as
`sum * (1.0f / ((float)count - 200.0f))` it is 58 instructions in 232 bytes**, because mwcceppc
then emits *two* subtractions of 200 (one against the double, one against the float). **So what this
supersedes is the "sum" reading, not a "200.0" one:** the passages that call `CMain`+0x40/+0x44 each
history's *sum* are `:3237` (the layout table row), `:3241` ("sums, not a running minimum"), `:4117`,
`docs/research/boot_path.md:140` and `docs/HANDOFF.md:2679`/`:2699` - each annotated in place where
it stands. `_SDA2_BASE_` is 0x804223C0 (`tools/sda.py`), confirmed twice: `-32740` ->
0x8041A3DC = `3F800000` = 1.0f, and in `fn_800597D8` `-31336` -> 0x8041A958 = `3F4CCCCD` = 0.8f.
The sum is a **pointer walk**; the same sum as an index loop is 58 instructions in 232 bytes.

**The store, and the one place this is not retail's:** `CMain::RsMain`'s 8-byte local is
`{ 0.0f, 0 }`-initialised here. `fn_80006954` returns early on `count == 0` without writing +0,
so retail stores an uninitialised word to `CMain`+0x40; that path is unreachable from the loop
(`fn_800069AC` at 0x80006108 runs first and raises the count, and `CMain` is placement-new'd into
`mainTail.cpp`'s `static uchar sMainSpace[]`, so the count starts at 0 and is >= 1 by then), and
reading an uninitialised local is undefined behaviour a host compiler may act on.

**Measured, not recalled, on the final tree:**

    $ ./tools/goal_check.sh build/goal/item.json
    goal_check: PASS port-boot-frame0-fn80049244
      ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
      ok    counts: matched 3980 -> 3980   linked 2557 -> 2557
      ok    check_symbol_names.py
      ok    All:  8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465 functions)
      ok    2 path(s) changed under src/ or include/
      ok    verify boot-progress.sh: BOOT_PROGRESS PASS: all 2 runs got further than all 2 head runs
      ok    port undefined 317 -> 317
      ok    probe: probe: (then 659 source) files, 0 failed, 0 errors; link: LINKED (317 undefined, 0 duplicates)

The judge passes by the documented declared-stop rule (`boot_progress.py`: the head stopped on a
`PORT_FRAME_STOP` and the candidate's stack goes through that rewritten line into a deeper frame).
The new stop is the first frame's draw, SIGSEGV, `fn_80049244` at `Carve80049244.cpp:143`,
called from `CMain::RsMain` at `PortBoot.cpp:459` (the marker line is spelled out in
`build/goal/notes/port-boot-frame0-fn80049244.md`, not here: `boot-progress.sh` fails a diff that
writes one). **The port probe's file count moved 658 -> 659** with the new
`.c`, so every "N files" claim in these docs moved with it; the four historical transcripts keep
their own figure, reworded so they no longer read as the current count.

**The `fn_80049244` fault is not this item and is not fixed.** Characterised in
`build/goal/notes/port-boot-frame0-fn80049244.md`: the draw list holds four IOWins and the two
whose constructors are not written (`CConsoleOutputWindow`, `CAudioStateWin`) carry garbage
vtable pointers, so the walk dispatches through them. Adding the two `configure.py` units to
`files.cmake` was measured and **rejected**: it takes the port's undefined count 317 -> 327, and
it would not fix the fault anyway, because both bodies store a **retail PowerPC vtable address**
(`lbl_803B37F0`, `lbl_803B3950`) as the object's vptr, which no host process can call.

## A loader can be 100% and still not link: `LoadTimeKeyframe` is `Matching` (2026-09-28, goal item `match-cunknown90`)

**`MetroidPrime/ScriptObjects/CUnknown90.cpp` is `Matching` at 100%** (320/320 `.text`,
8/8 `.rodata`), confirmed by `tools/flip_test.sh MetroidPrime/ScriptObjects/CUnknown90.cpp`:
DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs cmp-equal, and the `All:` line
went 8098 -> 8099 / 28465 functions. Two independent defects were standing behind the 99.75%
the previous run reported, and the first one is the one to generalise.

**1. The register wall was one redundant conversion, and the "twenty spellings" were the wrong
axis.** mwcceppc gives the `operator new` result the register the hoisted `0x44335aff` case
constant occupied (r29, dead by then); retail's is r28, the property count's. One extra
`u16`-typed node on the `input.ReadUint16()` read moves it: `static_cast< u16 >`, `+x`,
`x & 0xffff` and `u16(u16(x))` all reproduce retail byte for byte. What does *not*, measured:
the same node typed `u32`/`uint`/`long`; the same node on `propertySize` or `propertyId`; a
cast on the loop bound rather than the read; 192 combinations of the other spellings; and 26
flag sets (`-O4,s`, `-O3`, three `-inline` settings, `-fp_contract off`, `cats on`,
`common off`, two `inline_max_size` values, `peephole`/`schedule`/`unroll`/`extbug` off).
So the lever is *one same-typed node on this one read*, and no flag is involved. The
mechanism generalises - `LoadRelay` picks the FourCC register exactly as retail does, so
"the `new` result takes the hoisted constant's register" is MW's rule and retail's
`LoadTimeKeyframe` is the one that breaks it.

**2. At 100% bytes the unit still did not link, and this is the part worth carrying.** The
object referenced `__ct__16SLdrTimeKeyframeFv` / `__dt__16SLdrTimeKeyframeFv`; retail
references `__ct__20SLdrEditorPropertiesFv` / `__dt__20SLdrEditorPropertiesFv`. Both are `bl`
sites, and until the link resolves them both are the placeholder word `48 00 00 01`, so
**objdiff reported 100.00% and `tools/unit_fit.sh` reported "no extra functions" while the
link failed** with two `undefined:` lines. `include/MetroidPrime/ScriptLoader/SLdrTimeKeyframe.hpp`
now declares the pair under `#ifdef TARGET_PC`, the same treatment `SLdrRelay` already had,
and `SLdrStructMembers.cpp` still defines them for the port.

That is `docs/PROCESS_LESSONS.md`'s "three green checks agreeing on a broken change" in a new
costume, and the only gate that caught it was the one the project already insisted on. The
general form: **for a unit that contains a call, the relocation target is part of its
correctness and no byte comparison can see it.** Any `SLdr*` aggregate a `Matching` loader
instantiates on the stack needs the same check, and `LoadTypedefSLdrRelay` /
`LoadTypedefSLdrAreaAttributes` are the next candidates. What remains from `unit_fit.sh` is
the 84-byte weak COMDAT `__dt__16SLdrTimeKeyframeFv` the aggregate's scope exit now emits; the
flip confirms mwldeppc drops it.

## mwcceppc hoists inline-helper arguments in reverse call order (2026-09-28, goal item `match-cvectorelement`)

`Kyoto/Particles/CVectorElement` was **91 / 92** functions with one at 99.90%:
`CVEKEYF::GetValue(int, CVector3f&) const`, 476 bytes. The diff was **two instructions swapped and
nothing else** - no register-allocation difference, no operand difference:

```
retail  lwz r4,0x14(r30)   ; mLoopStart      ours (before): lwz r6,0x10(r30)  ; mLoopEnd
        lwz r6,0x10(r30)   ; mLoopEnd                 lwz r4,0x14(r30)  ; mLoopStart
```

Both callers inline the same
`static inline int GetKeyframeIndex(int frame, bool loop, int loopStart, int loopEnd)`, and
mwcceppc **emits the hoisted member loads in the reverse of the order the arguments are written at
the call**. So swapping the last two *written* arguments swaps the two loads and changes nothing
else - the same values still reach the same parameters, because the parameter *names* follow their
positions, not their spelling order at the call.

**Retail is not consistent between the two paths in the same TU.** Measured off retail's `.text`,
the asymmetry holds in all four particle element TUs:

| unit | `*KEYF::GetValue` hoists | `*KeyframeEmitter::GetValue` hoists |
|---|---|---|
| `CIntElement` | `0x14` (start), `0x10` (end) | `0x10`, `0x14` |
| `CRealElement` | `0x14`, `0x10` | `0x10`, `0x14` |
| `CColorElement` | `0x14`, `0x10` | `0x10`, `0x14` |
| `CVectorElement` | `0x14`, `0x10` | `0x10`, `0x14` |

The KEYF path wants the range **end-first**, so no single argument order serves both callers and
each TU needs its own spelling at one call site. The fix is a 9-line wrapper that takes the range
end-first and forwards to the original helper, used only by the KEYF path:

```cpp
static inline int GetKeyframeIndexEndFirst(int frame, bool loop, int loopEnd, int loopStart) {
  return GetKeyframeIndex(frame, loop, loopStart, loopEnd);
}
```

**`CRealElement.cpp` took the identical one-line change and is `Matching` 151 / 151** (goal item
`match-crealelement`, 2026-09-28, `flip_test` PASS) - same wrapper, same call-site transposition,
nothing else, measured the same way: `bytescmp` on `GetValue__7CREKEYFCFiRf` went from 2 real
differing instructions to 0, and retail's own `.text` has the same asymmetry
(`0x802f08b4 lwz r4,20(r30)` start-first in KEYF against `0x802f0bc8 lwz r4,16(r3)` end-first in
the emitter). **`CIntElement.cpp` wants exactly the same change and is still queued.
`CColorElement.cpp` did not take it**, and the reason is measured, not guessed: its
`CCEKEYF::GetValue` (0x802cf66c, 392 B) is 96.408% with **28 differing instructions of 98**, and
only two of those are the hoist pair. The other 26 are a **one-slot register shift across the
whole inlined `GetKeyframeIndex` expansion** - retail puts the index in `r6`
(`and r6,r3,r0` / `cmpw r6,r5` / `subf r6,r4,r6` / `divw r0,r6,r3`) where ours has `r5` - plus the
`CColor::Lerp` argument block built in the opposite order (ours `slwi r4,r5,2` then `add r4,r6,r4`
then `slwi r0,r0,2`; retail `slwi r0,r0,2` then `slwi r5,r5,2` then `add r5,r6,r0`). **Retail calls
`CColor::Lerp` out of line here too** (`bl 48050f05`, and ours carries the matching undefined
`Lerp__6CColorFRC6CColorRC6CColorf`), so this is *not* an inlining difference. Note also that this
TU hoists the pair into `r4`/`r5` rather than `r4`/`r6` as `CRealElement` does, which is why the
wrapper does not reach it: the register assignment itself has to move first.
**Superseded 2026-09-29 - it landed anyway, and the fix is in the next section.**

### Ruled out here (each changed the register allocation or made it worse)

- spelling the index computation inline instead of calling the helper - the loads stop being hoisted
  at all, 25 differing instructions;
- hoisting `mLoopStart` / `mLoopEnd` into `const int` locals **read from the members directly**,
  in either definition order (25) - ruled out for `CVectorElement` and `CRealElement`, and it
  still fails here: it is the *accessor* call that makes mwcceppc allocate the index last.
  With the accessors it is the fix, and the section below has the measurement;
- reordering the helper body so `loopStart` is mentioned first;
- splitting `GetKeyframeTime(...)` into its own statement;
- turning `bool lerp` into the `if` it stands for - that **loses** the `clrlwi.` / `li r3,1` pair the
  bool materialises, so the bool form is required.

### Three things to stop re-reading

1. **`tools/compare_unit.sh` prints `.text: DIFFERS` on this unit and that is not a verdict.** Its
   compare is raw: ours is `.text 0x3a58` against a retail-derived `0x3694`, and the 11 extra
   functions are the `__dt__` weak COMDAT copies and the `rstl::vector` instantiations that
   `unit_fit.sh` already explains (964 bytes). Only `flip_test.sh` decides, and the flip passed.
2. **The state block's two moving numbers move by different amounts and that is correct.**
   `matched 8640 -> 8641` is **+1** (objdiff's *matched* count; the one function that was
   fuzzy-only became matched), `linked 3497 -> 3589` is **+92** (the whole unit's function count,
   because it is now `Matching`), and `DOL units 7976 -> 7977` is **+1**. Run
   `python3 tools/check_docs_claims.py` after the flip: it prints the exact replacement string for
   each stale figure, so there is no guessing which number belongs in which line.
3. **A landed flip reads as no change until the report is regenerated.** `flip_test.sh` edits
   `configure.py` and relinks, but the `build/report.json` it leaves behind was produced by the
   *pre-flip* build: `complete` stayed `False` for the unit, `complete_units` did not move, and
   `report_diff.py` printed `0 units newly linked` - for a change that had just linked 151
   functions. Re-run `./tools/decomp_build.sh` before quoting any `complete` / `complete_units`
   figure. The reverse trap is in the same family: the config key is `hash:`, not `sha1:`, so a
   REL check written against `sha1` matches nothing and passes having verified nothing.

## The last particle element needs the locals too (2026-09-29, goal item `match-ccolorelement`)

`Kyoto/Particles/CColorElement` was **44 / 45** functions at 100% with `CCEKEYF::GetValue`
(0x802CF66C, 392 B) at **96.408%** - 28 differing instructions of 98. It landed on **two small
changes and nothing else**; `flip_test` PASS, `main.dol` bit-identical, all 86 RELs byte-equal,
45 / 45 at 100.00%, `matched` 9188 -> 9189, `linked` 4014 -> 4059.

**The register shift is not a separate problem from the hoist order - it is the hoist order
again, and the fix is to read both members through accessors into `const int` locals, in
source order.** The sibling TUs read `mLoopStart`/`mLoopEnd` directly and only wanted the call
arguments transposed. Here, reading them directly gives `r4 = start`, `r6 = end`, `r5 = index`;
retail wants `r4 = start`, `r5 = end`, `r6 = index`. Measured, the seven spellings tried at that
one call site:

| spelling of the range at the `*KEYF` call site | differing instrs |
| --- | --- |
| `mLoopStart, mLoopEnd` (baseline) | 28 |
| `GetLoopStart(), GetLoopEnd()` (the sibling fix) | 27 |
| `GetKeyframeIndexEndFirst(..., mLoopEnd, mLoopStart)` (the 9-line wrapper) | 27 |
| helper signature transposed to `(loopEnd, loopStart)` | 29 |
| `const int ls = mLoopStart; const int le = mLoopEnd;` | 25 |
| `const int le = GetLoopEnd(); const int ls = GetLoopStart();` | 18 |
| **`const int ls = GetLoopStart(); const int le = GetLoopEnd();`** | **10** |

Read the shape: with the locals, mwcceppc allocates the index **last**, so it lands in the
highest free register (`r6`) and the two hoisted loads take `r4`/`r5` in **source order**. Both
halves of retail's requirement - start hoisted first *and* end in `r5` - come out of the one
spelling, which is why none of the transpositions reach it. The accessors are the same two the
other three element headers already carry (`CRealElement.hpp`, `CVectorElement.hpp`,
`CIntElement.hpp`), so this is the house spelling, not a new invention. **`const int` matters:
the same two locals declared plain `int` score 26, and using the accessors *inline* at the call
instead of binding them scores 27.**

**The remaining five instructions are the `CColor::Lerp` argument block, and one more local
fixes it.** Retail scales `idx + 1` and adds the base **before** it scales `idx`:

```
retail  addi r0,r6,1 ; lwz r4,44(r30) ; slwi r0,r0,2 ; add r5,r4,r0 ; slwi r0,r6,2 ; add r4,r4,r0
ours    addi r0,r6,1 ; lwz r5,44(r30) ; slwi r4,r6,2 ; add r4,r5,r4 ; slwi r0,r0,2 ; add r5,r5,r0
```

i.e. retail evaluates the **second** argument's address before the first's. Binding only the
second one to a reference flips it, and the two instructions that move are the only ones that do:

```cpp
const CColor& b = mKeys[idx + 1];
valOut = CColor::Lerp(mKeys[idx], b, t);        // 5 differing instructions, all relocations
```

The same source line written straight as `CColor::Lerp(mKeys[idx], mKeys[idx + 1], t)` is what
`CCEKeyframeEmitter::GetValue` in this same TU uses, **and that function is already 100%** -
retail's two `GetValue`s disagree with each other here too, in the same way it disagrees between
the `*KEYF` and `*KeyframeEmitter` paths of the hoist pair. Do not rewrite the emitter's line to
match; it is already correct.

**After both changes `tools/bytescmp.py` reports 5 differing instructions of 98, and all five
are relocation fields** (three `lfs`/`lfd` against `R_PPC_EMB_SDA21` float constants and the
`bl` against `Lerp__6CColorFRC6CColorRC6CColorf`), which objdiff ignores - objdiff reports the
function at 100%.

## Retail's out-of-line copy ctor decides the translation unit, and a pair of `bool : 1` is one byte (2026-09-28, goal item `progress-cstatemanager-clightcopy`)

`__ct__6CLightFRC6CLight` (0x80038C9C, 0xA4 bytes) is retail's out-of-line `CLight` copy
constructor, and it is in **`CStateManager.o`** - not in `CLight.o`, which is where every other
`CLight` member lives and where the obvious place to write it is. `CStateManager.cpp` calls it from
`fn_80038C5C`, which returns a `{u16, CLight}` aggregate by value and copy-constructs the light
into the return slot. So it was written in `src/MetroidPrime/CStateManager.cpp` and it is
**100.00%, 41 instructions byte-identical to retail's**. That also closed the port's
`_ZN6CLightC1ERKS_`: `main/MetroidPrime/CStateManager.cpp` is in `files.cmake`, so a function
written for the DOL's sake handed the port a symbol for free. **Port undefined 314 -> 313**
(`CLight.cpp` was never the answer, and the flip's remaining undefined list is now three symbols
down from the four the baseline tree fails on).

**The function is 0xA4 bytes, not the 0x4D the item guessed, and the interesting part is one
byte.** Retail copies 0x00..0x4C: six `lfs/stfs` pairs for the two vectors, `lwz/stw` for
`CColor`'s packed word and for `mType`, eight more float pairs, `lwz/stw` for the two ids, two
float pairs, and then **one `lbz/stb` at 0x4C**. A memberwise initialiser list of the two trailing
`mutable bool : 1` members does *not* produce that: mwcceppc read-modify-writes each bit in turn
(`lbz; lbz; rlwimi; stb; lbz; lbz; rlwimi; stb`) and the function sits at **85.24%**, 188 bytes,
8 instructions too many. **The two flags are one byte, so they are one object.**

| spelling of the two flags | result |
| --- | --- |
| two `bool : 1` members in the initialiser list (baseline) | 85.24% |
| the same two, assigned in the body instead | 85.24% |
| the two declarators in one declaration, `bool a : 1, b : 1;` | 85.24% |
| declared radius before intensity (reversed) | 85.24% |
| `*this = other` | 24.05% |
| one nested `SDirtyFlags` member, `mDirty(other.mDirty)` | **100.00%** |

The nested struct keeps every observable fact: still one byte at 0x4C, still `mIntensityDirty` at
bit 7 and `mRadiusDirty` at bit 6, and `SetSpotCutoff` - which read-modify-writes the two bits
separately in retail and in ours - stays at 100%, as does `main/Kyoto/Graphics/CLight` at 19/19.
`mutable` on the struct member is what keeps `GetIntensity() const` and `GetRadius() const` able to
clear the flags: mutability propagates into a mutable member's subobjects. The rejected alternative
was a `reinterpret_cast<SDirtyFlags*>(reinterpret_cast<char*>(this) + 0x4c)` in the body, which also
reaches 100% and which `check_raw_offsets.py` rightly refuses - a modelled member is exactly what a
raw offset is not for.

**The generalisable rule, and it is about the object rather than the copy.** mwcceppc copies adjacent
`bool : 1` bitfields one bit at a time but moves a one-byte aggregate whole, so a class whose last
member is a *pair* of bit flags models them as a one-byte struct if anything ever copies the class
out of line. The same rule in reverse: `SetSpotCutoff` proves the bits are still separate
read-modify-writes, so do not "fix" them into one field - the struct is right *and* the bitfields
are right, and both are in the tree at once.

`main/MetroidPrime/CStateManager` is **69 -> 70 / 239** and stays `NonMatching`; `matched 8681 ->
8682`, `linked 3740 -> 3740`, DOL sha1 and all 86 RELs unchanged, `report_diff.py` reports
`+1 functions at 100%` and no regression anywhere.

## `mutable` on the members is what stops a copy constructor's tail from being pipelined (2026-09-29, goal item `match-cdeferredparticleeffect`)

`Kyoto/Particles/CDeferredParticleEffect` was 17 of 18 at 100.00%, the one holdout being
`__ct__21CDependencyGroupTokenFRC21CDependencyGroupToken` (0x8033ECCC, 0x58 = 88 bytes) at
**90.68%**. It landed on **two words in a header** and nothing else; the unit is `Matching`,
`flip_test.sh` PASS, `main.dol` bit-identical, all 86 RELs byte-equal, 18 / 18 at 100.00%,
`matched` 9325 -> 9326, `linked` 4608 -> 4626, `complete_units` 699 -> 700.

**The whole difference is the register allocator, and the diff says so.** Retail loads the word
into `r0` and stores it before loading the byte; ours loads the byte first, into `r0`, and keeps
the word in `r4` until the store:

```
retail  lwz r0,24(r31) ; mr r3,r30 ; stw r0,24(r30) ; lbz r0,28(r31) ; stb r0,28(r30)
ours    lwz r4,24(r31) ; mr r3,r30 ; lbz r0,28(r31) ; stw r4,24(r30) ; stb r0,28(r30)
```

Five instructions, two temporaries, one scheduling decision: `-O4,p`'s pipeliner sees two
independent load/store pairs and software-pipelines them, which costs the second load a live
range and so forces it into a second register. **`mutable` on both trailing members stops it**,
because a mutable subobject is reachable through a `const` path, so mwcceppc can no longer
prove that the store to `this` cannot disturb the load from `other` and the two pairs stay in
source order. It changes no layout, no mangled name, no observable behaviour - the class has no
`const` member function that writes either member - and the copy constructor's five instructions
then come out byte-identical to retail's.

**The negative half matters as much as the lever, because ten shapes of the class do not move
it.** Measured with a standalone `mwcceppc` probe at the unit's own flags: `bool : 1` vs a plain
`bool` vs `uchar` vs `u8 : 8` vs `bool : 2` vs three `bool : 1` in one byte, `uint` vs `int` vs
`u32` for the word, a one-byte named struct for the flag (the `bool : 1` rule above), a
`#pragma pack(1)` five-byte nested struct, a third trailing byte, and the pair as a private base
class - **every one emits the same interleaved five instructions**, so none of them is worth
trying again. Neither is the optimisation level, and the reason is the point: **`-O4,p` and
everything below it each move one half of the diff and not the other.**

| knob | tail order | `mr r3,r30` |
|---|---|---|
| `-O4,p` (the unit's flags) / `#pragma scheduling on` | pipelined | middle - **retail's** |
| `-O3,p`, `-O3`, `-O2,p`, `-O2`, `-O1,p`, `#pragma scheduling off` | source order | **last** - not retail's |
| `#pragma optimization_level 1/2/3`, `#pragma global_optimizer off` | pipelined | middle |
| `+ mutable` on both trailing members | source order | **middle** - retail's, 100.00% |

So the pipeliner and the `mr` hoisting are driven by the same scheduler and cannot be separated
with a flag; `mutable` is the only lever found that separates them, because it removes the
licence to pipeline rather than the licence to schedule. Start here for any remaining
"4-byte word then 1-byte byte" copy-constructor tail at 90-99%: the discriminator is whether
retail's `lbz` sits **after** the `stw`. When it does, the copy is not a five-byte block copy
(a packed five-byte struct pipelines too), and `mutable` on the two members is the fix.

## A local is allocated in the scope that declares it, and a `const&` to a 2-byte member is one load (2026-09-29, goal item `progress-cstatemanager-dtor-members`)

`main/MetroidPrime/CStateManager` is **70 -> 73 / 239** and stays `NonMatching`; `matched 8817 ->
8820`, `linked 3875 -> 3875`, DOL sha1 `6ef9b491...` and all 86 RELs unchanged, `report_diff.py`
reports `+3 functions at 100%` and no regression anywhere. **Every figure in this section was
measured in this run**; each is one ~1.5 s incremental `./tools/decomp_build.sh
main/MetroidPrime/CStateManager` away from being re-measurable, and the disassembly quoted is
`build/binutils/powerpc-eabi-objdump -dr --section=.text` on the two objects named beside it.

### Read the right object, or a change that moved the score 15 points looks like a null result

`build/G2ME01/obj/<unit>.o` is the **retail** base objdiff compares against - 129108 bytes for
this unit. **Ours** is `build/G2ME01/src/<unit>.o`, 26752 bytes. Disassembling `obj/` while asking
what *we* emit answers the retail question instead: stashing the change and diffing the two
`obj/` objects gave a byte-identical pair (same sha1) across an edit that took
`fn_8003BF84` from 85.50% to 100.00%, which reads as "the compiler ignored the edit" and is enough
to burn a session. `grep '<unit>' build.ninja` settles it in one command. (`build.ninja:696` is the
compile edge, `:21798` the objdiff base, `:25776` the link input.)

| function | before | after | the difference |
| --- | --- | --- | --- |
| `fn_8003C3A8` | 78.70% | **100.00%** | `return TIdListResult(a, b)` directly, not a named `const TIdListResult` that is then copied out of |
| `fn_8003BF84` | 85.50% | **100.00%** | `GraveyardBucket fresh;` declared **inside each `if` body**, not at function scope |
| `TouchPlayerActor` | 85.48% | **100.00%** | `const TUniqueId& head = m_playerActorHead;` used for **both** the test and the call |

**Rule 1 - a local lives in the scope that declares it, and MWCC's frame was the whole
percentage.** `fn_8003BF84`'s two objects already agreed one-for-one over 44 instructions; only
where the zero was spilled differed. One local at function scope is one object in one slot,
reused by both branches:

```
ours before  stwu r1,-160(r1)   stw r0,0x8(r1)   addi r4,r1,0x8   (and the same slot again)
ours after   stwu r1,-288(r1)   stw r0,0x8c(r1)  ... stw r0,0x8(r1)  (one each)
retail       stwu r1,-288(r1)   stw r0,140(r1)   ... stw r0,8(r1)    (one each)
```

0xa0 -> 0x120 is the whole 85.50% -> 100.00%. The comment in the tree had asserted the opposite
(that retail shared one slot); it was wrong and is corrected in place.

**Rule 2 - a `const&` to a 2-byte member is the same load, and re-reading it is not.**
Retail's compare and its argument are one `lhz r4,9298(r3)` (0x2452) / `cmplw r4,r0` / `beq` /
`sth r4,0x8(r1)`. Ours emitted a second `lhz r0,9298(r31)` for the call's argument. Binding the
member by reference once measures 100.00%; the second read measures 85.48%.

**Rule 3 - a copy out of a copy hoists all its words before it stores any.** `fn_8003C3A8` as a
named local is four loads then four stores (`lwz r3.. ; lwz r4.. ; lwz r5.. ; lwz r0.. ; stw.. ;
stw.. ; stw.. ; stw..`); retail and the direct return both interleave (`lwz r0,0x10(r1) ;
stw r0,0(r29) ; ...`). Swapping the two constructor arguments is *also* 78.70% - measured by an
earlier run of this item and not re-measured here - which is how you know the score is about the
copy and not the argument order, so the source order stays as it is.

### The item's premise, re-measured: the 12 destructor members are real and are not a slice

`python3 .tmp/opencode/dtor.py obj` (written by an earlier run of this pair; `obj` = retail,
`src` = ours, which is the right way round) lists, in order, every release site. It re-runs to the
same numbers here: **retail 407 lines, 37 sites; ours 111 lines, 11 sites.**

```
0x2904 0x24E4 0x1E98 0x16F4 0x16D8 0x16C8 0x16B8 0x169C 0x1694 0x168C 0x1684 0x167C
0x1658 0x1650 0x163C 0x1620 0x1608 0x08D4 0x08C0 0x0808 ...
```

**`dtor.py` undercounts, and its blind spot is where the item's twelfth member is.** It only pairs
an `addi`/`addic` with a following `bl`, so a release site that goes through a *vtable* is
invisible to it - there is at least one in this destructor, 0x1604. Read the raw disassembly
before believing any count of release sites:

```
c928:  addic.  r0,r28,5636          # 0x1604
c92c:  beq     c950
c930:  lwz     r3,0x1604(r28)
c934:  cmplwi  r3,0
c938:  beq     c950
c93c:  lwz     r12,0(r3)            # vtable
c940:  li      r4,1                 # deleting flag
c944:  lwz     r12,8(r12)           # slot 1 = the deleting destructor
c948:  mtctr   r12
c94c:  bctrl
```

That confirms the item's 0x1604 and pins its shape: **retail's `m_world` is an owning pointer to a
polymorphic object, not the raw `CWorld*` our header declares** (`CStateManager.hpp:318`) - it is
null-checked and then destroyed with the deleting flag, which a raw pointer member never is.

The item's offsets are otherwise all in that list and none in ours, so the item is **right about
the count**. Two further corrections, both measured here against the retail object:

- **`0x1694` is a thirteenth.** Retail releases four 8-byte slots (0x167C/0x1684/0x168C/0x1694)
  where our header has **three** `rc_ptr`s (`CStateManager.hpp:332-334`) and then
  `CWorldLayerState* m_currentWorldLayerState` at 0x1694. So the header is *missing* a member
  here, not only mistyping the ones it has - and because the release order is descending, whatever
  fills 0x1694 has to be declared after the three that are there. The four retail callees are
  `fn_80009008` / `ReleaseData__Q24rstl23rc_ptr<13CMapWorldInfo>Fv` / `fn_800095E4` /
  `fn_80009224`; **only the second is an `rc_ptr`**, and all four use the **double-`addic.`
  no-flag pattern** (the address is tested twice, no `li r4` before the `bl`), which is
  `rstl::ncrc_ptr`'s destructor. Three distinct out-of-line symbols, so three distinct `T`s - the
  same conclusion an earlier run reached from the ctor's four
  `ncrc_ptr<CScriptMailbox|CMapWorldInfo|CPlayerState|CWorldTransManager>` parameters. The
  header's "Four rc_ptrs" comment (`CStateManager.hpp:329`) is wrong: it declares three.
- **`0x1650`/`0x1658` are the first two words of `pad2_2`, not of `m_scriptIdMap`.** `TIdList` is
  `rstl::map<TEditorId, TUniqueId>` and `rstl::map` is 0x14 bytes (`CHECK_SIZEOF(unk_map,0x14)`,
  `include/rstl/map.hpp:34`), so `m_scriptIdMap` is 0x163C..0x1650 and `pad2_2[0x2C]` is
  0x1650..0x167C - exactly where our three `rc_ptr`s start, consistent with our own destructor
  releasing 0x163C and then 0x167C/0x1684/0x168C with nothing between. The `CToken` releases are
  real: `__dt__6CTokenFv` with `li r4,0` at `c900` for 0x1650.

**Why this run did not land any of it.** Seven of the twelve (0x24E4, 0x1E98, 0x16F4, 0x16C8,
0x16B8, 0x1658, 0x8D4) are released by an unnamed retail function through a `li r4,-1` deleting
call, so each slot needs an **invented class** whose destructor is a one-line wrapper around a
symbol we neither have nor can name - the "plausible stand-in" the goal prompt forbids. And it
buys **zero** matched functions: `__dt__13CStateManagerFv` measures **23.37%** on its own
hand-written body, so repairing the tail of a function whose body is wrong cannot move
`report.json`. The offsets only pay off in the same change as the body.

### Three walls, each measured here rather than asserted

- **Five functions sit at 96.06% / 64 B, and all five are one `lwzu`.** Four are named
  `fn_800379xx` - `fn_80037904`, `fn_80037944`, `fn_80037984`, `fn_800379C4` - and the fifth is
  `fn_80037A04`, which is the same shape and the same score. `report.json` lists all five and
  `objdump -t` on **our** object shows five consecutive 0x40-byte symbols; all five bodies are the
  same `if (list.size() == 20) return false; list.push_back(id); return true;` over a different
  `IdList13xxx()`. Retail keeps `r6 = r3+0x10000` and re-displaces every access
  (`lwz r0,16088(r6)` / `sth r5,16092(r4)` / `lwz r4,16088(r6)` / `stw r0,16088(r6)`); ours folds
  the address in once (`lwzu r0,16088(r6)`) and uses `0x0`/`0x4` after. The one spelling that
  *changes* the addressing - no local at all, calling `IdList13ED8().size()` and
  `.push_back(id)` separately - reaches retail's split form and measures **61.69%** (re-measured
  here), because the compiler then re-derives the address per use, reloads the count and puts the
  split base in r3 not r6. The addressing form and the CSE of the count are two independent asks
  and no spelling tried so far gives both.
- **The two `SendScriptMsg` (99.52 / 99.58) differ only in two dead stores.** The frame
  (`stwu r1,-48(r1)`), the call and the live object all agree; the spill block before the argument
  object is ten instructions in both, and the first two are transposed:

  ```
  retail  sth r7,0x8(r1)   sth r8,0xc(r1)   sth r5,0x10 ... (8 more, byte-identical)
  ours    sth r8,0x8(r1)   sth r7,0xc(r1)   sth r5,0x10 ... (8 more, byte-identical)
  ```

  Retail puts the 4th parameter `other` in the first dead slot where we put `dest`.
  **`DeleteObjectRequest` is the control and it is 100.00%**: it makes the same kind of dead stores
  and matches, which is what says these two are unreachable rather than a missing object.
- **`DeferStateTransition` (98.18%), `fn_80037784` (96.67%) and `AllocateUniqueId` (83.31%) are
  on the `@stringBase0` wall,** and the objects say why exactly. The retail object has **no
  `.rodata` section at all** and 54 sites across 14 functions referencing `lbl_803A64F0`; ours has
  a **0x19-byte `.rodata`** holding only `"Object list full!"` and **3** sites, in exactly those
  three functions, referencing the linker-synthesised `@stringBase0`. The `__FILE__` string's
  position is a property of the **merged** rodata of the whole link, so no per-unit change reaches
  it. `AllocateUniqueId` has one reachable defect on top of the wall: retail calls
  `__vc__Q24rstl38bit_vector<Q24rstl17rmemory_allocator>Fi` (= `rstl::bit_vector<rstl::rmemory_
  allocator>::operator[](int)`) twice, we call `fn_80041518(queryOutput&, MapWorldInfoAreas&,
  ushort)` twice - and the two call sites are instruction-for-instruction identical
  (`addi r3,r1,<hidden return> ; addi r4,r29,2240 ; mr r5,r31 ; bl`), so it is a **symbol rename**,
  not a missing function. The string offset survives the rename, so it scores nothing alone.

`python3 tools/check_decl_order.py --unit main/MetroidPrime/CStateManager` still reports 80+
violations. That is pre-existing and irrelevant here - the unit is `NonMatching` and the rule only
bites on a flip.

## 2026-09-29: an unmangled symbol is a free function, and the port's undefined count is a gate

**`fn_8003B21C` (0x8003B21C, 32 bytes) 0.00% -> 100.00%**; `main/MetroidPrime/CStateManager`
**73 -> 74 / 239**, `matched 8820 -> 8821`, DOL sha1 `6ef9b491...` and all 86 RELs unchanged,
port undefined 313, **`goal_check.sh` PASS**. The item asked for the four 8-byte slots at
0x167C-0x1694 to be retyped from `rc_ptr` to `ncrc_ptr`; that is still unlanded (see below) and
it moves no offset either way, so this run took the reachable function instead.

### The lesson: `nm` tells you member or free function, and objdiff will not tell you

Retail's body is seven instructions with no frame and no calls:

```
lis r4,31 ; li r0,0 ; addi r4,r4,-31616 ; stw r4,0x24dc(r3)
stw r0,0x15f8(r3) ; stw r0,0x15fc(r3) ; stw r0,0x1600(r3) ; blr
```

Written as a member (`mgr->mCurrentRenderPlayerIndex = 2000000; mCurrentRenderPlayer = nullptr;
m_playerState = nullptr; m_cameraManager = nullptr;` - all four members already exist at those
offsets, `mCurrentRenderPlayerIndex` is the one at 0x24dc) it compiles to **byte-identical
instructions** and objdiff still reports **0.00%**, because the symbol comes out as
`fn_8003B21C__13CStateManagerFv` while retail's is unmangled `fn_8003B21C`. objdiff pairs by
name, so a perfect body scores zero. **`nm -n build/G2ME01/obj/.../<unit>.o | awk '$2=="T"'`
and look for a name with no `__`: that is the free-function list.** This unit already uses the
form - `fn_8003AD74`, `fn_800388EC`, `fn_80039B1C` at the top of `CStateManager.cpp` are all
`extern "C"` for this reason.

### The lesson: adding a forwarder can fail the item, and the gate says so

`fn_80043180` / `fn_800434CC` / `fn_80043688` (0xCF80/0xD2CC/0xD488) and `fn_800391B4` (0x2FB4)
are bare one-`bl` forwarders and **all four measured 100.00%** as written. Adding them took
`matched` to 8825 - and `tools/probe_sources.sh` reported

```
link_check: STRICT FAIL - regression gate: 317 undefined against a baseline of 314 (GREW)
NEW  fn_800391E4   NEW  fn_800431A0   NEW  fn_800434EC   NEW  fn_800436A8
```

because each forwards to a callee the unit does not define. `probe_sources.sh` gates the port's
undefined count against `docs/research/port_link_baseline.txt` and a `progress` item's judge
runs the full gate, so four matched functions were worth **less than none**. Reverted; the
finding is recorded in the source as a comment at the point of use. **A forwarder is a function
whose callee you must also write** - a decomp item that adds calls has to add definitions, or
the count goes the wrong way.

## A string literal's pool slot is set by *where its first user is declared*, and that can be moved (2026-09-29, goal item `progress-cgamestate-fn-80143e88`)

`fn_80143E88` (0x80143E88, 0x238) reaches 100.00% in `src/MetroidPrime/Player/CGameState.cpp`;
the unit goes 70 -> 71 / 116 and stays `NonMatching`. Three things had to be right, and only the
first is in the previous notes for this item.

### 1. A file-scope `static` array's literals are emitted where it is *declared*, not where it is used

`CGameState.cpp` had `static rstl::pair< const char*, uint > sGameModeLayers[]` at the **top** of
the file. That put its three literals at pool `+0x07`, `+0x11`, `+0x19` - so retail's
`"InitialWorld"` at `+0x07` had nowhere to go, and the function could not pass no matter how it
was written. **Moving the table down the file, to just before `ConfigureGameModeLayers` (its only
user), moves its literals with it**: `"Samus01"`/`"Coins"` land at `+0x40`/`+0x48` and
`fn_80143E88` becomes retail's first user of a string literal, exactly as retail has it.

Retail confirms the target, not just the guess. `__sinit_CGameState_cpp` (0x80146874) builds the
table at runtime and its three `addi` immediates are `+42`, `+440`, `+448` against
`lbl_803A9208` - `+42` is the shared `"Deathmatch"`, and the other two are the last two literals
in a 0x1C8-byte pool. That is only reachable if `fn_80143E88` (which the descending declaration
order puts above `ConfigureGameModeLayers`) is the unit's first literal user.

Measured, after the move, in `build/G2ME01/main.elf`: `"InitialWorld"` 0x803A920F, `"FrontEnd"`
0x803A921C, `"Results"` 0x803A9225, `"Coin"` 0x803A922D, `"Deathmatch"` 0x803A9232, `"%s%s%d"`
0x803A923D - retail's `+0x07/+0x14/+0x1D/+0x25/+0x2A/+0x35` byte for byte. **The general rule:
a `static` with an initializer emits its literals at its declaration site, so on a unit where a
literal's offset is part of an instruction, the declaration's position in the file is part of
the match.**

### 2. `lbl_803A91C8` is real retail data below the unit's own pool, so the 64-byte copy takes a name

The local's sixteen `lwz`/`stw` pairs read `lis r4,0x803B ; addi r9,r4,-28216` = 0x803A91C8, the
0x40 zero bytes immediately *below* `lbl_803A9208`. `config/G2ME01/splits.txt` claims this unit's
`.rodata` from 0x803A9208, so that object is retail's and has to be referenced, not created:
`extern "C" const char lbl_803A91C8[];` plus
`SGameStateName name = *reinterpret_cast< const SGameStateName* >(lbl_803A91C8);` gives the
sixteen pairs. `memcpy` is one instruction; a plain `char[0x40]` local does not get them.

### 3. Register numbering: the three `const char* const` are declared *before* the member reads

At 99.61% the whole function was byte-identical except that `r4` and `r8` were swapped - retail
holds the pool base in `r4` and `mGameMode` in `r8`, and the source above holds them the other
way round. Moving the `kResults`/`kCoin`/`kDeathmatch` block **above** the two member reads puts
the pool base in `r4`. This is the same fact as point 1 seen from the allocator's side: the
value whose address is computed first gets the low register.

### The things the earlier notes for this item got right, re-confirmed

- `+0x1F4` (`fn_800068F4`'s argument) is the matching build's `mAudioGroups`, not
  `PreviousGameResults()`. One `#ifdef TARGET_PC`-guarded `AudioGroups()` and one `#else` twin in
  `CGameState.hpp`, both returning `void*` - `fn_800068F4` is retail code this port has no type
  for, and `reinterpret_cast` from a vector reference is rejected by mwcceppc ("illegal type
  cast"), so the accessor has to return the pointer itself.
- `'DTHM'` / `'COIN'`: `addis r3,r8,0xBBAC ; cmplwi r3,0x484D` is a full-word compare against
  `0x4454484D`, because `0xBBAC == -0x4454` in a 16-bit immediate.
- The pool order *within* the function: `Results` `+0x1D`, `Coin` `+0x25`, `Deathmatch` `+0x2A`
  named as `const char* const` in that order, with `"%s%s%d"` created last by the first `sprintf`
  at `+0x35`.
- Reading `mGameMode` and `mPlayerCount` before the copy fixes the frame. **Correcting point 2 of
  the previous run's notes: hoisting all three - `mShowResults` included - is also wrong**, and
  costs 99.61% -> nothing. Retail reads `mShowResults` at 0x80144050, after the sixteen stores.

### Gate

```
./tools/gate.sh build/goal/judge/report.base.json            ->  GATE PASS  9948823+3 changed
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  8821 -> 8822   linked 3875 -> 3875   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CGameState :: fn_80143E88__Fv
per-function sweep over both reports: 0 worse, 0 disappeared, 0 new
sha1sum build/G2ME01/main.dol  ->  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py  ->  0 missing names
python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CGameState  ->  ok
./tools/probe_sources.sh  ->  a 741-file sweep at the time (726 since the 2026-09-29 upstream sync), 0 failed; LINKED (313 undefined, 0 duplicates)
tools/unit_fit.sh MetroidPrime/Player/CGameState.cpp  ->  98 extra functions (unchanged)
```

The unit is still not flippable, for the reason the earlier notes recorded: it emits 98 functions
the retail object does not define. **This change added none of them** - calling
`gpResourceFactory->GetResourceIdByName` instantiated no new weak copy, so the 98 is the same
98 the previous run measured.

## `CGameState`'s pool is now complete, and the eleven option names were what was missing (2026-09-29)

`progress-cgamestate-diagnostic-strings`. The item asked for the two diagnostic strings
`CGameState::CGameState(CBitStreamReader&)` builds and throws away, which our source skipped with a
comment. They are at `lbl_803A9208 + 60` (74 bytes) and `+134` (79 bytes), and adding them as plain
literals put both at exactly the right pool offsets on the first build - **the pool's order is the
file's function-declaration order reversed, with static-data literals last**, so no nudging was
needed:

```
"Cannot find World Asset(%x) to load save data.  Skipping save game info.\n"
"Save game did not contain World Asset(%x).  Creating default world save info.\n"
```

The second one is gated on `mWorldStates.size()` changing across `StateForWorld` (0x8014470C:
`lwz r25,12(r30)` before the call, `cmpw` after), which is what the old comment's "the original
also constructs an unused diagnostic string" had elided. The ctor's own fuzzy went **82.24% ->
84.14%**.

**That alone moved no function to 100%, so the item as written could not pass its own judge**
(`progress` requires the unit's `matched_functions` to rise strictly). It does not need the pool
filled to +426 to be *written* - it needs the pool filled *past* the two messages, and the eleven
option names are the only thing that lives there. So `fn_80145C98` came with them, ported from
`src/MetroidPrime/Player/CPersistentOptionsInit.cpp` (which is still on disk, unbuilt) into
upstream's TU:

- **Its names are literals here, not `lbl_803A9208 + K`.** The carve could not use literals
  because a `Matching` unit may not own `.rodata`; this unit does (the split claims
  `0x803A9208..0x803A93D0`), and a literal is what makes `lis/addi/addi K` come out.
- **Declaration position is load-bearing twice over** - once for `check_decl_order.py` and once for
  the pool. Retail's offset order puts `fn_80145C98` (0x80145C98) between
  `CGameStateEnvVarManager::FindEnvironmentVariable` (0x80145E24) and `::AddVariable` (0x801442CC),
  so the block sits between those two definitions. Declared after `CPersistentOptions::PutTo` it is
  still exactly right for the pool but puts the unit's tail 7 slots out of retail order, and the
  gate's decl-order step is the only thing that reports that.
- `fn_80145ACC` is a declaration only. It is a relocation; the DOL link uses the retail-filled
  object for a `NonMatching` unit, so the missing callee costs nothing and the port does not build
  this file.

`fn_80145C98` is at **100%**, the unit 71 -> 72 of 116, `.rodata` 74 -> 454 bytes and byte-identical
to retail over its whole 456 (`cmp` of the two `objdump -s` dumps differs only in the two trailing
alignment NULs). **`__sinit_CGameState_cpp` is the one that still wants the pool**: it is 63.35%,
and its `addi r9,r10,42` / `+440` / `+448` are already right - what differs is that retail
materialises `'DTHM'`/`'SNGL'`/`'COIN'` as `lis`+`addi` pairs inside `__sinit` (68 bytes ours,
80 retail) while we emit three `lwz` relocations into `.sdata`. That is the next thing on this
unit, and it is a *constant-pool* question, not a source-order one.

```
./tools/gate.sh build/goal/judge/report.base.json            ->  GATE PASS  6e3b568+2 changed
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  8822 -> 8823   linked 3875 -> 3875   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CGameState :: fn_80145C98
  no regression
sha1sum build/G2ME01/main.dol  ->  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py  ->  484 units, 0 missing names
python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CGameState  ->  ok
./tools/probe_sources.sh  ->  a 741-file sweep at the time (726 since the 2026-09-29 upstream sync), 0 failed; LINKED (313 undefined, 0 duplicates)
docs/HANDOFF.md state block updated in the same commit (8822 -> 8823, DOL 8022 -> 8023)
```

The unit is still not flippable and still emits **98 functions the retail object does not define**
(10896 bytes) - unchanged by this edit, since `fn_80145C98` calls nothing that instantiates a new
weak copy.

## A by-value 4-byte class parameter is passed by pointer, and that is visible in the frame (2026-09-29, goal item `progress-cstatemanager-rest`)

`CStateManager` 74 -> 78 / 239, global `matched` 8823 -> 8827, `tools/gate.sh` PASS against
`build/goal/judge/report.base.json`, DOL sha1 and all 86 RELs unchanged, port undefined 313 -> 314
against the 314 in `docs/research/port_link_baseline.txt` (no growth). The unit stays
`NonMatching`; nothing was flipped and `flip_test` was not run.

| function | retail | before | after | how |
|---|---|---|---|---|
| `CStateManager::AreaUnloaded(TAreaId)` | 0x800419C8, 4B | 0.00% | 100.00% | a **rename** of `fn_800419C8`, no new bytes |
| `CStateManager::RayCollideWorld(start, end, filter, damagee)` | 0x800422D4, 220B | 0.00% | 100.00% | the non-const 4-arg overload |
| `CStateManager::AreaLoaded(TAreaId)` | 0x80041A60, 60B | 0.00% | 100.00% | needs `fn_800B89FC` |
| `CStateManager::UpdateActorInSortedLists(CActor*)` | 0x80041B08, 308B | 0.00% | 100.00% | needs `fn_80041CCC` |

Sections 1, 2 and 3 of the two earlier attempts at this item reproduced exactly, including the
`dir *= (1.f / len)` frame-size argument (99.16% with `*`, 100.00% with `*=`) and the
`fn_800419C8` rename. What is new is below.

### 1. mwcceppc passes a by-value 4-byte class type as a pointer, and you can see the copy

`AreaLoaded` declares no local, no frame slot and no argument shuffling: `mr r31,r3 ; mr r5,r31 ;
lwz r3,0x167c(r3) ; bl fn_800B89FC`. Written with `fn_800B89FC(CMapWorldInfo*, TAreaId, CStateManager*)`
it compiled to `stwu r1,-0x20` and carried three instructions that retail does not have -
`lwz r0,0x0(r4) ; addi r4,r1,0x8 ; stw r0,0x8(r1)` - a copy of the area id onto the callee's own
frame before forwarding the address. **Changing the parameter to `const TAreaId&` reproduced retail
instruction for instruction** (`stwu r1,-0x10`). A 4-byte struct by value is not passed in r4; the
caller passes a pointer to a temporary, and the callee may or may not copy it. When a function is a
few instructions short and the extra ones are a load/store pair, look at the parameter list before
the arithmetic.

The same effect, opposite direction, in `UpdateActorInSortedLists`: retail keeps the validity byte
live in **r4** across the whole tail (`cmplwi r4,0` twice, no second `lbz`). Reading it back off
the frame as `bounds.valid` makes mwcceppc reload it; assigning it to a plain local first does not.
A local of the same type costs nothing and changes the register allocation.

### 2. `cmplwi` vs `cmpwi` is a signedness choice, and it is the whole last 2.7%

With the control flow correct the function sat at **97.27%**, and every remaining difference was
`cmplwi r4,0` against `clrlwi. r0,r4,24` - the same test in two encodings. The chain:

| spelling | emitted |
|---|---|
| `const bool valid = bounds.valid;` | `clrlwi. r0,r4,24` (normalise to a bool, then test) |
| `const uchar valid` / `const int valid` | `cmpwi r4,0` (signed) |
| `const uint valid` with the member left `uchar` | `cmplwi r4,0` - **retail's encoding** |

`lbz` already zero-extends, so the member stays a byte and only the local is widened. The four
combinations were all measured; `uint` local over a `uchar` member is the one that matches.
**`cmpwi` and `cmplwi` are the same comparison with different sign extension, and objdiff scores
them as different instructions.**

### 3. `CEntity`'s bitfield order is not what the header reads like - and the probe settles it

Retail's second test is `lbz r0,32(r31) ; rlwinm. r0,r0,25,31,31` on `CEntity+0x20`. The header
declares `m_active:1, m_notInArea:1, m_castFlags:4, m_scriptingBlocked:1, m_entityUnknown:1`, so
`IsScriptingBlocked()` is bit 6 and should be exactly that instruction. It is not: mwcceppc emits
`rlwinm. r0,r0,31,31,31` for it, and `rlwinm. r0,r0,25,31,31` for `GetActive()`. **The two are
swapped relative to the declaration order.** `fn_8003BE54` in this same unit, 100.00%, uses
`rlwinm r6,r0,25,31,31` after `ent->GetActive()` - so the header's *field order* is right and
mwcceppc's layout is what retail emits; the accessor that produces retail's bytes here is
`GetActive()`, not `IsScriptingBlocked()`.

The one-line probe that settles it, for any bitfield question, is to compile each accessor into its
own function and read the rotate immediate:

```c
extern "C" int p_blocked(CEntity* e) { return e->IsScriptingBlocked() ? 0x11 : 0x22; }
```
```
p_blocked:  rlwinm. r0,r0,31,31,31      <- bit 0
p_active:   rlwinm. r0,r0,25,31,31      <- bit 6
```
Distinct return values per accessor, or the compiler folds two of them into one test. Do not
reason about a bitfield's position from the declaration order; measure it.

### 4. Block layout is chosen by the polarity of the `if`, and inverting it is worth 2.7%

The same 97.27% build had the right instructions in the wrong order: retail's `beq` skips over
**Move** and falls through to **Remove**, ours skipped Remove. Written `if (active && valid) Move;
else Remove;` mwcceppc emits Move as the fall-through. Written `if (!active || !valid) Remove; else
Move;` - the same condition, negated - it emits exactly retail's layout, and the function goes to
**100.00%**. Three other rearrangements of the same logic measured 89.16%, 89.22% and 97.27%; only
the negation matched. **When a function is one branch layout away from 100%, negate the `if` before
you try anything else** - the fall-through is the compiler's to choose and the source's `if`
polarity is what it chooses from.

### 5. `UpdateActorInSortedLists` is net zero on the port's undefined count, and the reason generalises

The earlier attempts recorded it as "net 0, therefore still open" and left it unwritten. It is
written now, and the cancellation is worth stating as a rule: **defining a DOL function removes its
symbol from the port's undefined list, so the cost of a new callee is measured against that.**
`UpdateActorInSortedLists` went in (-1) while `fn_80041CCC` went in (+1), and `AreaLoaded` spent
the last slot the tree had. Had either been attempted in the other order the pair would not have
fitted at all. `fn_80041CCC` is declared but not written - 0x194 bytes needing `TCastToPtr<CPhysicsActor>`
(not in the port) plus vtable slot 9 of `CPhysicsActor`.

`SFoundBounds` is `{CVector3f min; CVector3f max; uchar valid;}` with a `Box()` accessor that
reinterprets the first 24 bytes as a `CAABox`. A `CAABox` member does not work: it has no default
constructor, and every spelling that gave it one emitted a real
`bl CAABox::CAABox(CVector3f, CVector3f)` that retail does not have. A `uchar raw[0x19]` buffer with
the copy spelled as a `for` loop was worse still - the loop did not inline and became 25 `lbz`/`stb`
pairs against retail's six `lwz`/`stw`. **The box has to be a real `CAABox` lvalue for the compiler
to emit a word-wise copy, and the flag has to be a separate byte beside it.**

### 6. Still open

- `SetCurrentAreaId` (0x80041728, 168B) - unchanged from section 3 of the first attempt: its four
  callees live in three TUs `files.cmake` does not list, and it duplicates `stub_59`.
- `SetActorAreaId` (0x800383E4, 296B) - `fn_801E4F0C` (+1) and `stub_58` must go together. There is
  no longer a spare slot.
- `UpdateObjectInLists` (0x80042434, 332B) and `PrepareAreaUnload` (0x800419CC, 148B) - +3 each.
- `AddDrawableActor` / `AddDrawableActorPlane` - the `mutable mAddedToken` finding from the previous
  attempt stands and is **not** applied here; it moves 45 functions in five unrelated units.

## The destructor's addresses, measured, and a 16-byte-element copy that is not `vector<float>` (2026-09-29, goal item `progress-cstatemanager-dtor-body`)

`main/MetroidPrime/CStateManager` 78 -> **79 / 239**, `All:` 8827 -> 8828, DOL sha1 and all 86 RELs
unchanged, **no function anywhere worse**. The destructor itself is still unmoved and still 23.37%;
this section is the measurement that decides how it gets written, plus one landed function and
three walls that are now characterised rather than suspected.

### 1. `__dt__13CStateManagerFv` is at 0x8004269C, and the previous note's range was in object space

The earlier notes quote the body as `0xC4B8-0xC7D4`. That is **right in `objdump -dr` output and
wrong as a DOL address**, which is the `left`-is-retail trap for the third time in this unit: the
object's `.text` base is 0, so every offset in that dump is an object offset. The DOL addresses are
object + 0x80036200 (the unit's `splits.txt` start):

| | object offset | DOL address | size |
| --- | --- | --- | --- |
| whole function | `0xC49C` | **0x8004269C** | 1420 bytes, ends 0x80042C28 |
| hand-written body | `0xC4B8` | **0x800426B8** | through `0xCA00` / **0x80042C00** |
| epilogue | `0xCA00` | 0x80042C00 | `extsh. r0,r29 ; ble ; bl fn_80045DC8` |

Calibrate with `build/binutils/powerpc-eabi-objdump -h <obj> | awk '/.text/'` against
`splits.txt`, never by reading the offset column. The 407 instructions, the `0xD0` frame and the
22 member releases in the earlier note are confirmed; so is the tail: it is a
`~CStateManager(int deletingFlag)` that calls `fn_80045DC8` (the unnamed base *deleting*
destructor at 0x80045DC8) only when the flag is non-zero, and our declaration has no parameter for
it. The 22 release sites and the 37 distinct `bl` targets are tabulated in the goal notes.

### 2. A pointer-bounded copy loop is not an indexed one: 94.38% -> the last 2 instructions

`fn_800391E4` (0x800391E4, 96 bytes) is a copy-assign over a counted array of **16-byte** elements,
and `fn_800391B4` (0x800391B4, 48 bytes) is its `return this` forwarder - the fourth of the
forwarders the 2026-09-28 note had to leave out because the callee raised the port's undefined
count. With the callee written, **the forwarder is 100.00%** and the undefined count is unchanged
(the pair cancels: the forwarder asks for `fn_800391E4`, the callee defines it).

The spelling rule, and it generalises to every float-array copy in the tree:

- An **indexed** `for (int i = 0; i < n; ++i) a[i] = b[i];` over a 16-byte element makes mwcceppc
  unroll **4x with a remainder**: `srwi. r0,r4,2 ; mtctr r0 ; <16 lfs/stfs pairs> ; bdnz ;
  andi. r4,r4,3 ; beqlr ; <4 lfs/stfs pairs> ; bdnz`. That is 101 instructions against retail's 24.
- A **pointer-bounded** `while (src != end) { *dst = *src; ++dst; ++src; }` emits retail's loop
  exactly: four `lfs`/`stfs` pairs, `addi r5,r5,16 ; addi r6,r6,16` straddling the last store, and
  `cmplw r5,r7 ; bne`. **`fn_8003ABF0`, 0x7A0 bytes away, is the indexed form of the same copy** -
  the two are the same operation written two ways, and that is why guessing from one of them fails.
- The residue at 94.38% is entirely the prologue: retail builds the end pointer from the *raw*
  source pointer (`add r7,r4,r0 ; addi r7,r7,4`, so `end == (char*)other + count*16 + 4`) and
  allocates the two data pointers as r6 then r5; mwcceppc strength-reduces `other->m_items` into
  r5 first and puts `end` in r0. Fourteen further spellings (declaration order, `const` on the end
  pointer, `for` vs `while` vs `do`-`while`, a `(char*)other + 4 + count*16` end expression, a
  memberwise copy, a counting-down loop, a combined-increment `for`) all produce the same two
  bytes. The `while (src < end)` spelling is not equivalent: it compiles to 0%.

### 3. Three walls in this unit, each with the exact bytes

- **The five `fn_800379xx` / `fn_80037A04` at 96.06% are one `lwzu`.** Retail
  `lwz r0,16088(r6)` against our `lwzu r0,16088(r6)`: mwcceppc folds the address of the
  `reserved_vector` (at container + 0x13EE8) into r6 and reuses it, where retail keeps r6 at
  `container + 0x10000` and reaches the member with a 0x3EE8 displacement. Same addresses, same
  16 instructions, one addressing mode. Twelve more spellings measured here (repeat the accessor
  call, pointer-to-list, a named count, `capacity()`, `20 == size()`, a `size() != 20` inverted
  block, a `const TIdList&` for the test and the accessor for the push, a named `TUniqueId` value,
  a cached container pointer) leave it at 96.06%, 61.69% or 70.44% - the two lower numbers are the
  shapes that lose the early return. **This is worth five functions if it is ever reachable.**
- **The two `SendScriptMsg` at 99.58 / 99.52% are a dead spill, not a spelling.** Both build a
  `CScriptMsg` at `r1+24` / `r1+20` *and* spill three or four `TUniqueId` values into a dead
  parameter save area at `r1+8..r1+20`; every instruction matches except the two `sth` operands in
  that dead area, where retail and mwcceppc disagree about which of `r7`/`r8` is `other` and which
  is the id. Nine spellings of the `CScriptMsg` construction (a named `TUniqueId id`, a named local,
  a `const` local, a cached `CEntity*`, member-by-member assignment, swapped `m_unk`/`m_originator`,
  swapped `m_id`) moved it to 99.42% at
  best. `CScriptMsg`'s own layout is `{TUniqueId @0, @2, @4, int @8, int @12}` - 16 bytes, which the
  five stores at +0/+2/+4/+8/+12 fix.- **`fn_80037784` and `DeferStateTransition` are one string address.** Both call
  `rs_new CSaveGameScreen`, and retail's `operator new` placement args are `lbl_803A64F0 + 147`
  where ours is our own `stringBase0`; everything else, including the `li r4,0` / `li r5,0` that
  the dead setup feeds, is identical. The unit claims no `.rodata` section, so the string pool slot
  is set by the linker and there is nothing in this unit to move. This is the `CMemory::Alloc` /
  `CCallStack` wall the handoff already records, seen from the other end: it is not a source
  problem, it is a section-claim problem.

### 4. What is left in the unit, for the next run

`__ct__13CStateManager` (7.45%, 4536 bytes) and `__dt__13CStateManagerFv` (23.37%, 1420) are the
two large ones and both need the destructor body's member list and its two unwritten callees
(`fn_800417D0`, 168 bytes; `fn_800412EC`, 556 bytes). `ApplyLocalDamage` (40.99%, 1360 bytes) and
`AddDrawableActor` / `AddDrawableActorPlane` (52.10 / 65.00%) are the next-largest uncharacterised
ones; the two `AddDrawableActor` bodies are instruction-for-instruction identical and differ only in
load-chain scheduling, and the `mutable mAddedToken` finding recorded on 2026-09-29 moves 45
functions in five unrelated units, so it is not a cheap fix. Gate: `tools/gate.sh` and
`tools/goal_check.sh` both pass; `report_diff` reports `+1 functions at 100%, no regression`.

## `CMetareeSwarmRel` is the module head, and `>> 7` is a 25-bit rotate (2026-09-29, goal item `progress-rel-head-metareeswarm`)

MetareeSwarm is module 43 and the first of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28 to get any class code. The head is now the whole `.text 0x0..0xD8` - **five** functions,
all 100.00%, `CMetareeSwarmRel.cpp` - and the module's sha1 against `config/G2ME01/config.yml` is
**unchanged** (`e9b5a7bd0c482e1bfecfd104e9d805bf579df554`), with all 86 holding and
`main.dol` still `6ef9b491...`. `matched` 8828 -> 8833, `linked` 3875 -> 3880, the module's own
count 5 -> 10 of 61. `tools/audit_rel_claim.py MetareeSwarm` reports 0 problem claims and 0 of 61
text symbols dropped by `-strip_partial`; `tools/check_decl_order.py --unit
MetareeSwarm/MetroidPrime/ScriptObjects/CMetareeSwarmRel` is ok and `tools/flip_test.sh` on the unit
reports `PASS -> kept as Matching`.

Four things came out of it that are not obvious from the recipe.

### 1. `fn_43_0` is an ordinary flag test, and an earlier reading of it was wrong (superseded)

**This corrects the record left by the 2026-09-29 run of the same item**, which claimed `.text
0x3C..0xD8`, called `fn_43_0` "retail dead code" and said the bit it tests "is always 0". That was
wrong, and it is corrected here and in `src/MetroidPrime/ScriptObjects/CMetareeSwarmRel.cpp` rather
than left in the tree.

`fn_43_0`'s tail is

```
mulli r4,r4,0xb8 ; lwz r3,0x184(r3) ; addi r0,r4,0xb2 ; lbzx r0,3,r0
rlwinm. r0,r0,25,31,31 ; beq .L34 ; li r5,1
```

`54 00 cf ff` is `rlwinm. r0,r0,25,31,31`, and dtk's own asm spells it `extrwi. r0,r0,1,24`. Reading
the dtk name literally - "bit 24 of the register" - and concluding that a `lbzx` zero-extended byte
has nothing there, so the `beq` is always taken and the function always returns 0, is what produced
the dead-code claim. **The measurement that settles it is in-tree and is a `Matching` unit**:
`fn_42_36C` (`CScriptMetaree.cpp:78`) is spelled `(*(uchar*)(self + 0x34c) >> 3) & 1` and its
retail object is

```
00000048 <fn_42_36C>:
  48:  88 03 03 4c   lbz     r0,844(r3)
  4c:  54 03 ef fe   rlwinm  r3,r0,29,31,31
  50:  4e 80 00 20   blr
```

so `mwcceppc` emits **`SH = 32 - shift`, `MB = ME = 31`** for `(byte >> n) & 1`, and `SH = 25` is
`n = 7`: bit 7 of the byte, mask `0x80`. The same word is the `CMain` frame loop's back-edge at
0x8000645C, which `docs/research/boot_path.md:188` already documents as testing `0x80`,
`finished`. So `fn_43_0` is a plain index-guarded flags accessor and costs nothing to write.

**General form, and the reason the mistake was available: dtk's `extrwi`/`ext` names are a
decoding of the *register* bit position, and they are only as trustworthy as the `rlwinm` operands
behind them. Do not reason about a bit out of the mnemonic - reason about the encoding, and look for
a `Matching` sibling in the tree that already spells the same shape.** Three statements in the
earlier attempt were false and are superseded: "bit 24 of the loaded byte is always 0", "the
function returns 0 for every input", and "reproducing it is a bit-field question, not a codegen
one". The claim now starts at 0x0 and the function is `Matching`.

### 2. The registration stores the address of a slot, not a loader

`fn_43_A8` is 0x30 bytes and reads as

```
lis r4, fn_43_D8@ha ; lis r3, lbl_43_bss_20@ha ; addi r0,r4,fn_43_D8@l
stwu r0, lbl_43_bss_20@l(r3)     ; r3 is still &lbl_43_bss_20
bl fn_8022D5A8                   ; so the argument is that address
```

The DOL callee settles it: `fn_8022D5A8` (0x8022D5A8, 8 bytes) is `stw r3, gLoader_MetareeSwarm;
blr`, and `LoadMetareeSwarm` (0x8022D57C) reads the result as
`lwz r6, gLoader_MetareeSwarm; lwz r12, 0(r6); mtctr r12; bctrl`. So the DOL holds a **pointer to a
loader slot**, and the module owns the slot - `lbl_43_bss_20`, `.bss:0x20`, `size:0x4
data:4byte`. `CScriptPlayerProxy.cpp` already had this shape; what is new is that it is
*measurable from the DOL's own two instructions* rather than assumed from the 8-byte `.sbss` size,
and it is why the module's slot is 4 bytes while the DOL's is 8 (`struct SLoaderSlot` in
`src/MetroidPrime/ScriptLoader/MetareeSwarm.cpp`, whose second word is untouched by anything
retail does).

### 3. Three floats 0x10 apart have to be built, not indexed

`fn_43_3C` (0x3C, 0x28 = 10 instructions) copies three floats out of a 0xB8-byte record at
`self+0x184`, at `+0x0C`, `+0x1C` and `+0x2C`. Retail loads **all three before it stores any**:

```
mulli r0,r5,0xb8 ; lwz r4,0x184(r4) ; add r4,r4,r0
lfs f2,0x2c(r4) ; lfs f1,0x1c(r4) ; lfs f0,0xc(r4)
stfs f0,0(r3) ; stfs f1,4(r3) ; stfs f2,8(r3) ; blr
```

Written as three indexed stores it is the **same ten instructions, interleaved** - load, store,
load, store, all through `f0` - and scores **58.30%**. Written as
`*out = CVector3f(values[0x0C/4], values[0x1C/4], values[0x2C/4])` it is 100.00%, because
`CVector3f`'s three-argument constructor is what hoists the three arguments into `f0`/`f1`/`f2`
before the copy. This is the `mwcceppc hoists inline-helper arguments in reverse call order` note
above, and the general form is worth keeping: **when retail loads N values into N registers and
then stores them, the source constructed an N-wide value, and indexing will not reproduce it even
though the instruction count already agrees.**

The 0x10 stride is also a measurement, not an assumption: it rules out a 12-byte `CVector3f` at
+0x0C, so the record has three separate floats there rather than a vector.

### 4. `*(records + index * 0xB8 + 0xB2)` indexes, `records[index * 0xB8 + 0xB2]` does not

`fn_43_0` addresses its flag byte differently from `fn_43_3C` addressing its floats, and the
difference is the C++, not the compiler's mood. Retail:

```
mulli r4,r4,0xb8 ; lwz r3,0x184(r3) ; addi r0,r4,0xb2 ; lbzx r0,3,r0
```

- base in `r3`, the whole byte offset in `r0`, indexed load. **That is the subscript form**
  `records[index * 0xB8 + 0xB2]`, or its spelled-out equivalent - it is 100.00% only when the
  offset is added through a pointer dereference,
  `*(reinterpret_cast<const uchar*>(records + index * 0xB8 + 0xB2))` (measured: the other two
  spellings, `records[offset]` with `const int offset = index * 0xB8 + 0xB2` and
  `records[index * 0xB8 + 0xB2]`, both fold the constant into the displacement and give
  `mulli r0,r4,0xb8 ; add r3,r3,r0 ; lbz r0,0xb2(r3)`, 4 instructions different).
- `fn_43_3C`'s `add r4,r4,r0` + `lfs ...,0x2c(r4)` is the *pointer-arithmetic* form, and the two
  forms appear in the same module, 0x3C apart, over the same array.

**General form: when retail uses `lbzx`/`lwzx` with a base and an offset that is one register
holding the *whole* offset, the source formed the address with a pointer dereference rather than
forming an element pointer. A subscript and a dereference are the same expression to a reader and
not the same expression to `mwcceppc`.** The same split is already visible inside this tree:
`CFlyerSwarm.cpp:11` (the `fn_43_3C` shape) computes `boids + index * 0xB8` as a pointer.

### What is left in the module

51 functions, of which `fn_43_D8` (0xD8, 0x3D8 = 984 bytes) is the entity loader the registration
installs - behavioural class code, and it needs the CActor/CPatterned hierarchy this tree does
not model, exactly as the item expected. It is the same blocker the other 26 modules of 2026-09-28
have. The head is not on the boot path, so nothing here moves the boot.

### The port side, and why the file is not in `files.cmake`

Listing `CMetareeSwarmRel.cpp` in `files.cmake` and registering `mp_relmain_metareeswarm` in
`platform/compiled_modules.cpp` compiles, links and scores - and makes the probe's regression gate
fail: the host body references `fn_43_D8` and `fn_8022D5A8`, neither of which the port can link,
and the undefined count went **314 -> 316** (measured with `tools/link_check.sh --strict`;
`link_check` named both). `fn_43_D8` is 984 bytes of class code and cannot be stubbed honestly.
So the file is left out of the port build, which is a documented state rather than an omission:
`tools/check_files_cmake.py` counts it under "further units are out because they define a module
entry point (RELMain/RELExit), which collides in a flat link". The port keeps reading
`MetareeSwarm.rel` off the disc through `platform/rel.cpp`, which is the correct arrangement for a
module whose code is not in `mp_game` - and **is** the reason `CScriptPlayerProxy.cpp`'s
`fn_62_188` sits in the link-gap list rather than defined. Measured at this commit: the probe
reports `LINKED (314 undefined, 0 duplicates)`, equal to the baseline, with the file absent.

## `CIngPuddleRel` is a module head, and a vtable call needs a class (2026-09-29, goal item `progress-rel-head-ingpuddle`)

IngPuddle is module 32 and the second of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28 to get class code. The head is now the whole `.text 0x0..0xA8` - **five** functions,
all 100.00%, `src/MetroidPrime/ScriptObjects/CIngPuddleRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`312b87acb1dea81e5c03fccd6b87e68366f17a6f`), with all
86 holding and `main.dol` still `6ef9b491...`. `matched` 8833 -> 8838, `linked` 3880 -> 3885, the
module's own count 5 -> 10 of 68. `tools/audit_rel_claim.py IngPuddle` reports 0 problem claims
and 0 of 68 text symbols dropped by `-strip_partial`; `tools/check_decl_order.py --unit
IngPuddle/MetroidPrime/ScriptObjects/CIngPuddleRel` is ok; `tools/unit_fit.sh` reports
`.text claimed 168 ours 168 retail 168 fits` with no extra functions; `tools/flip_test.sh` on the
unit reports `PASS -> kept as Matching`.

The five, from `config/G2ME01/rels/IngPuddle/symbols.txt`:

```
0x00  fn_32_0   0x08   addi r3,r3,0x460 / blr
0x08  fn_32_8   0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr r12 / bctrl
0x34  RELExit   0x24   li r3,0 / bl fn_80229EE0
0x58  RELMain   0x20   bl fn_32_78
0x78  fn_32_78  0x30   lbl_32_bss_0 = fn_32_A8 ; fn_80229EE0(&lbl_32_bss_0)
```

`RELMain`, `RELExit` and `fn_32_78` are the `CScriptPlayerProxy.cpp` / `CMetareeSwarmRel.cpp`
arrangement, unchanged: the registration hands the setter the *address* of a four-byte `.bss` slot
(`fn_80229EE0` is the DOL's `stw r3, 0x80419598; blr`, and `LoadIngPuddle` at 0x80229EB4 reads it as
`lwz r6,slot; lwz r12,0(r6); mtctr r12; bctrl`), and the `.bss` symbol stays `extern` under MWCC
because dtk's `auto_05_00000000_bss.s` is the definition. The file is **not** in `files.cmake`, for
the reason measured on `CMetareeSwarmRel` two lanes earlier: a host body would reference `fn_32_A8`
and `fn_80229EE0`, and `tools/link_check.sh --strict` fails on a growing undefined count.

### The one instruction that mattered: `fn_32_8` has to be a member call

`fn_32_0` and `fn_32_8` are not free functions. dtk lists both in the module's FORCEACTIVE block,
and `build/G2ME01/IngPuddle/asm/auto_04_00000000_data.s` shows `.data:0xD0` - CIngPuddle's vtable -
storing `fn_32_0` at 0x38 and `fn_32_8` at 0x3C. `fn_32_0` is `return this + 0x460`; `fn_32_8` calls
whichever function is in vtable slot 0x38, which for CIngPuddle is `fn_32_0` itself.

The obvious spelling - load the vtable pointer and call through it by hand:

```cpp
void* const* vt = *reinterpret_cast<void* const* const*>(self);
(*reinterpret_cast<void (*)(void*)>(vt[0x38 / sizeof(void*)]))();
```

compiles, links, and scores **99.09%**: `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and one
wrong register in a twelve-instruction function. (`mwcceppc` also refuses the
`reinterpret_cast` outright - "function call does not match prototype" - so the load has to go
through a union or a stand-in class anyway.)

`mwcceppc` only reaches for r12 on its own virtual-dispatch path, so the call has to be written as
a member call. A class gives the slot number for free: MWCC lays a class's virtuals out the way
retail's vtable is laid out - **two leading words** (offset-to-top, then the RTTI pointer, which a
REL leaves null; `build/G2ME01/IngPuddle/asm/auto_04_00000000_data.s` shows `.data:0xD0` and
`.data:0xD4` both holding `0x00000000` before the first virtual) and then one word per virtual. So
**thirteen** virtuals put the last one at 0x38, and `self->Slot12()` gives all twelve instructions
byte for byte. Measured directly rather than reasoned from the pattern: a fifteen-virtual class
compiled to `lwz r12,0x40(r12)`, so the count is `(0x38 - 8) / 4 + 1`.

The stand-in class is named `CIngPuddleVTable`, declares its slots by position, and defines none of
them: no header in this tree models a CActor virtual, and the only object carrying this vtable is
the module's own retail bytes. The neighbouring function `fn_32_0` needs no class at all -
`return static_cast<const char*>(self) + 0x460` is retail's two instructions.

### What is left

`fn_32_A8` (0xA8, 0x1E4) is the module's `SLdrIngPuddle` entity loader, and the 57 functions from
there to `fn_32_3190` are CIngPuddle's methods (`TypesMatch.cpp` gives CIngPuddle the parent
`CPhysicsActor`). None is claimed; dtk fills `0xA8..0x31F4` from retail. That is class code and it
needs the CActor/CPhysicsActor hierarchy, which is the same blocker the item's `reason` names.

## `CPlantScarabSwarmRel` is `CMetareeSwarmRel` with another module number (2026-09-29, goal item `progress-rel-head-plantscarabswarm`)

PlantScarabSwarm is module 49 and the third of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28. The head is now the whole `.text 0x0..0xD8` - **five** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CPlantScarabSwarmRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`67240808f42d66482dfaa11daa09a9994cd63e92`, `cmp`-equal
to `orig/G2ME01/files/RelProd/PlantScarabSwarm.rel`), with all 86 holding and `main.dol` still
`6ef9b491...`. `matched` 8838 -> 8843, `linked` 3885 -> 3890, the module's own count 5 -> 10 of 71.
`tools/audit_rel_claim.py PlantScarabSwarm` reports 0 problem claims and 0 of 71 text symbols
dropped by `-strip_partial`; `tools/check_decl_order.py --unit
PlantScarabSwarm/MetroidPrime/ScriptObjects/CPlantScarabSwarmRel` is ok; `tools/unit_fit.sh` reports
`.text claimed 216 ours 216 retail 216 fits` with no extra functions; `tools/flip_test.sh` on the
unit reports `PASS -> kept as Matching`.

The five, from `config/G2ME01/rels/PlantScarabSwarm/symbols.txt`:

```
0x00  fn_49_0   0x3C   index-guarded flag test on record[index], index * 0xB8
0x3C  fn_49_3C  0x28   record[index].x0C/x1C/x2C -> *out, index * 0xB8
0x64  RELExit   0x24   li r3,0 / bl fn_8022FFF8
0x88  RELMain   0x20   bl fn_49_A8
0xA8  fn_49_A8  0x30   lbl_49_bss_20 = fn_49_D8 ; fn_8022FFF8(&lbl_49_bss_20)
```

### The finding: this head is module 43's head, instruction for instruction

Diffing `build/G2ME01/PlantScarabSwarm/asm/auto_00_00000000_text.s` (dtk's view of retail) against
`build/G2ME01/MetareeSwarm/asm/MetroidPrime/ScriptObjects/CMetareeSwarmRel.s` (dtk's view of *our*
object, which is 100.00% of retail) over `0x0..0xD8` - all 54 instructions - gives **7 differing
lines, and only 2 of them differ in bytes**: the two `bl` encodings (`48 00 32 2D` and
`48 00 31 DD` against `48 00 28 A9` and `48 00 28 59`). The other five differ only in a reloc name
(`fn_43_A8`, `fn_43_D8` twice, `lbl_43_bss_20` twice). That is a fact about the two modules, not
a coincidence of a template: both are 0xB8-byte records with a float triple at +0x0C/+0x1C/+0x2C
and a flag byte at +0xB2, counted at +0x17C and indexed at +0x184, and dtk puts **both** head
accessors in the module's FORCEACTIVE block and **both** vtables are 50 words with the two
accessors at words 38 and 39 - `.data:0x98` and `.data:0x9C`, in `auto_04_00000000_data.s` for each
module. So the two classes really are built from the same template in retail.

**The practical consequence is that none of module 43's three hard-won spellings had to be
rediscovered.** They transfer, and each is worth keeping for the next module of this family:

- the three floats have to be **built**, not indexed - `*out = CVector3f(v[3], v[7], v[11])` is
  100.00%, the three assignments are 58.30% (measured on `fn_43_3C`, 2026-09-29);
- `index > -1`, not `index >= 0` - the same test, but only the `<= -1` spelling gives retail's
  `cmpwi r4,-1 / ble`;
- the flag byte has to be read through a **pointer dereference**, not a subscript, or the constant
  folds into the load's displacement and retail's `addi r0,r4,0xb2 / lbzx r0,3,r0` becomes
  `add r3,r3,r0 / lbz r0,0xb2(r3)`.

A lane about to write a module head should **diff the candidate's `.text 0x0..0x1FF` against a head
that already landed before it writes any C++.** If the bytes match, the body is known and the item
is minutes; if they do not, the diff says which part is different, which is exactly the thing that
was expensive to find the first time.

`RELMain`, `RELExit` and `fn_49_A8` are the `CScriptPlayerProxy.cpp` / `CMetareeSwarmRel.cpp` /
`CIngPuddleRel.cpp` arrangement, unchanged: the registration hands the setter the *address* of a
four-byte `.bss` slot (`fn_8022FFF8` is the DOL's `stw r3, gLoader_PlantScarabSwarm; blr`, and
`LoadPlantScarabSwarm` at 0x8022FFCC reads it as `lwz r6,slot; lwz r12,0(r6); mtctr r12; bctrl` -
`src/MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp` already documents that half), and the `.bss`
symbol stays `extern` under MWCC because dtk's `auto_05_00000000_bss.s` is the definition. The file
is **not** in `files.cmake`, for the reason measured on `CMetareeSwarm` three lanes earlier: a host
body would reference `fn_49_D8` and `fn_8022FFF8`, and `tools/link_check.sh --strict` fails on a
growing undefined count. `tools/check_files_cmake.py` counts it as a module entry (6 units, up from
5) rather than as an omission, which is the right bin.

### What is left

`fn_49_D8` (0xD8, 0x6A0) is the module's entity loader, and the 61 functions from there to
`fn_49_323C` are CPlantScarabSwarm's methods. None is claimed; dtk fills `0xD8..0x32A0` from retail.
That is class code and it needs the CActor/CPatterned hierarchy, which is the same blocker the
item's `reason` names. Note the module is 71 functions, not 66: `auto_00_00000000_text` reports 66
and the five `REL_Setup` functions are the other five, so a "27 unclaimed" style count taken from
the `auto_*` unit alone is short by the setup tail.

## `CSnakeWeedSwarmRel` is a module head, and a pmf is 12 bytes (2026-09-29, goal item `progress-rel-head-snakeweedswarm`)

SnakeWeedSwarm is module 71 and the fourth of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28. The head is now the whole `.text 0x0..0xDC` - **four** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CSnakeWeedSwarmRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`f59a2a74718b31bff0e76d9716e39091e1e11bba`, `cmp`-equal
to `orig/G2ME01/files/RelProd/SnakeWeedSwarm.rel`), with all 86 holding and `main.dol` still
`6ef9b491...`. `matched` 8843 -> 8847, `linked` 3890 -> 3894, the module's own count 5 -> 9 of 74.
`tools/audit_rel_claim.py SnakeWeedSwarm` reports 0 problem claims and 0 of 74 text symbols dropped
by `-strip_partial`; `tools/check_decl_order.py --unit
SnakeWeedSwarm/MetroidPrime/ScriptObjects/CSnakeWeedSwarmRel` is ok; `tools/unit_fit.sh` reports
`.text claimed 220 ours 220 retail 220 fits` with no extra functions; `tools/flip_test.sh` on the
unit reports `PASS -> kept as Matching`.

**The head is not the other three modules' head, and the "diff the first 0x200 bytes against a head
that already landed" shortcut says so in one command.** The four, from
`config/G2ME01/rels/SnakeWeedSwarm/symbols.txt`:

```
0x00  fn_71_0   0x2C   the CActor `GetHealthInfo` slot, calling vtable slot 0x38
0x2C  RELExit   0x24   li r3,0 / bl SetLoader_SnakeWeedSwarm
0x50  RELMain   0x20   bl fn_71_70
0x70  fn_71_70   0x6C   lbl_71_bss_40 = {fn_71_DC, lbl_71_data_18, lbl_71_data_24}
```

`RELMain`, `RELExit` and the registration are the `CScriptPlayerProxy.cpp` / `CMetareeSwarmRel.cpp`
arrangement, unchanged, including the `extern`-under-MWCC `.bss` slot. Two things are new.

### 1. A CodeWarrior pointer-to-member-function is 12 bytes, so the record is 0x1C and not 8

`fn_71_70` writes **seven** words. `SetLoader_SnakeWeedSwarm` is the DOL's 0x8021BB08
(`stw r3, gLoader_SnakeWeed; blr`), and its two readers are in
`build/G2ME01/asm/MetroidPrime/ScriptLoaderRel.s`: `LoadSnakeWeedSwarm` (0x8021BADC) reads
`lwz r6, gLoader_SnakeWeed; lwz r12, 0(r6); mtctr r12; bctrl` - the loader at **+0x00** - and
`SnakeWeedAlt_8021BA94` (0x8021BA94) reads `lwz r7, gLoader_SnakeWeed; addi r12, r7, 0x4;
bl __ptmf_scall`, i.e. a pointer-to-member-function at **+0x04**. And `__ptmf_scall`
(`build/G2ME01/asm/Runtime/ptmf.s:0x80345454`) reads **three** words out of r12 - the `this`
adjustment, a vtable offset, and the address - which is why `__ptmf_null` is 0xC bytes. So the
record is `FScriptLoader` + 12 + 12 = **0x1C**, the size of `lbl_71_bss_40` in
`build/G2ME01/SnakeWeedSwarm/asm/auto_05_00000000_bss.s`, and the members land at +0x00, +0x04 and
+0x10 exactly as the stores in `fn_71_70` do.

`include/MetroidPrime/ScriptLoaderRel.hpp` models this struct as **eight** bytes (a loader and one
member-function pointer, which is a host-sized guess). The module fills seven words and the third
has no reader in the DOL, so the type is spelled locally in `CSnakeWeedSwarmRel.cpp` and the header
is left alone - fixing it is a port-side model change, not this item's business. **The general
rule: `__ptmf_scall` is three words, so any `*FuncPtrs` struct in this tree that holds a
pointer-to-member-function is 12 bytes wider than a host C++ member pointer, and a struct that
looks too small in a header is a header bug rather than a codegen puzzle.**

### 2. The two member-function pointers are copied out of `.data`, not assigned

The two 12-byte objects at `.data:0x18` and `.data:0x24` (`auto_04_00000000_data.s`, both
`0 / 0xFFFFFFFF / fn_71_1AF8` and `0 / 0xFFFFFFFF / fn_71_1B3C`) are non-virtual member-function
pointers: the vtable offset is -1, so `__ptmf_scall` skips the vtable lookup and calls the address
directly. `fn_71_70` loads all six words into r9/r8/r7 and r5/r4/r0, stores the loader through
`stwu` so r3 walks the record, stores the six words back, and only then calls the setter - so the
source is three field assignments with the two right-hand sides read out of `.data`:

```cpp
lbl_71_bss_40.swarm = fn_71_DC;
lbl_71_bss_40.damage = lbl_71_data_18;
lbl_71_bss_40.alt = lbl_71_data_24;
SetLoader_SnakeWeedSwarm(&lbl_71_bss_40);
```

That spelling is what produces the schedule, including the `stwu` in the middle of the loads, and it
is 100.00% on the first try. **Building the two pmfs in place** - `&CEntity::SomeMethod` written out
as a `{0, -1, fn}` aggregate - would put three `li`s and a `stw` per member in the function and
lose the `.data` objects; the copy is not an accident of the compiler. Both `.data` symbols stay
`extern` and unclaimed for the same reason the `.bss` slot does: dtk's data object is the
definition, and a second one under MWCC is what broke mwldeppc on `ScriptPlayerProxy`.

### 3. `fn_71_0` is a vtable entry, and the slot is CActor's `HealthInfo`

`lbl_71_data_30` (`.data:0x30`, 0x7C bytes) is CSnakeWeedSwarm's vtable and stores `fn_71_0` at
offset 0x3C. That table is **CActor's** (`__vt__6CActor`, in
`build/G2ME01/asm/MetroidPrime/CActor.s`: 29 entries after two zero words) with one slot replaced -
the 14th virtual, `GetHealthInfo__6CActorCFv`, is `fn_71_0` here, and the 13th, the one `fn_71_0`
dispatches on, is `HealthInfo__6CActorFv`. So the call is retail's own shape: the DOL's
`GetHealthInfo__6CActorCFv` at 0x8000B900 is these same eleven instructions, `lwz r12,0(r3) /
lwz r12,0x38(r12) / mtctr r12 / bctrl`, which is the `CIngPuddleRel` measurement from the day before
(hand-loading the vtable gives `lwz r3,0(r3)` and 99.09%). The stand-in class
`CSnakeWeedSwarmVTable` declares all 29 virtuals by position, so the called one is at
`(0x38 - 8) / 4 + 1` = the 13th, and none of them is defined: the only object carrying this vtable
is the module's own retail bytes.

**The cheap way to name a vtable slot is to find the base class's table in the DOL and diff the
two.** `lbl_71_data_30` and `__vt__6CActor` are the same 29 entries with one substitution, which
turns "call vtable offset 0x38" into "`GetHealthInfo`, returning `HealthInfo()`" without reading a
single instruction of the module's class code. `TypesMatch.cpp` already says CSnakeWeedSwarm's
parent is CActor.

### What is left

`fn_71_DC` (0xDC, 0x544) is the module's entity loader, and the 65 functions from there to
`fn_71_3CF4` are CSnakeWeedSwarm's methods. None is claimed; dtk fills `0xDC..0x3D44` from retail.
That is class code and it needs the CActor/CPatterned hierarchy, which is the blocker the item's
`reason` names. The module is 74 functions: `auto_00_00000000_text` reported 69 before the carve
and reports 65 after it, and the five `REL_Setup` functions are the other five.

## `CMysteryFlyerRel` is a module head, and `fn_45_10` is a hidden-return `optional_object` (2026-09-29, goal item `progress-rel-head-mysteryflyer`)

**Solved, later on 2026-09-29 - the rest of this section is the reasoning that led there, and its
conclusion that `fn_45_10` needs `optional_object`'s converting ctor is superseded.** The ctor is
**out of line in retail**: `fn_45_2BBC` *is* `optional_object<CAABox>(const CAABox&)` (six word
copies, then the flag), so `fn_45_10` never instantiates the template. It is one call:

```cpp
void fn_45_10(void* out, const CPhysicsActor* self) { fn_45_2BBC(out, self->GetBoundingBox()); }
```

with `fn_45_2BBC` declared `extern "C" void fn_45_2BBC(void* out, const CAABox& box)`. Taking the
box by value instead copies the temporary a second time (frame 0x40, extra `lfs`/`stfs`); the const
reference gives the retail 0x30 frame byte for byte. Two traps on the way to the sha1:
**including `MetroidPrime/CPhysicsActor.hpp` breaks the module hash with every function at 100%**,
because it pulls in `Collision/CMaterialList.hpp`, whose file-scope `static` material constants
add 0x28 bytes of `.data`; `CMysteryFlyerRel.cpp` declares a one-method local `class CPhysicsActor`
instead, and includes `MetroidPrime/TGameTypes.hpp` for `TUniqueId`. The head is now
`.text 0x0..0x170`, 18/18 at 100.00%, `matched` 9102 -> 9117, sha1 unchanged. **The same wrapper
sits near 0x0 in about twenty other modules** (`fn_81_10` Tryclops, `fn_40_10` Metroid,
`fn_55_10` SandBoss, ...), so this spelling is the lever for extending their heads to 0x0.

MysteryFlyer is module 45 and the fifth of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28. The head is **the loader trio only**, `.text 0xFC..0x170` - three functions, all
100.00%, `src/MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`2770bc0304d6e5511c151e34a89c5e0d57af1bc5`, `cmp`-equal
to `orig/G2ME01/files/RelProd/MysteryFlyer.rel`), with all 86 holding and `main.dol` still
`6ef9b491...`. `matched` 8865 -> 8868, `linked` 3912 -> 3915, the module's own count 5 -> 8 of 83.
`tools/audit_rel_claim.py MysteryFlyer` reports 0 problem claims, `tools/flip_test.sh` on the unit
passes and keeps it `Matching`, and `tools/unit_fit.sh` says
`.text claimed 116 / ours 116 / retail 116, fits` with no extra functions.

### Why the claim starts at 0xFC and not at 0x0

The four heads before this one all claimed from `0x0`, because the accessors the REL loader
generator emits come *before* the trio and are cheap. Here they do not come first, or rather
they come first but they are **not all cheap**, and one contiguous claim cannot skip any of them.
From `config/G2ME01/rels/MysteryFlyer/symbols.txt`, `.text 0x0..0xFC` is fifteen functions:
`fn_45_0` (0x0) and `fn_45_8` (0x8) are `li r3,1` and `addi r3,r3,0x818`; **`fn_45_10` (0x10, 0x3C)
is not a member accessor at all**; then ten functions that are the same fourteen-accessor block
`CAtomicAlphaRel.cpp` already reproduces (0x4C..0xFC, the float store, the two predicates, the
unique-id reset, the `>> 3 & 1` flag, the constant float, `+0x754`, two predicates and the
three-float copy); and `fn_45_D0` (0xD0, 0x2C), a vtable-0x38 dispatch that `CAtomicAlphaRel.cpp`
already writes as a member call against a thirteen-virtual stand-in class.

So **fourteen of the fifteen are known-good bodies today and only `fn_45_10` is new work** - but
because the claim must be one contiguous range, `fn_45_10` is the whole of the difference between
this head's three functions and eighteen. Landing the trio now and leaving the range below it to
dtk is the correct outcome for this item, not a smaller version of it.

### `fn_45_10` is a hidden-return `optional_object<CAABox>`, and here is the measurement

The disassembly (`build/G2ME01/MysteryFlyer/asm/auto_00_00000000_text.s:0x10`) is 0x3C bytes:

```
stwu r1,-0x30(r1) / mflr r0 / stw r0,0x34(r1) / stw r31,0x2c(r1) / mr r31,r3
addi r3, r1, 8
bl GetBoundingBox__13CPhysicsActorCFv
mr r3, r31
addi r4, r1, 8
bl fn_45_2BBC
```

**The caller never sets r4 before the `bl`, which settles the calling convention.** The DOL
callee at 0x800EA054 (`build/G2ME01/asm/MetroidPrime/CPhysicsActor.s:371`) reads its actor out of
**r4** throughout (`lfs f3, 0x258(r4)` and eleven more) and leaves r3 alone until it calls
`__ct__6CAABoxFRC9CVector3fRC9CVector3f`, which is a constructor and so takes its `this` in r3.
So `GetBoundingBox` receives **r3 = the destination box, r4 = `this`** - and since `fn_45_10` enters
with `this` in r3 and copies it to r31 without ever writing r4, **r4 must already hold `this`,
which means `fn_45_10` itself is a member function with a hidden return pointer in r3.** The
`mr r3, r31` after the call is that return pointer coming back, and `fn_45_2BBC` is therefore
writing the return value, not a member.

`fn_45_2BBC` (0x2BBC, 0x3C) is the rest of the reading: it copies **six** words, `r4+0x00` through
`r4+0x14`, to `r3+0x00` through `r3+0x14` and then `stb 1, 0x18(r3)`. Six words is a `CAABox` (a
`CVector3f` pair) and a byte at +0x18 is a validity flag, so the return type is
**`rstl::optional_object<CAABox>`, 0x1C bytes** - and `include/rstl/optional_object.hpp` is the type
that models it.

**So `fn_45_10` is one line of C++** - `return rstl::optional_object<CAABox>(GetBoundingBox());`
in a `CPhysicsActor`-derived class - and it is still the blocker, for a reason worth writing down
because it is not a spelling problem. `rstl::optional_object`'s converting constructor is
`optional_object(const T& item) : m_valid(true) { rstl::construct<T>(m_data, item); }`, so the
**flag store is the mem-init and the copy is the body** - the opposite order from retail, which
copies six words and *then* sets the flag. And instantiating `optional_object<CAABox>` in a unit
whose other eighteen functions are the loader trio pulls `rstl::construct`/`rstl::destroy` out of
line into a **trailing pool**, which is the emission-order wall "An emission-order wall: out-of-line
template instantiations" above measures on `CStaticAudioPlayer`. Three things would have to be
true at once - the mem-init order inverted, no pool, and the 0x30 frame - and this lane did not
spend the builds to find out. **`NEW:` in the notes records it.**

The frame is the one part that is settled: a single 0x18-byte local at `r1+8`, plus MWCC's 8-byte
doubleword at `r1+0`, the saved `r31` and the saved `LR`, is exactly the `stwu r1,-0x30(r1)` retail
emits, so the stack is not the problem - the type's initialisation order is.

### What is left

`fn_45_170` (0x170, 0x30C) is the module's entity loader, and the 74 functions from `fn_45_0` at
0x0 to `fn_45_2BF8` are CMysteryFlyer's members. None is claimed; dtk fills `0x0..0xFC` and
`0x170..0x2BF8` from retail, which is what `auto_00_00000000_text` (15 functions) and
`auto_00_00000170_text` (59) in `build/report.json` are. The module is 83 functions: 3 ours, 5
`REL_Setup`, 74 unclaimed, and the 83rd is `auto_fn_45_2BF8_text`, the `.ctors`/`.dtors` pointers -
the same convention the `AtomicAlpha` row above uses.

## `CAtomicAlphaRel` is a module head, and twelve of its fourteen accessors are shared (2026-09-29, goal item `progress-rel-head-atomicalpha`)

AtomicAlpha is module 2 and the fifth of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28. The head is now the whole `.text 0x0..0x13C` - **eighteen** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CAtomicAlphaRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`ade8972eb74648c3c5c99caa022ff457ff2b27fd`,
`cmp`-equal to `orig/G2ME01/files/RelProd/AtomicAlpha.rel`), with all 86 holding and `main.dol`
still `6ef9b491...`. `matched` 8847 -> 8865, `linked` 3894 -> 3912. `tools/audit_rel_claim.py
AtomicAlpha` reports 0 problem claims (`18/18 functions` inside the claim, so the unit really did
write every byte it claims) and 0 of 71 text symbols dropped by `-strip_partial`; `tools/unit_fit.sh`
reports `.text claimed 316 ours 316 retail 316 fits` with no extra functions; `tools/flip_test.sh`
on the unit reports `PASS -> kept as Matching`.

The eighteen, from `config/G2ME01/rels/AtomicAlpha/symbols.txt`:

```
0x00  fn_2_0   0x08   addi r3,r3,0x8c8 / blr
0x08  fn_2_8   0x08   addi r3,r3,0x7d8 / blr
0x10  fn_2_10  0x10   lbl_8041AAB8 -> *((float*)(self + 0x448))
0x20  fn_2_20  0x08   lbz r3, 0x44f(r3)
0x28  fn_2_28  0x08   li r3,0
0x30  fn_2_30  0x08   li r3,0
0x38  fn_2_38  0x10   *self = kInvalidUniqueId
0x48  fn_2_48  0x0C   byte at +0x34c, bit 3
0x54  fn_2_54  0x0C   lbl_8041B758
0x60  fn_2_60  0x08   addi r3,r3,0x754
0x68  fn_2_68  0x08   li r3,1
0x70  fn_2_70  0x08   li r3,0
0x78  fn_2_78  0x08   li r3,0
0x80  fn_2_80  0x1C   three floats from self+0x54 -> *out
0x9C  fn_2_9C  0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr / bctrl
0xC8  RELExit  0x24   li r3,0 / bl SetLoader_AtomicAlpha
0xEC  RELMain  0x20   bl fn_2_10C
0x10C fn_2_10C 0x30   lbl_2_bss_0 = fn_2_13C ; SetLoader_AtomicAlpha(&lbl_2_bss_0)
```

### The finding, stated correctly this time: twelve of the fourteen, not fourteen

**This is the first of the module heads that is larger than the loader trio**, and that changes
what the claim looks like. The other four claim five, four, five and five functions; here the
*fourteen-accessor block that the REL loader generator emits at the head of a scripted-actor module
comes first*, from 0x0 to 0x9C, and only then the vtable dispatch and the trio. So the claim
reaches from 0x0 rather than from `fn_2_9C`, and it is **18 functions** - the largest single step.

Twelve of those fourteen accessors were already written:
`src/MetroidPrime/ScriptObjects/AtomicBetaAccessors.cpp` claims AtomicBeta's `.text 0x0..0x9C` as
a `Matching` unit at 100.00%, with the same three relocations - `lbl_8041AAB8`, `kInvalidUniqueId`
and `lbl_8041B758`, all three in the DOL, which is exactly why one body serves every module. The
same `store default float at +0x448` / `read byte at +0x44f` / `li r3,0` predicates /
`reset unique id` / `bit 3 of +0x34C` / `constant float` / `+0x754` / `li r3,1` / two more
predicates / `copy three floats from +0x54` sequence, in the same order.

**But the two blocks are not byte for byte identical, and an earlier draft of this section said they
were.** AtomicAlpha's two *leading* accessors are **extra**: `fn_2_0` returns `self + 0x8C8` and
`fn_2_8` returns `self + 0x7D8`, where AtomicBeta opens with the float store at 0x0. In exchange
AtomicAlpha carries two fewer `li r3,0; blr` predicates after the byte read (its 0x28/0x30 are
Alpha's, AtomicBeta's 0x18 and 0x20 have no counterpart here). The count that survives checking
against the bytes is **twelve of fourteen**, and the honest generalisation is *"read
`AtomicBetaAccessors.cpp` and diff the candidate's `.text 0x0..0x1FF` against a landed head before
writing any C++, and expect 18 functions here"* - not *"copy the file"*.

Even so **no spelling had to be discovered**, which is the transferable part. A lane that diffs
against a head that already landed - the advice in the `CPlantScarabSwarmRel` section above - finds
this in one command; a lane that diffs against `AtomicBeta` finds twelve of the eighteen written and
the other six as three-line bodies.

Two of the fourteen still read oddly in dtk's rendering, and both are dtk's, not the source's:

- `fn_2_48` is `lbz r0, 0x34c(r3) / 54 03 EF FE / blr`. dtk prints the middle word
  `extrwi r3, r0, 1, 28`, which reads as bit 28 of a byte and would therefore be a function that is
  always false. The word's opcode is 21, not 31, so it is `rlwinm r3, r0, 29, 31, 31`: a rotate
  left by 32-3, masked to one bit - **bit 3**, the same bit `AtomicBetaAccessors.cpp`'s `& 8`
  already spells, and the same encoding as `fn_42_36C`'s measured `>> 3`.
- `fn_2_80` loads and stores **interleaved** (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 / stfs f0,4 /
  lfs f0,0x5c / stfs f0,8`), so it is written as three subscript stores over a `const float*`. Do
  not "improve" it into a `CVector3f` copy: `CMetareeSwarmRel.cpp` records that the built-in
  spelling reverses the loads and the score falls. This is the *opposite* of `fn_49_3C` in the same
  family, where retail does build the three floats first, and the difference is in the bytes.

### `fn_2_9C` is a vtable entry, and the same member-call trick applies

`fn_2_0`, `fn_2_8` and `fn_2_9C` are not free functions. dtk lists all three in the module's
FORCEACTIVE block and `build/G2ME01/AtomicAlpha/asm/auto_04_00000000_data.s` shows `.data:0xC4` -
AtomicAlpha's own 82-word (0x148-byte) vtable - storing `fn_2_9C` at 0x3C, so nothing here is a
dead-stripping hazard. `fn_2_9C` reads vtable slot 0x38, which `.data:0xC4` names
`HealthInfo__3CAiFv`, so it is written as a member call against a thirteen-virtual stand-in class
for the reason measured in the `CIngPuddleRel` section above: the hand-loaded spelling gives
`lwz r3,0(r3)` where retail has `lwz r12,0(r3)`. `Slot12` is declared `virtual float` because that
is what the vtable entry is; the call discards the result, so it does not affect the bytes, and
naming it anything else would misdescribe the vtable.

`RELMain`, `RELExit` and `fn_2_10C` are the `CScriptPlayerProxy.cpp` arrangement, unchanged. The one
difference from the four modules before it is the **name of the setter**: AtomicAlpha's is
`SetLoader_AtomicAlpha`, a real C++ function already at 100.00% in
`src/MetroidPrime/ScriptLoaderRel.cpp` (that unit is 42/42), so it is declared here in C++ rather
than as an `fn_80xxxxxx` DOL label, and `mwcceppc` mangles it to
`SetLoader_AtomicAlpha__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity` - the name
`config/G2ME01/symbols.txt` gives the DOL's 0x8021BB9C. The modules whose setter is still an
unnamed `fn_80xxxxxx` had to invent that name for the same two-instruction `stw r3, slot; blr`; a
module whose setter is already decompiled should **not**, because an invented name is a different
symbol and the call stops resolving to the function that actually exists. Check per module with
`grep SetLoader_ config/G2ME01/symbols.txt`. The `.bss` symbol stays `extern` under MWCC because
dtk's `auto_05_00000000_bss.s` is the definition, and the file is **not** in `files.cmake` for the
reason measured on `CMetareeSwarm`: a host body would reference `fn_2_13C`, which the port cannot
link, and `tools/link_check.sh --strict` fails on a growing undefined count.

### What is left

`fn_2_13C` (0x13C, 0x420) is the module's entity loader, and the 47 functions from there to
`fn_2_2578` are AtomicAlpha's methods (`auto_04_00000000_data.s` names them: `CPatterned`'s
`PreThink`, `CActor`'s `SetActive`, `CAi`'s `HealthInfo`, `CPhysicsActor`'s `GetWeight`, and so
on, so `TypesMatch.cpp`'s parent chain is what the tree is missing). None is claimed; dtk fills
`0x13C..0x267C` from retail. That is class code and it needs the CActor/CPatterned hierarchy,
which is the blocker the item's `reason` names.

**Counting a module's functions: read the per-unit `total_functions`, do not add up unit names.**
Before the claim, `build/report.json` had `auto_00_00000000_text` 65 + `auto_fn_2_2578_text` 1 +
`REL_Setup` 5 = 71. After it has `CAtomicAlphaRel` 18 + `auto_00_0000013C_text` 47 + 1 + 5 = 71.
The unit *names* changed and the total did not - the same attribution effect "Why the matched total
can go *down* when module work lands" describes. So the module is **71 functions**, and the 70 in
the status table above is 71 less the `auto_fn_2_2578_text` unit, on the same convention as the
three rows above it. `gate.sh` calls the split out itself, as `SPLIT ... exact count match - a
split, not a loss`, which is the right reading.

## `CIngSnatchingSwarmRel` is `CIngPuddleRel` with the loader import spelled out (2026-09-29, goal item `progress-rel-head-ingsnatchingswarm`)

IngSnatchingSwarm is module 33 and the sixth of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28. The head is now the whole `.text 0x0..0xA8` - **five** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`c84839632c931841a91b29e2a30dff53bc6f2408`, `cmp`-equal
to `orig/G2ME01/files/RelProd/IngSnatchingSwarm.rel`), with all 86 holding and `main.dol` still
`6ef9b491...`. `matched` 8868 -> 8873, `linked` 3915 -> 3920, the module's own count 5 -> 10 of 101.
`tools/audit_rel_claim.py IngSnatchingSwarm` reports 0 problem claims and 0 of 102 text symbols
dropped by `-strip_partial`; `tools/check_decl_order.py --unit
IngSnatchingSwarm/MetroidPrime/ScriptObjects/CIngSnatchingSwarmRel` is ok; `tools/unit_fit.sh`
reports `.text claimed 168 ours 168 retail 168 fits` with no extra functions; `tools/flip_test.sh`
on the unit reports `PASS -> kept as Matching`.

The five, from `config/G2ME01/rels/IngSnatchingSwarm/symbols.txt`:

```
0x00  fn_33_0   0x08   addi r3,r3,0x1F4 / blr
0x08  fn_33_8   0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr r12 / bctrl
0x34  RELExit   0x24   li r3,0 / bl SetLoader_IngSnatchingSwarm
0x58  RELMain   0x20   bl fn_33_78
0x78  fn_33_78  0x30   lbl_33_bss_0 = fn_33_A8 ; SetLoader_IngSnatchingSwarm(&lbl_33_bss_0)
```

### The finding: the head is module 32's head, instruction for instruction

Diffing `build/G2ME01/IngSnatchingSwarm/asm/auto_00_00000000_text.s` (dtk's view of retail) against
`build/G2ME01/IngPuddle/asm/MetroidPrime/ScriptObjects/CIngPuddleRel.s` (dtk's view of *our* object,
which is byte-identical) over the same 0xA8 bytes: **42 instructions, differing in 3** - the
`addi r3,r3,0x1F4` against IngPuddle's `addi r3,r3,0x460`, and the two `bl` displacements
(`48 00 53 51` and `48 00 53 01` against `48 00 32 B5` and `48 00 32 65`), because each module
registers its own loader. Every other line differs only in the module's own symbol names
(`fn_32_78`/`fn_33_78`, `lbl_32_bss_0`/`lbl_33_bss_0`, `fn_32_A8`/`fn_33_A8`) and is byte-identical. The two accessors are at the same two words of the same 31-word
vtable: `build/G2ME01/IngSnatchingSwarm/asm/auto_04_00000000_data.s` shows `.data:0x264` (0x7C
bytes: two zero words, then 29 virtuals) storing `fn_33_0` at 0x38 and `fn_33_8` at 0x3C, the same
0x38/0x3C as IngPuddle's `.data:0xD0`. So the member-call spelling the
`CIngPuddleRel` section measures is the spelling here, not a re-derivation: thirteen virtuals put
the last one at 0x38 and `self->Slot12()` gives retail's seven instructions byte for byte. **General
form, now twice measured: before rewriting a module head, diff its dtk `.s` against the previous
head's, and the codegen is free - only the class's own offsets and the two call targets move.**

The parent class differs and is worth recording: `TypesMatch__18CIngSnatchingSwarmCFi` at 0x8009C5F4
is `cmpwi r4,0x1e` falling through to `TypesMatch__6CActorCFi`, so `CIngSnatchingSwarm` derives from
**`CActor`**, where IngPuddle's derives from `CPhysicsActor`. That is one fewer class in the chain
the rest of the module is waiting on, and it is measured from the DOL, not from `TypesMatch.cpp`.

### The trap: an `extern "C"` import keeps its identifier, so the import has to be spelled out

IngPuddle's setter is the *unnamed* DOL symbol `fn_80229EE0`, so the C++ identifier and the import
name were the same string and the issue never arose. IngSnatchingSwarm's is already decompiled:
`src/MetroidPrime/ScriptLoaderRel.cpp:141` defines `SetLoader_IngSnatchingSwarm(FScriptLoader*)`,
and because that file is *not* `extern "C"`, `mwcceppc` mangles the `FScriptLoader*` parameter -
a function-pointer typedef - into
`SetLoader_IngSnatchingSwarm__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity`, which
is the name `config/G2ME01/symbols.txt:9529` gives the DOL's 0x8021BA8C and the name the module
imports.

Writing the short name here compiles, emits, and then fails at the REL step, not earlier:

```
[2/9] LINK build/G2ME01/IngSnatchingSwarm/IngSnatchingSwarm.plf
[4/9] REL  FAILED
Failed: While resolving relocations in 'build/G2ME01/IngSnatchingSwarm/IngSnatchingSwarm.plf'
Caused by:
    Failed to find symbol SetLoader_IngSnatchingSwarm in any module
```

**So the rule for a module head is: look up the setter's name in `config/G2ME01/symbols.txt` and
write *that*, not the C++ name.** An invented `fn_80xxxxxx` for a setter that is already named is a
different symbol and the call stops resolving; the `AtomicAlpha` section above says the same thing
for a module whose setter is named, and this is the other half of it - the mangled form, not the
short one. The C++ identifier is not recoverable from the mangled one, so nothing else in the file
has to change.

The rest of the arrangement is `CIngPuddleRel`'s unchanged: the registration hands the setter the
*address* of a four-byte `.bss` slot (`SetLoader_IngSnatchingSwarm` is the DOL's
`stw r3, gLoader_IngSnatchingSwarm; blr` at 0x8021BA8C, and `LoadIngSnatchingSwarm` at 0x8021BA60
reads it as `lwz r6,slot; lwz r12,0(r6); mtctr r12; bctrl`), the `.bss` symbol stays `extern` under
MWCC because dtk's `auto_05_00000000_bss.s` is the definition, and the file is **not** in
`files.cmake` for the reason measured on `CMetareeSwarmRel`: a host body would reference `fn_33_A8`,
which the port cannot link, and `tools/link_check.sh --strict` fails on a growing undefined count
(measured here: 314, unchanged, because the file is absent).

### What is left

`fn_33_A8` (0xA8, 0x5D4 = 1492 bytes) is the module's entity loader, and the 91 functions from
there to `fn_33_5004` are CIngSnatchingSwarm's methods. None is claimed; dtk fills `0xA8..0x5394`
from retail. That is class code and it needs the CActor hierarchy, which is the blocker the item's
`reason` names. Counting note, on the same convention as the status table above: the module is 101
functions (5 ours + 91 unclaimed + 5 setup), which excludes the `auto_fn_33_5078_text` unit -
a 102nd, the `.ctors`/`.dtors` pointer function. Read the per-unit `total_functions` in
`build/report.json`; `gate.sh` prints the claim as a `SPLIT ... exact count match - a split, not a
loss`.

## `CFishCloudRel` is a module head, and the header already had the record (2026-09-29, goal item `progress-rel-head-fishcloud`, lane 1)

FishCloud is module 20 and the eighth module head claimed, after `MetareeSwarm`, `IngPuddle`,
`IngSnatchingSwarm`, `PlantScarabSwarm`, `AtomicAlpha`, `SnakeWeedSwarm` and `MysteryFlyer`. The
head is the whole `.text 0x0..0xAC` - **four** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CFishCloudRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`79ae4b2efb35bbb627f3a76d811d5272e6643e62`,
`cmp`-equal to `orig/G2ME01/files/RelProd/FishCloud.rel`), with all 86 holding and `main.dol` still
`6ef9b491...`. `matched` 8873 -> 8877, `linked` 3920 -> 3924, REL 845 -> 849.
`tools/audit_rel_claim.py FishCloud` reports 0 problem claims (`4/4 functions` inside the claim) and
0 of 106 text symbols dropped by `-strip_partial`; `tools/unit_fit.sh` reports
`.text claimed 172 ours 172 retail 172 fits` with no extra functions; `tools/flip_test.sh` on the
unit reports `PASS -> kept as Matching`.

The four, from `config/G2ME01/rels/FishCloud/symbols.txt`:

```
0x00  fn_20_0   0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr / bctrl
0x2C  RELExit   0x24   li r3,0 / bl SetLoader_FishCloud
0x50  RELMain   0x20   bl fn_20_70
0x70  fn_20_70  0x3C   lbl_20_bss_0 = {fn_20_340, fn_20_AC} ; SetLoader_FishCloud(&lbl_20_bss_0)
```

### Nothing had to be discovered, and that is the finding

Every other head cost something. `MetareeSwarm` found that `>> 7` is a 25-bit rotate and that the
three floats have to be built rather than indexed. `AtomicAlpha` found that twelve of its fourteen
accessors are shared and two are not. `SnakeWeedSwarm` found that a CodeWarrior pmf is 12 bytes,
because `__ptmf_scall` reads three words of it. **FishCloud cost nothing at all**, and the reason
is worth recording precisely, because the temptation is to file it as a copy of SnakeWeedSwarm:

- **The vtable entry is not merely the same function - the bytes are identical.** `fn_20_0` and
  `CSnakeWeedSwarmRel.cpp`'s `fn_71_0` are both `.text:0x0`, `size:0x2C`, and the eleven
  instructions of one are the eleven of the other, word for word. That is CActor's
  `GetHealthInfo`, called through the same 0x38 slot. Two things make it safe for one body to serve
  both. `build/G2ME01/FishCloud/asm/auto_04_00000000_data.s` shows **two** 0x7C-byte tables,
  `lbl_20_data_8` (`.data:0x8`) and `lbl_20_data_84` (`.data:0x84`), and **both** store `fn_20_0` at
  vtable offset 0x3C with `HealthInfo__6CActorFv` at 0x38 - the same 31-word layout (two leading
  zero words, then 29 virtuals) and the same 15th/14th pair `lbl_71_data_30` has. And being in two
  tables is the opposite of a dead-stripping hazard: the link cannot drop it. The 29-virtual
  stand-in class therefore already exists, spelled out in `CSnakeWeedSwarmRel.cpp`, and is reused
  here unchanged.
- **The record is already in a header.** `include/MetroidPrime/ScriptLoaderRel.hpp` has
  `SFishCloud_FuncPtrs` {FScriptLoader fishCloud; FScriptLoader fishCloudModifier;}, and its
  `gLoader_FishCloud` is `.sbss:0x80419490` **size 0x8** - two words, matching the `.bss` object.
  SnakeWeedSwarm had to spell its record locally precisely *because* the header was one member
  short. So the `#include` is the whole of the difference, and the field names are retail's own:
  the DOL's `LoadFishCloud` (0x8021BB3C) reads `value->fishCloud` and `LoadFishCloudModifier`
  (0x8021BB10) reads `value->fishCloudModifier`, which is what fixes the assignment order and so
  the `r5`/`r4` in `fn_20_70`.
- **The setter is already decompiled.** `SetLoader_FishCloud` is the DOL's 0x8021BB68, two
  instructions, and its body lives in `src/MetroidPrime/ScriptLoaderRel.cpp` at 100.00%, so it is
  *declared* in C++ here and `mwcceppc` mangles it to
  `SetLoader_FishCloud__FP19SFishCloud_FuncPtrs` (`config/G2ME01/symbols.txt:9535`). The same trap
  as `CAtomicAlphaRel` applies in reverse: the header's own `SetSFishCloud_FuncPtrs` is a third
  name for the same idea, is not what the call site mangles to, and an `fn_80xxxxxx` alias would be
  a fourth. Only `SetLoader_FishCloud` resolves.

The one thing that is genuinely *this module's* is the record: **two plain `FScriptLoader`s, eight
bytes, no member-function pointers at all** - the first of these heads whose registration has no pmf
in it. What is *not* this module's is the shape of the head, and the useful habit is to check that
before assuming anything. Most of the heads are shaped as "accessors, then dispatch, then the trio",
and this one has no accessors at all, so `.text 0x0` is the vtable entry itself - **as it already was
in `SnakeWeedSwarm`**, whose `fn_71_0` is also 0x2C at 0x0 and byte for byte the same. So there are
two head shapes here, and `head -4 config/G2ME01/rels/<Module>/symbols.txt` - is `.text:0x0` a
`0x2C` vtable dispatch, or an `0x08`/`0x3C` accessor? - settles it in one command before any C++ is
written.

**Check that the byte comparison is not vacuous.** The obvious way to write the `diff` above is

```sh
awk '/^\.fn fn_[0-9]+_0,/,/^\.endfn/' build/G2ME01/$m/asm/auto_00_00000000_text.s \
  | grep -oE '\*/\s+[0-9A-F]{2} [0-9A-F]{2} [0-9A-F]{2} [0-9A-F]{2} ' | tr -d ' ' > .tmp/$m.txt
```

which **matches nothing at all** in dtk's listing format - the four bytes come *before* the `*/`,
not after it. Both files come out empty, `diff` reports no difference, and the check passes while
proving nothing. That is `docs/PROCESS_LESSONS.md`'s first lesson in its purest form, and it is
recorded here because a previous attempt at this item published the empty-`diff` result. The form
that actually works, and that still fails loudly if the extraction breaks:

```sh
for m in FishCloud SnakeWeedSwarm; do
  awk '/^\.fn fn_[0-9]+_0,/{f=1} f{print} /^\.endfn/{if(f)exit}' build/G2ME01/$m/asm/auto_00_00000000_text.s \
    | grep -oE '[0-9A-F]{2} [0-9A-F]{2} [0-9A-F]{2} [0-9A-F]{2} \*/' | tr -d ' */' > .tmp/$m.txt
done
wc -l .tmp/FishCloud.txt          # 11 - if this is 0, the diff below is meaningless
diff .tmp/FishCloud.txt .tmp/SnakeWeedSwarm.txt
```

`RELMain`, `RELExit` and `fn_20_70` are the `CScriptPlayerProxy.cpp` arrangement, unchanged,
including the `extern` `.bss` slot under MWCC (a second definition there is what produced
mwldeppc's internal linker error on ScriptPlayerProxy) and the file's absence from `files.cmake`
(measured: the port's undefined count is 314 before and 314 after this change - `fn_20_340`,
`fn_20_AC` and `SetLoader_FishCloud` are all symbols the host cannot link, so the port keeps
reading `FishCloud.rel` off the disc through `platform/rel.cpp`).

### What is left, measured rather than summarised

`fn_20_AC` (0xAC, 0x240) is the module's `LoadFishCloudModifier` entity loader. **It is not a small
function, and an earlier draft of this section said it called `__ct__20SLdrEditorPropertiesFv` "and
nothing else", which was wrong** - it makes **nine** distinct calls:

```
__ct__20SLdrEditorPropertiesFv   __dt__20SLdrEditorPropertiesFv
__nw__FUlPCcPCc                  ReadFloat__12CInputStreamFv  (twice)
ReadBytes__12CInputStreamFPvUl   LoadTypedefSLdrEditorProperties__FR20SLdrEditorPropertiesR12CInputStream
LdrToEntityInfo__FRC11CEntityInfoRC20SLdrEditorProperties
AllocateUniqueId__13CStateManagerFv
fn_20_6AAC                        (0x6AAC, 0x120)
```

and the last of those is the one that settles it: `fn_20_6AAC` is the `CFishCloud` **constructor**,
and it calls `__ct__16CActorParametersFv`, `__ct__10CModelDataFv` (through `fn_20_6BCC`, 0x20) and
`Translate__12CTransform4fFRC9CVector3f` before storing `lbl_20_data_84` - one of the two vtables
above - into the object. So the `CModelData` and the actor are built one call below `fn_20_AC`, not
by `fn_20_340` alone. Its neighbour `fn_20_340` (0x340, 0x838) is the `LoadFishCloud` half and makes
**nineteen** distinct calls, adding `__ct__6CColorFR12CInputStream`,
`__ct__10CModelDataFRC10CStaticRes`, `__dt__23SLdrAnimationParametersFv`,
`LoadTypedefSLdrAnimationParameters…`, `LoadEditorTransform__FRC20SLdrEditorProperties`, four calls
into the module's own class code (`fn_20_B78`, `fn_20_C88`, `fn_20_DFC`, `fn_20_57D0`) and
`fn_800DFFA8`.

The 97 functions from 0xAC to 0x7014 are FishCloud's methods (`auto_04_00000000_data.s` names them:
`TypesMatch__10CFishCloudCFi`, `CActor`'s `SetActive`, `PreThink`, `HealthInfo`, `GetAimPosition`,
`FluidFXThink`, and so on, so `TypesMatch.cpp`'s parent chain is what the tree is missing). None is
claimed; dtk fills `0xAC..0x7014` from retail. That is class code and it needs the
CActor/CPatterned hierarchy, which is the blocker the item's `reason` names.

**Counting this module: the per-unit `total_functions` are `CFishCloudRel` 4 +
`auto_00_000000AC_text` 97 + `REL_Setup` 5 = 106**, and that is exactly the 106
`audit_rel_claim.py` prints, because here the two agree. The off-by-one that forced the AtomicAlpha
row above to subtract a unit **does not recur here**, and the reason is worth stating rather than
guessing: FishCloud has **no** separate `.ctors`/`.dtors` unit in the report, because its
`ModuleConstructors` (0x716C) and `ModuleDestructors` (0x7120) both sit *inside* the claimed
`REL_Setup` range 0x7014..0x71B8 and are therefore counted in that unit's five. `.ctors` and
`.dtors` are empty section labels in `splits.txt` and contribute no unit. So whether the two numbers
agree is a property of the module, not of the counting rule, and the habit to keep is the same one
the AtomicAlpha row taught: read the per-unit `total_functions` in `build/report.json` rather than
adding up unit names.

## `CBacteriaSwarmRel` is a module head, and the shortest head from 0x0 is four functions (2026-09-29, goal item `progress-rel-head-bacteriaswarm`)

BacteriaSwarm is module 6 and one of the nine of the 27 modules whose `REL_Setup` tail was claimed
on 2026-09-28 that have since gained class code. The head is `.text 0x0..0xA0` - **four**
functions, all 100.00%, `src/MetroidPrime/ScriptObjects/CBacteriaSwarmRel.cpp` - and the module's
sha1 against `config/G2ME01/config.yml` is **unchanged**
(`11859125ea9ab676bf42431cc948ec8d08c86552`, `cmp`-equal to
`orig/G2ME01/files/RelProd/BacteriaSwarm.rel`), with all 86 holding and `main.dol` still
`6ef9b491...`. `matched` 8877 -> 8881, `linked` 3924 -> 3928. `tools/audit_rel_claim.py
BacteriaSwarm` reports 0 problem claims (`4/4 functions` inside the claim, so the unit really did
write every byte it claims) and 0 of 114 text symbols dropped by `-strip_partial`;
`tools/unit_fit.sh` reports `.text claimed 160 ours 160 retail 160 fits` with no extra functions;
`tools/check_decl_order.py --unit BacteriaSwarm/MetroidPrime/ScriptObjects/CBacteriaSwarmRel` is
ok; `tools/flip_test.sh` on the unit reports `PASS -> kept as Matching`.

The four, from `config/G2ME01/rels/BacteriaSwarm/symbols.txt`:

```
0x00  fn_6_0   0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr / bctrl
0x2C  RELExit  0x24   li r3,0 / bl fn_8022A5AC
0x50  RELMain  0x20   bl fn_6_70
0x70  fn_6_70  0x30   lbl_6_bss_10 = fn_6_A0 ; fn_8022A5AC(&lbl_6_bss_10)
```

### What makes it the shortest head from 0x0, and what that does *not* mean

Measured across all twelve heads landed so far, from each module's `splits.txt` and the per-unit
`total_functions` in `build/report.json`:

| head | `.text` claim | bytes | functions |
| --- | --- | --- | --- |
| **`BacteriaSwarm`** | **`0x0..0xA0`** | **160** | **4** |
| `IngPuddle` | `0x0..0xA8` | 168 | 5 |
| `IngSnatchingSwarm` | `0x0..0xA8` | 168 | 5 |
| `FishCloud` | `0x0..0xAC` | 172 | 4 |
| `MetareeSwarm` | `0x0..0xD8` | 216 | 5 |
| `PlantScarabSwarm` | `0x0..0xD8` | 216 | 5 |
| `Krocuss` | `0x0..0xD8` (was `0x3C..0xD8`, 156 bytes, 14) | 216 | 15 |
| `EyeBall` | `0x0..0xD8` (was `0x3C..0xD8`, 156 bytes, 14) | 216 | 15 |
| `IngSpiderballGuardian` | `0x0..0xD8` (was `0x44..0xD8`, 148 bytes, 13) | 216 | 15 |
| `Ripper` | `0x0..0xD8` (was `0x3C..0xD8`, 156 bytes, 14) | 216 | 15 |
| `SnakeWeedSwarm` | `0x0..0xDC` | 220 | 4 |
| `AtomicAlpha` | `0x0..0x13C` | 316 | 18 |
| `MysteryFlyer` | `0x0..0x170` (was `0xFC..0x170`, 116 bytes, 3) | 368 | 18 |
| `MediumIng` | `0x0..0x150` | 336 | 15 |
| `Metroid` | `0x0..0x17C` | 380 | 18 |
| `Rezbit` | `0x0..0x168` | 360 | 17 |
| `SandBoss` | `0x0..0x178` | 376 | 19 |
| `SwampBossStage1` | `0x0..0x160` | 352 | 17 |
| `SwampBossStage2` | `0x0..0x170` | 368 | 18 |
| `Parasite` | `0x0..0x148` | 328 | 17 |
| `ElitePirate` | `0x0..0x178` | 376 | 19 |
| `Shredder` | `0x0..0xC8` (was `0x3C..0xC8`, 140 bytes, 12) | 200 | 13 |

(MysteryFlyer's row was `0xFC..0x170` when this was written; it has since been extended to 0x0 - the sentence below is about the table as it stood.) **It is the shortest of the twelve that start at 0x0, and not the shortest head.** `MysteryFlyer`
claims fewer functions (three) and fewer bytes (116); it is excluded only because its claim starts
at 0xFC. The reason BacteriaSwarm is short *from 0x0* is that its loader registration is the module's first function, with no accessor block in front of it - measured, not visible: over the
range each module claims, **BacteriaSwarm's 0xA0 is 40 instructions against IngPuddle's 42**,
IngPuddle's extra two being `fn_32_0` (`addi r3,r3,0x460; blr`) at 0x0. Aligning `fn_6_0` on
`fn_32_8` leaves 40 against 40, and **exactly two instructions differ in encoding, both `bl`** - the
calls to each module's own loader-setter import. A third `bl` differs only in the symbol dtk prints,
`bl fn_6_70` against `bl fn_32_78`: both encode `48000015`, because each branches to the function
immediately after it. So `fn_6_0` is IngPuddle's `fn_32_8` renamed, and `CIngPuddleRel`'s
thirteen-virtual stand-in class reproduces it byte for byte with no new discovery -
`docs/research/raw_offsets.md` has no `CBacteriaSwarmRel` section to add, because no raw offset
appears in the file.

Two details that differ from IngPuddle and that a copy of that file would have got wrong, both
measured:

- **`fn_6_0` is vtable entry 0x3C, not 0x38.** `build/G2ME01/BacteriaSwarm/asm/auto_04_00000000_data.s`
  shows CBacteriaSwarm's vtable at `.data:0x18` (0x98 bytes = 38 words, two leading and 36
  virtuals) and `fn_6_0` at `.data:0x54`, which is offset 0x3C. Its *call target* is 0x38
  (`HealthInfo__6CActorFv`), which is what the stand-in class has to place the thirteenth virtual
  at - so the same class gives the right call, and the accessor's own position does not matter
  because the head does not call it.
- **The loader slot is `.bss:0x10`, not `+0x0`** as IngPuddle's and IngSnatchingSwarm's are, and it
  is `lbl_6_bss_10` rather than `lbl_32_bss_0`. Same shape (`.bss`, 0x4 bytes, `data:4byte`), same
  `extern`-under-MWCC treatment, different name and offset.

**And the import needed no rename, which is worth stating because IngSnatchingSwarm's did.**
`fn_8022A5AC` is the DOL's 0x8022A5AC, 8 bytes, immediately after
`LoadBacteriaSwarm__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x8022A580 (0x2C bytes, so
it ends exactly there). It is `stw r3, gLoader_BacteriaSwarm; blr`, so it stores the *address* of
a loader slot rather than a loader, and `src/MetroidPrime/ScriptLoader/BacteriaSwarm.cpp` - a
`Matching` unit that already exists - reads that slot as
`lwz r6, gLoader_BacteriaSwarm; lwz r12, 0(r6); mtctr r12; bctrl`. That file also records why the
setter is deliberately unclaimed in the DOL: REL modules import it by its retail name. The
`CIngSnatchingSwarmRel` section above had to write the long MWCC-mangled
`SetLoader_...__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity` out in full and cost
an item's only build failure; **here the module's own import table is the plain string** -
`strings build/G2ME01/BacteriaSwarm/BacteriaSwarm.preplf | grep 8022A5` gives `fn_8022A5AC` - so the
C++ identifier is the same string, `config/G2ME01/symbols.txt` needs no entry, and the DOL is
untouched. **So the mangled form is the exception, not the rule: read the import out of the
module's own `*.preplf` before writing anything.**

Not added to `files.cmake`, for the reason measured on `CMetareeSwarmRel` and `CIngPuddleRel`: a
host body would reference `fn_6_A0` and `fn_8022A5AC`, which the port cannot link, and
`tools/link_check.sh --strict` fails on a growing undefined count (measured here: 314, unchanged).
`tools/check_files_cmake.py` counts this file under "further units are out because they define a
module entry point (RELMain/RELExit), which collides in a flat link".

### What is left, and the module's function count

`fn_6_A0` (0xA0, 0x74C = 1868 bytes) is the module's entity loader, and the 105 functions from
there up to `fn_6_7300` are CBacteriaSwarm's methods. None is claimed; dtk fills `0xA0..0x734C` from
retail, which is `BacteriaSwarm/auto_00_000000A0_text` (105 functions) in `build/report.json` -
`gate.sh` prints it as `SPLIT BacteriaSwarm/auto_00_00000000_text: 109 function(s) accounted for
across 2 new unit(s) in BacteriaSwarm (exact count match - a split, not a loss)`, which is the
re-split of the former 109-function `auto_00_00000000_text` into our 4 plus dtk's 105. That is
class code and it needs the CActor/CPatterned hierarchy, which is the blocker the item's `reason`
names.

**114 is this module's complete function count, with no unit left out**: 4 ours + 105 unclaimed +
5 setup = 114, and `grep -c "type:function" config/G2ME01/rels/BacteriaSwarm/symbols.txt` is also
114. **This module has no `auto_fn_6_7300_text` unit** - unlike AtomicAlpha and IngSnatchingSwarm,
where such a unit really does exist and is excluded by the rows' counting convention.
`fn_6_7300` is a 76-byte function *inside* `auto_00_000000A0_text`, not a unit of its own. An
earlier draft of this section claimed a 115th function in an `auto_fn_6_7300_text` unit, inherited
from those two modules' rows; it does not exist here, and the claim is corrected in place rather
than left to mislead the next reader.

## `CTryclopsRel` is sixteen functions, and a REL unit's 100% is not the module's verdict (2026-09-29, goal item `progress-rel-head-tryclops`, lane 1)

**Superseded in part, same day:** the unit now claims `.text 0x0..0x178`, nineteen functions. `fn_81_10` is not blocked; it calls the out-of-line `optional_object<CAABox>` ctor at 0x4FEC, as MysteryFlyer's `fn_45_10` does. The rest of this section (the objdiff-100%-is-not-the-verdict finding) stands.

Tryclops is module 81 and the tenth of the 27 whose `REL_Setup` tail was claimed on 2026-09-28.
This run wrote `src/MetroidPrime/ScriptObjects/CTryclopsRel.cpp` - the module head, `.text
0x4C..0x178`, sixteen functions - and the module's sha1 against `config/G2ME01/config.yml` is
unchanged at `535aee6611c3cc1d6986d99d7d33448e6df5e1f6` (`cmp`-equal to
`orig/G2ME01/files/RelProd/Tryclops.rel`), with all 86 holding and `main.dol` still
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. `audit_rel_claim.py Tryclops` reports 0 problem claims
(`16/16 functions` inside the claim), 0 of 114 text symbols dropped by `-strip_partial`,
`flip_test.sh` PASS, `unit_fit.sh` `300 claimed / 300 ours / 300 retail, fits`.

The sixteen, from `config/G2ME01/rels/Tryclops/symbols.txt`:

```
0x4C  fn_81_4C  0x10   lbl_8041AAB8 -> *((float*)(self + 0x448))
0x5C  fn_81_5C  0x08   the byte at +0x44F
0x64  fn_81_64  0x08   li r3,0
0x6C  fn_81_6C  0x08   li r3,0
0x74  fn_81_74  0x08   li r3,0
0x7C  fn_81_7C  0x10   *self = kInvalidUniqueId
0x8C  fn_81_8C  0x0C   the byte at +0x34C, bit 3
0x98  fn_81_98  0x0C   lbl_8041B758
0xA4  fn_81_A4  0x08   addi r3,r3,0x754
0xAC  fn_81_AC  0x08   li r3,1
0xB4  fn_81_B4  0x08   li r3,0
0xBC  fn_81_BC  0x1C   three floats from self+0x54 -> *out
0xD8  fn_81_D8  0x2C   virtual dispatch, vtable slot 0x38
0x104 RELExit   0x24   li r3,0 / bl fn_80218D58
0x128 RELMain   0x20   bl fn_81_148
0x148 fn_81_148 0x30   lbl_81_bss_30 = fn_81_178 ; fn_80218D58(&lbl_81_bss_30)
```

### The claim starts at 0x4C, and that is forced, not caution

One unit cannot claim two discontiguous ranges, so taking the block above 0xD8 takes the three
functions in front of it too. `fn_81_0` (0x0, 0x8) and `fn_81_8` (0x8, 0x8) are free - they are the
`fn_2_0` / `fn_2_68` shape, vtable entries of the module's *second* vtable at 0x458 and 0x424 - but
`fn_81_10` (0x10, 0x3C) is **instruction for instruction MysteryFlyer's `fn_45_10`**: a member
function with a hidden return pointer in r3 returning `rstl::optional_object<CAABox>`, whose
converting ctor sets `m_valid` in the mem-init where retail copies the box and then sets the flag,
and whose template instantiation leaves a trailing pool. So extending this claim from 0x4C to 0x0
is blocked on the same problem, and it is worth **19 functions rather than 16** when it opens,
because `fn_81_0` and `fn_81_8` follow for free.

### The block is AtomicAlpha's, and that is measured

`Tryclops`' `.text 0x4C..0xD8` and `AtomicAlpha`'s `.text 0x10..0x9C` are both **0x8C = 140 bytes
and 35 instructions**, with an identical instruction multiset. (46 is the count for the wider ranges
that run on through the vtable entry - `0x4C..0x104` and `0x10..0xC8` - and only for those; an
earlier version of this section quoted 46 for the accessor block, which was wrong by the 11
instructions of `fn_81_D8` / `fn_2_9C`.) The `diff` (extracted from
`build/G2ME01/{Tryclops,AtomicAlpha}/asm/auto_00_00000000_text.s`, the working form of the
extraction `CFishCloudRel` records - the four bytes come *before* the `*/`) moves two lines and adds
none: Tryclops runs three `li r3,0; blr` predicates immediately after the byte read
(`lbz r3, 0x44f(r3)`) where AtomicAlpha runs two, its third sitting later beside the `li r3,0x1`.
**Print the line counts first** - an earlier attempt's first extraction silently produced 0 lines and
the `diff` was then vacuous, which is `docs/PROCESS_LESSONS.md`'s first lesson happening again.

So nothing had to be discovered. Every body is the one `CAtomicAlphaRel.cpp` /
`AtomicBetaAccessors.cpp` carries, and the two odd ones carry over with their caveats: `fn_81_BC` is
three subscript stores because retail interleaves the loads and the stores, and `fn_81_8C`'s dtk
`extrwi r3, r0, 1, 28` is the word `rlwinm r3, r0, 29, 31, 31` (bit 3, not bit 28).

The import needed no rename: `strings build/G2ME01/Tryclops/Tryclops.preplf | grep 80218D` gives the
plain DOL symbol `fn_80218D58`, and `build/G2ME01/asm/auto_03_80218D58_text.s` is
`stw r3, gLoader_Tryclops@sda21(r0); blr`. No mangled `SetLoader_*`, no `symbols.txt` edit, no DOL
change. The loader slot is `lbl_81_bss_30` (`.bss:0x30`, `size:0x4`). `fn_81_D8` is vtable entry
0x3C of a 0x148-byte table at `.data:0x378` (82 words: two leading plus eighty virtuals) whose
slot 0x38 is `HealthInfo__3CAiFv`, so the thirteen-virtual stand-in class is the one
`CIngPuddleRel.cpp` already measures.

**Thirteen of the sixteen, not sixteen, are in the module's FORCEACTIVE list**
(`build/G2ME01/Tryclops/ldscript.lcf`): `fn_81_4C` through `fn_81_D8`, the thirteen accessors and
the vtable entry. `RELExit`, `RELMain` and `fn_81_148` are not in it - an earlier version of this
section claimed all sixteen were. That is not a dead-stripping hazard, and the evidence that none
was stripped is the module's own hash holding plus `audit_rel_claim.py` reporting **0 of 114 text
symbols dropped by `-strip_partial`** (114 in, 114 out).

**Two of this section's own numbers were wrong and are corrected above** (third run, 2026-09-29,
lane 1, after a reviewer rejected the first version of it on exactly this point): the accessor block
is **35 instructions**, not 46 — 46 counts the range extended through the vtable entry — and
**13 of the 16** functions are in FORCEACTIVE, not all 16. Both were re-measured before being
restated, and the identical-multiset claim and the two-line-move claim were re-measured with them
and hold. The reusable lesson is the one this section already preaches further down: a number in a
doc is a claim about the tree, and the cheapest way to be wrong about one is to write it from the
run before last.

### The finding: on a REL unit, objdiff's 100% is not the module's verdict

Worth a lane of anyone's time, and the sharpest instance yet of "a percentage is not a result",
because **every REL-specific check passed** while the module was two bytes wrong.

`fn_81_4C` was first written with a one-character slip:

```cpp
*reinterpret_cast< float* >(static_cast< char* *>(self) + 0x448) = lbl_8041AAB8;   // char**, not char*
```

`mwcceppc` emitted `stfs f0, 0x1120(r3)` - pointer arithmetic on a `char**` advances by
`sizeof(char*)` = **4** on this ABI, so the displacement is `0x448 * 4`. Same length, still "an
offset into an object", nothing in the C++ reads as wrong. Measured with the typo in place:

| check | with the typo | with the fix |
| --- | --- | --- |
| `build/report.json`, per unit | **100.00% fuzzy, 16/16 matched** | 100.00%, 16/16 |
| `tools/unit_fit.sh` | `300 claimed / 300 ours / 300 retail, fits`, no extra functions | same |
| `tools/audit_rel_claim.py Tryclops` | `16/16 functions`, **0 claims with a problem** | same |
| `build.sha1` | **FAILED, 86 files OK** | 87 files OK |
| `cmp -l` vs `orig/.../Tryclops.rel` | bytes 283, 284 | identical |

**Why objdiff could not see it.** dtk's split leaves **two copies of the same object**: the one
objdiff reads, `build/G2ME01/Tryclops/obj/MetroidPrime/ScriptObjects/CTryclopsRel.o`, and the one
`build.ninja`'s `link build/G2ME01/Tryclops/Tryclops.preplf` rule hands to `mwldeppc`,
`build/G2ME01/src/MetroidPrime/ScriptObjects/CTryclopsRel.o`. With the typo the first held
`d0 03 04 48` and the second `d0 03 11 20` - same compile, two objects, different bytes, so the
report was measuring bytes the module did not contain. `objcopy -O binary --only-section=.text` on
both files shows it in one command:

```sh
for o in build/G2ME01/Tryclops/obj/MetroidPrime/ScriptObjects/CTryclopsRel.o \
         build/G2ME01/src/MetroidPrime/ScriptObjects/CTryclopsRel.o; do
  build/binutils/powerpc-eabi-objcopy -O binary --only-section=.text "$o" /tmp/x.bin && xxd -l 16 -p /tmp/x.bin
done
```

Why the split copy differs was not established, only that it does and that it is the one objdiff
reads. The rule to keep is the one this project already has, now with a measurement attached: **on
a REL, the module's sha1 against `config/G2ME01/config.yml` is the only verdict**, and
`cmp -l <built> orig/G2ME01/files/RelProd/<M>.rel` is the cheap second opinion. It printed bytes 283
and 284; this module's `.text` starts at file offset `0xC4` (find it by searching the `.rel` for
`fn_81_0`'s first word `38 63 07 C4`), so those are `.text:0x56`/`.text:0x57` - the low half of the
`stfs` displacement at `.text:0x54` inside `fn_81_4C`, and nowhere else.

**Second habit, worth more than the fix.** `mwcceppc` on one file takes **0.04 s**. Compiling the
unit standalone with the exact `mwcc_sjis` command line from `build.ninja` and running
`objdump -d` on it localises a wrong body in seconds instead of after a full link, and it is
non-vacuous because the object's own bytes are what is being read. The recipe is the `mwcc_sjis`
rule's own command with `-c` added and the output redirected somewhere disposable:

```sh
"$MP_TOOLCHAIN_DIR/build/tools/wibo" build/tools/sjiswrap.exe \
  "$MP_TOOLCHAIN_DIR/build/compilers/GC/1.3.2/mwcceppc.exe" <cflags from build.ninja> -c -o /tmp/u.o src/.../U.cpp
```

Without `-c` the driver aborts with "Can't find linker 'mwldeppc' in path" - it is not a real error.
Then `objcopy -O binary --only-section=.text` and a `cmp` against the retail bytes at the claim's
offset settles it. The expected differences are only relocation fields: for this unit exactly five
bytes, at `.text:0xCA`, `0xEB`, `0x11A` (the three `bl` R_PPC_REL24 displacements) and `0xCB`,
`0x11B` (two R_PPC_ADDR16_LO), everything else identical to retail.

## What is left (not this item)

`fn_81_178` (0x178, 0x30C) is the module's entity loader and the 89 functions from there to
`fn_81_5028` are Tryclops' methods; dtk fills `0x178..0x54EC` as
`Tryclops/auto_00_00000178_text`. Behavioural class code, and it needs the CActor/CPatterned
hierarchy. The cheap step inside it is the same shape as `AtomicAlpha`: nothing below 0x4C is left
unclaimed-by-choice, so the next extension of this claim is the three functions above 0x4C, and
they are waiting on `fn_81_10`.

## The module head is writable without the actor hierarchy (2026-09-29, goal item `progress-rel-head-ingblobswarm`)

`IngBlobSwarm`'s first five functions are ours now, in one `Matching` unit:
`src/MetroidPrime/ScriptObjects/CScriptIngBlobSwarmRel.cpp` claims module `.text 0x0..0xD8`
and reproduces it byte for byte - `fn_31_0`, `fn_31_3C`, `RELExit`, `RELMain` and
`fn_31_A8` - so `matched` went 8828 -> 8833, `linked` 3875 -> 3880, the DOL sha1 held and
all 86 REL hashes matched `config/G2ME01/config.yml`. The class code behind them is still
retail in the unclaimed `auto_00_000000D8_text` range, and that is the point worth keeping:
**a swarm module's head is `.text` wiring, not behaviour, so it is decompilable before
`CActor`/`CPatterned` exist.** The arrangement is `CScriptPlayerProxy.cpp` verbatim, and
three details are not obvious:

- **The loader setter is the DOL's, and its import name is fixed.** `fn_31_A8` stores
  `fn_31_D8` into the module's own `.bss` slot and calls `fn_8022E134` - eight bytes at
  0x8022E134 in the *DOL*, `stw r3,gLoader_IngBlobSwarm@sda21(r0) ; blr`. The name is in
  the module's `.plf` import table, so `SetLoader_IngBlobSwarm` or anything else does not
  link. (Carving it is possible and is what `RsfAudioLoaderSet.cpp` did for that family,
  but nothing here needs it: the relocation already resolves against the unclaimed
  `auto_03_8022E134_text` object.)
- **The `.bss` slot must be `extern` under `__MWERKS__`.** A definition there is a second
  definition in the module link, and `CScriptRsfAudio.cpp` records the
  `ELF_linker.c Line: 5083` internal linker error that comes with it.
- **A REL entry source is not always listed in `files.cmake`.** `fn_31_D8`, the module's
  own 1,036-byte loader, has no body, so compiling this file on the host would take the
  port's undefined count 314 -> 315, which `tools/link_check.sh --strict` fails.
  `tools/check_files_cmake.py` accepts it anyway because the file is a `MODULE_ENTRY`
  source - the same reason `CSwarmBasicsREL.cpp`, `CScriptPlayerActor.cpp` and
  `CScriptPlayerTurretRel.cpp` are out - so the port keeps loading the retail module from
  the disc and the host `mp_relmain_ingblobswarm` is simply never called. Registering it
  needs `fn_31_D8`, so it is queued as a follow-up rather than done here.

Two mwcceppc facts turned up while matching the two accessors, and both are the kind that
cost a build each:

- **`*out = CVector3f(a, b, c)` is what produces retail's load-all-then-store float order.**
  `fn_31_3C` wants `lfs f2,44(r4) ; lfs f1,28(r4) ; lfs f0,12(r4) ; stfs f0,0(r3) ;
  stfs f1,4(r3) ; stfs f2,8(r3)`. Three separate `out->SetX/SetY/SetZ` statements emit
  load-store-load-store and score **58.30%**; the single `*out = CVector3f(...)` assignment
  is 100.00% with the same object size.
- **A `bool : 1` bitfield is what produces a tested bit; masking a byte does not.**
  `result = (byte & 1) != 0` gives `clrlwi r0,r0,31 ; mr r5,r0` and a one-bit result that
  retail never produces. A struct member `bool flag : 1;` read as `result = flag;` gives
  `rlwinm r0,r0,25,31,31` (78.27%) - and only writing it as `if (flag) { result = true; }`
  makes MWCC use the **recording** form `rlwinm.` + `beq` + `li r5,1` that retail has.
  This is the same `const`-local lever as `CGameOptions::ToggleControls`, one level over:
  a bitfield read used as a *value* is not the same code as a bitfield read used as a
  *condition*.

The array's element is 0xB8 and the blob's one-bit "is emitter" flag is at +0xB2 of it;
the blob count is the word at +0x17C and the array pointer the one at +0x184, both read by
the two accessors. A 0xB8 stride with a vector at +0x0C is what the second accessor pins;
the struct in the source is `char[0xB2]`, `bool : 1`, `char[0x5]`, and its **size is
load-bearing** - leaving the trailing pad at 6 bytes gives `mulli r4,r4,185` where retail
has 184, one instruction and a wrong stride.

## `CPillBugRel` is a module head, and the accessor block is already in the DOL (2026-09-29, goal item `progress-rel-head-pillbug`)

(Rescued from the review queue the same day: the lane's code passed every gate but raw-offsets, which
wanted a `docs/research/raw_offsets.md` section for the file; the section was added and nothing else changed.)

PillBug is module 48, one of the 27 modules whose `REL_Setup` tail was claimed on 2026-09-28.
The head is now the whole `.text 0x0..0x130` - **seventeen** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CPillBugRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged**
(`261c9127510cc69a1ce664e6e07dc4165e9d9aca`, `cmp`-equal to `orig/G2ME01/files/RelProd/PillBug.rel`),
with all 86 holding and `main.dol` still `6ef9b491...`. `matched` 9145 -> 9162, `linked` 3985 -> 4002,
the module's own count 5 -> **22 of 82**. `tools/audit_rel_claim.py PillBug` reports 0 problem claims
and 0 of 82 text symbols dropped by `-strip_partial`; `tools/check_decl_order.py --unit
PillBug/MetroidPrime/ScriptObjects/CPillBugRel` is ok; `tools/unit_fit.sh` reports
`.text claimed 304 ours 304 retail 304 fits` with no extra functions; `tools/flip_test.sh` on the
unit reports `PASS -> kept as Matching`.

**The measurement that made this cheap is one grep, and it should be run before writing any C++ for
a module head.** The advice above was to diff the module's first 0x200 bytes against a head that
already landed. The better check is that the *thirteen short accessors* at `0x0..0x90` are the block
the REL loader generator emits at the head of every scripted-actor module, and they **already exist
in this tree, in the DOL, at 100%** - they share the three globals the relocations name,
`lbl_8041AAB8`, `kInvalidUniqueId` and `lbl_8041B758`, all of which live in the DOL, so one body
serves every module of the family:

    grep -l lbl_8041AAB8 src/

That returns 24 files, of which the `*Accessors.cpp` units are the block;
`MetroidPrime/ScriptObjects/GlowbugAccessors.cpp` is the reference body. 

**But the blocks are not interchangeable, and that is the trap.** PillBug's is thirteen accessors plus
a vtable call; `EmperorIngStage2Tentacle`'s is fourteen accessors with **no** vtable call - it has a
`bool : 1` flag test at `+0x34c` (`rlwinm r3,r0,29,31,31`, its `fn_17_48`) where PillBug has
`fn_48_90`'s `lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr r12 / bctrl`. The two blocks differ by exactly
those functions - 188 bytes against 156 - **and every offset after `0x38` is shifted by 0x20.** Read
the offsets out of the module's own `symbols.txt`; never copy an offset from a sibling's file.

Two spellings transferred without being re-found:

- **`fn_48_74` is three stores, not a `CVector3f`.** Retail reuses f0 for all three
  (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 / stfs f0,4 / lfs f0,0x5c / stfs f0,8`) - interleaved
  load/store pairs - which is `fn_17_80`'s shape exactly and the **opposite** of `fn_43_3C` in
  `CMetareeSwarmRel.cpp`, where the three floats had to be *built* through `CVector3f`'s
  three-argument constructor because retail loaded all three before storing any (58.30% -> 100.00%).
  **Both shapes are real and the disassembly says which: interleaved load/store pairs are a copy,
  all-loads-first is a constructor call.**
- **`fn_48_90` is a vtable call, and the slot is `CAi`'s `HealthInfo`, not `CActor`'s.**
  `.data:0x1CC` is CPillBug's 83-word table, with `fn_48_90` at offset 0x3C, one after
  `HealthInfo__3CAiFv` at 0x38 - the table is **CAi's**, `TypesMatch__8CPillBugCFi` and
  `DamageVulnerability__3CAiFv` are in it - so it needs fourteen virtuals where CSnakeWeedSwarm's
  CActor table needed 29. As at `CIngPuddleRel.cpp` a stand-in class of thirteen virtuals puts the
  *called* one at `(0x38 - 8) / 4` = index 12 and gives retail's twelve instructions byte for byte;
  none of the slots is defined, because the only object carrying the vtable is the module's own
  retail bytes.

`RELMain`, `RELExit` and `fn_48_100` are the `CScriptPlayerProxy.cpp` / `CMetareeSwarmRel.cpp`
arrangement unchanged, including the `extern`-under-MWCC `.bss` slot (`lbl_48_bss_0` is `.bss:0x0`,
`size:0x4`). The setter is `fn_80200F30`, the DOL's 0x80200F30, two instructions,
`stw r3, -0x6A10(r13); blr`; that displacement resolves through `_SDA_BASE_` to 0x80419370, which
`config/G2ME01/symbols.txt` names **`gLoader_PillBug`**, and `src/MetroidPrime/ScriptLoader/PillBug.cpp`
reads it as `(*gLoader_PillBug.value)(mgr, input, info)`. `tools/sda.py` is the supported way to make
that resolution - reading the displacement against the wrong base gives a plausible wrong answer.
**Not added to `files.cmake`**, for the same reason as `CMetareeSwarmRel.cpp` above: a host body
would reference `fn_48_130` and `fn_80200F30`, which the port cannot link.

### What is left

`fn_48_130` (0x130, 0x5A4) is the module's entity loader, and the 59 functions from there to
`fn_48_402C` are CPillBug's methods. None is claimed; dtk fills `0x130..0x402C` from retail. That is
class code and needs the CActor/CPatterned/CAi hierarchy - the blocker the item's `reason` names, and
the same one the other four heads stopped at. The module is 82 functions: 59 in
`auto_00_00000130_text`, 1 in `auto_fn_48_402C_text`, 5 in `REL_Setup`, 17 claimed here.

## `CDarkTrooperRel` is a module head, and the accessor block is a sibling's in another order (2026-09-29, goal item `progress-rel-head-darktrooper`)

DarkTrooper is module 12, one of the 27 modules whose `REL_Setup` tail was claimed on 2026-09-28.
The head is now the whole `.text 0x0..0x12C` - **sixteen** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CDarkTrooperRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged**
(`f216a5cdbf86753fa7d3e8a49b7e0344fbb2dbca`, `cmp`-equal to `orig/G2ME01/files/RelProd/DarkTrooper.rel`),
with all 86 holding and `main.dol` still `6ef9b491...`. `matched` 9185 -> 9201, `linked` 4011 -> 4027,
REL units 1138 -> 1154, the module's own count 5 -> **21 of 172**, and
`tools/check_module_wiring.py` **63 units in 49 modules -> 64 in 50**. `tools/audit_rel_claim.py
DarkTrooper` reports 0 problem claims and 0 of 172 text symbols dropped by `-strip_partial`;
`tools/check_decl_order.py` is ok; `tools/unit_fit.sh` reports `.text claimed 300 ours 300 retail
300 fits` with no extra functions; `tools/flip_test.sh` reports `PASS -> kept as Matching`.

**The claim reaches 0x0 because the head and the accessor block happen to be adjacent, and the two
must be read as one run rather than two.** `RELExit` is at **0xB8**, not at 0x0, and thirteen
functions sit in front of it - twelve accessors and `fn_12_8C`, the vtable call, where PillBug runs
one longer (`fn_48_0`..`fn_48_74` is thirteen accessors, plus its own `fn_48_90`) - so
`CMysteryFlyerRel.cpp`'s loader-trio claim, `0xFC..0x170`, and
`CBacteriaSwarmRel.cpp`'s `0x0..0xA0` are the two shapes this module does not have: here the claim is
0x0..0x12C, the accessor block plus the trio, or nothing. One unit cannot claim two discontiguous
ranges, so the alternative would be two units and a gap between them. Worth reading the module's
`symbols.txt` for where `RELMain` actually sits before deciding a head is only a trio - in
`MetareeSwarm` it is at 0x88 and in `IngPuddle` and `IngSnatchingSwarm` at 0x58, so the trio is *not*
at the module's start in this family either.

**The block is `CPillBugRel`'s, and the diff between the two modules is small enough to state as
facts - which is what made this cheap.** `diff` of the two `auto_00_00000000_text` listings over
their accessor runs, checked non-empty, moves three things and adds none:

| | PillBug (48) | DarkTrooper (12) |
|---|---|---|
| at 0x0 | `fn_48_0`, the 0x10-byte float store at +0x448 | `fn_12_0`, 8 bytes, `addi r3,r3,0x7c0` - the address of a member at +0x7C0 |
| the `li r3,0; blr` run | four, at 0x18/0x20/0x28/0x30 (six over the block) | one, at 0x20 (three over the block) |
| the `+0x34C` flag bit | absent | `fn_12_38`, `lbz r0,0x34c(r3) / extrwi r3,r0,1,28 / blr` |

Everything else - the float store (0x08 here, 0x00 there), the byte at +0x44F, the unique-id reset,
the constant float, `&self[0x754]`, the `true`/`false`/`false` tail, the 12-byte copy and the
12-instruction vtable call - is the same body in both, so **no spelling had to be discovered** and
the run took one build. Two spellings transferred with it, both from `CPillBugRel.cpp`: the copy is
interleaved load/store (a copy, not a `CVector3f` constructor call) and the vtable call needs a
thirteen-virtual stand-in class.

**The flag bit's encoding transferred with the rest of the block, verbatim.** The source is
`AtomicBetaAccessors.cpp`'s `fn_5_48` - `(self[0x34C] & 8) != 0`, over the same offset, the same
fourth `bool : 1` of that byte - and the two objects hold the same three instructions,
`88 03 03 4C / 54 03 EF FE / 4E 80 00 20`, with `extrwi r3, r0, 1, 28` in both listings and one
copy of that body in `orig/G2ME01/files/RelProd/AtomicBeta.rel`. **An earlier draft of this section
claimed the encoding did *not* transfer, and said AtomicBeta compiled the same source to
`rlwinm r3,r0,29,31,31`; that was copied from the sibling's own comment at
`AtomicBetaAccessors.cpp:57` and is superseded.** That word is stale everywhere it appears - 18
`*Accessors.cpp` files carry the same comment and `rlwinm r3, r0, 29, 31, 31` appears nowhere in
`build/` - so the sibling comment is the thing to correct, not this module. A lane that had trusted
it would have written the wrong instruction here.

**The import needed no rename, which is worth saying because `CIngSnatchingSwarmRel`'s did.** The
setter is the plain DOL symbol `fn_80218DF4`, read out of the module's own `DarkTrooper.preplf` -
the same shape as `BacteriaSwarm`'s `fn_8022A5AC` and `Tryclops`'s `fn_80218D58`, so
`config/G2ME01/rels/DarkTrooper/symbols.txt` and the DOL are untouched. The loader slot is
`lbl_12_bss_0` at `.bss:0x0`, `size:0x4`, so the `extern`-under-MWCC arrangement applies unchanged.
`fn_12_8C` is vtable entry **0x3C** of the 106-word table at `.data:0x2BC` calling slot 0x38
(`HealthInfo__3CAiFv`), the `fn_6_0` / `fn_32_8` shape, so the same stand-in class lands it.

**Not added to `files.cmake`**, for the same reason as the other module heads: a host body would
reference `fn_12_12C` and `fn_80218DF4`, which the port cannot link, and the probe's regression gate
is a hard failure on a growing undefined count. Measured here: **259 -> 259 undefined, 0
duplicates**. `tools/check_files_cmake.py` accepts the omission because the file defines `RELMain`
and `RELExit`, the `MODULE_ENTRY` rule every head relies on.

### What is left

`fn_12_12C` (0x12C, 0x614) is the module's entity loader, and the 151 functions from there to
`fn_12_6E58` are CDarkTrooper's methods. None is claimed; dtk fills `0x12C..0x6E58` from retail. That
is class code and needs the CActor/CPatterned/CAi hierarchy - the same blocker the other heads
stop at. The module is 172 text functions: 150 in `auto_00_0000012C_text`, 1 in
`auto_fn_12_6E58_text`, 5 in `REL_Setup`, 16 claimed here.

## `CGameState` 72 -> 86: two reviewer-rejected lanes, landed code-only (2026-09-29)

Goal items `progress-cgamestate-bodiless-runs` (lane 1, run 11) and `progress-cgamestate-partial-16`
(lane 1, run 4) both passed `goal_check.sh`, and both reviewers said the code was right and rejected
the diff over its doc prose (unmeasured or stale numbers). Both were set aside after two failures. They
were re-applied on the current tip with **only their `src/` and `include/` hunks** (`partial-16`
needed `git apply --3way`), and the docs were written from fresh measurements. `main.dol` is still
`6ef9b491...`. The two lanes' notes stay in `build/goal/notes/` in the goal worktree.

Measured on the unit from `build/report.json`, in two steps (bodiless-runs first: the `no body` rows; then partial-16: the rest):

| function | before | after | spelling |
|---|---|---|---|
| `fn_801447C4` (CHintOptions copy-assign) | no body | 100.00% | `extern "C"`, declared *before* the class so the `friend` names the C-linkage entity |
| `fn_801465A8` | no body | 100.00% | `{u32, float, float}` per element, `dst` null-tested inside the loop |
| `fn_801467A0`, `fn_801467C0`, `fn_801435D4`, `fn_80142718`, `fn_80142738` | no body | 100.00% | 36-byte element forwarders and destroy loop |
| `fn_8014601C` | no body | 99.05% | two-word out-parameter over `fn_80145BDC` |
| `fn_801426E0` | no body | 97.50% | count bumped *before* the element is built |
| `fn_8014680C` | no body | 92.69% | range copy from a by-address begin/end |
| `fn_801466F4` | no body | 66.63% | the four-word stack range built by separate assignments |
| `ConfigureGameModeLayers` | 94.64% | 100.00% | `(second - type) == 0`, table value on the **left**; `a == b` and `type - second == 0` give `subf` in the wrong operand order. The rc_ptr's pointee is bound to a named `CWorldLayerState&` |
| `StateForWorld` | 80.66% | 100.00% | loop *breaks* to one end test; `it` hoisted, `end` re-read at each test |
| `SetCinematicState` | 76.45% | 100.00% | `push_back_unsafe` after `reserve`, plus a `construct_impl` overload for `pair<CAssetId, TEditorId>` in `CPersistentOptions.hpp` (no null test on placement) |
| `CEnvironmentVariable` ctor / `PutTo` | 76.43 / 70.41% | 100.00% | read back the stored members, not the parameters; the difference into a local before `GetBitCount` |
| `FindEnvironmentVariable` | 81.08% | 100.00% | `end` bound to a local declared **after** the find, then `it != end ? ... : nullptr` |
| `InitializeMemoryWorlds` | 99.57% | 100.00% | the three `InitializeWorldLayers` arguments as named locals, in r4/r5/r6 order |
| `AddVariable` | 82.88% | 87.78% | the same `end` hoist, `it == end` polarity |
| `CPersistentOptions::PutTo` | 84.67% | 94.35% | `push_back_unsafe` |

**Walls still open**, from the lanes' notes and not re-measured here: `fn_8014601C`, `fn_801426E0` and
`fn_8014680C` differ only in the epilogue's r0/r31/r30 reload order. `fn_801466F4` needs the stack
range's four stores in retail's order.

## A `files.cmake` head needs `#ifdef __MWERKS__`, and the failure is `link_gap`, not `build.sha1` (2026-09-29, goal item `progress-rel-extend-shredder`, lane 2)

`Shredder`'s claim was `.text 0x3C..0xC8`, twelve accessors, and the 60 bytes below it held
`fn_68_0` - a `GetBoundingBox` wrapper, the one function every head in this family opens with. The
claim now runs `0x0..0xC8`: 13/13 functions at 100.00%, module sha1
`a70ac4a192c4ea785af74c09179b6fd64c8ed382` unchanged, `.rel` `cmp`-identical, all 86 holding,
`main.dol` `6ef9b491...`, `matched` 9176 -> 9177, `linked` 4002 -> 4003, the module's own count
12 -> 13 of 62 (`auto_00_000000C8_text` 43 + `auto_fn_68_2888_text` 1 + `auto_00_00002940_text` 5 +
the 13 here, which is all 62 text symbols `audit_rel_claim.py` prints and 0 are dropped by
`-strip_partial`).

**The body was not the hard part; the host build was.** `fn_68_0` is instruction for instruction
`CMysteryFlyerRel.cpp`'s `fn_45_10` (module 45, landed two commits before this item), so the
spelling transferred without being re-found - 15 instructions, frame 0x30,
`GetBoundingBox` in r4 alongside the hidden return pointer, so `self` needs no move:

    void fn_68_0(void* out, const CPhysicsActor* self) {
      fn_68_284C(out, self->GetBoundingBox());   // .text 0x284C, unclaimed
    }

`fn_68_284C` is the module's own out-of-line `optional_object<CAABox>` converting constructor (six
words copied, then `stb 1,0x18(r3)`); it is **not** in this claim and is called by name. The
parameter must be `const CAABox&`: by value the frame grows to 0x40 and the unit stops matching.
`CPhysicsActor` is the one-method stand-in, not `MetroidPrime/CPhysicsActor.hpp` - that header
reaches `Collision/CMaterialList.hpp`, whose file-scope statics put 0x28 bytes of `.data` in the
object, and the module's sha1 breaks on it with every function still at 100%.

**Every other head in this family is deliberately *absent* from `files.cmake`** - a head body would
reference the module's own functions, which the port cannot link, and the port's link gap would
grow. The accessor units are the exception, and all 20 of them **are** listed, this one among them:
they were safe because they read raw offsets and DOL globals and call nothing. `fn_68_0` breaks
that, and **the failure is not a byte diff**: it is `gate.sh`'s `link_gap` step, and a green
`build.sha1` and a green `All:` line will not show it. The fix is the `#ifdef __MWERKS__` arrangement
`CScriptWallCrawler.cpp` already uses for `RELMain`/`RELExit`, around the include, the stand-in, the
declaration and the body. The port reads `Shredder.rel` off the disc through `platform/rel.cpp` and
never calls into the module, so it costs nothing, and the MWCC branch is the retail source token for
token so the matching build cannot see it.

The check is one command, not a gate run, and it is worth doing **before** proposing any function
that calls out of a module in a file `files.cmake` already lists - which now means every one of the
20 accessor units, not just this one:

    # the host g++ line out of tools/probe_sources.sh, with the guard removed, then:
    build/binutils/powerpc-eabi-nm -u <that>.o

**Declare in reverse, and note what failed to catch a permuted claim.** `fn_68_0` is at retail
0x0, the lowest offset in the claim, so it is declared **last** in the file. Declared first, the
module's bytes came out permuted: `Shredder.rel` FAILED with `86 files OK`, and `cmp -l` showed the
`.text` running 0x8C further from the file head than retail's - a 0x8C shift. **objdiff reported
13/13 at 100.00% and `unit_fit.sh` said "fits" the whole time.** `check_decl_order.py --unit` printed
`ok` on the permuted source too, because it compares the *built* object's symbol order against
`build/report.json` and the build had not been re-run, so it was still looking at the previous
(correct) object - it is a pre-flip check for `NonMatching` units and **needs a build behind it**.
The instruments that saw it were the module sha1 and the `cmp`.

### The family, measured rather than assumed

The obvious follow-up - "every accessor module has a wrapper in front of it, so take it" - is
**half right, and the half that is wrong costs a session.** Measured across all 20 modules whose
claim is a `*Accessors.cpp`, from each `config/G2ME01/rels/<M>/splits.txt`:

| gap below the claim | modules | the function at 0x0 is |
| --- | --- | --- |
| 0 | `AtomicBeta`, `EmperorIngStage2Tentacle`, `Kralee`, `OctapedeSegment`, `WallWalker` | already claimed |
| **0x3C (60)** | **`Shredder` (now), `Ripper` (now), `EyeBall`, `Krocuss` (now)** | **`GetBoundingBox` wrapper, 0x3C bytes** |
| 0x44 (68) | `IngSpiderballGuardian` | a different 0x44-byte head |
| **0x3C (60)** | **`Shredder` (now), `Ripper`, `EyeBall` (now), `Krocuss` (now)** | **`GetBoundingBox` wrapper, 0x3C bytes** |
| 0x44 (68) | `IngSpiderballGuardian` (now) | a 0x08-byte predicate at 0x0, then the 0x3C wrapper at 0x8 |
| 0x78 (120) | `DigitalGuardian` | a different 0x78-byte head |
| 0x2F0 (752) .. 0x38C (908) | `PuddleSpore`, `Sporb`, `WispTentacle`, `SpankWeed`, `GunTurret`, `Glowbug`, `StoneToad` | **not a copy job - hundreds of bytes of module code** |

So the one-function extension is real and was three modules, and each was verified per module
rather than assumed: `Ripper` `fn_54_0`, `EyeBall` `fn_19_0` and `Krocuss` `fn_38_0` are each
`0x3C` bytes and each is `mr r31,r3 / bl GetBoundingBox__13CPhysicsActorCFv / bl fn_<mod>_<ctor>` -
the same wrapper, with the module number substituted. **Re-measure each module's
`config/G2ME01/rels/<M>/symbols.txt` before assuming the function below 0x0 is a `GetBoundingBox`
wrapper; a 0x2F0-byte gap is not the same problem as a 0x3C-byte one.** Of the three, `Shredder`,
`Ripper` and `Krocuss` have been taken; `EyeBall` has not. (`Ripper` is wired as of
the same wrapper, with the module number substituted. `EyeBall` and `Krocuss` are now claimed;
`Ripper` is the only one of the three still open, and it is the last clean instance of this shape.
`IngSpiderballGuardian` was the fourth to land and is the one that falsifies "the wrapper is always
at 0x0": there the head is 0x44 bytes and holds *two* functions, so the wrapper is at 0x8.
**Re-measure each module's `config/G2ME01/rels/<M>/symbols.txt`, and read
`build/G2ME01/<Module>/asm/auto_00_00000000_text.s` before assuming the function below 0x0 is a
`GetBoundingBox` wrapper: a 0x2F0-byte gap is not the same problem as a 0x3C-byte one, and a 0x44-byte
gap is not a 0x3C-byte gap either.** (`Ripper` is wired as of
2026-09-29: `check_module_wiring.py` reports 63 units of our own code in 49 modules and names it,
and `include/MetroidPrime/Enemies/CPatterned.hpp` exists - the "blocked, no CRipper/CPatterned"
note from an earlier run is superseded, though only for the head: `fn_54_178` is still class code.)

### What is left

`fn_68_C8` (0xC8) and the 49 functions above it are Shredder's own members and need the
CActor/CPatterned hierarchy. The module has no `REL_Setup` claim, so 13 + 49 = 62 is the complete
denominator here.

## `Krocuss` is the third `*Accessors.cpp` head extended past the wrapper (2026-09-29, goal item `progress-rel-extend-krocuss`, lane 2)

`Krocuss`'s claim was `.text 0x3C..0xD8`, fourteen accessors, and the 60 bytes below it held
`fn_38_0` - the `GetBoundingBox` wrapper that opens every head in this family. The claim now runs
`0x0..0xD8`: **15/15 functions at 100.00%**, module sha1
`fec35d1d4bd7e6815c37af398be8c1b0ff2864fa` unchanged against `config/G2ME01/config.yml` and equal to
`orig/G2ME01/files/RelProd/Krocuss.rel`, the `.rel` `cmp`-identical, all 86 holding, `main.dol`
`6ef9b491...`, `matched` 9177 -> 9178, `linked` 4003 -> 4004, the module's own count 14 -> **15 of
65** (`tools/audit_rel_claim.py Krocuss`: 65 text symbols in the preplf, 65 in the plf, 0 dropped by
`-strip_partial`, 0 problem claims). `unit_fit.sh`: claimed 216, ours 216, retail 216, **no extra
functions** - the object defines only what the retail unit object does.

**Nothing had to be re-derived.** `fn_38_0` is 15 instructions and is byte-identical to
`CMysteryFlyerRel.cpp`'s `fn_45_10` and to `ShredderAccessors.cpp`'s `fn_68_0`, so the body
transferred verbatim:

    void fn_38_0(void* out, const CPhysicsActor* self) {
      fn_38_22F0(out, self->GetBoundingBox());   // .text 0x22F0, unclaimed
    }

`fn_38_22F0` is this module's own out-of-line `optional_object<CAABox>` converting constructor
(`build/G2ME01/Krocuss/asm/auto_00_000000D8_text.s`: `li r0,1`, six `lwz`/`stw` pairs, `stb r0,0x18(r3)`,
`blr`) - it is at 0x22F0, in `auto_00_000000D8_text`, and stays unclaimed, so it is called by name.
`const CAABox&` is load-bearing again: by value the frame grows to 0x40 and the unit stops matching.
Retail takes the box by address in a 0x30 frame and needs no move for `self`, because
`GetBoundingBox__13CPhysicsActorCFv` takes `this` in r4 as well. The one-method `CPhysicsActor`
stand-in is not optional: `MetroidPrime/CPhysicsActor.hpp` reaches `Collision/CMaterialList.hpp`,
whose file-scope statics put 0x28 bytes of `.data` in the object, and the module's sha1 breaks on it
with every function still at 100%.

**The `#ifdef __MWERKS__` guard is what the item actually turned on.** `KrocussAccessors.cpp` is
listed in `files.cmake` (like all 20 `*Accessors.cpp` units there), and the module *head* files are
deliberately not, because a head body makes the host port link the module's own functions. Measured
with `tools/link_check.sh` after the change: **unique undefined symbols 259, duplicate definitions 0** -
unchanged, so the guard costs the port nothing and `gate.sh`'s `link_gap` step is green. The failure
mode without it is that step and not a byte diff: `build.sha1` and the `All:` line both stay green
while the port's gap grows. The port reads `Krocuss.rel` off the disc through `platform/rel.cpp` and
never calls into the module, exactly as `CScriptWallCrawler.cpp` arranges for its `RELMain`/`RELExit`.

### What is left here

`fn_38_D8` (0xD8) and the 49 functions above it in `auto_00_000000D8_text` are Krocuss's own members
and need the CActor/CPatterned hierarchy. `Krocuss` has no `REL_Setup` claim, so 15 + 44 + 1 + 5 = 65
is the complete denominator `audit_rel_claim.py` prints.

## `IngSpiderballGuardian` is the fourth `*Accessors.cpp` head extended past the wrapper (2026-09-29, goal item `progress-rel-extend-ingspiderballguardian`, lane 2)

`IngSpiderballGuardian`'s claim was `.text 0x44..0xD8`, thirteen accessors, and the 68 bytes below it
held **two** functions, not one. The claim now runs `0x0..0xD8`: **15/15 functions at 100.00%**,
module sha1 `2c171d03c7ee30a71249350731ad43256c96098d` unchanged against
`config/G2ME01/config.yml` and equal to `orig/G2ME01/files/RelProd/IngSpiderballGuardian.rel`, the
`.rel` `cmp`-identical, **all 86** REL sha1s re-checked and holding, `main.dol` `6ef9b491...`,
`matched` 9178 -> 9180, `linked` 4004 -> 4006, the module's own count 13 -> **15 of 87**
(`tools/audit_rel_claim.py IngSpiderballGuardian`: 87 text symbols in the preplf, 87 in the plf, 0
dropped by `-strip_partial`, 0 problem claims). `unit_fit.sh`: claimed 216, ours 216, retail 216,
**no extra functions**. `check_symbol_names.py` 484 units / 0 missing; `check_raw_offsets.py` 128
sites in 46 files, all documented, unchanged; `probe_sources.sh` 727 source files (then) / 0 failed and the port
link **259 undefined, 0 duplicates**, equal to the baseline.

**The one thing to re-measure, and it is not the wrapper.** `Krocuss` and `Shredder` each had one
function below the accessor block and it was the wrapper itself, at 0x0. Here there are two, so the
wrapper is at **0x8** and `fn_35_0` sits in front of it. The dtk `fn_<id>_<off>` name does not say
which is which, so the head's own dtk output - `build/G2ME01/IngSpiderballGuardian/asm/auto_00_00000000_text.s`,
68 bytes, exactly the range being added - is the only place to read it:

    fn_35_0   0x0  0x08  li r3,0x1 / blr
    fn_35_8   0x8  0x3C  stwu r1,-0x30(r1); mflr r0; stw r0,0x34(r1); stw r31,0x2c(r1)
                   mr r31,r3; addi r3,r1,0x8; bl GetBoundingBox__13CPhysicsActorCFv
                   mr r3,r31; addi r4,r1,0x8; bl fn_35_4030
                   lwz r0,0x34(r1); lwz r31,0x2c(r1); mtlr r0; addi r1,r1,0x30; blr

`fn_35_8` transferred verbatim from `CMysteryFlyerRel.cpp`'s `fn_45_10`, the same 15 instructions:

    void fn_35_8(void* out, const CPhysicsActor* self) {
      fn_35_4030(out, self->GetBoundingBox());
    }

`fn_35_4030` (`build/G2ME01/IngSpiderballGuardian/asm/auto_00_000000D8_text.s`) is this module's own
out-of-line `optional_object<CAABox>` converting constructor - `li r0,1`, six `lwz`/`stw` pairs,
`stb r0,0x18(r3)`, `blr` - and it sits in `auto_00_000000D8_text`, so it stays unclaimed and is
called by name. `const CAABox&` is load-bearing again: by value the 0x30 frame grows to 0x40 and the
unit stops matching. The `CPhysicsActor` stand-in is the one-method local class, not
`MetroidPrime/CPhysicsActor.hpp`, whose `CMaterialList` statics would put 0x28 bytes of `.data` in
the object and break the module sha1 with every function at 100%.

`fn_35_0` is `bool fn_35_0(void*) { return true; }` - `li r3,1; blr`, a real body, the same
always-true predicate `fn_45_0` and `fn_38_0`'s neighbours carry, and not a stub for the wrapper.

**The `#ifdef __MWERKS__` guard is `KrocussAccessors.cpp`'s and is load-bearing here too.** The
head bodies are not listed in `files.cmake` because a head body makes the host port link the module's
own functions, which it cannot. `IngSpiderballGuardianAccessors.cpp` *is* listed, and it was safe
only because it read raw offsets and DOL globals; `fn_35_8` makes the host link `fn_35_4030` and a
host-mangled `CPhysicsActor::GetBoundingBox`. `probe_sources.sh` measured 259 undefined both before
and after, so with the guard the host build is byte-identical; without it `link_gap`, and not
`build.sha1`, is the step that would have failed.

### What is left here

`fn_35_D8` (0xD8) and the 66 functions above it in `auto_00_000000D8_text` are
IngSpiderballGuardian's own members and need the CActor/CPatterned hierarchy.
`IngSpiderballGuardian` has no `REL_Setup` claim, so 15 + 66 + 1 + 5 = 87 is the complete
denominator `audit_rel_claim.py` prints.

## `Ripper` is the fourth `*Accessors.cpp` head extended past the wrapper (2026-09-29, goal item `progress-rel-extend-ripper`, lane 1)

`Ripper`'s claim was `.text 0x3C..0xD8`, fourteen accessors, and the 60 bytes below it held
`fn_54_0` - the `GetBoundingBox` wrapper that opens every head in this family. The claim now runs
`0x0..0xD8`: **15/15 functions at 100.00%**, module sha1
`f3ab11c967c58f4483a4264fbeb1ba4a837e8719` unchanged against `config/G2ME01/config.yml` and equal to
`orig/G2ME01/files/RelProd/Ripper.rel`, the `.rel` `cmp`-identical, `main.dol` `6ef9b491...`,
`matched` 9178 -> 9179, `linked` 4004 -> 4005, the module's own count 14 -> **15 of 56**
(`tools/audit_rel_claim.py Ripper`: 56 text symbols in the preplf, 56 in the plf, 0 dropped by
`-strip_partial`, 0 problem claims). `unit_fit.sh`: claimed 216, ours 216, retail 216, **no extra
functions**. `check_decl_order.py`: ok. The unit was already `Matching` and stays `Matching`, so
`flip_test.sh` was not run - `AGENTS.md` names the module sha1 plus the `cmp` as the acceptance
test for a REL unit, and both hold.

**Nothing had to be re-derived, a third time.** `build/G2ME01/Ripper/asm/auto_00_00000000_text.s`
is 15 instructions and is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10` and
`KrocussAccessors.cpp`'s `fn_38_0`, so the body transferred verbatim:

    void fn_54_0(void* out, const CPhysicsActor* self) {
      fn_54_158C(out, self->GetBoundingBox());   // .text 0x158C, unclaimed
    }

`fn_54_158C` is this module's own out-of-line `optional_object<CAABox>` converting constructor - it
is the **last** function in `auto_00_000000D8_text`, and the same 20 instructions as MysteryFlyer's
`fn_45_2BBC` and Tryclops's `fn_81_4FEC` - so it stays unclaimed and is called by name.
`const CAABox&` is load-bearing again: by value the frame grows to 0x40 and the unit stops matching.
Retail takes the box by address in a 0x30 frame and needs no move for `self`, because
`GetBoundingBox__13CPhysicsActorCFv` takes `this` in r4 as well. The one-method `CPhysicsActor`
stand-in is not optional: `MetroidPrime/CPhysicsActor.hpp` reaches `Collision/CMaterialList.hpp`,
whose file-scope statics put 0x28 bytes of `.data` in the object, and the module's sha1 breaks on it
with every function still at 100%.

**The `#ifdef __MWERKS__` guard is not optional in this file**, unlike in the module *head* files,
and that is the one thing about the recipe that is per-file rather than per-module.
`RipperAccessors.cpp` **is** listed in `files.cmake` - it is one of the 20 `*Accessors.cpp` units
there, and the head files are deliberately not - so a host-compiled `fn_54_0` would make the port
link `fn_54_158C` and a host-mangled `CPhysicsActor::GetBoundingBox`. The first attempt at this item
measured the failure directly: **`link_check: STRICT FAIL - regression gate: 260 undefined against
a baseline of 259 (GREW)`, `NEW fn_54_158C`**, and `build.sha1` and the `All:` line were both still
green while it happened. The Krocuss form - the whole block, stand-in and include, inside the guard -
avoids it without the host-only definition the first attempt used, and leaves the port compiling
exactly what it compiled before.

### What is left here, and what the next lane has

**41 functions are still unclaimed** (56 in the module, 15 claimed): 35 in
`auto_00_000000D8_text`, 5 in `auto_00_00001618_text` and `fn_54_158C` itself in
`auto_fn_54_15C8_text`. The first four are a contiguous 0xA0-byte block already reproduced for
MysteryFlyer, Tryclops and the rest of this family:

- `fn_54_D8` (0xD8, 0x2C) - `lwz r12,0(r3); lwz r12,0x38(r12); mtctr; bctrl`, a vtable dispatch:
  `CMysteryFlyerRel.cpp`'s `fn_45_D0` plus a thirteen-virtual stand-in class.
- `fn_54_104` (0x104, 0x24) - `li r3,0; bl SetLoader_Ripper__…P7CEntity`: `RELExit`.
- `fn_54_128` (0x128, 0x20) - `bl fn_54_148`: `RELMain`.
- `fn_54_148` (0x148, 0x30) - the loader registration, `CMysteryFlyerRel.cpp`'s `fn_45_140`.

Two traps for that step, both visible in the tree now: the import is the **long MWCC-mangled** form
`SetLoader_Ripper__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity`, so unlike
BacteriaSwarm's and Tryclops's plain `fn_802…` setter it needs the module's `symbols.txt` renamed
the way IngSnatchingSwarm's was; and `.text 0xD8` sits *inside* the existing
`auto_00_000000D8_text` unit, so taking `0xD8..0x178` is a **sub-range carve of one existing auto
unit**, not a new head.

**The wall is `fn_54_178`** (0x178, **0x35C** = 860 bytes), the module's entity loader, and it is
where the earlier "blocked, no `CRipper`/`CPatterned`" note still bites: it opens
`stwu r1,-0x790(r1)`, saves `r23`..`r31`, and its first act is
`addi r26,r1,0x430; bl __ct__20SLdrEditorPropertiesFv`, followed by `LoadTypedefSLdrEditorProperties`
and `__dt__20SLdrEditorPropertiesFv`. That is class construction, not wiring, and it needs the
CActor/CPatterned hierarchy this tree does not model. Everything from there to the end of the module
is the same story.

### And a correction worth keeping

**The loader generator does not put this block in front of every scripted-actor module, and an
earlier version of this section claimed it did.** `MetareeSwarm` and `PlantScarabSwarm` are loader
heads with **no accessor block at all**: their `0x0..0xD8` is five unrelated functions
(`fn_43_0`, `fn_43_3C`, `RELExit`, `RELMain`, `fn_43_A8` in MetareeSwarm's case), `lbl_8041AAB8`
appears nowhere in either module's disassembly, and `0x448` - the offset `fn_54_3C` stores the
default float at - appears nowhere in either module either. So "0x9C accessor block at the head" is
a property of the `*Accessors.cpp` family, not of the loader generator in general, and a head
skipped over it (`IngPuddle`, `BacteriaSwarm`, `FishCloud`) is a different shape rather than a
contradiction.

## `EyeBall` is the fifth `*Accessors.cpp` head extended past the wrapper (2026-09-29, goal item `progress-rel-extend-eyeball`, lane 2)

`EyeBall`'s claim was `.text 0x3C..0xD8`, fourteen accessors, and the 60 bytes below it held
`fn_19_0` - the `GetBoundingBox` wrapper that opens this family of heads. The claim now runs
`0x0..0xD8`: **15/15 functions at 100.00%**, `flip_test.sh` **PASS** ("kept as Matching"),
module sha1 `96c3406ac17b2e275b05b890070c4eeac75baa72` unchanged against
`config/G2ME01/config.yml` and `cmp`-equal to `orig/G2ME01/files/RelProd/EyeBall.rel`, the `.rel`
byte-identical, `main.dol` `6ef9b491...`, `matched` 9180 -> 9181, `linked` 4006 -> 4007, the
module's own count 14 -> **15 of 68** (`tools/audit_rel_claim.py EyeBall`: `15/15 functions` in
the claim, 68 text symbols in the preplf, 68 in the plf, 0 dropped by `-strip_partial`, 0 problem
claims). `unit_fit.sh`: claimed 216, ours 216, retail 216, **no extra functions** - the object
defines only what the retail unit object does. `check_symbol_names.py` 484 units / 0 missing;
`check_raw_offsets.py` 128 sites in 46 files, all documented, unchanged **as measured in this checkpoint and superseded since - the tool now prints 139 sites in 50 files** (`fn_19_0` reaches no raw
offset - it goes through `self->GetBoundingBox()` - so the existing `EyeBallAccessors.cpp` section
still lists the same two sites, 0x54 and 0x44F).

**Nothing had to be re-derived.** `fn_19_0` is 15 instructions and is byte-identical to
`CMysteryFlyerRel.cpp`'s `fn_45_10`, `ShredderAccessors.cpp`'s `fn_68_0` and
`KrocussAccessors.cpp`'s `fn_38_0`, so the body transferred verbatim:

    void fn_19_0(void* out, const CPhysicsActor* self) {
      fn_19_2590(out, self->GetBoundingBox());   // .text 0x2590, unclaimed
    }

`fn_19_2590` is this module's own out-of-line `optional_object<CAABox>` converting constructor
(`build/G2ME01/EyeBall/asm/auto_00_000000D8_text.s:2671` - `li r0,1`, six `lwz`/`stw` pairs,
`stb r0,0x18(r3)`, `blr`, 0x3C bytes) - it is in `auto_00_000000D8_text`, so it stays unclaimed and
is called by name. `const CAABox&` is load-bearing again: by value the frame grows to 0x40 and the
unit stops matching. Retail takes the box by address in a 0x30 frame and needs no move for `self`,
because `GetBoundingBox__13CPhysicsActorCFv` takes `this` in r4 as well. The one-method
`CPhysicsActor` stand-in is not optional: `MetroidPrime/CPhysicsActor.hpp` reaches
`Collision/CMaterialList.hpp`, whose file-scope statics put 0x28 bytes of `.data` in the object -
and `powerpc-eabi-objdump -h` on the built object confirms the reason directly: **`.text` 0xD8 and
no `.data` section at all**.

**The `#ifdef __MWERKS__` guard is what the item actually turned on**, for the fourth time.
`EyeBallAccessors.cpp` is listed in `files.cmake` (like all 20 `*Accessors.cpp` units), and the
module *head* files are deliberately not, because a head body makes the host port link the module's
own functions. It was safe only because it read raw offsets and DOL globals; `fn_19_0` makes the
host link `fn_19_2590` and a host-mangled `CPhysicsActor::GetBoundingBox` -
`powerpc-eabi-nm -u` on the MWCC object lists exactly those two new undefined names. Measured with
`tools/link_check.sh` after the change: **unique undefined symbols 259, duplicate definitions 0**,
equal to the judge's recorded baseline, so with the guard the host build is byte-identical; without
it `gate.sh`'s `link_gap` step, and not `build.sha1`, is what would have failed.

### What is left here

`fn_19_D8` (0xD8) is the first of the 47 functions in `auto_00_000000D8_text` and they are EyeBall's
own members, needing the CActor/CPatterned hierarchy; `auto_fn_19_25CC_text` holds 1 and
`auto_00_000026D0_text` holds 5. `EyeBall` has no `REL_Setup` claim, so 15 + 47 + 1 + 5 = 68 is the
complete denominator `audit_rel_claim.py` prints, and it is the sum of the `total_functions` in
`build/report.json` for the four units. **`Ripper` (`fn_54_0`) is now the only module in
this family still unclaimed at its 0x0 wrapper**, and it is the last clean instance of the shape.

## `DigitalGuardian` is the sixth head past the wrapper, and the only one that inlines it (2026-09-29, goal item `progress-rel-extend-digitalguardian`, lane 1)

`DigitalGuardian`'s claim was `.text 0x78..0x10C`, thirteen accessors. The claim now runs `0x0..0x10C`:
**16/16 functions at 100.00%**, module sha1 `a3798856ec6b175272529f6a6295a29140662bcc` unchanged
against `config/G2ME01/config.yml` and equal to `orig/G2ME01/files/RelProd/DigitalGuardian.rel`, the
`.rel` `cmp`-identical, `main.dol` `6ef9b491...`, `matched` 9181 -> 9184, `linked` 4007 -> 4010, the
module's own count 13 -> **16 of 420**. `tools/audit_rel_claim.py DigitalGuardian`: 16/16 functions in
the claim, 0 problem claims, 420 text symbols in the preplf, 420 in the plf, 0 dropped by
`-strip_partial`. `unit_fit.sh`: claimed 268, ours 268, retail 268, **no extra functions**. The unit
was already `Matching` and stays `Matching`, so `flip_test.sh` was not run - `AGENTS.md` names the
module sha1 plus the `cmp` as the acceptance test for a REL unit, and both hold.

### The one thing that did not transfer: the wrapper is inlined here

`fn_14_0` and `fn_14_8` are `li r3,1; blr` each and cost nothing. `fn_14_10` did not, and **the
FN_XX_10 HINT that six of these items were queued with is wrong for this module.** Every earlier
head in this family - `fn_45_10` (MysteryFlyer), `fn_81_10` (Tryclops), `fn_35_8`
(IngSpiderballGuardian), `fn_38_0` (Krocuss), `fn_54_0` (Ripper), `fn_68_0` (Shredder) - is **0x3C
bytes** and ends in `bl <module>_ctor`, calling its own out-of-line `optional_object<CAABox>`
converting constructor, so all six are written as the free function it compiles to:

    void fn_XX_10(void* out, const CPhysicsActor* self) { fn_XX_ctor(out, self->GetBoundingBox()); }

`fn_14_10` is **0x68 bytes and has no such call.** `build/G2ME01/DigitalGuardian/asm/auto_00_00000000_text.s`
shows the conversion in the body: `li r0,1; stb r0,0x18(r31)` for the valid flag and then six
`lwz`/`stw` pairs copying the `CAABox` word by word. A `bl fn_XX_ctor` there would be four
instructions of wrong bytes, and the function would drop well below 100% while the two 8-byte
predicates beside it stayed at 100% - so objdiff on the unit would still look healthy.

The module has no out-of-line converting constructor to call, and the two functions a grep for the
flag store finds are not one. `fn_14_D774` (`.text 0xD774`, 0x48 bytes) and `fn_14_1AB50` (`0x1AB50`,
0x48) each open `lbz r0,0x17c(r4)` / `0x730(r4)` and store it to `0x18(r3)`, where a converting
constructor has to `li r0,1` - so neither can be one. They reload the flag, `cmplwi r0,0; beqlr`,
and only then run the same six `lwz`/`stw` pairs, which makes them `optional_object<CAABox>`
builders out of a `{CAABox, bool}` member: the box sits at source+0x164 and source+0x718, the flag
just past it. Both are unclaimed. What tells the two shapes apart in a module you have not looked
at is the `bl`, not the flag store.

    for m in Ripper Krocuss MysteryFlyer IngSpiderballGuardian Shredder DigitalGuardian; do
      printf '%-24s ctor-call: ' "$m"
      grep -c 'bl fn_[0-9]*_[0-9A-F]*$' "build/G2ME01/$m/asm/auto_00_00000000_text.s"
    done

Five `1`s and one `0`. The `0` is the one that needs a different spelling.

**The spelling that works is the real return type, and it is the shortest of the six:**

    rstl::optional_object<CAABox> fn_14_10(const CPhysicsActor* self) { return self->GetBoundingBox(); }

MWCC reproduces the bytes from it with nothing hand-written: `optional_object`'s converting
constructor (`include/rstl/optional_object.hpp:16`) sets `m_valid` in its mem-init list and then
placement-constructs the box, and because `CAABox` carries `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE`
(`include/Kyoto/Math/CAABox.hpp`, bottom) that construction is a word-wise copy rather than a call.
The `li r0,1; stb` before the copy, rather than after it, is the mem-init running first, and the
`li r0,1` is the whole of the difference from the neighbours: they emit a `bl` to a converting
constructor and do no copy of their own, so they need the `fn_XX_ctor(out, box)` spelling and this
one does not.

The `CPhysicsActor` stand-in, the `const` return and the `GetBoundingBox` frame argument are the
same as the other five, for the same reasons: `MetroidPrime/CPhysicsActor.hpp` reaches
`Collision/CMaterialList.hpp`, whose file-scope statics put 0x28 bytes of `.data` in the object and
break the module sha1 with every function still at 100%; `GetBoundingBox` takes `this` in r4, the
same register `self` arrives in, so no move is needed, and it returns the box through a pointer at
r1+0x8 - which is the 0x30 frame and the 0x34 saved-LR slot retail has.

The `#ifdef __MWERKS__` guard is `ShredderAccessors.cpp`'s and is load-bearing for the reason
`RipperAccessors.cpp` records: `DigitalGuardianAccessors.cpp` **is** listed in `files.cmake`, so a
host-compiled `fn_14_10` would make the port link a host-mangled `CPhysicsActor::GetBoundingBox`.
Measured here with the guard: **259 -> 259 undefined, 0 duplicates.**

### What is left here, and what the next lane has

**404 functions are still unclaimed** (420 in the module, 16 claimed), almost all of it in
`auto_00_0000010C_text` (0x10C..0xD904). The first three are the same opening this family has
everywhere, and all three already have a spelling in the tree:

- `fn_14_10C` (0x10C, 0x2C) - `lwz r12,0(r3); lwz r12,0x38(r12); mtctr; bctrl`, a vtable dispatch:
  `CMysteryFlyerRel.cpp`'s `fn_45_D0` and a thirteen-virtual stand-in class.
- `fn_14_138` (0x138, 0x24) - `li r3,0; bl fn_8021F9B0`: `RELExit`, a plain DOL import needing no rename.
- `fn_14_15C` (0x15C, 0x20) - `bl fn_14_17C`: `RELMain`, the wrapper around the registration, exactly
  `CMetareeSwarmRel.cpp`'s `void RELMain() { fn_43_A8(); }`. `fn_14_17C` (0x17C, 0x3C) is the
  registration: `lis r5,fn_14_1B8@ha / lis r3,lbl_14_bss_C@ha`, then `addi` + `stwu r5,lbl_14_bss_C@l(r3)`
  and `addi` + `stw r0,0x4(r3)` filling the two words of the slot, then `bl fn_8021F9B0` with the
  slot address still in r3 (the same arrangement as `MetareeSwarm`'s `fn_43_A8`; no symbol exists
  at 0x170, it is retail's `lwz r0,0x14(r1)` inside `fn_14_15C` itself).

The wall is the same one as the rest of this family and it is not a spelling problem: from
`fn_14_1B8` (0x1B8, **0x330** = 816 bytes) on, the module is entity class code that needs the
CActor/CPatterned hierarchy this tree does not model. The largest unclaimed functions are
`fn_14_B100` (0xF90), `fn_14_18264` (0xE7C) and `fn_14_1AD2C` (0x9CC) - 0x9CC bytes is the same
order as MysteryFlyer's `fn_45_170`, which is what blocked that head.

## `EmperorIngStage3`'s accessor block is a fifth shape, and `RELMain` is not always at the head (2026-09-29, goal item `progress-rel-head-emperoringstage3`, lane 2)

The item's brief, and several rows above it, describe a scripted-actor module head as "the accessor
block, then `RELExit`, `RELMain` and the registration". That is `MetareeSwarm`, `IngPuddle`,
`FishCloud`, `BacteriaSwarm` - **it is not a property of the REL format, and it is not even the
common case.** `EmperorIngStage3` (module 18) is the clean counterexample, and both halves of the
lesson cost the shape of the claim:

1. **The accessor block is emitted per class, and it varies.** Five shapes are now measured, and a
   copy of any one of them into another module is a guess until `diff`ed:

   | module | byte at +0x448 (`lbl_8041AAB8`) | `li r3,0` run above `kInvalidUniqueId` | always-true predicate | extra function |
   |---|---|---|---|---|
   | `Krocuss` (38) | yes | 4 (`fn_38_54/5C/64/6C`) | at 0xA4 | - |
   | `MysteryFlyer` (45) | yes | 2 (`fn_45_64/6C`) | at 0x0 **and** at 0xA4 | `fn_45_8` (`addi r3,r3,0x818`) |
   | `Tryclops` (81) | yes | 3 (`fn_81_64/6C/74`) | at 0x8 **and** at 0xAC | - |
   | `IngSpiderballGuardian` (35) | yes | 4 (`fn_35_5C/64/6C/74`) | at 0x0 **and** at 0xAC | - |
   | `EmperorIngStage3` (18) | **no** | **4** (`fn_18_4C/54/5C/64`) | at 0x0 **and** at 0x90 | `fn_18_E0` |

   So **read the relocations in the range before deciding which globals to declare.** Module 18
   names only `kInvalidUniqueId`; declaring `lbl_8041AAB8`/`lbl_8041B758` "because every other head
   does" puts two unused `.rodata` references in the object and would move the module's bytes for
   nothing. (`unit_fit.sh` reports the same thing from the other side: `no extra functions` is what
   catches it.)

2. **`RELMain`/`RELExit` can be far from the head, and then they cannot be claimed with it.**
   `EmperorIngStage3`'s are at **0xC330 and 0xC30C**, immediately above `fn_18_C290` (0xC290, 0x7C)
   and 215 of the module's 270 text symbols in - **far above the head, and not claimable with it.**
   The common case is the opposite: **most modules that have a landed head carry the trio inside
   that head, at 0x2C-0xF0** - `BacteriaSwarm`, `DestructibleBarrier`, `FishCloud` and
   `SnakeWeedSwarm` at 0x2C, `IngPuddle`/`IngSnatchingSwarm` at 0x34, `AtomicAlpha` at 0xC8,
   `WallCrawler` at 0xCC, with `MysteryFlyer` (0xFC) and `Tryclops` (0x104) the two above the band.
   `IngBlobSwarm`'s is at **0x64/0x88**, inside its 0x0..0xD8 claim - **not** at 0x138.
   **`DigitalGuardian`'s sits just past 0x10C** (0x138/0x15C/0x17C against a head that stops at
   0x10C), and its `symbols.txt` leaves the trio **unnamed**, so the grep below returns nothing
   there and the three functions have to be recognised by shape: `fn_14_138` is `RELExit`,
   `fn_14_15C` is `RELMain` and `fn_14_17C` the registration. One unit cannot claim two
   discontiguous ranges (the link-order cycle, "Four structural facts" above), so a head at 0x0 and
   a trio at 0xC330 are **two items' work at best**, and the head is worth taking first because it
   is the contiguous part. **Check `grep -n "RELMain\|RELExit" config/G2ME01/rels/<Module>/symbols.txt`
   before planning a head** - it is the difference between a 14-function item and a 3-function one,
   and it is one grep.

**`fn_18_E0` is the one function here with no in-tree precedent, and it took one spelling.**
`lwz r3,0x48c(r3)` / `lwz r0,0x37c(r3)` / `subfic r0,r0,6` / `cntlzw r0,r0` / `srwi r3,r0,5` / `blr`
is `*(int*)(*(char**)(self + 0x48c) + 0x37c) == 6`, and it is 100.00% as:

    bool fn_18_E0(const void* self) {
      const char* sub = *reinterpret_cast<const char* const*>(static_cast<const char*>(self) + 0x48C);
      return *reinterpret_cast<const int*>(sub + 0x37C) == 6;
    }

The `subfic`/`cntlzw`/`srwi 5` tail is mwcceppc's `== constant` idiom when the comparison *is* the
return value and nothing is branched on - the same shape `CMetareeSwarmRel.cpp` measures for
`index > -1`, and the reason a compare-and-branch spelling is wrong here is that it is longer, not
that it is wrong logic. The pointer-then-int spelling is also load-bearing: reading
`*reinterpret_cast<const int*>(...)` off `self + 0x48C` directly is a different instruction.

**Measured, the gates that matter for a REL unit**: `flip_test.sh` PASS
(`PASS -> kept as Matching`), module sha1 `775b095db13cb3e1ec6c5bb51a129263f7b0fa18` unchanged
against `config/G2ME01/config.yml` and `cmp`-equal to `orig/G2ME01/files/RelProd/EmperorIngStage3.rel`,
all 86 RELs `cmp`-identical, `main.dol` `6ef9b491...`, `audit_rel_claim.py` 0 problems with 270 of 270
preplf text symbols in the plf, `unit_fit.sh` `248 claimed / 248 ours / 248 retail, fits`,
`check_decl_order.py` ok, `tools/check_files_cmake.py` ok, and `tools/probe_sources.sh` **727 source files (then),
0 failures, 259 undefined, 0 duplicates** - the undefined count unchanged by the `files.cmake`
entry, which the `#ifdef __MWERKS__` guard is there to guarantee.

### What is left here

`fn_18_F8` (0xF8, 0x11C) through `fn_18_C8CC` and the `RELMain`/`RELExit` trio at 0xC30C are
unclaimed, all of it entity class code needing the CActor/CPatterned hierarchy this tree does not
model. The trio on its own is 3 functions and would need a **second** unit whose claim starts at
0xC30C, not an extension of this one - that is the natural next step for this module.

## `CGeomBlobV2`'s accessor block is six functions in two units, and `unit_fit.sh` said it fit (2026-09-29, goal item `progress-rel-head-geomblobv2`, lane 2)

Module 25's accessor block is eight trivial functions at `0x2544..0x2584`, and **six of them are
ours in two units, because the other two are not in dtk's FORCEACTIVE list.** The measurement is
the point, and it is the third time this family has produced a surprise, so the order of the facts
below is the order they were found in.

**One unit claiming all eight built, linked, and broke the module's hash.** `CGeomBlobV2Accessors.cpp`
claimed `.text 0x2544..0x2584`, all eight bodies written, and every check that is not the module's
sha1 said yes:

```
$ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CGeomBlobV2Accessors.cpp
   .text      claimed     64   ours     64   retail     64   fits
   no extra functions: our object defines only what the retail unit object does
```

`objdiff` reported the unit at 100%, `powerpc-eabi-nm` over our object and the retail unit object
agreed on all eight symbols and their sizes, and the link succeeded. And
`GeomBlobV2.rel` came out **16 bytes short of retail**: 33756 against 33772. Those 16 bytes are
`fn_25_255C` and `fn_25_2564`, the two float **getters**, and `build/G2ME01/GeomBlobV2/ldscript.lcf`
says why - dtk's FORCEACTIVE list for this module names `fn_25_2544`, `fn_25_254C`, `fn_25_2554`,
`fn_25_256C`, `fn_25_2574` and `fn_25_2578` and **not** those two, because nothing in the module's
own data or code references them. mwldeppc dropped them and every byte after them moved down 16.

This is the dead-stripping trap of the "Four structural facts" section above, and the **superseded
2026-09-29 note there is the whole finding**: `scope:global` in `symbols.txt` does not fix it; only
a `force_active:` list in `config/G2ME01/config.yml` does, which is what `Tweaks` carries. So the
block is split in two and the two middle functions stay retail:

```
0x23E8..0x2490   CGeomBlobV2Rel.cpp              4 functions, the entry points
0x2490..0x2544   unclaimed: fn_25_2490, the module's entity loader
0x2544..0x255C   CGeomBlobV2Accessors.cpp        3 functions, all FORCEACTIVE
0x255C..0x256C   unclaimed: fn_25_255C, fn_25_2564, the two float getters
0x256C..0x2584   CGeomBlobV2AccessorsTail.cpp   3 functions, all FORCEACTIVE
```

**Two more things the same module contradicts, both worth not re-deriving.**

1. **Its head is not at 0x0, and the accessor block is not the family.** The landed heads
   (`MetareeSwarm`, `IngPuddle`, `IngSnatchingSwarm`, `PlantScarabSwarm`, `SnakeWeedSwarm`,
   `AtomicAlpha`, `MysteryFlyer`, `FishCloud`, `Tryclops`, `DigitalGuardian`, `EmperorIngStage3`)
   all open with the loader generator's thirteen-accessor block: the `kInvalidUniqueId` store, the
   `li r3,0` predicate run, the `+0x44f` byte, the `+0x34c` flag, the `lbl_8041AAB8` /
   `lbl_8041B758` float pair, the `+0x754` address. **None of it is in module 25.** Its entry points
   are at `0x23E8..0x2490`, and `fn_25_0` (0x0, 0x1FC) is a real bone-blend loop over 0x50-byte
   records calling `close_enough__FRC11CQuaternionRC11CQuaternionf` and
   `__as__9CMatrix3fFRC9CMatrix3f`. Its eight accessors are two pointer getters at `+0x15C`, float
   accessors at `+0x190` / `+0x198`, a bare-`blr` empty virtual and a byte clear at `+0x18`.
   **Before writing a module head, read the module's own ldscript and the first function in its
   `symbols.txt`; do not assume the family.**

2. **`fn_25_2578`'s byte is at `+0x18`, and `*self = 0` gives `+0x0`.** The obvious spelling of a
   `li r0,0 ; stb r0, N(r3) ; blr` clear - `*static_cast<unsigned char*>(self) = 0;` - produces
   `stb r0, 0x0(r3)`, and the module's `.rel` came out **one byte** from retail at `0x2633`. Every
   size-based check still passed; only `cmp` against `orig` found it. The accessor files' own header
   comments now carry both measurements.

`fn_25_23E8` is the family's vtable-dispatch shape unchanged (`.data:0x10` and `.data:0xA8` both
store it at offset 0x3C, it calls slot 0x38, which is `HealthInfo__6CActorFv`), and the
thirteen-virtual stand-in class of `CIngPuddleRel.cpp` reproduces its seven instructions byte for
byte. Both loaders here are **unnamed DOL setters** - `fn_80229EAC` (0x80229EAC, `stw r3,
lbl_80419590`, resolved with `tools/sda.py` against `_SDA_BASE_` 0x8041FD80) and `fn_802274FC`
(0x802274FC, `stw r3, lbl_80419558`) - so there is no `symbols.txt` rename and no DOL change, the
same situation as `Tryclops` and `Blogg`.

**What is left here.** 115 of the module's 130 text symbols stay retail, starting at `fn_25_2490`
(0x2490, 0xB4), which allocates 0x1E0 bytes and calls `fn_25_4290` - entity class code needing the
CActor/CPatterned hierarchy. The two unclaimed float getters at `0x255C` / `0x2564` are one
`force_active:` entry away and are worth a two-line follow-up rather than a lane.

## `CIngSpaceJumpGuardianRel` is a module head, and one accessor is a module-local `.rodata` constant (2026-09-29, goal item `progress-rel-head-ingspacejumpguardian`, lane 2)

Module 34 is the eleventh of the 27 modules whose `REL_Setup` tail was claimed on 2026-09-28 to
gain class code. The head is now the whole `.text 0x0..0x170` - **eighteen** functions, all
100.00% - in `src/MetroidPrime/ScriptObjects/CIngSpaceJumpGuardianRel.cpp`, and the module's sha1
`96e5208fc681177378fcaf1ed15abe20d436073a` is **unchanged** against `config/G2ME01/config.yml`
and the `.rel` is `cmp`-identical to `orig/G2ME01/files/RelProd/`, with all 86 holding and
`main.dol` still `6ef9b491...`. `matched` 9236 -> **9254**, `linked` 4521 -> **4539**, the module's
own count 5 -> **23 of 148** (`tools/audit_rel_claim.py IngSpaceJumpGuardian`: 148 preplf text
symbols, 148 in the plf, 0 dropped by `-strip_partial`, and 0 problem claims - `18/18 functions`
in the claim). `tools/unit_fit.sh` says `.text claimed 368 ours 368 retail 368, fits` with no extra
functions, `tools/flip_test.sh` passes and keeps it `Matching`, and `tools/check_docs_claims.py`
agrees with the tree.

### The one body with no precedent: `fn_34_10` reads the module's own `.rodata`

The brief's hint was that this module's head is the `GetBoundingBox` wrapper plus the family block,
and **thirteen of the fifteen accessors turned out to be bodies already in the tree** - but the
block is the family in an order none of the landed heads has, and diffing
`build/G2ME01/IngSpaceJumpGuardian/asm/auto_00_00000000_text.s` against Tryclops is what shows it:

```
fn_34_0   addi r3,r3,0x8d0      fn_81_0   addi r3,r3,0x7c4
fn_34_8   li r3,0x1             fn_81_8   li r3,0x1
fn_34_10  lis r3,lbl_34_rodata_0@ha / lfs f1,...@l(r3) / blr     <-- no counterpart
fn_34_1C  stwu r1,-0x30(r1) ... bl fn_34_6814                   fn_81_10 the same shape
fn_34_58  lbl_8041AAB8 -> +0x448                                 fn_81_4C the same
...  fn_34_98 (the +0x34C bit 3), fn_34_A4 (+0x754), fn_34_AC, fn_34_B4 (the interleaved
    three-float copy), fn_34_D0 (the vtable-0x38 call), RELExit, RELMain, fn_34_140
```

So **`fn_34_10` (0x10, 0xC) is the only function here with no precedent in the tree**, and the
difference from the family is the point: the family's thirteenth accessor is a *DOL* constant
(`lbl_8041B758`, `.sdata2` at 0x8041B758), and this module's is a **constant in its own `.rodata`**
- `build/G2ME01/IngSpaceJumpGuardian/asm/auto_03_00000000_rodata.s:0` names it
`lbl_34_rodata_0, size:0x4, .float 60` - and it sits at 0x10, where the family puts the
`GetBoundingBox` wrapper (which is here instead at 0x1C). The `lbl_8041B758` accessor is **not in
this module at all**.

The spelling is the same one either way and it was already in the tree:
`src/MetroidPrime/ScriptObjects/CScriptRubiksPuzzle.cpp:5` declares `extern "C" const float
lbl_4_rodata_0;` for its own module's `.rodata:0x0` and returns it from a constructor. **The split
claims `.text` only**, so `lbl_34_rodata_0` stays defined in dtk's `.rodata` object and the
reference is an ordinary cross-object relocation - the same situation as `lbl_8041AAB8` and
`kInvalidUniqueId` beside it. `float fn_34_10(void*) { return lbl_34_rodata_0; }` was the whole of
it, and the object's `.rodata` is empty, so nothing moves.

**The `kInvalidUniqueId` spelling is not free, and the compile error says so.** `CTryclopsRel.cpp`
and `CAtomicAlphaRel.cpp` write `extern "C" const unsigned short kInvalidUniqueId;`, but
`include/MetroidPrime/TGameTypes.hpp:17` already declares it as `const TUniqueId`, and
`CIngSpaceJumpGuardianRel.cpp` includes that header for the wrapper, so the second declaration is
`identifier 'kInvalidUniqueId' redeclared` and the build stops. `CMysteryFlyerRel.cpp` has it
right: **no second declaration at all**, and the body is
`void fn_34_88(TUniqueId* id) { *id = kInvalidUniqueId; }`, which is the same
`lis r4, kInvalidUniqueId@ha / lhz r0, ...@l(r4) / sth r0, 0x0(r3)`. `TUniqueId` is 0x2 bytes
(`CHECK_SIZEOF(TUniqueId, 0x2)` in the same header), so the store is the same word.

### Two things this module has that `CGeomBlobV2` did not, both worth not re-deriving

1. **No dead-strip hazard, and no `force_active:` entry is needed.** `CGeomBlobV2`'s accessor
   block came out 16 bytes short of retail because dtk's FORCEACTIVE list omitted two of its
   getters. This module's `build/G2ME01/IngSpaceJumpGuardian/ldscript.lcf` lists **all fifteen** of
   `fn_34_0` .. `fn_34_D0` in FORCEACTIVE, *and* `asm/auto_04_00000000_data.s:0x3E4` - a 0x148-byte
   vtable, two leading words and one per virtual - stores fifteen of them, so the link keeps them
   twice over. That is why the 18-function claim came out byte-exact on the first build, with no
   carve and no second unit.
2. **`fn_34_D0` is vtable entry 0x3C of a 82-word table, and the thirteen-virtual stand-in class
   is the same one IngPuddle, AtomicAlpha, BacteriaSwarm, Tryclops and PillBug use.** `.data:0x3E4`
   stores `fn_34_D0` at offset 0x3C and `HealthInfo__3CAiFv` at 0x38, so the call target is slot
   0x38 and `CIngSpaceJumpGuardianDispatch` with thirteen virtuals puts its thirteenth there.
   The table itself is unclaimed - it stays in dtk's `.data` object - so the class is only ever
   named, never instantiated by anything but the member call.

**What is left here.** 125 of the module's 148 text symbols stay retail, starting at `fn_34_170`
(0x170, 0x330), the module's own entity loader: a 0x810 frame whose first act is
`bl __ct__20SLdrEditorPropertiesFv` and which then makes thirty further calls (31 `bl` sites, 25
distinct callees), so it is entity
class code needing the CActor/CPatterned/CAi hierarchy. The next step is not this head - the head
is finished at 0x170, and the neighbour above it is a loader, so extending means the loader
itself, which is the wall every other landed head in this family is parked on.

## A 1-byte class passed **by value** keeps a byte temporary that retail has no trace of (2026-09-29, goal item `match-csequencehelper`, lane 1)

`Kyoto/Animation/CSequenceHelper` sat at 17/18 with only `__defctor__16CParticlePOINodeFv` (retail
`0x80299DCC`, 0x98 bytes) unmatched. The previous attempt's notes called it "one function plus three
data sections" and stopped at `#pragma inline_max_size`; the pragma is necessary and **not
sufficient**, and the twelve missing bytes are a one-word change in a shared header.

**The shape of the defect.** Retail inlines the whole 9-argument `CParticlePOINode` constructor into
the implicit default constructor, so retail's DOL defines no such symbol. mwcceppc will not inline it
under the project-wide `inline_max_size(125)`, so the TU emitted a forwarding 0x94-byte defctor plus a
`__ct__16CParticlePOINode...` the retail symbol table has no name for. Because that callee is weak and
this TU is first in link order, mwldeppc kept *this* copy and every function after it moved 0x74.
`#pragma inline_max_size(140)` in `CSequenceHelper.cpp` fixes that part - measured with
`fast_try.sh`: at 134 and below the constructor is still not inlined (16.18%), 136 and up inline it.

**The part the pragma cannot reach.** At 140 the defctor is 0xA4, not 0x98, and the surplus is exactly
three instructions: `stb r0,8(r1)`, `stb r0,12(r1)`, `lbz r4,12(r1)`. The frame is -64 rather than -48,
`CCharAnimTime` sits at `sp+16` rather than `sp+8`, and `mBone` is written from a reloaded byte instead
of from the register that already holds zero. **Those are two temporaries for a 1-byte class.** The
default argument `CSegId bone = CSegId(0)` materialises one at `sp+8`, and the by-value parameter is a
second copy of it at `sp+12`; the member initialiser reads the second back. Every other default
argument collapses - the `SObjectTag(0,0)` is written straight through as two `stw`, and only the
`EParentedMode` keeps a temporary, in retail too (`lwz r0,36(r1)` then `stw r0,64(r31)`).

**The fix, and why it is safe to make in a shared header.** `CParticleData`'s `bone` becomes
`const CSegId&`, exactly like the `const SObjectTag& tag` beside it, and the unit goes 18/18 at
100.00% with `flip_test.sh` PASSing. The generalisable rule: **when mwcceppc materialises a
temporary that retail does not have, check the parameter's size before its type** - a 1-byte class by
value is the case that survives copy propagation, where a 4- or 8-byte one does not. Taking it by
const reference is semantically identical at every call site, and it lifted two `CAnimData` functions
on the way past (`InitializeEffects...` 36.17% -> 49.23%, `__ct__9CAnimData...` 87.11% -> 87.45%) with
no function anywhere worse.

**Both of the previous run's "blockers" were one bug.** It also reported `.sdata` "over by 20" and 50
extra COMDAT functions from `unit_fit.sh`. Those are still there after the fix and the flip still
passes: mwldeppc folds every weak instantiation away, exactly as the tool's own "harmless causes first"
note says. **`unit_fit.sh` complaining about extra COMDAT weak copies is not a blocker; only
`flip_test.sh` decides**, and the extra constant-pool entries it predicted would follow the constructor
did - there simply was no second bug.

**Measured.** `All: 29.04% fuzzy, 21.15% matched, 11.16% linked (9344 / 28465 functions)`, from 9343.
`linked`, the one rule's count, rose 4643 -> **4661**: +18, not +1, because a `NonMatching` unit's
already-matched functions do not count until the unit is complete. `main.dol` still hashes to
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and all 86 RELs are still `cmp`-equal.

## `x * 0.5f` and `x / 2.f` are different instructions, and a weak copy of an unnamed retail
function is what stops a flip (2026-09-29, goal item `match-cguipane`)

`GuiSys/CGuiPane.cpp` was a near-miss: 10 of 11 functions at 100.00%, and only
`InitializeBuffers__8CGuiPaneFv` (188 bytes) at 98.30%. Two things had to be right, and they are
unrelated, so the item needed both.

### 1. mwcceppc strength-reduces `x / 2.f` to a multiply and does **not** touch `x * 0.5f`

The whole residual was eight instructions, and every one of them was the same two bytes:

```
retail   ec 00 00 b2   fmuls f0,f0,f2      ; f0 = +-width/height, f2 = 0.5f
ours     ec 02 00 32   fmuls f0,f2,f0      ; the same multiply, the operands the other way round
```

`f2` is the `.sdata2` constant `3f000000` = 0.5f, loaded once at the top and live across all eight,
so this is not a scheduling difference - only the source order of a commutative `fmuls`. Measured
with `tools/try_edit.py` over six spellings of the eight assignments:

| spelling | `InitializeBuffers` |
|---|---|
| `-mWidth * 0.5f` (the obvious one) | 98.30% |
| `0.5f * -mWidth` (constant first) | 98.30% |
| `-(mWidth * 0.5f)` (negate after) | 74.89% |
| `0.f - mWidth * 0.5f` | 74.89% |
| `mWidth * -0.5f` (fold the sign into the constant) | 82.98% |
| **`-mWidth / 2.f`** | **100.00%** |

**`/ 2.f` is the only one of the six that matches, and the reason generalises: MWCC rewrites a
division by a power-of-two constant into a multiply by its reciprocal, and that rewrite goes through
a different path than a multiply written in the source, so it keeps the source operand order. `*`
does not canonicalise, and in this tree it comes out constant-first.** The same rule is already in
this file for comparisons ("mwcceppc keeps a comparison's source operand order", and the
`ConfigureGameModeLayers` table row); this is the arithmetic twin. `2.f` is also the tree's own
spelling for halving a float (`CCredits.cpp:585`, `CSimpleShadow.cpp:29`).

### 2. A `Matching` unit fails the link on a weak copy of a function retail's own copy of is **unnamed**

With all 11 functions at 100.00% and 100.00% matched code, `flip_test.sh` still failed, and the
DOL was 96 bytes long. `unit_fit.sh` had been reporting this all along and its "harmless causes
first" note is not always right:

```
.text      claimed   1692   ours   1812   over by 120
   +   92  __ct__Q210CGuiWidget15CGuiWidgetParmsFRCQ210CGuiWidget15CGuiWidgetParms
   +   12  GetIsActive__10CGuiWidgetCFv
   +   12  GetIsVisible__10CGuiWidgetCFv
   +    4  Initialize__10CGuiWidgetFv
```

Only the first one mattered. The other three are weak COMDAT copies of functions retail defines
strongly in `auto_03_802740A4_text.o` and in `MetroidPrime/HUD/CSamusHud.cpp`, so mwldeppc folded
them. The copy ctor had **no other owner**: the retail-derived `build/G2ME01/obj/GuiSys/CGuiPane.o`
carries it as an **undefined** `fn_80274608` (that is dtk reading the DOL, in which the name does
not exist), and the 0x5C bytes live at 0x80274608 in an unclaimed range filled by
`auto_03_802740A4_text.o`, which defines them strongly - under a *different* name. So our weak copy
was not a duplicate of anything as far as the linker was concerned, and it was placed immediately
after our claim, at 0x80278BF8, straight on top of retail's `fn_80278BF8`:

```
first moved symbol   fn_80278BF8   80278bf8 -> 80278c54   (+0x5C)
symbols that moved   14730
```

**0x5C is the copy ctor's size, and it moved every function in the DOL after 0x80278BF8.** This is
the "weak instantiations can steal a symbol retail has somewhere else" mechanism above, and the
section there says the only fixes are a wider claim or accepting the loss. There is a third, and it
is one line. The function *is* `CGuiWidgetParms::CGuiWidgetParms(const CGuiWidgetParms&)` - the
bytes are a member-wise copy of exactly its 22-byte layout (4-byte `mFrame`, two `short`s, two
4-byte words, six `bool`s, `lha`/`sth` for the shorts, `lbz`/`stb` for the bools), and
`powerpc-eabi-objdump` of our weak copy is byte-identical to retail's 0x80274608. So give retail's
copy its name, with `tools/apply_rename.py`:

```
fn_80274608 = __ct__Q210CGuiWidget15CGuiWidgetParmsFRCQ210CGuiWidget15CGuiWidgetParms
```

Now the two definitions are one symbol, the strong one wins, the weak one folds at link time (the
object is still 0x714 bytes - the 0x5C is what stops reaching the DOL), and the flip passes.
**So the fix for "our object emits a weak copy retail also has" is to check whether retail's copy
has a *name*, and if it does not, whether it should - and to rename it in
`config/G2ME01/symbols.txt` when the bytes identify it. No byte of the DOL changes; only the name the linker can match on.** `total_functions` is
unchanged at 28465 and no function anywhere scores worse (`gate.sh`'s per-function diff is the
check). Eight `auto_*` objects referenced the symbol before the rename and dtk regenerates all
of them consistently from `symbols.txt`; `build/G2ME01/obj/GuiSys/CGuiPane.o`'s undefined
`fn_80274608` becomes an undefined `__ct__Q2...`, which our object now defines.

This is a `config/` change, so it is reported as an intended change rather than a copy:
`config/G2ME01/symbols.txt` line 10935, `fn_80274608` -> `__ct__Q210CGuiWidget15CGuiWidgetParmsFRC
Q210CGuiWidget15CGuiWidgetParms`, and nothing else in that file.

**Measured.** `GuiSys/CGuiPane.cpp` is 11/11 at 100.00% fuzzy, 100.00% matched code, and
`flip_test.sh GuiSys/CGuiPane.cpp` prints `PASS -> kept as Matching`.
`All: 29.05% fuzzy, 21.16% matched, 11.19% linked (9363 / 28465 functions)`; `matched`
9362 -> **9363** and `linked` 4679 -> **4690**, i.e. +1 for the function and +11 for the flip.
`main.dol` hashes to `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and all 86 RELs are `cmp`-equal to
`orig/G2ME01/files/RelProd/`.
