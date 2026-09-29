# Running the decompilation

**Starting fresh? Read `docs/HANDOFF.md` first** - it has the current position, the verdict
tools, the open blocker and what to do next. This file is the method; that one is the map.

How the work is organised, what has been proven to work, and what keeps failing. This is
the operating companion to `PORT_NOTES.md` (which holds the techniques and mechanisms).
Update it when the strategy changes, not only when a fact is learned.

## Where the work is

Two repositories, both forks we own:

- **`MetroidPrime2Port`** (here) is the **port** and the **measurement rig**. It builds the
  decompiled sources against Aurora for a PC target, and it carries the matching build, the
  test suite and the tooling.
- The **decompilation itself** is what upstream `PrimeDecomp/echoes` is doing, and what we
  contribute to. Our fork's `master` is the shared tree: one branch, committed centrally by
  the orchestrator.

Everything that measures progress lives here: `build/report.json`, `tools/decomp_build.sh`,
`tools/flip_test.sh`, `tools/compare_unit.sh`, `tools/check_symbol_names.py`,
`tools/find_trivial_functions.py`, `tools/scaffold_rel_module.py`.

## Upstream, and what we take from it

The decompilation we contribute to is the public `PrimeDecomp/echoes`, cloned read-only for
reference at `/tmp/opencode/upstream` (`git clone --depth 1 https://github.com/PrimeDecomp/echoes`).
Until 2026-09-25 this repository never looked at it. It is worth looking at, in both directions:

- **We are ahead in some units and it is ahead in others.** Our fork names unknown members `xNN_name`
  and keeps `fn_` prefixes with an owner annotation (`fn_80036200__13CStateManagerFv`); upstream uses
  `mName` members and real names for a different subset. In one measurement our `symbols.txt` had 434
  function names upstream lacks and upstream had 988 we lack - a mix of genuinely different progress
  and the two naming conventions.
- **There is no shared git history** (our commits are not in it and vice versa), so a merge is
  impossible. Syncing is file by file.

The decision taken (2026-09-25) is **sync where the gates pass, and say so**: a unit ported from
upstream is landed only when it is `Matching`, the DOL sha1 and all 86 REL hashes still reproduce
retail, the probe and symbol checks are green, and the commit says plainly that the code came from
upstream. Anything that does not hold there is left as a candidate, not merged.

What porting one actually costs, measured on eight candidate units from upstream `d83da79`:

| outcome | units |
| --- | --- |
| flips clean, landed | `Kyoto/Animation/CSegId`, `Kyoto/Animation/CSegIdList`, `Kyoto/CTimeProvider` (1/1, 1/1, 2/2 - all three `Matching`) |
| compiles with fixups, +26 functions, unit still `NonMatching` | `Kyoto/CToken` (5/9), `Kyoto/Text/CStringTable` (11/14), `Kyoto/CDependencyGroup` (10/13) |
| blocked | `Kyoto/Animation/CAdditiveAnimPlayback` (header size 0x28 expected, local `rstl::rc_ptr` makes it 0x24), `Kyoto/CSimplePool` (upstream's `.data` split ends inside `lbl_803BAF90` - unsafe to guess) |

The fixups are the cost: member renames to our convention, external symbol renames, a header expanded
from its stub, and a constructor moved out of a header. Do **not** copy upstream's `config/`,
`configure.py`, `symbols.txt` or `splits.txt` wholesale - merge by hand, and never touch
`config/G2ME01/config.yml`. Which units are worth porting is a judgement call: prefer the ones whose
dependencies our tree already has.

### What a port costs, measured over two batches

**Four files per unit**: a `splits.txt` entry, a `configure.py` entry, `symbols.txt` renames, and the
source. The renames are the hidden cost and they are mechanical: `tools/fnmap.py` pairs retail's
functions with the ones our object emits byte-for-byte, `tools/apply_rename.py` writes those names
into `symbols.txt`, and `tools/autorename.py` does both - it turned nine `CPakFile` functions from 0%
to 100% in one call. Check a range before claiming it with `tools/range_owner.py` (is it already
claimed?) and `tools/range_bounds.py` (does it start and end on real symbols? a split that cuts a
function in half can never be reproduced).

Second batch, measured 2026-09-25: of eight candidates, **one landed** (`CCubeSurface`, `Matching`,
2/2) and four were carried as `NonMatching` partials worth 45 exact functions (`CObjectReference`
98.53%, `NMWException` 96.02%, `CPakFile` 77.75%, `CFontImageDef` 67.16%); three were dropped for
missing dependencies (`CPlayerGunBase` needs `CWorldShadow.hpp`/`CRainSplashGenerator.hpp`,
`CCubeMaterial` needs `CGX_Impl.hpp`, `DolphinCMemoryCardSys` needs `rstl::aligned_allocator` and a
replacement for our stub `CCardFileInfo`).

Two root causes came out of it, both shared-header divergences that block whole families:

- **Our `rstl::rc_ptr` is a 4-byte `CRefData*`; retail's is 8 bytes - a pointer plus a raw `int*`
  refcount.** That makes `CVParamTransfer` 0x4 instead of 0x8 and `CObjectReference` 0x20 instead of
  0x24, and it is the same reason `CAdditiveAnimPlayback` was blocked. Every unit holding either
  class is stuck behind it. Aligning `rc_ptr` is a whole-tree change and deserves its own lane.
- **`rstl/vector.hpp` and `construct.hpp` have diverged from upstream in ways that change inlining.**
  `vector(int)` does not set `x4_count`, and the three-argument fill constructor inlines here where
  retail keeps it out of line.

`tools/flip_test.sh` and `tools/unit_fit.sh` now accept `.cp` and `.c` sources as well as `.cpp`
(`Runtime/NMWException.cp` needs `extra_cflags=["-RTTI on","-Cpp_exceptions on"]`, and both tools had
assumed the suffix in two separate places - the source-path check and the `configure.py` entry match,
which also has to survive an entry carrying extra arguments).

## The measurement rig

| tool | question it answers |
| `tools/decomp_build.sh [unit]` | ninja + objdiff + that unit's unmatched functions with per-function percentages. The worklist is `build/report.json`. |
| `tools/compare_unit.sh <unit>` | diagnostic: how our object differs from the retail-derived one, section by section. Stricter than the link. |
| `tools/flip_test.sh <unit>...` | **the acceptance test.** Flips a unit to `Matching`, rebuilds, checks the DOL and all 86 RELs, keeps the flip only if retail is still reproduced byte-for-byte. |
| `tools/check_symbol_names.py` | every name `symbols.txt` declares inside a unit's `.text` ranges, checked against what the retail-derived object defines. |
| `tools/find_trivial_functions.py` | unmatched functions classified by the shape of their machine code - the cheap-work queue. |
| `tools/unit_fit.sh <unit>` | why a unit will not promote: claimed range vs our object's sections, and the functions we emit that the retail unit object does not define. |
| `tools/gate.sh [--baseline]` | **the whole acceptance test in one command**: configure, ninja (whose exit status *is* the hash check), an independent re-hash against `config.yml`, the per-function report diff, wiring, docs claims and the probe. Non-zero exit on any failure. `--baseline` records `build/report.base.json` from a clean tree. |
| `tools/report_diff.py <base> <new>` | per-function diff of two reports: `WORSE`, `GONE`, `UNLINKED`, and a fell linked total. Replaces the hand-typed comparison that missed things. |
| `tools/check_module_wiring.py` | is every module with sources in `src/` actually wired into the build - catches a `Rel(...)` block lost to a config clobber, and counts the modules that link our own code. |
| `tools/range_owner.py <section> <start> <end>` | which unit claims a split range, if any - before carving one for a new unit. |
| `tools/range_bounds.py <start> <end>` | does a proposed range start and end on real symbols in retail. |
| `tools/fnmap.py <unit>` | byte-identical function pairing between a unit's retail object and ours (the mechanical half of a port). |
| `tools/autorename.py <unit>` | rename every byte-identical `fn_` function after our own symbol, via the two above. |
| `tools/apply_rename.py` | apply `old=new` renames to `symbols.txt` from stdin, reporting any it could not find. |
| `tools/scaffold_rel_module.py` | the three artifacts needed to start a REL module, printed or `--write`. |
| `tools/probe_sources.sh` | the port build's **compile and link** sweep: 741 files, must stay 0 failures. |
| `tools/sync_files_cmake_excluded.py` | derives `check_files_cmake.py`'s `EXCLUDED` list from the tree: prunes entries for sources that are now listed, reports `Matching` objects in neither list. `--check` for a gate step. A hand-maintained list describing a tree that changes every commit will be wrong. |
| `tools/probe_cc.sh <src> <out.o>` | compile **one** scratch source with the exact `MWCC GC/2.7` flags a DOL unit gets - the fastest way to ask what mwcceppc does with a body before giving it a unit. The argument order is `wibo sjiswrap.exe mwcceppc.exe <cflags> -c <src> -o <out.o>` and the two `-pragma` options need their quotes kept, or the compiler reports `Specified file 'off' not found` and silently produces an unrelated object. |

## The one rule that decides completion

**A unit is done when the build still reproduces retail with the unit's own object in the
link.**

Marking a unit `Matching` in `configure.py` swaps its input from `build/G2ME01/obj/<unit>.o`
(bytes `dtk` split out of the retail binary) to `build/G2ME01/src/<unit>.o` (our compile).
Until then, objdiff percentages are the only signal, and the "DOL and RELs are identical"
check says nothing about the unit - it validates the untouched parts of the binary.

Two consequences that have each cost a session:

- **A `NonMatching` unit's code is not in the binary at all.** A lane that writes an empty
  source file and reports "the module hash matches" has proven nothing: the module linked the
  `obj/` copy. Always report the hash **both ways** - `NonMatching` and `Matching`.
- **The same applies to REL modules.** A module's `.rel` links `obj/` for `NonMatching` units
  and `src/` for `Matching` ones. `cmp` on the `.rel` is vacuous unless the unit is `Matching`
  and the linked object is the one compiled from source.

**`configure.py` does *not* refuse a `Matching` object with no source file** - an earlier version of
this paragraph said it did, and that was wrong. `tools/project.py` prints `Missing source file
<path>`, sets `link_built_obj = False` and links the **retail** object instead, so the unit looks
`Matching` and is not ours. That is how `flip_test.sh` used to report `PASS` for units that proved
nothing; it now refuses, and `tools/gate.sh` greps the configure log for that line. It only prints it
when `warn_missing_source` is set or the unit is `completed`, so silence means nothing either.

### Why a unit will not promote: extra functions, not size

Measure it with `tools/unit_fit.sh <unit>`. Two facts, both measured on 2026-09-25, and the second
is the one that matters:

- **An object bigger than the claimed range is not by itself fatal.** `Kyoto/Basics/RAssertDolphin.cpp`
  is `Matching` today with `.text` 1964 bytes against a claimed 1852. The reason is that its one
  extra function, `hack__Fv` (112 bytes), is byte-identical to what retail has immediately after
  the claimed range - `dtk`'s split attributed those bytes to the neighbouring unit, so our object
  overlapping them changes nothing. The proof that our object really is in the link: editing one
  string inside it moves `build/G2ME01/main.dol`'s sha1 off retail immediately.
- **What actually blocks a flip is emitting functions the retail unit does not have *and* that are
  not retail's bytes there.** `Kyoto/Audio/CStaticAudioPlayer` emits eight of them, 868 bytes of
  weak container and destructor instantiations (`reserve<vector<auto_ptr<uchar>>>`, `__dt__CDvdRequest`,
  `__dt__basic_string`, `destroy<pointer_iterator...>`, ...) - the unit measured 4668 bytes against a
  claimed 3800, and `tools/flip_test.sh` showed the REL differing. `CRumbleVoice` is the same shape
  (132 bytes over, its six weak vector instantiations emitted out of line), and four units hit this
  one cause in a single session.

So: run the tool first, read the *extra function* list, and only then decide whether the remaining
work is matching (fixable) or codegen the source cannot express (report it as blocked). Do not
trust a percentage - a 99.9% unit with 868 bytes of extra emissions will never be `Matching`.

**Correction, 2026-09-25: the `CStaticAudioPlayer` half of that bullet was wrong.** It is the one
thing in this file most likely to be copied forward, so it is corrected in place. Those 868 bytes
are **not** what blocks that unit: the DOL link passes `-strip_partial`, mwldeppc deletes the
duplicate weak copies from the middle of the section, and the flipped `main.dol` comes out the
**same size as retail**. Re-measured: the unit is 23/24 and its flip now fails on the *order* of
those instantiations, not their presence - see "An emission-order wall: out-of-line template
instantiations" below, which has the ELF-symbol proof. The other half of the bullet stands: a
strong (non-weak) extra definition, or a weak one that no other object also defines, is real and
does block the flip. The way to tell the two apart in one build is the ELF symbol check in that
new section; `unit_fit.sh`'s list alone cannot.

### Pairing a function the retail symbol table has no name for

Four units were blocked in one session by the same thing, and it is solvable. `dtk` cannot name a
TU-local weak template instantiation, so the base object calls it `fn_803254FC`; objdiff pairs by
name, so the byte-identical function our compiler emits scores 0%, and hand-writing an `extern "C"`
body under that name only makes MWCC emit its own copy as well.

**Rename the retail symbol instead** - `config/G2ME01/symbols.txt` *is* the rename mechanism, and
`dtk` will name the base-object symbol accordingly:

```
__dt__Q24rstl47vector<10SAdsrDelta,Q24rstl17rmemory_allocator>Fv = .text:0x803254FC; // type:function size:0x84
```

Do not guess the name: write the function in the unit's source, compile, and read the name MWCC
emitted out of our own object with `build/binutils/powerpc-eabi-nm`. Measured 2026-09-25 on
`Kyoto/Input/CRumbleVoice`: six functions went from 0% to 100% this way, taking the unit from 8/16
to 13/16 and the project from 2759 to 2764.

Two limits, both measured: it **enables pairing, not matching** - the code still has to be
byte-exact - and it does not fix a unit that emits functions retail does not have
(`tools/unit_fit.sh` still lists `__dt__rstl::reserved_vector<ushort,4>` and a second fill
instantiation there, 180 bytes over, which is why the unit still cannot be promoted). Constructors
declared inline emit no standalone helper symbol, so there is no source shape that suppresses the
extra destructor while keeping these pairs.

## A REL unit that defines a function nothing calls cannot be flipped (2026-09-25)

`ForgottenObject`'s unit cannot promote, and one of the two reasons is not a source problem at all.

With the unit `Matching`, the linked module comes out **2736 bytes instead of retail's 2832** -
96 short - because `fn_24_1E4` (module `.text` 0x1E4..0x238, 84 bytes) is referenced by nothing in
the module and **mwldeppc dead-strips it**, along with the 8-byte `.rodata` and 4-byte `.text` gaps
that leaves. `build/G2ME01/<Module>/ldscript.lcf` is written by `dtk dol split`, and its FORCEACTIVE
block holds only the module roots (`_prolog`, `_epilog`, `_unresolved`, `_ctors`, `_dtors`),
everything reachable **from data** - the vtable's functions, the rodata objects - and the data
objects themselves. An orphan that only code called is not in that set.

`config/` cannot influence it: putting `scope:global` on `fn_24_1E4` in `symbols.txt` and re-running
`dtk dol split` leaves the lcf byte-identical, and the file lives under `build/` and is regenerated
by the `split` ninja rule, so no `configure.py` edit can carry it. Adding the one line `fn_24_1E4`
to the FORCEACTIVE block is enough - nothing else needs force-active - and with it the module is
**2832/2832 bytes with 55 differing bytes in 19 runs**.

**So the fix is in dtk, or a post-split hook in `tools/project.py`** taking a per-module extra
list, e.g. `config/G2ME01/rels/<Module>/forceactive.txt`, whose content for `ForgottenObject` is
one line. Until that lands, **no REL unit that defines an uncalled function can be flipped** -
a class of module, not one module. It is the same shape as the build-clone defect below: a rig
property silently deciding whether a unit's work counts.

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

### The unattended goal loop, and five ways it passed changes nobody had checked (2026-09-27)

`tools/run_goal.sh` (run by `mp2-goal.service`) takes items from `build/goal/queue.json` in the
`../wt-mp2-goal` worktree, runs one agent per item using `docs/goal-unit-prompt.md`, judges the result
with `tools/goal_check.sh` and commits on `goal/decomp` only when the judge says PASS. Every
`MP_GOAL_FF_EVERY` passes (default 10) it fast-forwards master to `goal/decomp` if master is clean.
It first merges master in, because master gains tooling commits the branch lacks. A conflict
aborts that merge and skips the fast-forward. The first version ran for hours and never produced
a result anyone could trust:

- **Ghost agents.** `opencode run` without `--standalone` is a client of the shared server. When the
  timeout killed the client, the session kept running on the server, and several agents ended up
  editing the one worktree at once. Agents now run with `--standalone` under `timeout -k`, so they die
  with the run.
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
  claims. A REJECT fails the attempt and appends its reason to the item's notes. No verdict means
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
  attempt, not failed, and the reason goes into its notes.
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

## The recipe for decompiling a REL module

This works and is verified. It is the one arrangement that survives the module's hash check,
and every earlier attempt failed by not using it.

### The problem it solves

A module's `.rel` is linked from objects, and `dtk` fills every range **no unit claims** with
bytes split out of the retail module (the `auto_*` units in `build/report.json`). So a unit does
not have to own the whole module - and it must not, because a unit that claims a range its own
object does not reproduce removes those bytes from the link and the module stops matching.

### The arrangement

1. Scaffold the module: `python3 tools/scaffold_rel_module.py <Module> [Class] --write` prints a
   splits entry, a `Rel(...)` call and a source skeleton in address order. It is a starting
   point, not the answer - its default claims the whole module for one unit.
2. Write the functions you can.
3. **Re-split so the `Matching` unit claims only the ranges its own object actually reproduces.**
   Everything else stays unclaimed (or is claimed by `NonMatching` units with no source), and
   `dtk` fills it from retail.
4. Verify with the module's hash against `config/G2ME01/config.yml`, not with `87 files OK`.

Two worked examples, both committed:

`ScriptRiftPortal` - a three-way split, where only the middle unit is ours:

```
MetroidPrime/ScriptObjects/CScriptRiftPortalPrefix.cpp:   NonMatching, no source file
    .text 0x0..0xB4          (.bss 0x0..0x30)
MetroidPrime/ScriptObjects/CScriptRiftPortal.cpp:         Matching, and it exists
    .text 0xB4..0x128        (.bss 0x30..0x34)
MetroidPrime/ScriptObjects/CScriptRiftPortalTail.cpp:     NonMatching, no source file
    .text 0x128..0x2B38      (.rodata 0x0..0x78, .data 0x0..0x84)
REL/REL_Setup.cpp:          the module's prolog/epilog scaffolding
    .text 0x2B38..0x2CDC     (.rodata 0x78..0xFC)
```

`Metaree` - the same idea with the ranges left unclaimed instead of named:

```
MetroidPrime/ScriptObjects/CScriptMetaree.cpp:  Matching
    .text 0x324..0x460, .bss 0x0..0x4
REL/REL_Setup.cpp:
    .text 0x1F80..0x2124, .rodata 0x90..0x114
```

Everything between `0x460` and `0x1F80` is unclaimed and therefore retail bytes, which is where
the module's remaining functions live. The named units total 23 functions (18 ours plus the 5
`REL_Setup` ones, all exact); the module has 59 in total, so the unclaimed 36 are still retail
and show up in `build/report.json` as `Metaree/auto_*` entries. The module still hashes to what
`config.yml` records - that is the point of the arrangement: a module can be partly decompiled
and still correct.

**Every module's `REL_Setup` tail is free, and `tools/wire_rel_setup.py` claims it (2026-09-28).**
The shared "REL" lib compiles `_unresolved`/`_epilog`/`_prolog`/`ModuleDestructors`/
`ModuleConstructors` for every module; a module links them once its `splits.txt` claims the
range. 27 modules had no claim at all and went 0 -> 5 each (+135 matched, +135 linked, all 86
hashes held) with no C++ written. The claim alone breaks the hash, by 48 bytes of relocations:
`_epilog`/`_prolog` call `RELExit`/`RELMain`, and in these modules those were unnamed
(`fn_55_104`) or named without `scope:global` (AtomicAlpha), so the reference stayed unresolved.
The tool names all four, reading the entry points from dtk's disassembly between two builds.
It judges nothing - build, check the hash, and revert a module that moved.

### Four structural facts about a REL split, learned wiring `ScriptCoin` (2026-09-25)

Each of these cost a lane something, and all four are properties of the arrangement rather than
of the module:

1. **One unit cannot claim two discontiguous ranges.** `dtk dol split` fails with
   `Cyclic dependency encountered while resolving link order: ...Rest.cpp -> ...Coin.cpp`. So a
   module with k separated claims needs k files and k unit entries - which is why `ScriptCoin`
   has six. (`CAi` gets away with four ranges because they are four *sections*, not two ranges
   within one section.)
2. **The scaffold's name for the REL tail is the wrong unit name, not the wrong range.**
   `REL/REL_Setup.cpp` claiming `.text 0x36A4..0x3848` plus `.rodata 0x68..0xEC` builds and links
   and still **breaks the module hash**: the GOT grows 40 bytes and the `bl` in `_epilog`/`_prolog`
   gets a real displacement where retail holds a placeholder. The *same ranges* under a
   module-unique name (`CScriptCoinTail.cpp`) hash correctly. Naming the tail `REL/REL_Setup.cpp`
   at any other start fails to link outright with `multiply-defined: '_unresolved'`.
3. **A `Matching` unit's `.text` is dead-stripped unless something in FORCEACTIVE references it.**
   dtk's generated `ldscript.lcf` FORCEACTIVE list holds the module's entry points and whatever its
   own data and code reference. A unit claiming `.text 0x27C8..0x27D0` for an 8-byte accessor links
   and produces a `.text` **8 bytes short**, because nothing references `fn_58_27C8` and it is simply
   dropped. Adding `scope:global` to the symbol in `symbols.txt` does **not** get it into
   FORCEACTIVE - tested, rejected, reverted. **This blocks the whole tail of `ScriptCoin`**
   (`0x2600..0x36A4`: four functions and their neighbours are all unreferenced), so no unit there
   can be promoted without a source that also reproduces an adjacent *referenced* function. It will
   apply to any module whose remaining functions are unreferenced helpers.
4. **A non-`Matching` object can contribute bytes the split does not claim.**
   `CScriptCoinTouchBounds.o` emits 1 byte of local `.bss` for no claimed range; the module still
   hashes because mwld absorbed it, but it is a latent hazard in the same family as "a DOL unit can
   be blocked by data". Related trap: `CScriptCoinRel.o` puts its 4-byte slot in `.comm`, not
   `.bss`, so `unit_fit.sh` prints `.bss claimed 4 ours 0 SHORT by 4` on a module that is in fact
   correct - one more reason its REL column cannot be trusted (see below).

The shape that works, then: **one contiguous range per unit, one file per unit, the module's own
name for every unit, and the `Matching` units only where the object reproduces the range exactly.**
`ScriptCoin` is the worked example: three `Matching` units at `0x0..0xA0`, `0x1350..0x1370` and
`0x1B24..0x1BA4`, three `NonMatching` units carrying the rest, and `0xA0..0x1350` left unclaimed
as a single `auto_00_000000A0_text.o`.

### Why the `NonMatching`-with-no-source trick is legal

`configure.py` requires a source file only for `Matching` objects (it exits with
"Missing source file" otherwise, taking `build.ninja` with it). A `NonMatching` entry may name a
path that does not exist, which is how a module keeps retail bytes for a range while still giving
that range a name in the splits. It is also why the earlier `SkyRipple`-style scaffolds broke:
they marked the claimed ranges as the unit's own while the unit had nothing in them.

### The check that actually means something

```sh
python3 - <<'PY'
import re, hashlib
cfg = open('config/G2ME01/config.yml').read()
for name, expected in re.findall(r'object: files/RelProd/(\S+)\n\s+hash: ([0-9a-f]{40})', cfg):
    mod = name[:-4]
    actual = hashlib.sha1(open(f'build/G2ME01/{mod}/{mod}.rel', 'rb').read()).hexdigest()
    print(('OK  ' if actual == expected else 'DIFF'), mod)
PY
```

`87 files OK` from a lane is not that check; see the rig defect above.

### Why the matched total can go *down* when module work lands

Claiming ranges in a named unit removes those bytes from the `auto_*` units that `dtk` builds from
the retail module. The old explanation here - that an `auto_*` unit's functions count as matched by
default - is **wrong**: measured 2026-09-25, the report holds 791 `auto_*` units with 24,456
functions and **none of them matched**. What moves the headline is attribution: a rename can change
which unit owns a function, and a unit can stop being `Matching`. The Puffer 2633 -> 2629 anecdote
below needs re-deriving in that light.

Puffer is the example: its 9 functions are now 6 + 3 in two named units, all exact, and the
project total went 2633 -> 2629. Nothing regressed; the 9 were previously counted for free and
the ranges they left behind are the ones now listed as unmatched `auto_*` entries. Judge module
work by the module's hash and by the named units' percentages, not by the global total.

### A technique that works on the generated loader structs

`scripts/generate_script_loaders.py` writes a constructor as `Name() : a(), b(), c() { a = 1.0f;
b = 2.0f; }` - every member default-constructed, then assigned in the body. Retail does not do
that: it initialises most members *in the mem-init list* and leaves only the ones needing a
statement in the body. Moving the assignments into the list is a pure source rearrangenent that
moves the score by tens of points (measured 2026-09-25):

| unit | before | after |
| --- | --- | --- |
| `SLdrTweakTargeting_Scan` | 78.04% | **99.52%**, `__ct__` 0.00 -> 100.00% |
| `SLdrTweakTargeting_VulnerabilityIndicator` | 86.88% | **99.52%**, `__ct__` 56.48 -> 100.00% |
| `SLdrTweakCameraBob` | 85.49% | 91.02% |
| `SLdrTweakSlideShow` | 76.23% | 81.87% |
| `SLdrTweakPlayerRes` | 54.90% | 57.18% |

Three things the lane proved the hard way: retail's *program order* is the mem-init order followed
by the body, so where the list stops matters; a member can genuinely be set twice -
`indicatorColor(CColor::Green())` with `indicatorColor = CColor(1.f, 1.f, 1.f)` in the body is
what retail does, the first call really executed and really overwritten; and `CColor(1.f,1.f,1.f)`
passes floats, not bytes. The retail default values are readable from the module's
`build/G2ME01/<Module>/asm/auto_*_rodata.s` plus its symbols file.

**Do not delete an initialisation to gain percent.** Two of that lane's seven files also dropped
real assignments - 27 beam-icon angles in `SLdrTweakTargeting`, and the whole weapon configuration
in `SLdrTweakPlayerGun_Weapons`. Both raised the *unit-level* fuzzy average and both were rejected
here: the function itself scored worse (`__ct__` 50.76 -> 21.33) and nothing was left setting the
members. Land only rearrangements that keep every value, and check the per-function score, not the
unit average.

### A technique that works: model the retail layout, do not fight it

`MetroidPrime/ScriptLoaderRel.cpp` reached 42/42 and became `Matching` on 2026-09-25 by naming the
problem correctly. Retail keeps each script-loader global as an 8-byte slot - a pointer plus a
padding word - and its setters are real mangled functions (`ScriptGUI_SetPtrs__FP10GUILoaders`,
`SetLoader_SafeZone`, ...), not the invented C++ names the file used. Modelling the slot as a
one-member template with an implicit conversion and an assignment operator reproduces both the BSS
layout and the code shape:

```cpp
template < typename T >
struct SLoaderSlot {
  T* value;
  uint padding;
  operator T*() const { return value; }
  SLoaderSlot& operator=(T* ptr) { value = ptr; return *this; }
};
```

Two lessons from the same session, both cheap and both general: **a BSS slot's padding is part of
the layout** - if a unit's `.bss`/`.sbss` is a few bytes off, look for a word the original kept for
alignment - and **name the function what the retail symbol says**, using `symbols.txt` to check,
rather than inventing a friendlier name; objdiff pairs by name, so an invented name scores 0%
however identical the code is.

The same lane moved `CScriptAreaProperties::LoadAreaProperties` from 0.00% to 81.25% with a
partially reconstructed body (the unit is still `NonMatching`).

### A technique that works: `const` on by-value parameters, and a `const` local

Three of four small leaf functions that a lane brought to 100% on 2026-09-25 were fixed by a change
that cannot affect the symbol or the callers:

```cpp
void CHealthInfo::SetCauseOfDeathWeapon(CWeaponMode mode, TUniqueId id) const { ... }  // retail's mask lands in r5
void CHealthInfo::SetCauseOfDeathWeapon(const CWeaponMode mode, const TUniqueId id) const { ... }  // ours does too
```

Top-level `const` on a by-value parameter does **not** appear in the mangled name, so it is invisible
to every other unit, and it is enough to move MWCC's register allocation. The same lever in a
function body is a one-statement `const` local of the value being compared - `const bool swap = flag;`
used throughout made retail mask the bool in place in `r4` instead of `r0` in
`CGameOptions::ToggleControls`; replacing a one-line ternary with two locals fixed
`InitSoundMode`. Confirm per function with the report: it can move a score the wrong way too.

### A negative result: constant-trip-count loops are always unrolled

`CPlayerState::InitializeScanTimes` sits at 97.63% and cannot be finished. Retail's first loop is a
`for`/`while` shape - the test block sits *after* the entry branch - and its iterator is in `r6`
where ours is in `r7`. But MWCC fully unrolls any `for`/`while` with a constant trip count in these
units (5-8 iterations, 38-42 instructions of difference), and the only shape it does *not* unroll,
`do...while`, is exactly the one that cannot produce retail's entry branch. Every form was tried.
Treat a retail loop with a test-after-entry and a constant count as blocked, not as unfinished.

### Rig trap, found and fixed 2026-09-25: a failed flip left a broken DOL behind

`tools/flip_test.sh` reverts `configure.py` when a flip fails, but it did not rebuild - so
`build/G2ME01/main.dol` went on holding the binary the failed flip had produced, and the next
`sha1sum` read that instead of retail. A lane hit it and reported `ee273df2...` as the DOL hash.
The script now rebuilds after reverting and prints the restored hash, or says loudly that the rebuild
failed. If you ever see a DOL hash that is not `6ef9b491...` with a clean `git status`, rebuild before
investigating anything else.

### Identifying an unnamed class from its vtable (the TypesMatch ids)

`TypesMatch.cpp` needs the classes behind 32 type ids, and the tree names none of them. The recipe
that worked (2026-09-25, worth 94 functions):

1. Find the vtable that holds the id. `dtk dol info config/G2ME01/config.yml` prints the section
   table; search the DOL's `.data`/`.rodata`/`.sdata` for the id as a word. A class's vtable starts
   with `[0][0][dtor][TypesMatch]`, then the flat `CEntity`/`CActor` slots, so the entry that points
   at the id's `TypesMatch` pins the whole table - and the table gives you the class's own virtuals,
   which is what its member helpers will be.
2. The class's **parent** is the class whose `::TypesMatch` its override calls. Read it off the
   call, not off a plausible-looking name.
3. Name the class, then rename the retail symbol: `config/G2ME01/symbols.txt` carries the
   `TypesMatch__<class>CFi` and both `TCastToPtr<...>` names, and dtk will name the base object from
   them. Take the mangled names from our own compiled object (`powerpc-eabi-nm`).

Two traps, both found the hard way:

- **A rename must replace its `fn_` line, never be inserted beside it.** Two symbols on one line is
  a parse error for dtk (`invalid digit found in string`) and the whole build dies, so it is caught
  immediately - unlike a rename that is *dropped*.
- **dtk rewrites `config/G2ME01/symbols.txt` on every build and drops a symbol that duplicates an
  address.** That is why the reference-form cast has to replace its `fn_` line: leave both and the
  reference form silently stays unnamed and scores 0.00%, which reads exactly like "the code is
  wrong".

`docs/research/TypesMatch_unnamed_ids.txt` is the table this produced - per id: parent, `TypesMatch`
address, both cast addresses, vtable address, the class's own virtuals. `docs/research/rename_typesmatch_ids.py`
regenerates the `CUnknown<id>` block from it; put a real name in `CLASS` and re-run when one is found.

### Writing a destructor whose class is only a type id (+16 functions, 2026-09-25)

Once the class exists, its own virtual destructor in the same unit is writable, and it is what
establishes the members. Two MWCC rules decide whether the derived destructor matches, and both cost
a build to find:

- **MWCC inlines a base destructor only when the base destructor is compiler-generated.** With
  `~Base();` declared in the class and defined out of line, the derived destructor emits
  `bl ~Base` (as it must for a base in another TU) and lands ~20% short. Delete the declaration *and*
  the out-of-line definition and MWCC still emits `__dt__<Base>Fv` for the vtable, but the derived
  destructor then inlines the base's body and matches byte for byte. `CUnknown50` is exactly that: an
  empty `CScriptDamageableTrigger` subclass.
- **MWCC emits a null guard (`addic. r0,rN,off; beq`) in front of a member's destructor when the
  member's destructor is defined in its class, and none when it is only declared.** One instruction
  apart, and getting it wrong costs the *outer* destructor its 100% while the member's own
  destructor still matches.

Other things that were true here, all measured:

- **objdiff pairs by symbol name and ignores relocation targets in an unlinked object**, so a retail
  `fn_8009D51C` that is our `SRefHolder::Release` is a naming problem, not a codegen problem.
- A destructor of a class with no members and a polymorphic base is `if (this) { vptr = ...;
  if (flag > 0) Free(this); }`.
- `rstl::optional_object<T>` puts its valid flag at `round4(sizeof(T))`, so the offset of the byte a
  destructor tests *pins* `sizeof(T)` - that is how id 63's member was identified as
  `optional_object<TCachedToken<T>>` rather than guessed.
- **Corrected here: ids 40, 46 and 68 derive from `CUnknown33`, not `CActor`** - their overrides all
  call `TypesMatch__10CUnknown33CFi`, which the first pass through this list got wrong.

### A static initialiser has to go through a function to reach `__sinit`

`CGX::__sinit_CGX_cpp` was 76.92% because it was missing a store: retail computes
`0x1C807` into `lbl_80419910` at run time (`lis r3,2; addi r0,r3,-14329; stw r0,lbl_80419910`) and
the symbol lives in **`.sbss`**, unclaimed. Every obvious spelling puts it in `.sdata` with a static
initializer and leaves `__sinit` unchanged - measured: a plain `= 0x1C807`, a `static` one, a
`volatile` one, and the same value written out as an enum/shift expression all give
`.sdata=0x8` and `store_in_sinit=0`.

**MWCC routes an initializer into `__sinit` (and the object into `.bss`) when the initializer is not a
constant expression to the front end, even when the back end folds it.** Putting the value behind a
function is what does it:

```cpp
// __sinit_CGX_cpp then emits exactly retail's lis/addi/stw, and the object is `B` in .sbss
static inline uint alphaCompareAlways() {
  return GX_ALWAYS | (0 << 3) | (GX_AOP_OR << 11) | (GX_ALWAYS << 14) | (0 << 17);
}
extern "C" uint lbl_80419910 = alphaCompareAlways();
```

`inline` is required: without it MWCC emits a real `bl` to the helper (87.31%, and an extra function
in the object). The same trick applied to `CGX::sGXState` does not help - it has no initializer to
move, which is why it stays COMMON. Worth knowing before concluding "MWCC put my global in the wrong
section": check whether the initializer is a constant expression first.

Two more negatives from the lane that first hit this, so nobody spends a session on them:

- Adding `operator=(const T*)` to `rstl::single_ptr` to match retail's `__as__...FPQ2...` mangling is
  not viable: it makes the `= nullptr` idiom ambiguous tree-wide (MWCC stops at `CActor.cpp:255`,
  `CCubeMoviePlayer.cpp:412`, `:536`, `:588`, `:909`) and MWCC inlines the 8-byte body at `-O4,p`
  anyway, so the out-of-line symbol never appears.
- `decomp_build.sh <unit>`'s per-function percentages are the ground truth. A bare two-object
  `objdiff-cli diff` disagrees on units that set `reverse_fn_order` (it reports 99.6x% for functions
  the project counts as matched). Score with the tool, not with the raw diff.

### A `Matching` unit's weak instantiations can steal a symbol retail has somewhere else (measured 2026-09-26)

Found promoting `FStringTableFactory` (retail 0x80312320, 0x64) out of the `NonMatching`
`src/Kyoto/Text/CStringTable.cpp` into a `Matching` unit of its own. It was already at 100.00%,
the new unit came out at **100.00% on both its functions, `flip_test.sh` PASS, DOL sha1 held** -
and the gate still reported

```
matched  3131 -> 3129   linked  1754 -> 1756
  WORSE  main/Kyoto/Text/CStringTable :: GetIObjObjectFor__22TToken<12CStringTable>...  100.00% -> 0.00%
  WORSE  main/Kyoto/Text/CStringTable :: GetNewDerivedObject__40TObjOwnerDerivedFromIObj<12CStringTable>...  100.00% -> 0.00%
```

**The mechanism.** `CFactoryFnReturn`'s converting constructor is defined in the header, so any
translation unit that builds one emits it - and, through it, the weak inline template members
`TToken<T>::GetIObjObjectFor` and `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject`. Those two
were *also* being emitted by `src/Kyoto/Text/CStringTable.o`, which is where retail's copies came
from, and retail has them at 0x80312434/0x80312460 while the new object puts them at +0x2F0 and
+0x31C (three weak `__dt__` instantiations land between). Two owners for one symbol.

**`flip_test.sh` cannot see this, and neither can the sha1.** The linked ELF still has both
symbols at retail's addresses and the DOL is byte-identical, because the new object's copies sit
past the claimed range and are dead-stripped. What breaks is the *report*: objdiff pairs the
vanilla function with the dropped copy. So the acceptance test passes and the gate fails, and the
only thing that catches it is a per-function baseline recorded on a clean tree.

Three rules out of it:

- **Before promoting a unit, list what its object emits that another object also emits.**
  `powerpc-eabi-nm -n build/G2ME01/src/<new>.o` against `build/G2ME01/obj/<other>.o`. Any symbol
  in both is a coin toss, and the extras past the claim are not harmless.
- **A new `Matching` unit cannot be a subset of an existing unit's claim if the two objects would
  both define a symbol the existing claim also covers.** The split has to go the other way round,
  or the new unit has to take the whole run.
- **MWCC 2.7 has no `extern template`**, so there is no source-level way to suppress the extra
  instantiation; the only fixes are a wider claim with a matching order, or accepting the loss.
  `docs/research/paks.md` records the worked example and both attempts (`FStringTableFactory` and
  `FRuleSetFactory`, the second blocked for an unrelated reason: its `operator new` names a
  `scope:local` symbol, and claiming it breaks the link with
  `undefined: '@stringBase0_803AC548'`).

### Adding a *string literal* to a unit can move an unrelated function (measured 2026-09-25)

Found while defining the port's retail globals in `main.cpp`. The DOL is unaffected either way -
the unit is `NonMatching` - but `report_diff.py` is a ratchet on per-function percentages, so an
unrelated function going from 96% to 95.97% is a red gate and blocks the change.

**Adding one string literal to `main.cpp` grew `CGameArchitectureSupport`'s constructor by 32
bytes and gave it a `__cvt_dbl_usll` call, and cost `AddWorldPaks` a fraction of a point** - while
`StreamNewGameState` in the same unit went *up* 6.6 points, so the unit average improved and the
gate still failed. `main.o`'s `.text` grew 0x2094 -> 0x20b8. The same two symbols defined in
`CPowerBeam.cpp` cost `Update` and `EnableSecondaryFx` 100% -> 98.56% and 100% -> 97.87%, and
`matched` fell 3032 -> 3030. It is the literal, not the symbol: 17 other globals in `main.cpp`,
including relocated pointer words in `.sdata2`, leave `.text` at 0x2094 byte for byte, and a
`static char k[] = "..."` buffer that the pointer then refers to is also inert. The workaround is
to spell string storage as a named mutable buffer and point the pointer at that.

The general lesson: **mwcceppc's codegen is not stable under additions that look like data.** Before
adding a definition to a `NonMatching` unit someone is actively decompiling, check

```sh
$MP_TOOLCHAIN_DIR/build/review-tools/bin/ninja -f build.ninja build/G2ME01/src/<unit>.o
build/binutils/powerpc-eabi-objdump -h build/G2ME01/src/<unit>.o | grep ' .text'   # must be unchanged
```

Two other host-compiler facts, both of which silently delete the definition you just wrote (the
link then fails on a symbol that looks defined in the source):

- **GCC drops an uninitialised tentative definition that nothing in the translation unit reads.**
  `extern "C" int x;` in a TU that never mentions `x` again produces *no symbol at all*, at every
  optimisation level. Every port-side definition of a retail global needs an explicit `= 0`.
- **Inside `extern "C" { }`, GCC gives a `const` declaration internal linkage** unless it also says
  `extern`, and an unreferenced internal object is then dropped the same way. Six of the 19 needed
  the redundant `extern`.

### One instruction of register allocation, fixed by assigning the widened local back

`CGX::SetDstAlpha` was 99.43% - 140 bytes, every instruction in the right order, and one
register-allocation difference in the tail: retail materialised the constant `1` in `r0` and the
masked alpha in `r7`, we used `r5` and `r0`. **Removing the `const uint` local made it worse**
(three instructions short instead - MWCC then elides the second mask entirely), and every spelling of
the mask expression was a wash. What fixed it was assigning the *widened* local back to the member
rather than the original `uchar` parameter:

```cpp
gpGXState->x24c_fogParams.x14_ = 1;
const uint normalizedAlpha = alpha;
gpGXState->x24c_fogParams.x15_ = normalizedAlpha;   // was: = alpha
GXSetDstAlpha(enable, normalizedAlpha);
```

Same value, same instruction count, and the allocator stops needing a second temporary. Worth trying
on any last-percent function where the only difference is which register a value lands in: making the
*store* consume the widened temporary is a lever that costs nothing semantically. Ruled out in the
same batch, all 4-5 differing instructions: `static_cast<uint>`, `alpha & 0xff`, a `uchar` local, a
`const` local, swapping the two stores, and rewriting the `if (!enable)` as `if (enable)`.

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
  129-vs-132. The DOL's `Kyoto/CToken.cpp` says the same thing. **The recipe that actually answers the question, in one build:**

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

## What still blocks most modules

- **A module's `.rodata` is not always splittable per unit.** `Tweaks` shows the shape of it:
  `config/G2ME01/rels/Tweaks/splits.txt` splits only `.text` and `.bss`, so all 0x408 bytes of the
  module's `.rodata` come from the base object `auto_03_00000000_rodata.s`, whose symbols are
  FORCEACTIVE. A `Matching` unit that contributes any `.rodata` therefore adds a second
  contribution and the module's hash breaks - and the constants a unit needs are not even
  contiguous (one unit wanted `.rodata` 0x28 and 0x30 but not 0x2C). Only a unit that owns the
  whole pool can claim it. Check the module's split before promising a unit there, and prefer the
  units whose gains are `.text` only.
- **`__nw__` vs `__nwa__` decides whether objdiff pairs a `new` at all (measured 2026-09-25,
  `Tweaks`).** mwcceppc mangles the *array* operator as `__nwa__FUlPCcPCc` and the *scalar*
  one as `__nw__FUlPCcPCc`, and the suffix is part of the call target, so writing
  `new (file, line) T[1]` where retail used `new (file, line) T` makes every allocation in the
  function fail to pair. Read the suffix off the retail symbol table before writing the first
  `new`: it also tells you whether the source had an array subscript, which decides whether an
  array-construction loop belongs in the body. Dropping the `[1]` in `REL_CreateTweakGlobals`
  moved it 140 bytes closer to retail in one edit.
- **MWCC common-subexpression-eliminates the `__FILE__` argument of `new`, retail does not
  (measured 2026-09-25, `Tweaks`).** Retail's `REL_CreateTweakGlobals` re-materialises
  `lis r4, lbl_82_section4_3F0@ha; addi r4, r4, lbl@l; addi r4, r4, 0xe` at all 15 of its
  allocation sites - 30 references to the symbol. Ours emits `addi r31,r4,14; mr r4,r31` once
  and reuses `r31`, which is 2 instructions short per site. There is no source expression that
  stops the hoist, and the only way to get the literal back is the `NEW` macro - which adds
  `.rodata`, i.e. exactly the wall above. Treat a unit that allocates with `new` against an
  unsplittable `.rodata` pool as capped, and say so instead of chasing the percentage.
- **A 99.2% wall that is not source-expressible (measured 2026-09-25, `Tweaks`).** Seven units
  sit at exactly the same two-instruction difference: retail moves the first stream pointer into
  `r4` and reuses `r4` for the switch's `propertyId`, ours uses `r3` and `r6`. Retail's own
  *Matching* units in the same module (`SLdrTweakPlayer`: 14 cases, `SLdrTweakGuiColors`: 15)
  emit the same shape ours does - the difference is the switch size. Ruled out by the lane:
  id/size type and constness, declaration order, all six case permutations, `default:` first, an
  if-chain, suffixed literals and casts. That is MWCC register allocation, and no source rewrite
  reaches it; treat these as blocked, not as unfinished.
- **`CGX::SetVtxDescv_Compressed` is the same wall at 95.78% (436 bytes, 2026-09-25).** The logic
  is identical instruction for instruction - same two loops, same unrolling (11, then 2 x 4), same
  `slw`/`srw`/`clrlwi` sequence, same early-out - and the *only* difference is which of `r4`..`r9`
  each value lands in. Retail fills them in the order mask-`3`, `gpGXState`, shift, `list`, index;
  we fill them in the order `list`, mask-`3`, `gpGXState`, shift, scratch. Both use exactly
  `r0, r3..r9, r31` and neither spills, so it is one allocation-order decision, not pressure.
  **45 source variants failed to move it** (best 61 differing instructions from 63, by putting
  `idx` and `shift` in one `for` header): loop variable `uint`/`int`/`u32`/`uchar`, `<` vs `<=` vs
  `!=` bounds, `idx * 2` vs `idx + idx` vs an explicit `shift` induction variable, `continue` vs a
  positive `if`, both store orders, `const` and named-mask locals, the class's own
  `MaskAndShiftLeft`/`ShiftRightAndMask` helpers, `reinterpret_cast<uint*>` stores, swapping the
  two loops' bodies, merging them into one, hoisting `idx` above `list`, `static const GXColor`
  initialisers, and writing through `list++` instead of `++list`. Treat it as blocked; the unit's
  remaining blockers are its data sections anyway (see above), so this is not where the value is.

- ~~**`UnkVtable20__6CActorFv` has no definition**~~ **Superseded, 2026-09-25** (commit `8f5b538`):
  retail's vtable slot +0x20 points at `0x8004B3E0`; the function clears the two reserved-vector
  counts at +0x110 and +0x11c and bit 7 of the byte at +0x128. It is named in `symbols.txt`, defined
  in `CActor.cpp`, and the linked DOL exports it. **Measured again on current `HEAD`** (with the
  fixed `flip_test.sh`, see the rig defects): promoting `CScriptCannonBall` no longer fails on a
  symbol at all - the DOL links and the module's REL differs
  (`build/G2ME01/ScriptCannonBall/ScriptCannonBall.rel: FAILED`), because its split claims the whole
  `.text` while only 12 of its 26 functions are at 100%. The `__ct__6CActorF...` failure recorded in
  the commit message does not reproduce on `HEAD`; the next real step for that module is the other
  14 functions, not a missing symbol.
- **`include/MetroidPrime/Enemies/` holds only the `SwarmBasics` layer** - `CSwarmBasics.hpp` and
  five `CSwarmBasics*` sources, landed with the module - and now `CAi` (11/11) and `CPatterned`
  (10/10) as `Matching` units too. The hierarchy exists; what remains thin is the *behaviour*: the
  creature classes' own virtuals are largely unnamed and `CPatterned`'s constructor is unwritten.
- **There is no GUI hierarchy at all.** Both `src/GuiSys/` and `include/GuiSys/` are empty and
  neither is listed in `configure.py` or `files.cmake`. An earlier version of this entry claimed
  `src/GuiSys/` held the decompiled `CGui*` hierarchy and only the include tree was missing - that
  was wrong, and it was written here from recollection rather than checked. The available GUI header
  is a stub. Any GUI-dependent module (ScriptGui, ScriptFrontEndDataNetwork) can do its accessors
  and loader wiring but not its widget work.
- **`ScriptGui`'s loader table** is written and verified, but the loaders it registers are named
  only by address (`fn_60_6FF0` and friends) and their bodies are not written.

## Where a module can even be written

`include/MetroidPrime/Enemies/` now has `CAi` and `CPatterned` as `Matching` units (`CPatterned` is
`NonMatching` again since the 2026-09-28 upstream merge widened it to 103 functions), so a module
whose objects derive from them *can* be written - that was the blocker, and it is gone. What limits
those modules now is the behaviour inside the classes: most of the creature virtuals are unnamed,
`CPatterned`'s 0xB58-byte constructor is unwritten, and 75 modules' worth of actor code has to be
decompiled one function at a time like anything else.

Of the 86 modules, **11 are `Script*` units** (script objects that lean on `CEntity`/`CActor`,
which do exist) and the other 75 are creatures, bosses and swarms that need the missing Enemy
hierarchy. Acknowledge this before assigning module work: check that the base classes a module
needs actually exist.

### A DOL unit can be blocked by data, not by code

`Kyoto/Graphics/CGX` matched every function it could (51 of 54, 99.47%) and **is not promotable**,
because its *sections* cannot be reproduced by C++ source. **Updated 2026-09-25: the functions are
now 53 of 54 (99.69%) and the data blocker is fully characterised - but it is not the
"unsourceable constant pool" this section used to claim.** The corrected version is below, because
the correction is the useful part: the constants are not compiler-generated at all.

**What retail's object actually does** (`build/G2ME01/obj/Kyoto/Graphics/CGX.o`, six symbols):

| symbol | section, address | referenced from | defined in |
| `lbl_8041E4A0` | `.sdata2` 0x8041E4A0, `0xffffffff` | `__ct__SGXState` (the white `GXColor`) | `auto_11_8041E278_sdata2.o` |
| `lbl_8041E4A4` | `.sdata2` 0x8041E4A4, `0.0f` | `__ct__SGXState` (fog start Z) | same |
| `lbl_8041E4A8` | `.sdata2` 0x8041E4A8, `1.0f` | `__ct__SGXState` (fog end Z) | same |
| `lbl_8041E4AC` | `.sdata2` 0x8041E4AC, `0.1f` | `__ct__SGXState` (fog near Z) | same |
| `lbl_8041F8D8` | `.sbss2` 0x8041F8D8, 8 bytes of zero | `__ct__SGXState`, `__ct__SFogParams` (the clear `GXColor`) | `auto_10_80419828_sbss2.o` / `auto_12_8041F880_sbss2.o` |
| `black$localstatic3$apply_fog__3CGXFv` | `.sdata2` 0x8041B018 | `SetFog` (the `black` in the header's `apply_fog`) | `auto_11_8041AD50_sdata2.o` |

All six are **imports** in retail's object: `powerpc-eabi-nm --undefined-only` on it lists every
one, and none is defined. So retail's `CGX.cpp` *referenced* globals that live in other translation
units, and dtk moved each definition's bytes into the unclaimed `.sdata2`/`.sbss2` blobs
(`auto_*` objects named for the start of the gap they fill), leaving an import behind. Our object
instead **defines** them, as anonymous compiler-generated words (`@358`, `@359`, `@746..@748`) and as
the local static in `apply_fog`.

**So the fix is `extern`, not cleverer source.** Declaring the five `lbl_*` objects at file scope
and using them in the header's `SFogParams()`/`SGXState()` constructors is legal and reproduces retail
exactly; the naming is not a guess either, because nothing else in the DOL references them (checked
across every object), so the dtk placeholder name is the name the link needs. The sixth,
`black$localstatic3$apply_fog__3CGXFv`, is a `static` local in the header's inline `apply_fog`, and
its 4 bytes sit in a **third** blob, 0x1218 bytes below the other four - so it cannot be claimed by
CGX's split at all (one input section cannot land in two output ranges) and has to stay an import
too, which means an `extern "C"` declaration carrying MWCC's own generated name, `$` included. That
last step is the one with a real cost, and it is a **shared-header** change: `apply_fog` is inlined
into `Kyoto/Graphics/CCubeMoviePlayer.cpp` too, which currently emits its own copy of the symbol.

**The measured consequence.** Flipping CGX by hand (not with `flip_test.sh`, to see the shape of the
damage) makes `main.dol` **32 bytes longer** and shifts everything after the first insertion:
`lbl_8041E4A0` lands at 0x8041E480 instead of 0x8041E4A0, `lbl_8041F8D8` at 0x8041F8C8 instead of
0x8041F8D8, and `sGXState` at 0x804170E0 instead of the claimed 0x803DF828. There is no partial
credit: the module hash is a single comparison. Note that once the *code* is at 100% the failure
moves earlier than the hash - it becomes a **link** error, `multiply-defined`, because our object
defines `lbl_80419910` and so does the blob (see below for the fix).

**`sGXState` is the second, independent problem.** Ours is a **COMMON** symbol (`C`, 0x264) and
retail's is a real `.bss` object (`B`); a common symbol is placed by mwldeppc in a later section, so
`CGX::sGXState` lands 0x178B8 bytes past its claimed range. No source form tried moves it: a plain
`CGX::SGXState CGX::sGXState;` and every spelling of the out-of-class definition stay `C`,
`= CGX::SGXState()` is much worse (it turns `__sinit_CGX_cpp` into a 736-byte frame with a temporary
and a copy), and both `__attribute__((aligned(32)))` and `alignas(32)` **fail to compile** under
MWCC 2.6.2. The distinction looks like class type versus POD: every `.bss` symbol in our whole object
set is a POD static or array (`main.cpp`'s `static uchar sMainSpace[...]`, `CFrameDelayedKiller`'s
`sFrameDelayedList`, the C runtime's), and every class static with a user constructor comes out
COMMON (`CGX::sGXState`, `CStopwatch::mData`, `CCubeSurface::skDefaultNormal`). Note that a
`Matching` unit *can* hold COMMON symbols - `MetroidPrime/CAxisAngle.cpp` is `Matching` with
`.bss ... align:4 common` - so this is a placement problem, not a legality one, and the `align:4
common` spelling is the thing to try next.

**One of the three needed `splits.txt` lines is verified to work.** Giving CGX
`.sbss start:0x80419910 end:0x80419918` (the exact size dtk records for `lbl_80419910`) both fixes a
link error and places the symbol correctly:

```

### The positive form of the section above: name retail's constant, do not write a literal (measured 2026-09-26)

The CGX section above characterises the data blocker and does not get past it. The way out **is**
`extern "C" const T lbl_<addr>;` at file scope and using the name - and it is now measured on three
constants rather than argued, by the frame-loop lane writing five new `Matching` units.

The trap that makes this worth its own entry: **a `Matching` object's `.rodata`, `.sdata2` and
`.data` are linked into the DOL.** Any byte they add that `splits.txt` does not claim for that unit
grows the section, moves every address above it, and breaks the DOL's sha1 **with every function in
every unit still reading 100%**. Measured, for four bytes:

| | |
| what was written | `x10_timerPeriod = 1.0f / static_cast<float>(x0_timerFreq);` |
| what the object grew | a 4-byte `.sdata2` |
| what objdiff said | `100.00% fuzzy, 100.00% matched code, 1/1 functions` |
| what `unit_fit.sh` said | fits |
| what the linked ELF's section sizes said | all correct - `.text` 0x3a1c54, `.rodata` 0xb530, `.data` 0x14e10 |
| what actually happened | `.sdata2` went 0x54C0 -> 0x54E0, the BSS address moved, `main.dol` grew 32 bytes, `dtk shasum -c` printed `main.dol: FAILED`, and **all 86 RELs failed too** because they depend on that check |

Only the sha1 catches it. Neither `fast_try.sh` nor `unit_fit.sh` does, so a new unit is not
believed until `ninja build/G2ME01/main.dol && sha1sum build/G2ME01/main.dol` prints
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

The fix is byte-identical, because the linker fills the small-data offset in the relocation:

```cpp
extern "C" const float lbl_8041E258;   // .sdata2 0x8041E258, 0x3F800000 = 1.0f
...
x10_timerPeriod = lbl_8041E258 / static_cast< float >(x0_timerFreq);
```

which emits `lfs f0,-16744(r2)` against `R_PPC_EMB_SDA21 lbl_8041E258` - retail's own instruction.

**To find the offender in one step** when the sha1 breaks and every function reads 100%:

```sh
build/binutils/powerpc-eabi-objdump -h build/G2ME01/src/<unit>.o | grep -E 'sdata2|rodata|data'
```

Anything that prints which `splits.txt` does not claim for that unit is the cause. Three such
constants turned up in one lane's five units, and all three are the same shape - a value retail
already has a name and an address for:

| name | section, address | value | why a source literal cannot be used |
| `lbl_8041E258` | `.sdata2` 0x8041E258 | `0x3F800000` = 1.0f | `CStopwatch::CSWData::Initialize` divides by it |
| `lbl_8041E260` | `.sdata2` 0x8041E260 | `0x4330000000000000` = 2^52 | `CSWData::Wait` adds and subtracts it |
| `lbl_803A60A0` | `.rodata` 0x803A60A0 | `"??(??)\0MainFlow"` | `CMainFlow::CMainFlow` points **seven bytes into** it, because retail's linker merged `"MainFlow"` with the tail of a longer literal; the `+7` is a separate `addi` and the source has to say so |

Two things follow that are easy to get wrong. First, **these names must be defined somewhere on the
port**, with the *value* and not the address - a 64-bit host cannot hold 0x8041E258, and undefined
they are zero fills, so `lbl_8041E258` being 0.0f would make `GetElapsedTime()` return 0.0 for the
whole game. `src/MetroidPrime/PortGlobals.cpp` is the place, because it is a unit `configure.py`
never claims; putting them in the `Matching` unit itself would collide with the retail object. Second,
**a `NonMatching` unit is exempt**, because its object is not in the link at all: that is why a
`NonMatching` unit can claim a range and still be safe, and also why the *weak* template
instantiations a `Matching` object emits past its claimed range (`__dt__rstl::list<...>`,
`ReleaseData__...rc_ptr<24IArchitectureMessageParm>`, `__vt__24IArchitectureMessageParm` - 0xF0 bytes
past 0x8C in `CIOWinManagerCtor.o`) are harmless: dtk drops them. They would not be, in a unit whose
claimed range they fell inside.

Worked examples, all `Matching` and all verified byte-exact:
`src/MetroidPrime/CIOWinManagerCtor.cpp`, `src/MetroidPrime/CIOWinCtor.cpp`,
`src/MetroidPrime/CMainFlowCtor.cpp`, `src/MetroidPrime/CInputGeneratorCtor.cpp`,
`src/Kyoto/Basics/CStopwatchCSWData.cpp`. `docs/research/frame_loop.md` has the per-function detail.

### MWCC's inlining and scheduling levers, and their limits

Collected from lanes on 2026-09-25, all measured:

- **`#pragma noinline` is the only working no-inline pragma** in this compiler; MWCC 2.7 (GC) rejects
  `__declspec(noinline)`. Putting it above an out-of-line destructor is what lets retail's
  out-of-line call site stay out of line (`auto_ptr<CDependencyGroup>::~auto_ptr` 69.84% -> 100%).
- **A 4x loop unroll needs a straight-line body, and `rstl::construct`'s placement-new form puts a
  branch in it** - MWCC then guards the store with the address arithmetic's flags (`add.`/`beq`) and
  the unroll is lost. A file-local `construct<T>` specialisation in assignment form restores it:
  `CDependencyGroup::ReadFromStream` went 34.97% -> 98.65% that way, unrolled 4x exactly as retail.
- **MWCC hands out callee-saved registers in declaration order.** In `rstl::algorithm.hpp`'s
  `lower_bound`, moving `It it;` inside the loop after `halfDist` is what puts `halfDist` in `r30` and
  `it` in `r29` like retail - a pure declaration-order difference.
- **A unit at 100% of its functions can still be unpromotable, and the tool that says so is the DOL,
  not the percentage.** `CStringTable` and `CDependencyGroup` are at 98.84% with every fixable
  function fixed; flipping them adds 384 and 800 bytes to the binary respectively because our objects
  emit container/COMDAT code retail does not have there. Measure it by flipping and diffing the DOL
  size, not by looking at the fuzzy number.

### A negative result that saves a family: `rstl::rc_ptr` is 8 bytes, and flipping it costs 18 functions

Measured 2026-09-25, and it is worth recording because the same wrong model was re-derived three
times. Retail's `rstl::rc_ptr<T>` **is 8 bytes**, and its word 1 is never initialised or read:

- `ReleaseData__Q24rstl20rc_ptr<10IVParamObj>Fv`: `lwz r4,0(r3); lwz r3,4(r4); addic. r0,r3,-1;
  stw r0,4(r4)` - word 0 is a `CRefData*`, and the count is **inside** the CRefData at +4.
- `rstl::CRefData` does exist in Echoes and is 8 bytes `{ptr, int}` with a static `sNull`
  (`R_PPC_EMB_SDA21 sNull__Q24rstl8CRefData` in `CStateManager.o`). Upstream's `CRefData` (4 bytes,
  count at +0) and its `rc_ptr {ptr, int*}` are both wrong.
- `CToken(IObj*)` does `li r3,36`, so `CObjectReference` is 0x24 - one word more than its members sum
  to with a 4-byte `rc_ptr`; `CAdditiveAnimPlayback` at 0x28 agrees.

**Do not flip the header on its own.** A lane did, with the gates green and the DOL unchanged, and
measured the whole tree: **0 functions gained, 18 lost at 100%** (13 `CStateManager`, 2 `CActor`,
1 `CPlayerState`, 1 `CPlayerGun`, 1 `CScriptCannonBall`), 30 more regressed, only 7 units' code moved
at all - and `CObjectReference` itself was byte-for-byte unchanged, still 8/10. The reason is that
every class embedding an `rc_ptr` has its later members shifted +4, and several of them carry filler
words written for the 4-byte model (`CStateManager` alone has four; `CAnimData` has six `rc_ptr`s).
So this is a **per-class offset-repair job** - fix the members of every rc_ptr-embedding class, then
move the header - not a shared-header change. As a header flip it is a regression, and the
`CObjectReference` constructors are stuck on something else entirely (the `Null()`/`GetFactory()`
call shape), not on `rc_ptr`'s size.

### Declare in reverse: the rule that keeps a module's hash from breaking invisibly

**mwcceppc emits function definitions in reverse source order, and mwldeppc places an input
object's `.text` in that object's own section order.** So a unit's functions must be *declared
descending by retail offset* or the module's bytes come out permuted.

This is not a theory. `AIMannedTurret`'s unit declared its three functions ascending
(`fn_1_0`, `fn_1_8`, `fn_1_10`); the object came out as `fn_1_10@0, fn_1_8@8, fn_1_0@0x10` and
the module hash broke. Reversing the declarations gives `fn_1_0@0, fn_1_8@8, fn_1_10@0x10`,
which is retail, and the flip passes. The bodies were never wrong and no symbol was wrong.

**Nothing else reports it.** objdiff pairs functions by name, so all three stayed at 100%;
`tools/unit_fit.sh` compares sizes, and the sizes were identical; the link succeeded, because a
permutation does not change the module's size. The failure was 4 bytes of `.text` plus two
relocation offsets. **Only `tools/flip_test.sh` catches it**, which is the whole argument for
that tool being the acceptance test rather than a percentage.

It is also visible after the fact, cheaply: `powerpc-eabi-nm -n` the object and compare the
address order with the source order reversed.

**18 units are permuted right now** (18 before `CGX` was reordered), all of them `NonMatching` -
which is the point, since a `Matching` unit cannot be permuted without the hash already having
broken. The list, with a reason and a note of what else blocks each, is
`docs/research/decl_order.md`, and `python3 tools/check_decl_order.py` measures it and checks the
list, in `tools/gate.sh`. It finds the defect in a `NonMatching` unit, which is the point: the
alternative is spending a lane discovering it at the end. The one worth doing next is
`CStaticAudioPlayer` (22/24); `CPakFile` is permuted too and is the largest unmatched pool in the
tree. **`CGX` was reordered and is no longer on the list, but it is still not promotable** - the
reorder removes the *silent* blocker, not the real one, which turned out to be its data sections
("A DOL unit can be blocked by data, not by code"). **Read that before spending a lane on the next
unit `check_decl_order.py` names**: being off the list is not the same as being ready to flip.

**`CGX`'s permutation was five local moves, not a rewrite** - about 15 lines moved, and
`check_decl_order.py` went from "would break on a flip" to ok. Its definition order was already
descending for 47 of the 52 functions; only `fn_802BCC74`/`fn_802BCC80` (which belong after
`SetAlphaCompare`, not in the middle), the swapped `fn_802BDFC8`/`fn_802BDF20`, `fn_802BE0E8`
(one position out), `CallDisplayList` (before `Begin` rather than after it) and `SetAlphaCompare`
itself were out of place. So **read the retail `nm -n` list and move only the positions that
disagree** - diffing the two orderings and splicing the misplaced blocks is exact and takes
minutes, where reordering the whole file by hand is where mistakes come from. A useful trick: a
lane can compare the two `nm -n` orderings directly and print the mismatched positions.

```sh
build/binutils/powerpc-eabi-nm -n --defined-only build/G2ME01/src/<unit>.o | grep ' [tT] '
```

`Puffer` and `WallCrawler` already write their sources in this order, and so does `CPatterned`
(`TakeDamage` last in the source, first at `0x0`) - which is why those modules hold their hashes.
The idiom was there without being written down.

### An emission-order wall: out-of-line template instantiations

Measured on `CStaticAudioPlayer` (2026-09-25), and it is the reason a unit can be 23/24 with
every function at 100% and still not flip. **"Declare in reverse" only orders the functions you
write.** The out-of-line copies of `rstl::vector<T>::reserve`, `operator=`, `clear`, `~vector`,
`destroy`, `uninitialized_copy`, `rstl::reserved_vector::erase` and the implicit `__dt__`
instantiations are emitted by mwcceppc in a **trailing pool**, after every source-defined
function, in an order that is *not* the order they are used:

```
retail ascending : ... StartMixOut  as  clear  destroy  dt_vector  IsReady  __dt__  __ct__
                    reserve  uninit_copy  Cancel  erase  Run  AICb  Install ...
ours             : ... StartMixOut  IsReady  __dt__  __ct__  Cancel  Run  AICb  Install
                    as  reserve  dt_vector  destroy  erase  clear  uninit_copy ...
```

Retail's order is the source functions descending *with each function followed by the
instantiations it needs*; ours is the source functions descending and then one pool. No
`#pragma inline_max_size` value, no `inline` marker, no reordering of the declarations in
`rstl/vector.hpp` and no reordering of the source statements moves it. `Kyoto/Streams/CFilePreload`
is `Matching` and *does* have a trailing pool - so retail's own sources do it both ways, and the
difference is per-translation-unit, not per-header. **Treat it as a wall and stop**: it costs
more builds than the last two functions of a unit are worth.

**Re-measured 2026-09-28 (goal item `match-cstaticaudioplayer`): the wall stands, and there is a
second, independent blocker behind it.** The unit is unchanged at 99.87369% / 23 of 24 functions,
`unit_fit.sh` still says 868 bytes over with the same 8 extras, and `flip_test.sh` FAILs. Two
things are now pinned rather than inferred.

*The permutation is exactly the pool, and it is 10 of 24, not "the two functions".* Numbering
retail's 24 functions 1..24 by ascending offset and listing our emission in that numbering gives

    1 2 3 4 5 6 7 8 9 | 12 11 13 14 15 18 20 21 22 | 10 16 17 19 | 23 24

- the first nine are the source-defined functions, all in place (`MixToMono` is already after
  `Decode`, which is what the 2026-09-25 reorder bought);
- position 10 is a pure adjacent transposition, `__dt__vector` (retail 12) emitted before
  `destroy` (retail 11);
- positions 19-22 are the trailing pool: `clear` (10), `reserve` (16), `uninitialized_copy` (17),
  `erase` (19) - the four pool members whose retail offsets fall *inside* the source-function run.
  Their order **relative to each other already matches retail's**; only their position does not, and
  a single trailing pool has one position.

*`DecodeMonoAndMix` is a second blocker, and it is two register tie-breaks, not 18 instructions of
logic.* The function is 92 instructions; 18 differ and every one is a register choice, with the
same opcodes in the same order:

- retail `outCursor` = r26 and the outer loop counter `remBytes` = r29; ours has r29 and r26
  (so every `sth`/`addi`/`subf`/`cmpw` on those two differs);
- retail computes the second sample into r3 (`mullw r3,r22,r3` / `add r3,r0,r3`) and the first
  clamp's result into r0; ours computes it into r0 and the clamp into r3. The inner clamp uses r0
  in both.

Note what the swapped pair actually is: `this` is r3, so the parameters are r3=this, r4=out,
r5=in, r6=numSamples, r7=startSample, r8=sampleEnd, r9=sampleStart, r10=vol and `state` on the
stack at 72(r1). The two variables fighting for r26/r29 are **`outCursor` and `remBytes`**, not
`outCursor` and `curSample` - `curSample` is r28 in both. All six permutations of the three
function-scope locals (`outCursor`/`curSample`/`inCursor`) were measured and the one in the file
is the best of them: 98.695656% (ABC), 98.532610% (CBA), 98.315216% (BAC), 98.206520% (BCA),
98.097824% (ACB), 98.043480% (CAB). Hoisting `remBytes` out of the `for`-init changes nothing
(98.695656%), and reversing the two operands of `samp2`'s `+` changes nothing either. So
**do not spend a lane re-ordering these three declarations** - it is already the optimum, and a
lane that tries will burn builds to arrive back here.

**How to tell it apart from a real size problem, in one build.** `unit_fit.sh` reports this
unit "868 bytes over" with 8 extra emitted functions, which reads as fatal. It is not. The DOL
link flags are `-lcf build/G2ME01/ldscript.lcf -m _prolog -strip_partial`, and `-strip_partial`
makes mwldeppc *delete* the duplicate weak copies out of the middle of the section and pack the
rest, so the bytes come back out of whichever object held retail's copy. Proof, from one flipped
build:

- the flipped `main.dol` and the retail-reproducing one are **the same size, 3 969 024 bytes**;
- the 8 extra symbols are **absent from `build/G2ME01/main.elf`** entirely;
- the symbol addresses in the flipped ELF are exactly *our object minus the 8 stripped
  functions* - `IsReady` at our `+0x4c4`, `CancelDMACallback` at our `+0x9d4 - 0x170`, `__sinit`
  at our `+0x11c4 - 0x364` = `0x80327474`, retail's address exactly;
- the diffs are not confined to the unit: `CFilePreload`, `CCubeMoviePlayer` and
  `auto_03_8018A188_text` also change, because their copies of those functions are the ones
  that got stripped.

So **`unit_fit.sh`'s extra-function list is not a verdict** - `flip_test.sh` is, and for a DOL
unit the cheap intermediate measurement is: flip it by hand, then compare
`powerpc-eabi-nm -n build/G2ME01/main.elf` against the report's `virtual_address`es. If the
sizes match and only the *order* inside the unit is wrong, it is this wall.

Two smaller things that flip turns up and are not faults: `.rodata` "SHORT by 1" is alignment
padding (our section is 7 bytes with `2**3` alignment against a claimed 8, so the linker pads
it identically), and `FORCEACTIVE symbol '__sinit_<unit>_cpp' is either not a global symbol` is
because our `__sinit` is local (`t`) where retail's is global - the `.ctors` entry still comes
out at the right size.

### An `inline` in a shared header costs whole functions, silently

Found on `CPakFile` (2026-09-25), and the symptom points nowhere near the cause.

**`rstl::vector::resize` is not `inline` in retail.** Our header had it `inline`, so MWCC inlined
it into every caller; retail emits it out of line and calls it. The visible damage was
`CPakFile::Warmup` stuck at 44% with a 796-byte `InitialHeaderLoad` at 84% - both of which are
*callers*, and neither of which mentions `resize` - plus two whole functions at 0.00% that were
`resize` instantiations our object never emitted, so objdiff had nothing to pair. Dropping the
one keyword: `Warmup` 44.53% -> **100%**, `InitialHeaderLoad` 83.95% -> 99.72%, one function in
`MetroidPrime/main` to 100%, and three unpaired functions became pairable.

**`rstl::vector(int count)` was a real bug, not a codegen difference.** It called `reserve(count)`
and left `x4_count` at 0, so `vector<T> v(n)` produced n elements' worth of uninitialised storage
and a size of 0. Retail's constructor stores the count after the `bl reserve` (`stw r30,12(r1)`).
`CPakFile::EnsureWorldPakReady` depends on it and went 65.65% -> 76.21%.

**`is_trivially_destructible` is specialised in retail, and not uniformly.** With it specialised
for `unsigned int` and `unsigned char`, `clear<vector<unsigned int>>` becomes the 12 bytes retail
has (`li`/`stw`/`blr`) instead of our 68 with a live element loop, and five extra emitted
functions disappear. It is scoped to those two types *on purpose*: adding `unsigned short` sends
`CStateManager::__dt__` from 18.39% to 13.07%, so the trait is not uniform in retail's codegen.
**Widen it one type at a time and re-gate** - the other arithmetic types are individually safe on
`CStateManager` but untested tree-wide.

**What was tried and rejected:** changing `rstl::construct<T>` from `new (dest) T(src)` to
`*static_cast<T*>(dest) = src` removes the null guard MWCC puts on placement new, and it was worth
**+5.44 points on the unit (88.30% -> 93.74%) and 25/33 functions**. It also broke the build:
`main.dol` -> `954faa0d…` and the report fell to 3001 functions. The guard is required by the units
that currently reproduce retail, and it is also the whole remaining gap in
`resize<vector<unsigned char>>`, in `RebuildResourceLists`' zero-fill loop and in
`EnsureWorldPakReady`'s `depList` fill. If anyone revisits it, it has to be **per call site**, not
in the shared header.

### MWCC 2.7's bit-field granularity, and the `rlwimi` trap

Both measured on `CPatterned`'s constructor (2026-09-25), and both will mislead anyone reasoning
from the encoding alone.

**`rlwimi`'s shift in a one-bit field is `31-p`, and the value comes from source bit 0.** Read
`rlwimi r0,r6,7,24,24` with the textbook mask semantics - destination bit 24 takes source bit 17 -
and every one of the 39 one-bit writes in this constructor stores **0**, which would make a
handful of correct header comments wrong. They are not wrong. The project's own MWCC emits
`li r0,1; rlwimi r4,r0,7,24,24` for a `true` bit-field, and `clrlwi r4,r4,24; rlwimi r0,r4,7,24,24`
for a bool variable. So for a 1-bit field at position `p` the shift is `31-p` and the bit taken is
source bit 0. **Do not "correct" a header's bit-field values from the encoding.**

**Word granularity is unreachable for a run of one-bit fields, at least in MWCC 2.7.** Retail writes
the word at `0x420` twenty-six times as `lwz`/`rlwimi`/`stw` - word granularity, shift always
`31-bit`, destination bits 0..25 in order. A standalone test compiled with the project's own
`GC/2.7/mwcceppc.exe` and the flags from `build.ninja`: MWCC 2.7 emits `lbz`/`rlwimi`/`stb` - **byte**
granularity - for a run of one-bit fields in a 4-byte-aligned struct member, and does so identically
for `bool`, `uint` and `short` declarations. Assigning all 26 fields `false` in turn reproduces the
shape exactly (78 instructions) but always byte-wise. **No declaration tried reproduces those 312
bytes.** By contrast the 11-field group at `0x34c`/`0x34d` *is* byte-wise in retail and *is*
reproduced by `bool x34c_24_ : 1;` members assigned one at a time - so the two groups in the same
constructor need different shapes, and one of them is currently unreachable.

The next thing to try, if someone picks it up: a named struct with a whole-word `uint` plus
bitfields, or a union, rather than a bare run of `bool : 1`.

### MWCC rotates a loop only when it cannot count it

Measured on `CPlayerState::InitializeScanTimes` (2026-09-25), and it is a rule rather than an
accident: **MWCC rotates a pre-test loop into `preheader; b latch; body; latch; br body` if and only
if it cannot compute the trip count.** A pre-test loop with a computable constant trip count is fully
unrolled instead, and a post-test loop is never rotated.

The in-tree evidence is three functions in one translation unit that are already matched at 100%:
`GetBitCount(uint)` (`for (; val != 0; val >>= 1) bits += 1;`), and every pointer loop -
`reserve<vector>`, `clear<vector>`, `uninitialized_copy<...>`, `__as__<...>` - all emit exactly
`preheader; b cmp; body; cmp; br body`, and all of them have a trip count the compiler cannot
compute. Against that, every pre-test loop written with a computable count is fully unrolled:
`for (i=0;i<4;++i)`, `while (i<4)`, `i != 4`, `i <= 3`, a `static const uint` bound, a non-const
local bound, `continue`, a dead `break` or `if`, a nested scope, a `switch`, a comma. About fifty
variants, none both.

**The practical consequence: you cannot get the `b` by writing a counted `for`, however you spell
it.** The near-miss that proves the mechanism is worth keeping in mind - `uint i =
static_cast<uint>(-1); while (++i < 4) { ... }` produces the `b` and every other instruction, and
differs from retail by exactly one: `li r6,-1` where retail has `li r6,0`. Reaching retail's shape
would need MWCC to hoist the first `++i` into the preheader, which it does not do. A unit blocked on
one missing `b` is blocked on the compiler, not on the source.

### mwcceppc allocates r30, r29, r28 to the first, second and third local (2026-09-26)

**The register a local gets is decided by its position in the declaration list, counting down from
r30, not up from r28.** This is the mechanism behind "A named temporary can move a register without
changing semantics" above, stated as a rule, and it is worth knowing before spending a lane on
body variants that are all one register swap away.

Measured on `CResLoader::AsyncIdlePakLoading` (0x802FCCF4, 0x9C bytes), whose five live values are
`this`, a bool latch, a node cursor, a `CPakFile*` and the pak's ARAM-file bit, and which retail
holds in **r27, r28, r29, r30, r31** in that order:

| declaration order | emitted |
| `latch, node, pak` (the order the code reads in) | `pak`=r28, `node`=r29, `latch`=r30 |
| `node, pak, latch` | `latch`=r28, `pak`=r29, `node`=r30 |
| `pak, node, latch` | `latch`=r28, `node`=r29, **`pak`=r30** - retail's |

So to reproduce retail's registers the declaration order is the **reverse** of the reading order,
and in the winning shape `pak` is declared **uninitialised, before the cursor it is derived from**,
and assigned inside the loop body. Everything else - the flag read into a local rather than
re-read, the end test written inside the loop condition rather than hoisted into an `end` local, the
cursor as a raw `rstl::list<T>::node*` rather than an `iterator` - follows from keeping the live
set at five, which is what makes retail's 32-byte frame and `stmw r27,12(r1)` come out at all.
Two shapes that look equivalent and are not: hoisting `end` into a local costs a sixth live value
and the frame becomes 48 bytes with seven saved registers, and taking `SPakLoadEntry& entry = *it`
before the body needs the item's address in a register throughout, which costs the same.

**How to find it without a variant search:** read retail's register numbers off the disassembly
(`this` is r27, and the rest are r28.. in order), then permute *only the declaration order* of the
locals - the bodies do not have to change at all. Two instructions per rebuild.

### Declaring a unit's functions ascending is right when the lower one is emitted first

"Declare in reverse" (above) says a unit's functions must be declared **descending** by retail
offset, because mwcceppc emits in reverse source order. `src/Kyoto/CResLoaderPakPump.cpp` is the
case that makes the rule mechanical rather than memorable: it holds two functions and they are
declared **ascending** - `AsyncIdlePakLoading` (0x802FCCF4) first, `AreAllPaksLoaded` (0x802FCCE4)
second - because the *lower* offset has to be emitted *first*, and emitting in reverse source order
means the *higher* offset has to be declared first. Declaring them the readable way round
(`AreAllPaksLoaded`, the predicate, first) gives an object that is still **exactly 0xAC bytes**,
`unit_fit.sh` reports "fits", objdiff still pairs by name and reads **100.00%** on both functions -
and the object then lands **0x200 bytes early** in the DOL, so `dtk shasum` fails and all 86 RELs
go with it. The only instrument that sees it is `flip_test.sh`, and the symptom reads like a
wildly wrong body rather than a transposition.

Two smaller things that came with it, both in the same unit and both worth knowing:

- **`CResLoader` is 0x60 bytes, not 0x58, and the +0x5C the tree assumed is a `rstl::list`'s
  `x14_count`.** The evidence is three counts read at +0x2C, +0x44 and +0x5C plus the erase at
  0x802fd1f4 decrementing the *same* word `AreAllPaksLoaded` reads; written out in
  `docs/research/paks.md`. Every offset downstream of it moved by 8, including `CFactoryMgr`
  (0x5C -> 0x64) and `CResFactory`'s size (0xC8 -> 0xD0), so this is a header change with a
  `CHECK_SIZEOF` blast radius, not a one-line fix.
- **Renaming an unnamed DOL symbol needs the *mangled* name, and `mwcceppc` will not accept
  `friend extern "C"`.** `fn_802FCCE4`/`fn_802FCCF4` are referenced by `main.o` and two
  `auto_*` objects, so claiming their bytes without renaming leaves the link undefined. The
  mangled names are `AreAllPaksLoaded__10CResLoaderCFv` / `AsyncIdlePakLoading__10CResLoaderFv`
  (read them off our own object with `nm`, not off a C++ compiler's mangling). And a port-side
  `extern "C"` copy of a retail helper that has to reach a private member cannot be a friend
  declared as `friend extern "C" void* f(void*, void*);` - mwcceppc reads the `extern` as a storage
  class and stops. Declare it `extern "C"` at namespace scope above the class and then write a
  plain `friend` declaration, which binds the same entity.

### An unnamed function is often a template instantiation you can identify by diffing it

This is the technique that landed `fn_802FC350`/`fn_802FC378` (2026-09-26, lane `k4`), and it is
worth trying on **any** unnamed function before writing a body, because when it works the body is
already written somewhere in the tree at 100%.

`rstl::list< rstl::auto_ptr< CFilePreloadData > >::do_insert_before` is a **`Matching` unit**
(`src/Kyoto/Streams/CFilePreload.cpp`, 100.00%, `scope:weak` in `symbols.txt` at 0x803445DC,
0xA8). `fn_802FC378` is unnamed, 0xA8, and in the same loader. Disassemble both and diff:

```sh
tools/dis.sh 0x803445DC 0xA8 > /tmp/a; tools/dis.sh 0x802FC378 0xA8 > /tmp/b
sed -E 's/^[0-9a-f]+ <[^>]*>:/\n/' /tmp/a   # strip addresses, keep mnemonics and operands
```

They are **identical instruction for instruction and register for register**, apart from the two
`bl` displacements. That is not a coincidence to be explained - it is the identification. The
function you are looking at *is* that instantiation with a different template argument, so:

* the **element type** is whatever the two instantiations have in common, and the `addic. r5,r3,8`
  / `beq` / three stores inside the copy are **that element's copy constructor**, not statements
  in the function. Here they are `rstl::auto_ptr`'s auto-relinquishing constructor, and the
  erasure side (`fn_802FD174`: `lbz` the byte, then `bl __dt__CPakFileFv` on `*(item+4)`) is
  that class's destructor, which is what confirms it.
* you can then write the function as **the container's own member** - `do_insert_before` called
  through the public `node*` - rather than a transcription, and it comes out byte-identical on
  the first build. The only thing left is the `extern "C"` wrapper for retail's dtk name.

The diagnostic generalises: **grep `symbols.txt` for a `size:` that equals your function's**, and
prefer a `scope:weak` template member over a named function. `do_insert_before` appears three
times in `symbols.txt` (0x8026D088 0x28, 0x803277C4 0x90, 0x803445DC 0xA8) and the third was the
one to compare against.

The corollary is a trap: **if the match is a template member, mwcceppc emits it out of line and
calls it** unless `#pragma inline_max_size` is large enough. Left at the default, `fn_802FC378`
came out as a 0x20-byte forwarder to a separate COMDAT - 0x58 of the 0xA8 missing and a symbol
retail does not have, with `unit_fit.sh` reporting a third function and "over by 32".
`#pragma inline_max_size(0)` is the opposite mistake: it stops *every* inline, so
`rstl::construct` stops being a placement `new` and becomes `__nw__FUlPv` plus a null test
plus a call.

**And the threshold is not a constant of the compiler - it moves when a header does.** 125 was
measured working for that unit, and stopped working when `Kyoto/CResLoader.hpp` started
including `Kyoto/CPakFile.hpp`; 190 is the new floor and the unit uses 200. Nothing about the
source or the body changed. So when a unit that has flipped before suddenly reports an extra
COMDAT template member, raise the pragma first - that is a cheaper hypothesis than "the body
regressed" and the symptom looks nothing like it.

### Four codegen rules that are not about register allocation, and one that is

Measured 2026-09-26 on four `CResLoader` units that went to 100% (see the Attempted modules table
and `docs/research/paks.md`). The first four are general and cheap; the fifth is the wall.

1. **Declare the return type MWCC can see is dead.** A function whose result every caller ignores
   must be declared `void`, not `void*`: `fn_802FC420` as `void*` emits a trailing `li r3,0` and is
   8 bytes longer than retail's 0xB8. Same polarity rule as `fn_802FCAE8`'s
   `IsCompressed() ? 1 : 0` and as `if (found) { return ...; } return 0;` - MWCC normalises what it
   can prove dead, so the *declaration* has to be the dead one.
2. **A named local decides the evaluation order of an expression's callees.** mwcceppc does not
   evaluate arguments left to right. `AsyncSeekRead(buf, (res->GetSize() + 31) & ~31, kSO_Set,
   res->GetOffset())` gave 87.03% with the accessors in the wrong order; hoisting one into a named
   local gave retail's exact order and 100.00%. **This is the cheapest experiment on any unit that
   is near 100% and whose diff is "the same calls, the wrong order".**
3. **A temporary passed straight into a call is not the same as a named local, and retail's
   instruction sequence tells you which it was.** `CMemory::Alloc(n, h, s, t, CCallStack(...))` in a
   temporary emits `addi r3,r1,8` / `bl <ctor>` / `mr r7,r3`; the same object in a named local emits
   `addi r7,r1,8`. Retail has the first, and the reason is that its 12-byte constructor
   (`stw r5,0(r3)` / `stw r6,4(r3)` / `blr`) leaves r3 alone, so the reference *is* r3. 91.70% vs
   100.00%. When retail's `mr rX,r3` immediately follows a call, the argument was the callee's own
   register, not a recomputed address.
4. **A private parameter with a default can still be the answer, and the default is usually a named
   retail constant.** `CCallStack`'s third parameter is `static const char kUnknownType[]`, private -
   unreachable from out here - and retail's relocation is `kUnknownType__10CCallStack` at
   0x803AEAB8. The two-argument call reaches it *through the default*. Before reaching for a
   `friend` or a `#define private public`, check whether the default argument is already the
   symbol. (This one also had the wrong address in the previous lane's table, which is the general
   point: **read the address out of `symbols.txt`, not out of the previous lane's prose.**)
5. **The wall: mwcceppc's choice among two free callee-saved registers is not reachable from the
   source.** `fn_802FC4D8` is 99.10% and `fn_802FC63C` 98.04% with every instruction right; the whole
   residual is that the compressed arm gets r6/r7 and r29/r30 where retail uses r7/r6 and r30/r29 (20
   instructions), and r27/r28/r29 against r28/r29/r30 (16). Roughly forty body shapes - cursor
   spelling, cursor type, size type, naming the temporaries, `Get(4)` vs the member, `get()` vs
   `operator->`, a `CMemoryInStream*` cast, an `if` vs `?:`, hoisted and sunk declarations, a named
   `owns` bool, a named `resSize` - moved it by nothing. **Report that as a register-allocation
   blocker and leave the unit `NonMatching` with its range claimed** (which is what
   `CResLoaderGetPakFile.cpp` and `MetroidPrime/Player/CPlayerState.cpp` do), rather than deleting a
   real initialisation to raise the average. The generalised form of rule 2's companion note in
   "A named temporary can move a register" applies here too, with the opposite outcome: if the whole
   diff is a register *number*, try naming things, but **stop after one systematic sweep and say so** -
   forty variants is a measurement, not a search.

### A named temporary can move a register without changing semantics

`InitializeScanTimes`, 97.63% -> 98.25% with one line and no logic change: `push_back_unsafe(
SScanState(it->first))` became

```cpp
const CAssetId id = it->first;
unkStruct.vec.push_back_unsafe(SPersistentState::SScanState(id));
```

MWCC had allocated the source iterator to r7 and the loaded id to r6; retail has them the other way
round. Five instructions moved. **Worth trying on any unit sitting near 99% with a "wrong register"
complaint** - it is cheaper than the body-variant search, because it is a single naming decision
rather than a control-flow experiment.

### Reading an address in the DOL, and a module's `.text` out of its `.rel`

Both cost a lane real time, and both end in a wrong answer rather than an error, so they are
worth writing down. `docs/research/port_globals.md` is the worked example.

**Which base register a small-data address uses.** `readelf -s` on `build/G2ME01/main.elf`
says `_SDA_BASE_ = 0x8041FD80` and `_SDA2_BASE_ = 0x804223C0`, and the naming implies r2 gets
the first. It is the other way round for the code that matters: **r13 is 0x8041FD80** and is
what `.sbss` is addressed through, **r2 is 0x804223C0** and is what `.sdata2`/`.rodata` are
addressed through. Compute the offset for both and grep for both before concluding a symbol is
unreferenced - `gpTweakGame` (`.sbss:0x80418F30`) is 10 hits at `-28240(r13)` and zero at any
`r2` offset.

**Section file offsets are not the VMAs,** in the DOL *or* the ELF, and the two disagree with
each other: `.rodata` is VMA 0x803A56C0 at file offset 0x3A27A0, and `.sdata2` is
0x8041A3C0 at 0x3C3C20 in the ELF but 0x3C3B40 in the DOL. Read the offset out of
`objdump -h` and validate it on a known string before believing anything you read back -
`0x803AEAF2` really is `"%s\n"`, which is what makes the mapping trustworthy. Getting this
wrong produces a *plausible* wrong value rather than a failure: the eight bytes at
`.sdata2:0x8041D550` read as `0xC6C33A80` under the wrong mapping and as `0x803AC3C6` - a
perfectly good pointer into `.rodata` - under the right one, and only the second is real.

**A REL module's `.text` is at file offset 0xA4 of its `.rel`**, not at the end of the 0x4C
header. Find it from the module's own map: `RELExit` / `RELMain` / `TweaksInit` have prologues
at 0xA4, 0xC8 and 0xE8, which pins module offset 0 to 0xA4. Then:

```sh
python3 -c "d=open('orig/G2ME01/files/RelProd/Tweaks.rel','rb').read(); \
  open('/tmp/t.bin','wb').write(d[0xa4:])"
./build/binutils/powerpc-eabi-objdump -D -b binary -EB -m powerpc:common \
  --start-address=0x508 --stop-address=0xab4 /tmp/t.bin
```

**`-EB` is not optional.** Without it objdump decodes the words byte-swapped, and the output
looks like plausible PowerPC with occasional garbage rather than an error. It is the reason
`REL_CreateTweakGlobals` first came out as nonsense.

The lesson is the one this file keeps making: **a check that cannot fail is not a check.** Both
tools still work where they are pointed at the right thing; the trap is that they report
success where they measure nothing.

### The port's link gap is 652 symbols, and most of it is bulk work, not decompilation

**Superseded, 2026-09-25, after the loader thunks landed:** the figure in the heading was 724, and
the measurement below still stands except for the counts. `docs/research/port_link_gap.md` has
the current table; 72 of the 234 "REL module loaders" closed as 64 `Matching` DOL units, and all
159 of the entity loaders are now identified (`docs/research/rel_loaders.md`) - they were never
the unknowns this section implied.

Measured 2026-09-25 with `tools/link_gap.py`; the work list is `docs/research/port_link_gap.md`
and the checker is in `tools/gate.sh`. This is the decompilation's half of the port's blocking
path, and it was unquantified until now - the port builds its game sources as an OBJECT library,
so no link step exists to fail and nothing ever reported what was missing.

**Of 1376 undefined symbols in `mp_game`, 44 are genuinely unaccounted for** (63 when first
measured; the 19 that closed are the retail globals below). The rest are the C++ runtime (722),
libc (23), and 107 that appear somewhere in Aurora's own trees. Those four kinds of missing symbol
are, in order of interest:

1. **29 functions nobody has written.** The port's own sources declare them `extern "C"` and
   call them. Where retail names the function, the port is calling it under its `fn_` name and
   the rename is the first step. **Two are on the port's blocking path by name:**
   `CreateFrameEnd__7MakeMsgF14EArchMsgTargetRCi` (0x800489AC, 204 bytes) is called by
   `CGameArchitectureSupport::Update`, and `SolveQuadratic__5CMathFfffRfRf` (0x802CC064, 188
   bytes) by `CMayaSpline`. Sizes run from 8 bytes to **604** (`fn_80038624`, in
   `CStateManager`).
2. **19 retail globals declared `extern` and never defined - CLOSED.** This was the class worth
   understanding, because it is *correct* in the decompilation and *impossible* in a PC link:

   ```cpp
   extern "C" int lbl_80419A10;   // CStateManager.cpp:33 - a declaration, not a definition
   lbl_80419A10 = x16a8;          // and assigned at :501
   ```

   For the decompilation that is right - retail's own objects define those symbols and the DOL
   links against them. **A standalone PC link is what finally forces this repository's data to
   be complete**, and this list is where it was not. Note also that dtk renames retail's
   `kInvalidUniqueId`-style constants and the `.rodata` float pools to `lbl_*`, so the value a
   definition needs has to come out of the DOL's data, not out of a header.

   All 19 are defined in `src/MetroidPrime/main.cpp` under one `extern "C"` block. Four things
   the reading needs, each of which cost a build:

   - **The width comes from the retail instruction, not dtk's `size:`.** dtk's `size:` is the gap
     to the next symbol, so `lbl_80419A10` claims 8 bytes and `lbl_8041A8BC` is the only one whose
     gap equals its type. `objdump -d -r build/G2ME01/obj/<unit>.o` is the arbiter: `lhz`/`lwz`/
     `lfs`/`stb`/`stw` pin `lbl_8041E2E6` to two bytes and `lbl_80419A98` to one.
   - **`.bss`/`.sbss` symbols have no contents in the ELF**, so their value at load is 0. That is
     13 of the 19, and it is a fact rather than a guess.
   - **Two GCC traps.** An uninitialised tentative definition that nothing in the translation unit
     reads is *dropped*, so every one of these needs an explicit `= 0` or the link is no better
     off; and inside an `extern "C" { }` block GCC gives a `const` declaration **internal**
     linkage without an explicit `extern`, so six of the 19 vanish unless it is written.
   - **`lbl_8041D394`/`lbl_8041D398` are guest addresses** (0x803AADF2/0x803AADFC, the `.rodata`
     strings `"ShotSmoke"` and `"Power2nd_1"`) and a 64-bit link cannot hold them, so the
     definition is the string.
3. **8 game globals and constants** - `gpRender`, four `gpTweak*` pointers, and the three
   `kInvalid*Id` values.
4. **6 REL module symbols** - `REL_loader_CannonBall` and five `lbl_57_rodata_*` labels. Port
   code (`platform/rel.cpp`), not decompilation.

**What the number does not prove.** The 107 attributed to Aurora are attributed because the
identifier appears in a file under `extern/aurora`; a name in a source is not a definition in an
object. The one symbol where the distinction is already known to bite is `AIStartDMA`, which
appears in an Aurora *header* and in none of its sources. **The authoritative answer is an actual
link**, and until one succeeds the Aurora half of the gap is unverified.

### A `static` on a namespace-scope declaration is internal linkage, and it hides a missing body

Found 2026-09-25 closing the three symbols `CGameArchitectureSupport::Update` needed, and it
is a trap because the build *does* tell you, in a warning most people read as noise:

```cpp
namespace MakeMsg {
  static CArchitectureMessage CreateFrameBegin(EArchMsgTarget, int);   // never satisfiable
};
```

`static` at namespace scope is internal linkage, so no other translation unit can ever define
or call it, and the port's build said so for every call site: `warning: 'CArchitectureMessage
MakeMsg::CreateFrameBegin(EArchMsgTarget, int)' used but never defined`. `main.cpp` calls both
factories from `UpdateTicks`, so the warning named the two declarations and nothing else. Drop
the `static` and define them, and the warning goes with it. The same shape hides a
class-scope `static` member function that is never defined out of line.

### Retail's `CArchitectureMessage` has one parm, not two - the fourth word is the refcount

Worth writing down because the disassembly invites the wrong reading and then the header gets
"fixed" to match. `MakeMsg::CreateFrameEnd` (0x800489AC), `CreateFrameBegin` (0x80048A80) and
`CreateTimerTick` (0x80048DC8) are 0xCC bytes each and end with four stores into the message
they return - at +0x00 target, +0x04 type, +0x08 and +0x0c - which looks like two parameters.
It is not: +0x08 and +0x0c are the two words of retail's `rstl::rc_ptr` (`{ T* x0_ptr;
u32* x4_refCount; }`). The evidence is the same three lines in all three factories - a
`new(4)` whose first word is set to 1 and stored at +0x0c, then `*(refCount) += 1` and a
`ReleaseData` on the stack copy - and it is the same pattern `CGameState`'s constructor uses at
0x80144140 for its own `rc_ptr`. Retail's message is 16 bytes with one parm, and
`CArchitectureMessage`'s three-argument constructor was already right.

Two more from the same job, both about writing an accessor retail keeps out of line:

- **An accessor retail calls is not inlinable, and inlining it costs the caller real
  percent.** `CGameArchitectureSupport::Update` is 100% against retail 0x80007A14 and stays
  that way only because `CGameState::GetWorldState` is out of line: retail's definition is in
  `CGameState.cpp`, a different translation unit. Inline it and the call folds into
  `lwz r4,gpGameState; lwz r3,60(r4)`, dropping the function to **95.89%**; add a null test on
  the result and it is **84.78%**. The port has no `CGameState.cpp`, so the definition sits at
  the bottom of `src/MetroidPrime/main.cpp` under `#pragma inline_max_size(0)` - the pattern
  `src/Kyoto/Input/CRumbleVoice.cpp` already uses.
- **Returning a reference to the pointer, not the pointer, is what reproduces the pair.** The
  accessor's whole body is `addi r3,r3,60; blr` and its caller then does one `lwz r3,0(r3)`, so
  `CWorldState*& GetWorldState()` is the signature that generates both halves. Returning
  `CWorldState*` puts the load inside the accessor and the caller's stream is one instruction
  short; returning `rstl::rc_ptr<CWorldState>&` needs two dependent loads on this port, whose
  `rc_ptr` is one word wide (see the negative result above).

## Two `fn_` functions that are `rstl` template members retail left out of line

Measured 2026-09-25 on `main/MetroidPrime/CStateManager`. Both matched on the first or second
attempt, and both lessons are reusable.

**1. A `fn_` name with four arguments and no `this` read is a non-static member.** `fn_8003C0C4`
(0x8003C0C4, 236 bytes) takes `r3` (the list, never read), `r4` (prev), `r5` (next), `r6` (the
value). `rstl::list<T>::create_node(node* prev, node* next, const T& val)` has exactly that
register signature, and `li r3,140` is `sizeof(node) = 8 + sizeof(T)`, so `sizeof(T) = 132` -
which is `sizeof(rstl::reserved_vector<CEntity*, 32>)`, the element type of
`CStateManager::m_graveyard`. Its caller `fn_8003C054` is then
`do_insert_before(node* n, const T&)`, and **`<rstl/list.hpp>` already contains that body
verbatim**, member for member: `if (n == x4_start) x4_start = nn; nn->prev->next = nn;
nn->next->prev = nn; ++x14_count;`. dtk gives no name to an out-of-line template member
here, so objdiff scored both 0% and there was nothing to rename *to* - an `extern "C"` copy over
a local POD mirror of the layout is what pairs them.

Three things make that work and are worth repeating:

- **Model the list object and its node locally, with `CHECK_SIZEOF`, and keep the real type
  only in the signatures that already match.** `fn_8003C02C` was at 100% and had to stay
  there; it takes `rstl::list<...>&`, so the list view is a `reinterpret_cast` and nothing
  else changes.
- **`rstl::construct` is what produces MWCC's unrolled block move.** A hand-written
  `uninitialized_copy_n` gives a naive one-word loop, and `memcpy` gives a `bl memcpy`. The
  8-word-unrolled `srwi r0,rX,3` body with the 4-byte tail is what the *copy constructor* of
  the element type compiles to, and only that.
- **`rmemory_allocator::allocate(size)` vs `allocate(out, count)` is 9 instructions of 59.**
  The templated out-parameter form gives the pointer two definitions; MWCC then spills `r3`
  across the copy loop and reloads it at the end, and the copy length lands in `r3` instead of
  the register that held `&item`. Writing
  `T* n = reinterpret_cast<T*>(rmemory_allocator::allocate(sizeof(T)));`
  is byte-exact. **When a function is one spill away, try the two spellings of the allocation
  before touching anything else** - this was 6 of the 9 differing instructions.

**2. Take a function's size from `report.json` or `nm`, never from subtracting addresses.**
See the correction in "What the port still needs in order to link" above. `0x8003ABF0 -
0x80038624` is 9,676 bytes and was quoted as one 604-byte function; it is 39 functions. The
subtraction is only a function's size when the next symbol is the next *function*, which in a
unit with 239 of them mostly is not. This cost a lane a whole turn of hunting a mystery that
was 604 bytes of dead code.

### Where a module can even be written

`include/MetroidPrime/Enemies/` now has `CAi` and `CPatterned` as `Matching` units (`CPatterned` is
`NonMatching` again since the 2026-09-28 upstream merge widened it to 103 functions), so a module
whose objects derive from them *can* be written - that was the blocker, and it is gone. What limits
those modules now is the behaviour inside the classes: most of the creature virtuals are unnamed,
`CPatterned`'s 0xB58-byte constructor is unwritten, and 75 modules' worth of actor code has to be
decompiled one function at a time like anything else.

Of the 86 modules, **11 are `Script*` units** (script objects that lean on `CEntity`/`CActor`,
which do exist) and the other 75 are creatures, bosses and swarms that need the missing Enemy
hierarchy. Acknowledge this before assigning module work: check that the base classes a module
needs actually exist.

## CAi: landed, and the "cyclic link-order dependency" was never real

`MetroidPrime/Enemies/CAi.cpp` is `Matching` and complete (11 of 11 functions, 100%) as of
2026-09-25. The DOL sha1 and all 86 RELs still reproduce retail with its own object in the link.

**The cycle did not exist.** The earlier session's conclusion - that adding CAi's range fails on a
cyclic dependency and needs a project-wide change to how the DOL's link order is resolved - is
wrong, and the `configure.py` `link_order_callback` question it raised was never the issue. Claiming
the ranges makes `dtk dol split` accept the graph immediately; the build goes straight to the link.
What looked like a cycle is **CodeWarrior COMDAT weak symbols** - inline virtuals and template
destructors emitted in many translation units. The retail linker kept one copy each and discarded
the rest; `mwldeppc` discards them too, so they are harmless. This is also why
`tools/unit_fit.sh` reports `CAi` as carrying 224 bytes of "extra" functions while the flip holds:
those bytes are weak copies that never reach the binary.

What the unit actually needed was ordinary work, in this order:

1. **Claim all four sections, not just `.text` and `.data`:**
   ```
   MetroidPrime/Enemies/CAi.cpp:
   	.text       start:0x80096C94 end:0x800972BC
   	.data       start:0x803B29E0 end:0x803B2A98
   	.sdata      start:0x80417FC8 end:0x80417FD8
   	.sdata2     start:0x8041AD38 end:0x8041AD50
   ```
   Leaving `.sdata`/`.sdata2` unclaimed moved every `lfs`/`bl` displacement and every REL import
   address (`0x8041B758` -> `0x8041B778`) and broke 71 RELs.
2. **Rename every function in the range** in `symbols.txt` to the mangled name our object emits,
   because retail's other units reference them by name. Eleven of them, e.g.
   `fn_80096EBC` -> `AcceptScriptMsg__3CAiFR13CStateManagerRC10CScriptMsg`. Read the names from our
   own object with `powerpc-eabi-nm`; pair by disassembly, not by size. `GetStateMachine2` emits
   *before* `GetStateMachine` even though the source is the other way round - their bodies are
   identical, so the pairing is by emission order and the names are a guess pinned to it.
3. **Rename the slots and callees the link complains about, one at a time**, each because `mwldeppc`
   printed `undefined:` for it: `fn_8004A0D8` -> `Think__6CActorFfR13CStateManager`, the four vtable
   copies the linker had kept elsewhere (`fn_800358D8`, `fn_80073CAC`, `fn_8003C59C`, `fn_80073CB4`,
   all `CAi` members whose bodies are identical to the CAi ones), and ten callees
   (`fn_8001C814` -> `__ct__10SMoverData...`, `fn_80070D60` -> `__ct__11CHealthInfoFRC...`,
   `fn_80072560` -> `GetTriggerBoundsWR__14CScriptTriggerCFv`, and so on). Deleting a dead rename is
   not optional: rename `fn_8003C59C` and `CStateManager` loses one *bookkeeping* match because our
   `CStateManager.o` does not define it - the bytes and the DOL are unchanged.
4. **A `.sdata2` split may not end inside a dtk `lbl_` symbol.** `lbl_8041AD50` spans
   0x8041AD50..0x8041AFE0, so CAi can only carve 24 bytes while its object wants 28. The seventh
   float constant was declared `extern` and its pool address named `kCAiSplashDenom` in
   `symbols.txt` - exactly what retail's linker did.

**Correction to an earlier claim in this file.** The orchestrator's reading of the disassembly -
that `fn_80096F8C` "takes no arguments, so the accessor's signature is wrong" - was wrong. The
retail vtable relocates `+0x38` to `HealthInfo` and `+0x3c` to
`GetHealthInfo__6CActorCFRC13CStateManager`, so `+0x38` *is* `CActor::HealthInfo(CStateManager&)`'s
slot and the parameter has to stay; the override merely ignores it. A no-argument accessor would add
a vtable slot and break the 46-slot table. Read the slot's neighbours before changing a signature.

**`CPatterned` landed too** (2026-09-25, same day; superseded 2026-09-28, when upstream's unit took
the whole class and it became `NonMatching` 27/103 with these ten still matching): a `Matching` unit, 10 of 10 functions, by
*not* attacking its 0xB58-byte constructor. The lane disassembled the vtable cluster instead and found
fifteen tiny accessors at `0x80073BF0..0x80073D14` - mostly `li r3,0; blr` - of which ten reproduce
exactly; those are claimed (`.text 0x80073C58..0x80073CB4`, 92 bytes) and the other five are left
retail. The class now exists, CPatterned's vtable relocations resolve, and the constructor is blocked
on nothing but the constructor.

Five things that cost that lane a build each, all general:

- **A translation unit is emitted in *reverse* source order.** The source had to be written bottom-up
  for its functions to land in retail address order. Anyone hand-writing a `Matching` DOL unit needs
  this; it is also why `GetStateMachine2` emits before `GetStateMachine` in `CAi`.
- **A function that belongs to another class is defined with that class's qualifier even when it lives
  in this TU**: `CAi::IsListening` sits among CPatterned's functions but mangles `__3CAi`; as
  `CPatterned::` it breaks the vtable relocation.
- **Writing a float literal re-creates the pooled `.sdata2` entry** and shifts every address above it
  (the `kCAiSplashDenom` trap again). Reference the existing word - `extern const float
  lbl_8041B758; return lbl_8041B758;` - instead of typing the value.
- **A bitfield's position counts from the LSB, and MWCC emits shift *n+1* for a field named
  `x34c_n_`.** A `rlwinm r3,r0,29,31,31` that looks like `x34c_25_flyer` is actually
  `x34c_28_notFlyer`.
- **A private virtual can return by reference**: slot 73 is `addi r3,r3,1876; blr` - `&this+0x754`,
  not a pointer load.

Two of the cluster's functions are characterised rather than finished: `GetOrigin` (5 of 7
instructions - MWCC hoists the second and third `lfs` above the first `stfs`, and no source shape
tried stopped it) and `GetTouchBounds` (26 of 26 instructions, but the epilogue restores `r0` before
`r31` where retail restores `r31` first). Both are single-instruction-class walls, not logic.


## Parallel lanes: running many Luna workers at once

The work is run as many agents in parallel, one **lane** per module or unit. This is worth doing
properly because spawning is free and immediate while **collecting is the dominant cost** - a
dozen lanes can be in flight at once, but each one has to be verified and merged by hand.

### Spawning a lane (the whole sequence)

```sh
SRC=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port
N=m7
git -C $SRC worktree add -f /tmp/opencode/$N -b mod-$N HEAD    # cut from current HEAD
mkdir -p /tmp/opencode/$N/build /tmp/opencode/$N/orig         # REAL dirs, see below
ln -sfn $SRC/orig/G2ME01 /tmp/opencode/$N/orig/G2ME01          # disc files, read-only
cp -r $SRC/build/binutils /tmp/opencode/$N/build/binutils      # symbol tools, see below
printf '\nbuild-clone/\n' >> /tmp/opencode/$N/.gitignore
cp $SRC/docs/LANE_BRIEFING.md /tmp/opencode/$N/LANE.md           # the briefing, versioned in the repo
```

`docs/LANE_BRIEFING.md` is the briefing itself, in the repo rather than in `/tmp` so it survives a
reboot and can be corrected in a commit. The old copies lived under `/tmp/opencode/n1/LANE.md` and
were lost with the boot.

`binutils` is not generated by `configure.py`, so a fresh lane has no
`build/binutils/powerpc-eabi-nm` and `tools/check_symbol_names.py` cannot run there - and that is the
check which catches a rename that breaks every REL link. Copy it in as above, or fetch it with
`python3 tools/download_tool.py binutils build/binutils --tag 2.42-2`.

Then spawn the agent with the brief pointing at that worktree, and tell it which module it owns.

**Reference the model explicitly, including its reasoning variant.** `Space Bunny Free` exposes
`low`/`medium`/`high`/`xhigh`/`max`, and a lane spawned without a variant runs at the provider's
default - which nothing in the lane's report reveals, so a weak result reads exactly like a hard
task. Space Bunny lanes are spawned as `opencode-go/space-bunny-free#max`.

### Non-negotiable details

- **The lane's `build/` must be a real directory, not a symlink to the master's.**
  `config/G2ME01/build.sha1` names files as `build/G2ME01/...`, and `dtk shasum` reads those
  literal paths - so a lane sharing the master's `build/` has its integrity check hash the
  *master's* files and will report `87 files OK` while its own output differs. This produced
  several false "verified" reports before it was found.
- **One lane per module or unit.** Two lanes on the same unit overwrite each other's source; that
  cost a whole lane's work early on.
- **Cut from current `HEAD` every time.** A lane carries its `config/` as of its commit, so an
  older worktree silently reverts a later `symbols.txt` fix when collected.
- **`LANE.md` is the lane's only briefing.** Update it when a failure mode is found - the
  no-assembly rule and the config.yml verification both live there now, and every new lane copies
  it.
- **Lanes write to their own worktree.** One lane wrote into the master tree instead; that is
  harmless only while its edits stay uncommitted, and it makes the tree ambiguous. Point them at
  the worktree explicitly.

### Collecting a lane

**`tools/collect.sh <lane>`** does steps 1-4 below in one command, in about seven seconds:

```sh
tools/collect.sh a1            # one lane, or several: tools/collect.sh a1 a3 a5
```

It builds a fresh worktree at **current** HEAD with its own real `build/`, records a report
**baseline from unmodified HEAD** (`tools/gate.sh --baseline`), exports the lane's diff over
`src include config configure.py libc tools docs`, applies it with `git apply --3way`, runs
`tools/gate.sh` on the merged result and prints the diff stat. The merged worktree is left at
`/tmp/opencode/collect-<lane>` for reading, hand-fixing, or copying files across.

Two things it does that hand collection does not. A lane's `config/` is its own view, so a
**stale file becomes a visible conflict instead of a silent revert** - and the silent revert is
what cost three modules their `Rel(...)` blocks. And the baseline is built from HEAD rather than
from the lane, so the per-function diff compares like with like: the lane's own numbers are
measured against a tree that never had its changes.

What is still yours: **read the lane's report and check its claims against the gate's output.**
A passing gate means the tree is sound, not that the lane did what it said. Then commit, with
the lane's findings in the message.

By hand, the steps are:

1. Read its report, then **verify it independently** - lane reports have been wrong in both
   directions (one understated its own result by 10 functions, one claimed a hash that did not
   hold).
2. Apply its `src/` and `include/` changes to the master tree by copying the files.
3. **Re-check its `config/` changes against the current tree** rather than copying them - its
   `symbols.txt` may predate a fix.
4. Run the gates, including all 86 module hashes against `config.yml`.
5. Commit, with the lane's findings in the message.

### Scale

Fourteen lanes ran in one wave without compute trouble (16 cores; builds are short and bursty).
The limit is not hardware, and since `tools/collect.sh` it is not the mechanics of collection
either - applying a lane and gating the result is one command and about seven seconds. The limit
is **judgement**: a gate that passes says the tree is sound, not that the lane's claim is true,
and every lane still has to be read against it. A wave of six to eight is comfortable to judge
honestly in a turn; more than that and reports pile up unprocessed, which is how unverified
claims reach the tree.

### What fails, repeatedly

- **Stale `config/`** - described above. **And stale `configure.py`, which is worse in one way:
  it fails silently.** Three modules (`Puffer`, `WallCrawler`, `ScriptGui`) lost their `Rel(...)`
  blocks to commits that copied an older `configure.py` (`33b73a3` replaced Puffer's block with
  WallCrawler's own; `f599488` dropped the other two). Their sources sat in `src/` compiled by
  nothing, their units still appeared in the report - because `config.yml` lists every retail module -
  and they read 0.00%, which looks like "not started" rather than "not wired". Restoring them was
  worth 30 matched functions. **Run `python3 tools/check_module_wiring.py` after any config merge**,
  and never copy `configure.py` from a lane.
- **Vague success criteria.** "cmp silent and 87 files OK" is satisfied by doing nothing, and the
  criterion must name the state in which the check is meaningful (the unit `Matching`).
- **Assembly as a shortcut.** A transcribed `.s` unit reproduces the bytes and scores 100% while
  decompiling nothing. One lane did this for a whole module (`FogOverlay`, 1,014 instructions)
  and it was rejected. A module that can only be reproduced that way is **blocked**, not done.
- **Claiming ranges the object does not reproduce.** Breaks the module's hash for every REL. The
  fix is to claim only what reproduces - see the recipe above.
- **Assuming a module is writable.** `include/MetroidPrime/Enemies/` holds only the `SwarmBasics`
  layer, and `CPatterned`/`CAi` now exist as `Matching` units (`CActor::UnkVtable20` is resolved, superseded
  above), so creature behaviour cannot be written however many
  lanes are pointed at it. Check the base classes exist before assigning a module.

## What to delegate, and how

- Cheapest lane that can do the job: `qwen27b`/`qwen` -> `worker` -> the `claude-code` tool.
- The **worker** lane produces volume on mechanical, reference-backed work (a matched Prime 1
  counterpart, script-unit scaffolding, name identification) and is weak at the last 1%
  (register allocation, instruction scheduling). It is also the lane that found the module
  recipe, by trying the `Matching` flip and reporting the exact symbol that blocked it rather
  than the check that passed.
- The **claude-code** tool is the one that converts near-misses and does multi-function units
  in one pass; it is also the one worth giving a whole unit and a long report.
- Every delegation ends with the lane stating its own verification result, and the
  orchestrator **re-measuring it independently** before committing. Lane reports have been
  wrong in both directions - understating and overstating.

## Where the remaining work is

- **The DOL tail** (~26k functions in `auto_*` units and the named `NonMatching` units). The
  named units that are close to complete are the cheapest; the rest is genuinely hard
  matching.
- **The REL modules** (~11.3k functions across 86 modules). ~~The pipeline is proven for wiring
  (`AIMannedTurret` links our object and stays byte-identical) but no module has yet been
  *decompiled*~~ **Superseded, 2026-09-25:** several modules now link our own C++ and still hash
  to `config.yml` - `AIMannedTurret`, `ScriptRiftPortal`, `Metaree`, `ScriptGui`, `Puffer`,
  `WallCrawler`, `FlyerSwarm`, `ScriptSafeZone`, `SwarmBasics`, `ScriptPlayerActor`,
  `ScriptPlayerTurret`, `ScriptFrontEndDataNetwork` (the table at the end of this file is the
  current list). What remains blocked in most of them is *behaviour*, not wiring - see "What still
  blocks most modules".

Record here which modules have been attempted and what blocked each one, so the next lane
does not rediscover it.

## Hard gates, every time

- `sha1sum build/G2ME01/main.dol` == `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
- every built REL `cmp`s equal to `orig/G2ME01/files/RelProd/`
- **"All 86 RELs differ" is one fact, not 87.** `makerel` runs `dtk rel make ... @$rspfile` with
  `build/G2ME01/main.elf` as its first input, so *any* change to the linked DOL shifts every REL.
  A lane spent a bisect proving this. The corollary is the one that matters: `ninja`'s exit status
  **is** the hash gate, and `main.dol` must never be read after a failed `ninja` - it is the
  previous build's file
- `./tools/probe_sources.sh` green (741 files, 0 failures)
- `python3 tools/check_symbol_names.py` reports 0 missing names- `All:` matched count from the report does not fall
- `config/G2ME01/splits.txt` and `configure.py` only change when the task is explicitly a
  config task (REL modules), never as a side effect
- `python3 tools/check_docs_claims.py` reports no disagreement (it derives the docs' numbers from the
  report; a doc claim that cannot be checked is a claim that will drift)
- `python3 tools/check_module_wiring.py` reports nothing UNWIRED (a module whose sources no
  `configure.py` entry declares is in no link, whatever the report shows)
- `python3 tools/link_check.sh` reports no rise in undefined symbols and **zero** duplicate
  definitions, against `docs/research/port_link_baseline.txt`. This is the *slow* gate - it
  configures Aurora, fetches its SDL3 and Dawn, and builds 118 game units - so it is not in
  `gate.sh`, and it is the only check that can see a change to `CMakeLists.txt`, `files.cmake`
  or `platform/`. Run it before committing any of those. A **duplicate definition** is the
  signal to watch: it means two translation units claim one name, which is how the fourteen
  `RELMain`s surfaced.
- `python3 tools/check_files_cmake.py` reports no omission. **A configured, on-disk unit that
  `files.cmake` does not name is in no port binary, and `link_gap.py` cannot see it** - the tool
  derives the gap from the objects `files.cmake` produces, so the omission is invisible to the
  instrument meant to measure it. 96 units were in that state, including all 72 `Matching` loader
  thunks, which were `Matching` in the DOL and `MISSING` in the port's link for a release. Only
  `tools/link_check.sh`, which asks the real linker, found it. **A gap number from a tool is a
  statement about the tool's inputs.** Exclusions are declared in the tool with a one-line reason
  and a *stale* one fails too, so the list cannot become a place where things go to die. Note what it
  does not claim: being listed is not the same as being a win. Defining a default constructor
  constructs its members and can *open* a gap - one unit measured net -1 in one configuration and +1
  in another, so **measure the net in the configuration you are in.**
- `python3 tools/check_raw_offsets.py` is clean (a raw offset is a documented stopgap, not a
  decompilation - see `docs/research/raw_offsets.md`)
- `python3 tools/check_decl_order.py` agrees with `docs/research/decl_order.md` (a permuted unit
  compiles, links, scores 100% and breaks the hash on a few bytes - see "Declare in reverse")

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

## Attempted modules (keep this list current)


| module | what happened |
| `CCubeRenderer::EndScene` | **Landed, 2026-09-27** - `Matching` 100.00% 1/1, retail 0x8026FB80, 0x7C = 124 B, `flip_test` PASS, `linked` 2556 -> 2557, DOL bit-identical. **124 bytes and it abuts `BeginScene` exactly**: 0x8026FB80 + 0x7C = 0x8026FBFC. Two traps cleared, both measured: retail's `r13` is **`_SDA_BASE_` (0x8041FD80), not `_SDA2_BASE_`**, and `lbl_80418AE4` **must be declared non-`const`** - declared `const` it compiled, linked, and hoisted the `lbz` above the frame stores, leaving 2 differing instructions at the **same length**. `tools/sda.py` hardcodes the `.sdata` base and is silently wrong for `.sdata2`. **No pixels**: the draw methods are still logging stubs. |
| `CCubeRenderer` (four units) | **Landed, 2026-09-27** - `matched` 3977 -> 3979, `linked` 2555 -> 2556, DOL bit-identical, 87/87. **`Carve80272958.c` is `Matching` 100.00%** (0x80272958, 0x30) and is the only one of the four that counts as linked. **`BeginScene` is also 100.00% and must stay `NonMatching`** - mwldeppc attributes 20 bytes of `.sdata2` to its object that retail does not have, which costs 32 bytes of `main.dol` and breaks 43 REL hashes. See "A percentage is not a link result" below. The vtable is now **real**: `Carve80270848.cpp` is the key function and the only thing that emits `vtable for CCubeRenderer`, and the port gap 309 -> 391 is the known cost of listing it (76 arriving symbols are `CCubeRenderer::` methods with no body yet). |
| `CMain` (header) | **Landed, 2026-09-27** - `sizeof(CMain)` was 0x94 and is **0x98**, proved by retail's own `sMainSpace` (`.bss:0x803C5A20; size:0x98`, next object at 0x803C5AB8), not inferred. The `+0x18..+0x48` region stopped being `char x10_pad[0x38]` and became a `double`, two 20-byte `SFrameTimeHistory` and their two **sums**. **Adds 0 to `matched` and 0 to `linked`** - and is still worth landing, because it is the port's type model being right about a boot-path object. See "`sizeof(CMain)` is 0x98" below. |
| `CCallStack` | **Landed, 2026-09-27** - retail's `RAssert` call-stack scaffolding, and **it formats nothing.** The class is eight bytes (two `char const*`), the constructor discards its `uint` argument, and the two accessors are plain `lwz`/`blr`. `include/Kyoto/Alloc/CCallStack.hpp` is right about the layout and wrong about the names: `x0_line`/`x4_type` are the *second* and *third* arguments. Which accessor is which is **not guessed** - `CGameAllocator::FixupAllocPtrs`, the only caller, stores the +0 read into `SGameMemInfo::x8_fileAndLine` and the +4 read into `xc_type`, which settles both names at once. New `src/MetroidPrime/CCallStack.cpp`, 0x8028BFD8..0x8028BFF4, 0x1C = 28 B, 3 functions, **`Matching` 100.00% (3/3)**, `flip_test` PASS, port gap 318 -> 315 MISSING. **And the required follow-up was the fourth instance of its class:** stubs 28/29/30 had to be deleted from `PortReachStubs.cpp` by hand, because `boot_probe.sh` builds `-DMP_BOOT_STUBS=ON` and `gate.sh`'s duplicate count cannot see that configuration. |
| `main.cpp` (three-way split) | **Landed, 2026-09-27** - the split is **not free**, and the reason is structural rather than tunable. See "The `@stringBase0` pool is PER TRANSLATION UNIT" below. `CMain::FillInAssetIDs` (0x80006B38, 0x48 = 72 B) is now an isolated **`Matching` 100.00% 1/1** unit and `linked` rose 2554 -> 2555; the cost is -5.113 on `__ct__24CGameArchitectureSupport`, a 1-of-11 function that contributes 0 to both counts and whose behaviour is unchanged. `CMain::AsyncIdle` was **declined** on the mirror-image reasoning: 1-of-11, contributing 0 to both, for no `Matching` unit. |
| `AIMannedTurret` | **Landed, 2026-09-25** - the first module whose unit genuinely flips, and the failure this table recorded for several sessions was real but was not a blocked module. Declared ascending, the unit broke the module's hash (85/86, exactly as measured); the cause was **declaration order**, not a rename, a symbol, a data section or extra functions. See "Declare in reverse" below. With the order fixed: unit `Matching`, `flip_test.sh` PASS, sha1 `949b8c21caf1112b10d07748dbe8c32d3bd7efac` verified against `config.yml`, DOL and all 86 RELs unchanged. The first modules to link our own code are still `ScriptRiftPortal` and `Metaree`; `AIMannedTurret` is the first whose unit **flips**. |
| `Tweaks` | **Partly landed, 2026-09-26 (lane `e1`)** - the module's 76 `LoadTypedef<T>` bodies are **not** 68 distinct functions: 56 are in `Tweaks`, 7 in the DOL, and 5 of the port's names are retail's `UnknownStruct1/2`. The generated bodies are already **99.1-100%**; seven of them are at exactly 100% and three more landed as `Matching` units by **re-splitting the existing `[LoadTypedef, ~T, T]` triples** so each new unit claims only its `LoadTypedef` - see "A `Matching` unit may claim one function of a three-function triple" below. The retail member layout of all 79 `SLdr*`/`CTweak*` structs is now in `docs/research/sldr_tweak_sizes.md`, and it **overturns** the 1,500-byte `CTweakContents` drift in `docs/research/tweak_globals.md`: that figure is an LP64 artifact of a host probe (`sizeof(rstl::string)` is 24 there, 16 in the MWCC build), and with retail's widths the headers reproduce retail's layout exactly except for **one** struct, `SLdrTweakPlayerRes_AutoMapperIcons`, which carries five members that are not properties of it (+0x50). |
| `IngSwarm`, `WallCrawlerSwarm` | wired; no class code at all (all `REL_Setup`), so nothing to decompile. |
| 27 unwired modules (`AtomicAlpha` `BacteriaSwarm` `Blogg` `DarkTrooper` `DestructibleBarrier` `ElitePirate` `EmperorIngStage3` `FishCloud` `GeomBlobV2` `IngBlobSwarm` `IngPuddle` `IngSnatchingSwarm` `IngSpaceJumpGuardian` `MediumIng` `MetareeSwarm` `Metroid` `MysteryFlyer` `Parasite` `PillBug` `PlantScarabSwarm` `Rezbit` `SandBoss` `SnakeWeedSwarm` `Splitter` `SwampBossStage1` `SwampBossStage2` `Tryclops`) | **Landed, 2026-09-28 - `REL_Setup` tail claimed in each, 5/5 exact, +135 matched, +135 linked, 87/87 hashes.** No C++: `tools/wire_rel_setup.py` (see "The recipe"). `audit_rel_claim.py` reports 0 problem claims on all 27. The class code of each stays retail and is the next item per module (`progress`, target `module:<Module>`). |
| `SkyRipple` | scaffold broke the hash (85/86 RELs) - claimed ranges did not match the object. Reverted. |
| `CGraphicsTimeProvider` | **Landed, 2026-09-26 (lane `h2`)** - `CGraphics::SetExternalTimeProvider` (0x802BF618, 0x8) and `CGraphics::GetSecondsMod900` (0x802BF620, 0x20) in one `Matching` unit claiming the contiguous 0x802BF618..0x802BF640, both at **100.00%**, `flip_test.sh` `PASS -> kept as Matching`, DOL sha1 held. The technique worth keeping: **`CGraphics` has no `.cpp` at all**, so a `Matching` unit can only reach its statics by retail's *unnamed* dtk labels (`lbl_804199DC`, `lbl_804199D8`), never by the invented C++ member names in `CGraphics.hpp` - a reference to `CGraphics::mpExternalTimeProvider` mangles to a symbol nothing defines in the DOL. The port-side definitions of the `lbl_` objects are in `PortGlobals.cpp`, and the C++-named members are deliberately left undefined so there is only ever one object per concept |
| `CGraphicsScreenPosition` | **Landed, 2026-09-26 (lane `h2`)** - `CGraphics::GetScreenPosition` (0x802BE9A4, 0x34) in one `Matching` unit, **100.00%**, `flip_test.sh` `PASS`. **And the trap, which cost this lane two builds: the SDA21 field is the *full* signed displacement, so `field = (address - 0x8041FD80) & 0xFFFF`.** Two wrong answers (0x804199D0/D4/D8, then 0x804199E4/E8/EC, against the right 0x804199E0/E4/E8) each produced an object that was byte-identical, paired at 100% under objdiff and passed `unit_fit.sh` as *fits, no extra functions* - and each broke the DOL's sha1 on exactly three bytes. `flip_test.sh`'s "the REBUILD FAILED - do not trust build/ until it is green again" is the message to read first, and `cmp -l` against `orig/G2ME01/sys/main.dol` names the bytes. Do the subtraction in a script |
| `CGraphicsSetScreenPosition` / `SetUseVideoFilter` / `SetModelMatrix` / `SetViewPointMatrix` | **Not attempted, 2026-09-26 (lane `h2`), each for a measured reason.** `SetScreenPosition` (0x802BE8F0, 0xB4) is 180 bytes of register-allocated arithmetic over the 0x3C-byte render-mode object at 0x80417264, whose layout this tree does not model. `SetUseVideoFilter` (0x802BEC24, 0x48) **cannot be Matching at all**: retail never writes `r7`, so it passes an uninitialised fifth argument to `GXSetCopyFilter`, and no source expression reproduces an argument the compiler invents - Aurora's own `GXSetCopyFilter` takes four parameters, so the fifth is dead on the host and the behaviour is genuinely undefined on the cube. `SetModelMatrix` (0x802C24AC, 0x60) and `SetViewPointMatrix` (0x802C2534, 0xE0) both call `fn_802C2614`, which has no body in the tree, so a `Matching` unit closes one port-gap symbol and opens one - the trap `check_files_cmake.py`'s own header warns about |
| `FogOverlay` | "completed" by transcribing 1,014 instructions into a `.s` unit. Rejected as not a decompilation. |
| `ScriptRsfAudio` | wiring, 7 correct symbol names, and an empty source; hash "matched" only because the unit was `NonMatching`. The symbol names are worth keeping; the rest proves nothing. **Superseded, 2026-09-25**: a later lane added exact `RELMain`/`RELExit`, loader registration and the setup range - 8 functions, unit `Matching`, sha1 `af0941ce5e81230282eda9cfb59e1839dc45443a` verified against config.yml. The remaining 14 module functions stay retail/unclaimed. |
| `ScriptPlayerProxy` | 9 functions (loader registration, `RELMain`/`RELExit`, an unnamed setup function and one field accessor) plus all 5 `REL_Setup` ones - unit `Matching`, sha1 `19ea68a377b4908848b9d640245842526a8dd968` verified. The remaining 48 class functions stay retail/unclaimed; this is the cheapest module shape yet found (its writable code is all `.text` wiring). |
| `DarkSamusBattleStage` | scaffolded split produced a 5,184-byte REL and an assembly object would not link. Correctly reverted with 0 functions. |
| `ScriptCoin` | **Landed, 2026-09-25** - 6 functions in 3 `Matching` units, module hash held, **+6 linked**. *Superseded:* the row above used to say "3 real functions written (a class, `Render`, `GetTouchBounds`) ... does not hold its hash yet", and **no `CScriptCoin.cpp` and no `Rel("ScriptCoin", ...)` block existed in the tree at all** - the claim was written from memory about a lane that never landed. The 6 functions are `RegisterCoinLoader`, `RELMain`, `RELExit` and a vtable slot in `CScriptCoinRel.cpp`, `CScriptCoin::Render` in `CScriptCoin.cpp`, and `CActor::GetTouchBounds` in `CScriptCoinTouchBounds.cpp`; all 100%, all with the unit `Matching`. The rest of the module stays retail and unclaimed. |
| `Ripper` | blocked with evidence: no `CRipper`, no `CPatterned`, no `include/MetroidPrime/Enemies/` at all. Reverted the scaffold rather than claim ranges it could not fill. The range check passed, so the block is the missing base classes, not the splits. |
| `Tweaks` | 2 generated constructors brought to exactly 100% (`SLdrTweakTargeting_Scan`, `SLdrTweakTargeting_VulnerabilityIndicator`) and 3 more moved 5-40 points closer, by moving the member assignments from the constructor body into the mem-init list. Not promoted - the other 12 units are blocked (seven `LoadTypedef*` at a 99.2% register-allocation wall, three on float-literal pooling, and the module's `.rodata` cannot be split per unit). |
| `Tweaks` (2nd pass, `REL_CreateTweakGlobals`) | `REL_CreateTweakGlobals` (module `.text:0x508`, 1,452 bytes) went from `{}` to a full body at **68.29%**, 1,184 bytes. Not promoted and **no range claimed** - the unit stays `NonMatching` over the whole `0x0..0x1338`, so `Tweaks.rel` still hashes to `config.yml` and the gate is unmoved (`matched 3043 -> 3043, linked 1653 -> 1653`, port link gap 721 before and after). Two blockers, both measured: the module's `.rodata` is unsplittable, so mwcceppc's CSE of the `__FILE__` argument cannot be undone; and retail re-materialises that argument at all 15 sites while ours hoists it. The pass's real output is `docs/research/tweak_globals.md`, a store-by-store map of all 1,452 bytes, which is what establishes that `gpTweakPlayerA` ends up pointing at a 4-byte heap cell and **not** at a `CTweakPlayer` - so the function is *not* what unblocks the frame loop. |
| `CRumbleVoice`, `CRumbleGenerator` | `CRumbleVoice` now matches **five** of them (8/16 -> 13/16) after the fix below; 0 of `CRumbleGenerator`'s. The unmatched `fn_8032*` functions are TU-local weak `rstl::vector<SAdsrDelta>`/`<SAdsrData>` instantiations with no name in the retail object, so objdiff scored them 0% even when the bodies were byte-identical. **Solved for pairing** by writing explicit specialisations in the source and renaming the retail symbols in `symbols.txt` to the mangled names MWCC emits (read them from our own object with `nm`) - see "Pairing a function the retail symbol table has no name for". Neither unit can be promoted yet: `CRumbleVoice` emits 180 bytes the retail unit object does not have, `CRumbleGenerator` 452. |
| `CScriptStreamedMusic`, `CStaticAudioPlayer` | **Superseded for `CStaticAudioPlayer`, re-measured 2026-09-25.** The old reading - "pure register allocation, and 868 bytes of extra emitted functions on top" - was half right and has been corrected. `CStaticAudioPlayer` is now **23/24 at 99.87%**, and the "extra functions" are *not* the blocker: the DOL link passes `-strip_partial`, so mwldeppc deletes the 8 duplicate weak copies out of the middle of our `.text` and the flipped DOL comes out **exactly the same size as retail** (3 969 024 bytes both), with the bytes coming back out of the three objects that hold retail's copies (`CFilePreload`, `CCubeMoviePlayer`, `auto_03_8018A188_text`). What now blocks the flip is the **emission order of the out-of-line template instantiations** - see the new section "An emission-order wall: out-of-line template instantiations". `Decode` went 99.39% -> 100% on a one-statement `const` local; `DecodeMonoAndMix` 97.50% -> 98.70% and is stopped at 18 differing instructions. `CScriptStreamedMusic` was not re-measured. |
| `CGX` (DOL, not a module) | **53 of 54 and still not promotable, and the reason is data, not code.** The permutation went first (five local moves, ~15 lines - it was the unit `docs/research/decl_order.md` called the best value per line moved, and that is now paid out), then `SetDstAlpha` 99.43% -> 100% by assigning a widened local back to a `uchar` member, and `__sinit_CGX_cpp` 76.92% -> 100% by routing a constant initializer through an `inline` function. `.text` now measures 5936 against a claimed 5936, "fits", no extra functions. **Not flipped**, and a hand flip was measured rather than assumed: `main.dol` grows 32 bytes, `lbl_8041E4A0` moves to 0x8041E480, and `sGXState` (COMMON for us, `.bss` in retail) lands at 0x804170E0 against a claimed 0x803DF828. Three separate problems remain - six data symbols that must be *imports* rather than compiler-generated constants, `sGXState`'s COMMON-vs-`.bss` placement, and `SetVtxDescv_Compressed` on the register-allocation wall. Full symbol/address table and the DOL evidence in "A DOL unit can be blocked by data, not by code". The intended config changes, not applied here, are three `splits.txt` lines plus the six `extern` declarations - see the report. |
| `CPlayerGun` / `CGunWeapon` / `CPlayer` (DOL, not modules) | **The 40 boot-path candidates are all gameplay-only, and one tool run establishes it.** `docs/research/port_link_stubs.md` lists 342 unstubbable symbols because they are *referenced by a reachable object*; lane `h3` crossed 40 of them (`CPlayerGun` 22, `CGunWeapon` 6, `CGunStateMachine` 5, `CPlayer` 7) against `docs/research/boot_path.md` and **0 are reached before the first frame, 0 during initialisation, all 40 in gameplay**. Not one is referenced from a static initialiser: of 69 relocation sites, 7 are static data in `sStateFuncs`/`sTriggerFuncs` and 62 are calls inside `CPlayerGun`, `CStateManager` or `CScriptCannonBall` methods. The gate is one function - `CPlayer::CPlayer` (0x8001B018, 0x15C8) is the only creator of `CPlayerGun` and has one caller chain (`fn_8001EE58` / `fn_801F42A0` -> `fn_800401D8` -> `fn_80040D88` -> `CPlayer::CPlayer`), and it needs a loaded world, which step 13 cannot yet provide. Two landed anyway as `Matching` units, `CPlayerGetPlayerIndex` (0x8000D084, 8 bytes) and `CPlayerGetTweakPlayer` (0x8000BF94, 0x18), both `flip_test.sh` PASS, and both needed `CPlayer.hpp` to name two fields that were inside one `char m_pad_6[0x1A8]` - which turned up that the header's own comment put that pad at 0x1320 when mwcceppc puts it at 0x131C. New tool `tools/link_fn_reach.py` does the partition at referencing-site granularity; over the whole 342 it reports 457 `call` sites, 41 `data` and **0 `pre-main`**, so `link_reach.py`'s static-initialiser roots are not the reason those 342 look dangerous. Full table in `docs/research/gun_boot_path.md`. |

| 72 REL entity-loader thunks (DOL, not modules) | **Landed, 2026-09-25, +72 matched and +72 linked, port link gap 724 -> 652.** 64 new `Matching` DOL units, 3168 bytes of `.text` and 512 of `.sbss`, and the **port link gap** closed 72 symbols. The trigger was a *static initialiser*: retail builds the same 184-entry `{FourCC, FScriptLoader}` table the port's `ScriptLoader.cpp` has, in `__sinit_ScriptLoader_cpp` (0x80242894, 5696 bytes, already `Matching`), so decoding its `lis`/`addi`/`stw` dataflow gives **every** loader's retail address and the two tables then match position for position. See the new section below and `docs/research/rel_loaders.md`. |
| `CAudioSys`, `CStreamAudioManager` (DOL, not modules) | **Landed, 2026-09-26 (lane `h1`), +12 matched and +12 linked, port link gap 311 -> 297, `link_check.sh` 342 -> 328 undefined.** Seven new `Matching` units in `src/Kyoto/Audio/`, 392 bytes of `.text`, `.text`-only claims, every one `flip_test.sh` PASS with `unit_fit.sh` "fits / no extra functions". The route split and, more usefully, **the finding that only 11 of the port's 29 audio symbols are reached before a first frame** - they come from two objects and two call sites - are in `docs/research/audio_stack.md`. Three traps, all measured there: **`dtk dol split` refuses a `.sdata` claim that is not eight-byte aligned**, and a `Matching` unit that owns such a slot then has to define all of it under retail's *names*, because retail text outside the unit reads them; **`clrlwi` at a call site comes from a conversion written in the source, not from the callee's declared prototype**; and **`cmplwi` vs `cmpwi` is decided by the left operand's type and is not cosmetic** (`static_cast<int>(x) > 0x7F` gives `cmpwi`, and the two disagree above 0x8000). |

| `rstl::CRcPtrData` (DOL, not a module) | **The out-of-line `rc_ptr` copy constructor, half-landed 2026-09-26 (lane `g4`)** - `fn_80049010` (0x80049010, 0x24) is retail's copy constructor, calls it 15 times, and the map gives it **no mangled name** while `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` 0x24 bytes away *is* named: one copy constructor for every `T` is only possible if the words live in a **non-template** class. `rstl::CRcPtrData` is that class now, `src/rstl/rc_ptr_copy.cpp` defines its copy constructor out of line, and `CIOWinManager::RemoveAllIOWins` reached **100.00%** (was 51.88%) and became byte-exact. Retail's asymmetry - inline at six sites, a call at fifteen - is reproduced with a **tag** (`rstl::CRcPtrData::OutOfLine`) rather than one definition, so `IOWinPQNode::IOWinPQNode` keeps its 100% and `AddIOWin` its 95.74%. **Not promotable: the copy constructor is 97.22%**, because mwcceppc allocates the AddRef to r5/r4 where retail uses r4/r3 - the *out-of-line* allocator differs from the one it uses for an inlined expansion, and twenty body spellings, four class shapes, a template base and every `-O`/`-pragma` combination leave it alone. So `RemoveAllIOWins` stays `NonMatching` although it is byte-exact. Two traps: the base's default constructor must be **empty** (an initialising one is not eliminated and cost `IOWinPQNode` 36 points and both `CObjectReference` constructors ~17), and MWCC does not encode base classes, so no mangled name in the tree changed. `docs/research/rc_ptr.md`. |
| `CArchitectureQueue::Pop`, `CIOWinManager::PumpMessages` (DOL, not modules) | **Landed 2026-09-26 (lane `g4`), both 100.00%, 372 bytes** - `PumpMessages` (0x800496A0, 196 B) and `CArchitectureQueue::Pop` (`fn_800495F0`, 0x800495F0, 176 B), one contiguous range, `NonMatching`. Two things make them writable: mwcceppc 2.7 **does not elide the copy out of a return value**, so `CArchitectureMessage msg = queue.Pop();` is a return slot *plus* a copy-initialised local and the slot's destructor runs between the copy and the use - which is exactly retail's order and is why there are two 16-byte messages and two **dead** `beq`s per function; and `Pop()` had to be moved **out of line** in the header, or it is inlined and `PumpMessages` carries a 176-byte expansion where retail has one call. Not promotable for two independent reasons, both measured: `rstl::list<CArchitectureMessage>::do_erase` is a template member so mwcceppc emits it `W` where retail has `fn_80048F78` as a strong `T`, and the unit emits a 12-byte `__vt__24IArchitectureMessageParm` in `.data` that `splits.txt` does not claim. `tools/flip_test.sh` FAILs it and reverts. |
| `CInputGenerator::Update` (DOL, not a module) | **Written 98.27%, 2026-09-26 (lane `g4`)** - 0x8001D888, 0x1FC = 508 bytes, the largest single symbol in the frame loop, `NonMatching`. Every instruction of the body is retail's; the whole 1.72% is **mwcceppc reserving 16 bytes of stack slack**: it allocates a 0x40-byte slot for the 0x30-byte parm aggregate where retail allocated 0x30, which shifts the prologue and epilogue's saved-area offsets by 16 (`stwu r1,-192` against `-176`, `stmw r23,140` against `124`, `stfd f31,176` against `160`) and replaces retail's `psq_st` with an `xxsel`. Compiling the *identical* source with a 0x28 parm makes the prologue **byte-identical** to retail's, which is the proof that nothing else is wrong - and 0x28 is wrong, because `fn_80306BB0` writes a byte at +41 and `fn_80048CF4` allocates 48. The three virtual calls need no renaming: `__vt__18CDolphinController` at 0x803BB068 reads `[3],[4],[5] = 0x8030bd1c, 0x8030b5e0, 0x8030b5cc`, which are the header's own `Poll`, `GetDeviceCount`, `GetGamepadData` in declaration order - and retail calls `GetDeviceCount` **first** and `Poll` second. `fn_80306BB0` also reads a byte at +0x29 of the gamepad data, so `CControllerGamepadData` is at least 0x2a. |
| `CModel::Touch` (DOL, not a module) | **Landed, 2026-09-26 (lane `g4`) - `Matching`, 100.00%, 76 bytes, `flip_test.sh` PASS** - `Touch__6CModelCFi`, 0x803112DC, 0x4C: three calls and a two-register prologue, `fn_80310F38(this)`, `fn_803115F8(this, part)`, `fn_802BBDB8(*(void**)(this+0x28))`. It is the function `CModelTouchParts.cpp` (itself `Matching`) has been calling since it landed, which is why it was invisible on the link gap list until now: nothing *referenced* it until `CGunEffectTouch.cpp` and `CGunEffectTouchAll.cpp` were compiled. One header change was needed - `CModel` had **no member at +0x28** and the header was not self-contained (it uses `uint` and never included `types.h`). The +0x28 pointer is opaque: the callee reads a byte flag at +0x40 of it and nothing in the tree names the type. Closes `_ZNK6CModel5TouchEi` for the port, which is the +1 lane `f4` was measuring. |
| `CGameState::CGameState(CInputStream&, int)` (DOL, not a module) | **Attempted 2026-09-26 (lane `j1`) - the recorded reason it was never attempted is wrong, and measuring that is the lane's real output.** The 1,668-byte constructor was left alone because it has "seventeen unwritten callees"; **a `Matching` unit needs only relocations to its callees**, and `dtk dol split` writes a *filled* `build/G2ME01/obj/<unit>.o` for every unit and links that one for `NonMatching` units, so a callee whose range a `NonMatching` unit claims **is** defined. Measured: all 42 named callees of `fn_80144140` resolve (37 from ranges nobody claims, 4 from `NonMatching` units, 3 from `Matching` ones), and a probe unit calling one of each linked cleanly with all three defined in `main.elf`. **`configure.py`'s note on `CInputGeneratorUpdate.cpp` states the same false premise** ("nothing in the DOL link defines it and a Matching unit calling it would not link") and should not be repeated. The function is **unattempted, not blocked**; what it needs is twenty unnamed member types. `CGameState` is **0x2F0, measured with mwcceppc, and agrees with `operator new(0x2F0)`**; `CHintOptions` was 0x16 and is **0x18**, which moves `CPersistentOptions` from 0xDA onto the 0xDC retail constructs. Byte split: 417 instructions, **0x580 = 1,408 inline against 0x104 = 260 of call instructions**, nothing blocked on a callee. One leaf landed: `fn_80180738` (36 bytes, the `CHintOptions` constructor) at 100%, `Matching`, `flip_test.sh` PASS - the only callee of the 1,668 bytes that calls nothing, so the only one that is net -1 on the port's link. Next, in order: `fn_80144924`+`fn_8014495C`+`fn_80142A10` (164 contiguous bytes, the +0x110/+0x144 constructors), then `CWorldState::CWorldState` = `fn_8015C34C` (276 bytes, the **+0x3C** member, and its only two callees are *named* retail functions), then the constructor. Full map, blockers and reproduction commands in `docs/research/cgamestate_layout.md`. |
| `CWorldState::CWorldState` = `fn_8015C34C` (DOL, not a module) | **Attempted 2026-09-26 (lane `k1`) - written, `NonMatching` at 89.13%, 272 of 276 bytes, and the whole 0x4B0 member map is landed.** `src/MetroidPrime/CWorldStateCtor.cpp` claims `.text 0x8015C34C-0x8015C460`; `include/MetroidPrime/CWorldState.hpp` now names **all 25 members the constructor touches**, every offset measured with mwcceppc's own flags against the instruction that fixes it, and `CHECK_SIZEOF(CWorldState, 0x4b0)` passes. `flip_test.sh` says FAIL and reverts, as it must: retail has an **unused `mr r3,r31`** between its `+0x4A4` and `+0x4A8` stores that no spelling of the source produces, and the register allocator then gives the bitfield block r3/r4 where retail uses r4/r5. Tried for that `mr` and measured not to produce it: an empty inline member function and an empty `static` free function at that point, a member function carrying the whole tail, a static member function taking `CWorldState*`, a member function with a dummy argument, the flags written through a local pointer, `volatile` lvalues for the two stores, and all six permutations of the three tail statements. Two smaller differences come from the same place: retail's first `lfs` is its **third** instruction where mwcceppc always puts it seventh, and moving the read in the source does not move it. **Three techniques here are worth keeping.** (1) *mwcceppc deletes the construction of a class member this translation unit never reads* - measured for `rstl::string`, for a three-word struct with a user-provided constructor with and without a non-trivial destructor, from a mem-init list, and from `x = rstl::string()`; a **scalar** member written from the body always survives. That is why a constructor which must write an `rstl::string`'s three words has to call a member function to do it, which is what `rstl::basic_string::SetEmpty()` was added for. (2) *mwcceppc re-reads a non-`const` global after every store it cannot prove does not alias it*, so a constant used for five members is re-read five times where retail reads it twice; reading it into a **non-`const` local** reproduces retail's two reads, and a **`const` local is worse than nothing** - it gets an FPR *pair*, a `xscmpeqdp` and a 16-byte-larger frame. (3) *placement `new` costs a null check*: `new (&m) CRandom16(99)` emits `addic.` plus a `beq` over the call, so a member with no default constructor is built by calling retail's constructor by name. Point 3's declarations **must be `__MWERKS__`-only**: the host mangles the members to `_ZN9CRandom16C1Ej`, and the C-linkage retail spellings were measured at **+2 undefined** on `link_check.sh`. The second named callee, `__ct__12CTransform4fFRC12CTransform4f` (0x802C9054, 52 bytes), is **already at 100% - but as an inline-assembly transcription**, which this project does not count; a plain C++ mem-init list gives 192 bytes (12 `lfs`/`stfs` pairs) and a `double`-view loop gives the right 52-byte shape with a different FPR rotation, so it was not promoted. `fn_80144924`+`fn_8014495C`+`fn_80142A10` was **not attempted** - out of budget. |
| `CResFactory::Build` and `vtable for CResFactory` (DOL, not a module) | **Landed, 2026-09-26 (lane `m3`) - the third and last frame-0 vtable.** `CResFactory::Build` (retail `fn_802FA960`, 0x802FA960, 0xC0 = 192 bytes) is a `Matching` unit, `src/Kyoto/CResFactoryBuild.cpp`, **100.00%, 1 of 1, `flip_test.sh` PASS**, and the port's `link_check.sh` no longer asks for `vtable for CResFactory` at all: **330 -> 332 undefined, gross +3 (`fn_802FAAE4`, `fn_802FA1BC`, `fn_802FA7D4`) against gross -1, net +2, 0 duplicates.** Reaching it took the class's whole interior, which is in `docs/research/paks.md`; three things there are worth repeating here. (1) *A constructor that nothing else calls is the cheapest map of a class there is.* `CResFactory::CResFactory` (`fn_802FB154`) is twenty instructions that touch every member except one, and it is what fixed the last 0x44 bytes of the object - including **`CFactoryMgr` is 0x28, not 0x38**, whose four unnamed `uint`s were the first half of an `rstl::list` the manager was swallowing. (2) *`AsyncIdle` is worth reading for what it does not touch*: its four words are `+0xA0`, `+0xB0` (the first list's count) and the second list's `x4_start`/`x8_end`, and its timed half is a 64-bit divisor at `0x80411050` that is **not a member of anything**. (3) *A `Matching` unit's object must be exactly the size of its claimed range.* A `Build` that was 24 bytes short still linked and still failed only the hash - and `.text` **shrank by 0x18 and everything after it moved**, which is a much more alarming symptom than the per-function diff suggests; bisect `main.elf` against a saved baseline rather than the REL hashes. Two MWCC facts cost the most time and are recorded in full in `paks.md`: **MWCC emits a class's vtable in the unit defining its *first* virtual, not its key function** (so with the destructor merely declared, `Build`'s unit emits none at all), and **a `CFactoryFnReturn` returned by value must be a temporary, not a named local** - a named one is built in the frame, copied into the return slot and then destroyed, which is 0x50 bytes of `__dt__16CFactoryFnReturnFv` retail does not have. `~CResFactory` is written and byte-exact and still not landed: its object also carries `__vt__8IFactory` and a weak `__dt__8IFactoryFv`, and **retail's all-zero `__vt__8IFactory` and retail's base-vptr store cannot both be produced** from this header - measured both ways, in `paks.md`. |
| `CGameState`'s five remaining constructor callees (DOL, not modules) | **Four landed, 2026-09-26 (lane `cal3`), +4 matched and +4 linked.** `fn_80142CF8` (`CGameStateSysOptsPutTo.cpp`, 0x80142CF8, 0x80), `fn_80142DD4` (`CGameStateSlotDefaults.cpp`, 0x80142DD4, 0x94), `fn_801440C0` (`CGameStatePlayerLoop.cpp`, 0x801440C0, 0x80) and `fn_80009898` (`SGameStateMemcardReset.cpp`, 0x80009898, 0x34) are all **`Matching` at 100.00%** with `flip_test.sh` PASS, `unit_fit.sh` clean and `check_decl_order.py` ok. Three findings worth carrying: **(a) a retail `.sdata` global's address needs a NON-`const` declaration** - `const` puts the object in the read-only small-data area and mwcceppc emits `lis`+`addi` with `R_PPC_ADDR16_HA`/`LO` where retail has a single `R_PPC_EMB_SDA21` (100% -> 88.44%), which is the mirror image of the missing-`const` reload finding in `CGameStateMemcardCtor.cpp` and the same root cause: the C++ type decides which small-data section the object lands in. **(b) An array element's address comes out strength-reduced the other way round if the array base goes into its own local first** - `&self->x144.x04_blk[idx]` emits `addi r31,r4,328 ; add r31,r30,r31` (`self + (idx*16 + 0x148)`), and two statements (`SGameStateBlock* blocks = self->x144.x04_blk; block = blocks + idx;`) emit `slwi r0,r4,4 ; add r31,r30,r0 ; addi r31,r31,328` (`(self + idx*16) + 0x148`), which is retail's (83.05% -> 100.00%, same object size, so nothing but the allocator differs). **(c) A loop's two comma operands are ordered, and `player += 2, ++i` is the spelling retail has** (99.31% -> 100%). | `fn_800098CC`, the other half of `fn_80009898` (DOL, not a module) | **Written, `NonMatching` at 99.55%, 2026-09-26 (lane `cal3`)** - 356 bytes, and the only thing stopping the pair being one `Matching` unit. The body is a **76-iteration byte append that mwcceppc has unrolled by eight** (9 iterations of 8, then a bottom-tested remainder of 4; nothing in the source says 72 or 9) plus a **22-instruction loop whose body is empty** - the unroller's `addi r4,r4,8` and a `bdnz` to itself, with no stores anywhere. The empty body is **measured, not inferred**: putting a store in it stops the unroll entirely and the unit falls to 81-88%. Three spelling facts, each measured: the destination must be a named `unsigned char*` (77.16% -> 99.19% - written as `self->x54_buf[count]`, mwcceppc folds `+0x54` into the index and uses `self` as the `stbx` base where retail computes the base once before the loop), `+0x50` must be written through `self` every iteration rather than kept in a local, and `lbl_80417D93` must be non-`const`. The last **7 instructions** are an r4/r5 swap inside that empty loop and did not move for ~30 spellings (`for`/`while`, `++i`/`i++`/`i += 1`, increment in the third clause or the body, `int`/`u32`/`long` for either variable, bound hoisted or inline, declarations in either order or at function scope, the loop and the destination pointer each in a nested scope, a `volatile` read, an extra unused local). **One real finding inside the failure**: rewriting the loop as `while` with the increment in the body puts the *bound* in `r6`, as retail has it, where `for` puts it in `r5` - worth 3 of the 7. The pair was then **split at the symbol boundary** so `fn_80009898` is `Matching` and this is not; that is the `ScriptRiftPortal` prefix/matching/tail arrangement. |
| `CStateManager` and `CAnimData` layouts (DOL, not modules) | **Landed, 2026-09-26 (lane `v3`), +18 matched, `linked` held at 1811, 0 units newly linked, no regression on any function.** Not a function written: **one character in a pad, twice.** `CStateManager.hpp` had `pad2_2[0x34]` where retail has `0x2C`, so every member from `x1684` up was 8 high, `m_isDarkWorld` sat at 0x2954 instead of 0x294C and `sizeof(CStateManager)` was 0x2958 instead of 0x2950; retail's own `SetIsDarkWorld` reads `lbz 10572(r3)` against our 10580, which is the whole of a 99.79% function. Fixing the pad took 17 functions to 100% - 12 in `CStateManager`, plus `CPlayerGun::RenderBeamParticles` and `CPlayerState::GetRenderSuit`, which read `CStateManager` members from other translation units. `CAnimData.hpp` had the same disease with a note that said so: it recorded mwcceppc's `x178_particleDB` at 0x188 and treated the figure as retail's, so `x120_unk` is `0x48` and not `0x58`; retail's `CActor::SetModelData` does `addi r3,r3,376` where we emitted 392. All 25 documented `CStateManager` offsets now agree with the compiler and `sizeof` is 0x2950. **Why it was free:** of the 19 functions in `CStateManager` that touch the moved range, **none was at 100%**, so nothing could fall. **The method is in the new section "A header comment that recorded *mwcceppc's own output*" - the scan is `tools/offset_shift.py`, the probe is `tools/probe_offsets.cpp`, and the discriminator is a *uniform operand delta*, which a codegen difference never produces.** After both fixes the same scan finds no third instance in `main/`. Everything else this lane looked at is register allocation or scheduling and is recorded as not-moved. |
| `fn_8000934C` = `rstl::rc_ptr<CPlayerState>::ReleaseData` (DOL, not a module) | **Written and **correct, and BLOCKED on `dtk`'s link-order cycle - 2026-09-26 (lane `cal3`).** 0x8000934C, 0x50 = 80 bytes, `rstl::rc_ptr<CPlayerState>::ReleaseData()` on the 8-byte `{ptr, refcount}` pair the constructor builds four of. The source is in `src/MetroidPrime/Player/CPlayerStateRefRelease.cpp` and is **deliberately not registered** in `configure.py`, because the range cannot be claimed. 0x8000934C is inside `MetroidPrime/main.cpp`'s `.text 0x800053B8..0x80009880`, and **a unit may not claim two discontiguous ranges in one section** (the `CModelDataModelSlots.cpp` failure), so main.cpp cannot keep the two pieces either side of a 0x50-byte hole. Splitting it and re-adding the range does not help either: the real cycle is four hops - `main.cpp` calls `fn_801449C8` (`CGameStateCtor.cpp`), which calls `fn_8000934C` at 0x80144C40, which is this unit, which calls `__dt__12CPlayerStateFv` at 0x8000939C, still inside main.cpp - and `dtk` reports it as `Cyclic dependency encountered while resolving link order: MetroidPrime/main.cpp -> MetroidPrime/Player/CPlayerStateRefRelease.cpp`. **The minimum carve-out that would work, measured off `symbols.txt`: `0x80008F40..0x8000934C` (11 functions, 0x40C bytes) and `0x8000939C..0x80009880` (13 functions, 0x4E4 bytes) must both leave main.cpp, as two new units, before an 80-byte win can be claimed** - and the tail reaches back into main.cpp through `ReleaseData__Q24rstl34rc_ptr<24IArchitectureMessageParm>Fv` at 0x80008F40, which is why moving only `__dt__12CPlayerStateFv` is not enough. Not the port's blocker; `fn_80009898` is. |
| `CResFactory::CResFactory()` = `fn_802FB154` (DOL, not a module) | **Body written, 93.86%, 2026-09-26 (lane `pool`), `NonMatching`, and it *fits*.** 0x802FB154, 0xA8 = 168 bytes, `src/Kyoto/CResFactoryCtor.cpp`; `tools/unit_fit.sh` reports 168 claimed / 168 ours / 168 retail and no extra functions, so it is one instruction placement from a `flip_test`. **This file used to be the port-only home of `fn_803096C4` and called *that* `CResFactory::CResFactory()` - a mis-attribution three sessions had recorded as settled, and the fix is a move, not a rewrite: `fn_803096C4` is the constructor of the four bytes at `CGameGlobalObjects`+0x00 (a member that holds nothing but a vptr and whose body is a one-shot `CARDInit`) and is now `src/MetroidPrime/CGameGlobalObjectsPad0Ctor.cpp`.** Three measurements here are worth more than the 168 bytes. (1) **mwcceppc decides small-data addressing from the declared _size_ of a global**, so a retail `.data` operand comes out as `lwz r0,0(r13)` / `R_PPC_EMB_SDA21` unless the symbol is declared as a *sized array* (`extern "C" char lbl_803B19B8[0x20];` gets retail's `lis`+`addi`). This is the same fact as the `const` rule in the other direction, and it applies to every constructor that stores a vtable. (2) **A constructor's dead `mr r3,r31` is the return-value copy and it is load-bearing**: retail's epilogue has no `mr r3,r31` and MWCC puts the copy in the *middle* of the body, so "the epilogue has none, therefore the function returns void" is the wrong inference - declared `void` this is 91.36% with r6/r5/r4/r3 where retail has r7/r6/r5/r4, and declared `CResFactory*` with `return self;` it is 93.86% with retail's registers. (3) **The two vtable stores must be data operands, not a derived class**: deriving them makes the object emit `__vt__8IFactory` (0x20 bytes at 0x803B19B8), `__vt__11CResFactory` (0x20 at 0x803BAF08) and a weak `__dt__8IFactoryFv` (0x48 at 0x802FB0E0, which is retail's `fn_802FB0E0`), none of whose ranges is claimed - which is the same wall `paks.md` records for `~CResFactory`. Left: eight instructions, all one list-schedule priority (`li r6,0` and `addi r0,r31,212` want to be hoisted above the two volatile byte loads and are not); eleven orderings measured, this is the best. Port: **excluded from `files.cmake`, net +4** (326 -> 330), so the port's `CResFactory::CResFactory()` is the default one in `CResFactoryPortVirtuals.cpp` - which is still a net improvement, because the old body called `CARDInit` on a `CResFactory`. |
| `CSimplePool::CSimplePool(IFactory&)` = `fn_80301008` (DOL, not a module) | **Body written, 94.32%, 2026-09-26 (lane `pool`), `NonMatching`, and it *fits*.** 0x80301008, 0x150 = 336 bytes, `src/Kyoto/CSimplePoolCtor.cpp`; `unit_fit.sh` reports 336/336/336 and no extra functions. The second of the two constructors on the port's frame-0 critical path, and the nearest unwritten body on it. Two structural findings, both worth stealing. (1) **The class is not modelled the way `Kyoto/CSimplePool.hpp` models it, and the header is left alone**: the 8-byte object `operator new(8)` returns has a vtable *and* a `CSimplePool*`, and it is what `+0x1C` points at, while the header calls `+0x1C` an `rstl::rc_ptr< CVParamTransfer >` and `CVParamTransfer` is `{ rstl::rc_ptr< IVParamObj > }` - 8 bytes and no vtable of its own. The body carries a two-word overlay instead, so this is **not** a header change and has no blast radius; a lane that identifies the 8-byte object can fix the member's type properly. (2) **`stb r6,12(r1)` is the `.sbss` byte going into a second uninitialised frame slot out of the same register**, so the value is a named local and the global is read once - the `CPersistentOptionsCtor` spelling again, extended from two `volatile bool`s to one 8-byte `volatile u32` read as two bytes four apart plus a byte store through it. Also measured: **`tmpEntry.x0_ptr` must be stored *before* the second `operator new` call**, or the pointer lives in r30 across it and the function spends `stw r30,40(r1)` / `lwz r30,40(r1)` on a register retail has no room for (13 fewer differing instructions with the store first, 0 with it last - the `uint*` alias for the member's `rc_ptr` is worth 8 on its own). Left: five instructions, one list-schedule priority; twenty-two orderings measured. Port: **excluded from `files.cmake`, net +8** (326 -> 334) and closing nothing, because `CSimplePool.hpp` declares the constructor inline and `main.cpp` already emits it. The port's actual `CSimplePool` gap is `vtable for CSimplePool`, which needs `~CSimplePool` (the key function) and the ten virtuals. |
| `fn_8000934C` = `rstl::rc_ptr<CPlayerState>::ReleaseData` (DOL, not a module) | **Written and **correct, and BLOCKED on `dtk`'s link-order cycle - 2026-09-26 (lane `cal3`).** 0x8000934C, 0x50 = 80 bytes, `rstl::rc_ptr<CPlayerState>::ReleaseData()` on the 8-byte `{ptr, refcount}` pair the constructor builds four of. The source is in `src/MetroidPrime/Player/CPlayerStateRefRelease.cpp` and is **deliberately not registered** in `configure.py`, because the range cannot be claimed. 0x8000934C is inside `MetroidPrime/main.cpp`'s `.text 0x800053B8..0x80009880`, and **a unit may not claim two discontiguous ranges in one section** (the `CModelDataModelSlots.cpp` failure), so main.cpp cannot keep the two pieces either side of a 0x50-byte hole. **SUPERSEDED 2026-09-26 (lane `v4`), and the correction is worth more than the row: there is no link-order cycle, and the carve-out works.** The four-hop cycle above does not exist. Measured, by doing it: adding the split on its own fails *earlier and differently* - `Split 3:0x8000934C..3:0x8000939C overlaps with previous split`, dtk refusing the claim outright because main.cpp already has it, so link order is never reached. Doing the **full** carve-out the row itself describes - `main.cpp` cut into `0x800053B8..0x80008F40` + `mainMid.cpp` at `0x80008F40..0x8000934C` + `mainTail.cpp` at `0x8000939C..0x80009880` with `.ctors` and `.sbss` moved to `mainTail` - plus this unit `Matching` at `0x8000934C..0x8000939C` **builds and links cleanly**: `dtk dol split` succeeds, the DOL sha1 stays `6ef9b491...`, all 86 RELs are byte-identical, and `fn_8000934C` reports **100.00% in a `Matching` unit**, `flip_test` PASS. The reason it is still not registered is the price, which no earlier note had measured: **the two carve-out units have no source, so objdiff pairs nothing in them and `matched` goes 3195 -> 3187 against `linked` 1811 -> 1812 - eight functions for one**, four of the eight having been at 100.00% inside main.cpp (`__dt__12CPlayerStateFv`, `__dt__Q212CPlayerState16SPersistentStateFv`, `__dt__Q24rstl81vector<Q312CPlayerState16SPersistentState10SScanState,...>Fv`, `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv`). That is the same failure `HANDOFF.md` records for the `CGameGlobalObjects` constructor experiment (`__dt__24CGameArchitectureSupportFv` 95.27% -> 0.00%), eight times the size. And the port does not want it either: `tools/link_check.sh` **326 -> 327**, listing this file opens `__dt__12CPlayerStateFv` and closes nothing, 0 compile errors and 0 duplicate definitions both ways. So the re-split is available to any lane that wants it and is declined on the numbers, not on an impossibility; the minimum carve-out is still `0x80008F40..0x8000934C` (11 functions, 0x40C) **and** `0x8000939C..0x80009880` (13 functions, 0x4E4), both leaving main.cpp, which is 24 functions of main.cpp re-homed for an 80-byte win. `fn_80009898`, the row's "not the port's blocker" note, has since landed (`Matching`); the remaining blocker on that path is `CGameGlobalObjects`'s constructor, which is a stub. |

**Superseded, 2026-09-25:** an earlier version of this table concluded that no module had been
decompiled and that the route was gated on the DOL hierarchy. Both halves were wrong in an
important way. `ScriptRiftPortal`, `Metaree` and `AIMannedTurret` now link our own C++ and hash to
what `config.yml` records, using the split described under "The recipe" - a module can be
*partly* decompiled and still correct, which is what makes the route viable before the actor
hierarchy exists. The hierarchy still gates the *behavioural* functions (see that section), but
accessors, predicates, loaders and setup can be taken now.

Current module status:

| module | our code in the link | notes |
| `AIMannedTurret` | 3 functions (`fn_1_0`, `fn_1_8`, `fn_1_10`, all `extern "C"`) | unit `Matching`, sha1 `949b8c21…` verified; the first module whose unit flips - see "Declare in reverse" |
| `ScriptRiftPortal` | 3 functions (`SetFuncPtrs`, `RELMain`, `RELExit`) | first with a three-way split; sha1 `a0fa6c69…` verified against config.yml |
| `Metaree` | 23 named functions exact (18 ours + 5 setup), of 59 total; the rest unclaimed | first creature-family module; ranges unclaimed rather than named |
| `CScriptCannonBall` | 12 of 26 matched, unit still `NonMatching` | was blocked on `UnkVtable20`, which is resolved; the link now fails on `__ct__6CActorF...` instead |
| `CScriptForgottenObject` | 9 of 12 at 95.86%, unit still `NonMatching` | .text/.rodata/.data a few bytes off |
| `ForgottenObject` (the unit; see also the module table) | **not promoted, 95.86% -> 97.53% fuzzy**, and 55 bytes from retail in 19 runs. `.text` and `.data` now fit exactly and `.bss` always did; `.rodata` short 5 is harmless (mwldeppc pads). The remaining 55 bytes are pure register allocation in 3 functions - 13 in `LoadForgottenObject`, 28 in `RenderInternal`, 14 in `__ct__` - and all three are the entry-block load-hoisting and register-choice walls described above, so the unit is *not* one edit away. A second, non-source blocker also applies: the module defines `fn_24_1E4`, which nothing calls, and mwldeppc dead-strips it - see "A REL unit that defines a function nothing calls cannot be flipped". **Worth a follow-up lane only after that rig fix lands** |
| `MetroidPrime/Player/CPlayerState.cpp` (DOL unit, not a module) | **not promoted, and the blocker is one instruction.** 69/72 at 100%, 99.80% fuzzy. The flip is blocked by a single 4-byte unconditional `b` in `InitializeScanTimes` (0xe0 against retail's 0xe4), which is unreachable by source: see "MWCC rotates a loop only when it cannot count it". `unit_fit.sh` blamed 6 extra functions (532 B) and a `.sbss` shortfall; all six are harmless (five `WEAK`, one `LOCAL` that mwldeppc drops) and the tool could not see the real 4-byte *deficit*. The other two functions, `ShouldDrawGravityBoost` and `GetActiveVisor`, are single-instruction scheduling walls: our build hoists one `lwz` one or two prologue slots earlier than retail, and 20 variants each did not move it |
| `ScriptCoin` | 6 functions in 3 `Matching` units (`CScriptCoinRel` 4, `CScriptCoin` 1, `CScriptCoinTouchBounds` 1) | module hash held, +6 linked; six units because a unit may claim only one contiguous range, and the tail named after the module rather than `REL/REL_Setup.cpp` |
| `ScriptGui` | 3 functions (`SetFuncPtrs`, `RELMain`, `RELExit`) + a 5-entry loader table | sha1 `2b58f6d3…` verified; widget bodies blocked, see below |
| `ScriptPlayerProxy` | 9 functions (loader registration, `RELMain`/`RELExit`, an unnamed setup function, 1 accessor) + 5 setup | sha1 `19ea68a377b4908848b9d640245842526a8dd968` verified; 48 class functions unclaimed |
| `ScriptRsfAudio` | 8 functions (loader registration, `RELMain`/`RELExit`) + 5 setup | sha1 `af0941ce5e81230282eda9cfb59e1839dc45443a` verified; 14 class functions unclaimed |
| `ScriptStreamedMovie` | 6 functions: a local `CModelData` constructor wrapper (`__ct__10CModelDataFv`, which forwards to the DOL's routine) + `RELMain`/`RELExit` and the 3 setup functions | sha1 `d9b45eae…` verified; the rest of the module stays retail |
| `RubiksPuzzle` | 6 functions: `SLdrRubiksPuzzleData::SLdrRubiksPuzzleData()` (state machine `0xFFFFFFFF`, rotation speed from `.rodata`) + `RELMain`/`RELExit` and the 3 setup functions | sha1 `a29343f9…` verified; the rest of the module stays retail. The lane checked the base classes exist before starting, which is why this one was writable |
| `SkyRipple` | 7 exact of 15 named + fuzzy loader/constructor | unit kept `NonMatching` on purpose - promoting it would break the module |
| `Puffer` | 9 functions (6 + 3 in two named units) | sha1 `ab46667b…` verified |
| `CPakFile` | 0 of 33 in the link - **not** a module | DOL unit, not REL: 24/33 at 100% after 2026-09-25, still `NonMatching`, `.text` 1904 bytes over its claimed range. **Re-measured 2026-09-26 (lane g1), unchanged and now with the blockers named**: the two functions the pak chain needs, `__ct__8CPakFileF...` (0x8032458C, 0xEC) and `__dt__8CPakFileFv` (0x80324494, 0xF8), are **already 100%**, and so is `AsyncIdle__8CPakFileFv`. What blocks the unit is `reserve<rstl::vector<CPakFile::SResInfo>>` at **33.84%**, which cannot be written without editing `include/rstl/rmemory_allocator.hpp` (its `allocate` is out of line and uses `rs_new`, where retail inlines `CMemory::Alloc` with a `CCallStack`), and `RebuildResourceLists` at **39.63%**, which calls an unnamed `fn_80052220` where the port calls `reserve<rstl::vector<uint>>`. Both are outside the files a pak lane may edit, so the unit is **blocked on `include/rstl/`**, not on decompilation. |
| `Kyoto/CResLoaderPakPump.cpp` | **2 functions, 100.00%, unit `Matching`** | `AreAllPaksLoaded` (0x802FCCE4) and `AsyncIdlePakLoading` (0x802FCCF4), 172 bytes, `flip_test.sh` PASS with the DOL sha1 held. Required renaming two unnamed DOL symbols and correcting `include/Kyoto/CResLoader.hpp` (`CResLoader` is 0x60, four `rstl::list<SPakLoadEntry>`); see `docs/research/paks.md` and the register-allocation section above |
| `Kyoto/CResLoaderInsert.cpp` | **2 functions, 100.00%, unit `Matching`**, lane `k4` 2026-09-26 | `fn_802FC350` (0x802FC350, 0x28) and `fn_802FC378` (0x802FC378, 0xA8), 208 bytes, `flip_test.sh` PASS. **The insert every pak load goes through**, and the long-blocking item in this table. Neither body is a transcription: `fn_802FC378` is `rstl::list< SPakLoadEntry >::do_insert_before(node*, const SPakLoadEntry&)` called through the list's public members, and its 0xA8 bytes are identical, register for register, to retail's own named `do_insert_before<list<auto_ptr<CFilePreloadData>>>` at 0x803445DC. Three findings: **(a)** `SPakLoadEntry` is `rstl::auto_ptr< CPakFile >`, and the `stb r0,0(r30)` that clears the caller's flag byte is that class's auto-relinquishing copy constructor - the insert has no such statement; **(b)** the `addic. r5,r3,8` / `beq` guard is `rstl::construct`'s placement `new` and it is correct - the fix is the *element*, not the guard (see `rstl/construct.hpp`); **(c)** `#pragma inline_max_size` must be **large** (125 works, 0 does not) or mwcceppc emits `do_insert_before` as a separate COMDAT and `fn_802FC378` becomes a 0x20-byte forwarder. `fn_802FC378` declared *before* `fn_802FC350` (reverse emission order). Deleted the port's transcriptions from `PortGlobals.cpp`, so the port now links the real 64-bit `rstl::list`. Full write-up: `docs/research/paks.md`, "The pak insert, landed" |
| `Kyoto/CResLoaderResAccessors.cpp` | **5 functions, 100.00%, unit `Matching`**, lane `k4` 2026-09-26 | `fn_802FCAE8`, `fn_802FCB40`, `fn_802FCB88`, `fn_802FCBD0`, `fn_802FCC00` over `.text 0x802FCAE8..0x802FCC44`, 348 bytes, `flip_test.sh` PASS. Each is `fn_802FCDE8(this, id)` and then, only if that returned non-null, a call on `this->x68_curRes`. **That fixes two of `CResLoader`'s four unnamed words**: `fn_802FCF98` writes `x64_ = id` and `x68_ = CPakFile::SResInfo*` on success (`stw r31,100(r30)` / `stw r3,104(r30)` at 0x802fcfd0/0x802fcfd4), and each of the five reads only `+0x68` - so the header now types them `CAssetId x64_curId` and `CPakFile::SResInfo* x68_curRes`, and `Kyoto/CPakFile.hpp` is included there (no cycle). Two more: `fn_802FCC00` is the only one that does not read `4(r4)`, which is what makes its parameter a bare `CAssetId` (`GetResourceTypeById`) rather than a `const SObjectTag&`; and `fn_802FCAE8` needs `IsCompressed() ? 1 : 0` and not `IsCompressed()`, which is 12 bytes and the difference between a 0x4C and retail's 0x58 function. `extern "C"`, not members, because **21 dtk objects call them by their dtk names**. The unit needs a `TARGET_PC` `fn_802FCDE8` or it adds a link-gap symbol instead of closing one |
| `Kyoto/CResLoaderFindPak.cpp` | **1 function, 100.00%, unit `Matching`**, lane `k4` 2026-09-26 | `fn_802FCEEC` (0x802FCEEC, 0x24 = 36 bytes, six instructions), `flip_test.sh` PASS. `fn_802FCDE8` with `tag.id` hoisted into r4 and the result passed through - no `stw r31` / `mr r31,r3`, which proves `this` is never live across the call. Its own unit only because 0x802FCC44..0x802FCEEC is `fn_802FCDE8`'s 0x104 bytes and a unit may not claim two discontiguous ranges |
| `Kyoto/CResLoaderLoadPartAsync.cpp`, `CResLoaderLoadAsync.cpp`, `CResLoaderGetResIdByName.cpp`, `CResLoaderLoadResourceSync.cpp` | **4 functions, 100.00% each, 4 units `Matching`**, lane `m2` 2026-09-26 | `fn_802FC81C` (0x802FC81C, 0x7C), `fn_802FCA68` (0x802FCA68, 0x80), `fn_802FCC44` (0x802FCC44, 0xA0) and `fn_802FC420` (0x802FC420, 0xB8) - 412 bytes, four `flip_test.sh` PASSes, DOL sha1 held. Four units, not one: the four ranges are not contiguous, and `fn_802FC898` sits between the first and the second. **Four codegen rules came out of it and all four are general.** (a) **Declare the return type MWCC can see is dead** - `fn_802FC420` returns `void`; as `void*` it emits a trailing `li r3,0` and is eight bytes long. This is the same polarity rule as `fn_802FCAE8`'s `IsCompressed() ? 1 : 0`. (b) **A named local decides the evaluation order of an expression's callees** - `AsyncSeekRead(buf, (res->GetSize() + 31) & ~31, kSO_Set, res->GetOffset())` inlines both accessors in the *wrong* order and puts the `+31`/`clrrwi` in the wrong place (87.03%); `const uint size = res->GetSize();` first gives retail's exact order and 100.00%. (c) **A temporary passed straight into a call is not the same as a named local** - `CMemory::Alloc(n, h, s, t, CCallStack(-1, lbl_803AFAA0))` gives retail's `addi r3,r1,8` / `bl <ctor>` / `mr r7,r3`; naming the `CCallStack` gives `addi r7,r1,8` and 91.70%, because retail's 12-byte ctor leaves r3 alone and the reference *is* r3. (d) **`CCallStack`'s third parameter is private, and its *default* is retail's symbol** - `kUnknownType__10CCallStack` at 0x803AEAB8, which the characterisation in `docs/research/paks.md` had at the wrong address (0x803AE558 is `lbl_803AE558`). The two-argument call is the only way to reach it. Full write-up and the four corrections to the previous lane's table: `docs/research/paks.md` |
| `Kyoto/CResLoaderLoadResourceSyncCompressed.cpp` / `CResLoaderLoadNewResourceSync.cpp` | **2 functions at 99.10% and 98.04%, both `NonMatching` on purpose, ranges claimed so retail's bytes stay in the link**, lane `m2` 2026-09-26 | `fn_802FC4D8` (0x164) and `fn_802FC63C` (0x1E0) - the compressed loaders. **Every instruction is right; the whole of the residual is which register MWCC hands the compressed arm's four temporaries** - retail uses r7/r6 and r30/r29 where this build uses r6/r7 and r29/r30 (20 instructions differ), and r28/r29/r30 against r27/r28/r29 in the other (16). **About forty body shapes did not move it**, so this is a register-allocator preference, not a source-shape problem. What *was* settled and is worth having: the four-byte decompressed-size prefix is read through `CInputStream`'s private `x8_ptr` and the cursor advanced, identically in both functions; **the compressed length is `GetSize() - GetReadPosition()`, not arithmetic on `SResInfo`** - the `+4`/`+8` loads the earlier characterisation read as `SResInfo` words are `CInputStream::x4_buffer` and `x8_ptr`; the teardown calls `~CInputStream(1)`, the *deleting* destructor, at **vtable slot +8** (confirmed against `__dt__15CMemoryInStreamFv`'s own `extsh. r0,r31 ; ble` before `CMemory::Free`); and `fn_802FC63C`'s `CMemoryInStream` takes `EOwnerShip` = `callerBuf != nullptr`. Two build-level traps: **a `friend` declaration inside a class is the *first* declaration of the function if nothing precedes it**, so it gets C++ linkage and then conflicts with the `extern "C"` elsewhere - the pair has to be repeated at namespace scope above the class (mwcceppc rejects `friend extern "C"`); and a `Matching` unit's `new (lbl, nullptr)` does not compile on the host, which has no three-argument `operator new`. Write-up: `docs/research/paks.md` |
| `fn_802FC898` (DOL unit, not written) | **Blocked, and the blocker is four unnamed functions rather than one**, lane `m2` 2026-09-26 | 0x1D0 = 464 bytes, the grouped-resource path. It needs `fn_802FB994` (0x802FB994, **0x88**), `fn_803434E8` (0x803434E8, **0x54**), `fn_8034353C` (0x8034353C, **0x9C**, which materialises a `CCallStack` string at **0x803B0340**) and `fn_803433D4` (0x803433D4, **0xA0**) - all four unclaimed, none referenced by anything but this function. `fn_803434E8` is a dependency-group range test reading `+8`/`+0xC`/`+0x10` of its node, and `src/Kyoto/CDependencyGroup.cpp` claims a *different* range (0x803208F0..0x80320FD8), so the class is modelled and the method is not. **And `fn_802FB994` contradicts the header**: it reads a byte at `node + 0x1E` and passes `node + 8` to the range test, so the fourth list's element is **0x18 bytes, not 8** - `rstl::list< SPakLoadEntry > x0_aramList` in `include/Kyoto/CResLoader.hpp:141` is the wrong element type. The four lists are still 0x18 apart and the 0x70 size is unaffected |
| `WallCrawler` | 18 functions | verified; no `LoadWallCrawler` or Think to attach to yet |
| `CMainFlowOnMessage` (DOL unit, not a module) | **`OnMessage` 100.00%, unit `Matching`; 4 units, 5 functions, 372 bytes** | Retail `fn_8001DF54`, `0x8001DF54`, `0xB4` = 180 bytes - the function `vtable for CMainFlow`'s slot 2 points at, and the last named hole `docs/research/boot_probe.md` found in the frame loop. Three blockers, all closed, all `flip_test` PASS: **`GetParm()`/`GetParm() const`** (`fn_80048CEC`/`fn_80048CE4`, 8 B each, unnamed until `symbols.txt` was renamed) as their own unit with `inline_max_size(0)` on the declarations; **`~CFrameMsgParm` and `~CTimerMsgParm`** (92 B each) in their own units, each carrying its class's vtable, which is what lets the destructor be the key function and so be called out of line; and **the parm classes promoted out of `main.cpp`'s anonymous namespace** into `include/MetroidPrime/CArchitectureMessageParm.hpp`, so their vtable symbols are nameable at all. Two measurements worth keeping: **the local is a copy of the message's parm, not a fresh one from its int** - written as an int the unit is 86.33% and mwcceppc hoists the `lwz` and reuses r3; written as `CFrameMsgParm parm(*static_cast<const CFrameMsgParm*>(msg.GetParm()))` it is the copy constructor expanded in place and 100.00%, the only one of five `try_batch` spellings at zero differing instructions. And **`kMR_Normal` must be one `return` after the `switch`, not one per arm**, or mwcceppc emits `li r3,0` twice (84.00%). The port's link gap moved 202 -> 203: `OnMessage` closed, `AdvanceGameState` and `SetGameState` (1,012 bytes, now named) opened |
| `CFrameMsgParmDtor` / `CTimerMsgParmDtor` (DOL units) | **2 x 92 bytes at 100.00%, both `Matching`** | Retail `fn_800487B8` and `fn_80048834`. Retail's four parm destructors are all 0x5C and differ only in which vtable they store. **The vtable is 0xC, not 0x10**: MWCC's is two zero header words plus **one** slot for the destructor, so the trailing zero at `0x803B1B6C` is the next symbol's and claiming 0x10 makes dtk refuse the split. **The base's destructor must stay inline and empty, not pure**: mwcceppc expands it and all that survives is the base-vptr store, and because the expansion needs no call nothing clobbers r4, the deleting flag stays in r4 and `this` alone gets r31 - one saved register, which is the shape. Make it pure and you get `li r4,0 ; bl __dt__24IArchitectureMessageParmFv` where retail has the store, 0x60 bytes, and 96.33% |
| `MetroidPrime/Player/CGameStateSlotsCtor.cpp` (DOL unit) | **2 functions, 164 bytes, 100.00%, unit `Matching`**, lane `m1` 2026-09-26 | `fn_80144924` (0x80144924, 0x38) and `fn_8014495C` (0x8014495C, 0x6C) over `.text 0x80144924..0x801449C8`, `flip_test.sh` PASS, `compare_unit.sh` "sections identical". The `(count, element)` fill constructor of `CGameState`'s two `{count; block[3]}` members at +0x110 and +0x144. **Three spellings had to be exact and none is the obvious one**, all found with a variant sweep: the counter is an `int` (`uint` emits `cmplw` against retail's signed `cmpw`); the element pointer is a **separate variable stepped in the increment clause**, `for (int i = 0; i < n; i++, p++)`, because `&elems[i]` puts `addi r31,r31,16` *before* `addi r30,r30,1` and retail has them the other way round; and `fn_80144924` must **return `this`**, which is a constructor's implicit `return this` and is the only way to get retail's `mr r3,r31 ; lwz r31,12(r1)` epilogue. Declared **descending** (`fn_8014495C` first). The two member types it needed are new: `SGameStateBlock` (16 bytes) and `SGameStateSlots` (0x34) in `include/MetroidPrime/Player/CGameStateBlocks.hpp` |
| `MetroidPrime/Player/CGameStateBlockCopy.cpp` (DOL unit) | **1 function, 32 bytes, 100.00%, unit `Matching`**, lane `m1` 2026-09-26 | `fn_80142A10` (0x80142A10, 0x20), `flip_test.sh` PASS. Eight instructions: a 16-byte frame and a tail call to `fn_80004D5C`, which is the 16-byte block's own copy (`if (this) fn_80004AA0(this, src)`). **Its own unit only because 0x80142A10 is 0x848 bytes from the 164 contiguous bytes above**, and a `configure.py` unit may claim only one range - `docs/research/cgamestate_layout.md` §5 originally listed all three as one 164-byte range, and that was wrong |
| `MetroidPrime/Player/CGameStateStreamCtor.cpp` (DOL unit) | **1 function, `fn_80144140`, 24.33%, `NonMatching`** (claiming 0x80144140..0x801447C4 so objdiff measures it), lane `m1` 2026-09-26 | `CGameState::CGameState(CInputStream&, int)`, 0x684 = 1,668 bytes, the **only** writer of `gpGameState` on the boot path. Written from the prologue through the +0x204 member's constructor (0x80144140..0x801442E0) and left there. **The finding that matters is that the doc's stated blocker was the wrong kind of blocker**: it compiles, links and scores with *no callee written*, because a `Matching` unit needs only relocations - the expensive things are the merged `.rodata` object (`lbl_803A9208`, referenced **nine** times, not five, and the two `CBasics::Stringize` arguments are `lbl_803A9208 + 60` and `+ 134`), the two named `.sdata2` constants (`lbl_8041C1A8` = 359999.0, `lbl_8041C1B8` = 100.0f), and member *types*. All **42** member offsets and sizes are now in `include/MetroidPrime/Player/CGameState.hpp` and are re-measured with mwcceppc's flags by `tools/probe_gs_offsets.py`, which is a new step in `tools/gate.sh` - a header edit that moves a member now fails the gate instead of quietly costing a percentage two sessions later. Three corrections to `docs/research/cgamestate_layout.md` are recorded in place: the virtual dispatch is through **`gpSimplePool`** (0x80418EA8), not a "gpTweakGame at 0x80418EF0" - the displacement arithmetic was 0x48 out; the second loop's invariant is **`gpMemoryCard`**, not `gpGameState`; and a **seventh** blocker exists that the list of six did not have, a 22-instruction `memset` at 0x801444E0 whose length is read out of an uninitialised stack word |
| `MetroidPrime/Player/CGameStateCardOptsCtor.cpp` + `CPersistentOptionsCtor.cpp` (DOL units) | **2 functions, 180 bytes, 100.00% each, both units `Matching`**, lane `cal1` 2026-09-26 | `fn_80145950` (0x80145950, 0x5C) and `fn_80146154` (0x80146154, 0x58), the constructors of `CGameState`'s two 0x2C blocks at +0x54 and +0xDC, `flip_test.sh` PASS on both, DOL sha1 and all 86 RELs unchanged, `matched` 3187 -> 3189 and `linked` 1803 -> 1805. **The second one overturns a claim three files in this tree made**, that `fn_80146154` "cannot be written as source" because it `lbz`es two bytes of "its own outgoing parameter save area". The mechanism was wrong: with `stwu r1,-32(r1)`, LR saved at 36(r1) and r31 at 28(r1), offsets 8(r1) and 12(r1) are the frame's **own local area**, not the caller's - so what is needed is not "garbage passed in" but "two uninitialised bytes of my own frame", and that is expressible. The measurements, ranked by differing instructions with `tools/try_batch.py`: two `volatile bool` locals give 8(r1) and 9(r1), 2 instructions out (a plain uninitialised `bool` is folded away entirely into `stb r3,4(r3)`); two `volatile u32` slots read through a `volatile u8*` give 12(r1) and 8(r1), the two slots the wrong way round, also 2 out; a padded struct of two `volatile bool`s, the same; **one 8-byte `volatile` local read as two bytes four apart is 0 out** and reproduces all 22 instructions including mwcceppc's choice of r5 for the hoisted first load and r4 for the second. `volatile` is load-bearing: without it there is no stack slot at all. The trap in the experiment is that a variants file which assigns a member in both the body and a shared tail double-stores it, and the diff then blames the offsets. `fn_80145950`'s own body is `fn_80146154(this, 0)`, four zero words, and `if (gpMemoryCard) fn_80145628(this)` - the header documented only the first two thirds. |
| `MetroidPrime/Player/CGameStateBlockDtor.cpp` (DOL unit) | **1 function, 84 bytes, 100.00%, unit `Matching`**, lane `cal2` 2026-09-26 | `fn_80004A4C` over `.text 0x80004A4C..0x80004AA0`, `flip_test.sh` PASS, `unit_fit.sh` "fits / no extra functions", first try. A **deleting destructor** for the 16-byte `{u32, u32, u32, void*}` block `CGameStateBlocks.hpp` calls `SGameStateBlock`: `if (this) { CMemory::Free(x0c_data); if ((short)flag > 0) CMemory::Free(this); } return this;`. Ten instructions because `CMemory::Free` (0x802CE388) is null-safe - its own `cmplwi r31,0` at 0x802CE3A4 is why the null `x0c_data` needs no test. 22 callers in the DOL, two of them from `CGameState::CGameState()`. **The parameter is compared as `(short)`, not `int`**: retail sign-extends the halfword itself (`extsh. r0,r31 ; ble`). All 22 callers pass -1, so that free is dead for every one of them and only the emitted branch says the source has it - and the branch is corroborated by the twin, `__dt__80004B9C` (0x80004B9C, 0x50), which is this function instruction for instruction apart from its first call being `fn_80004BEC` instead of `CMemory::Free`. And because the only call is `CMemory::Free`, which takes its argument in `r3`, **the flag never has to be spilled and stays in `r4`** - the frame saves `r30` and `r31` only. Same shape as `~CIOWin` and `~CMainFlow`, whose "rest of destruction" *does* contain a call and so keeps the flag in `r31` and `this` in `r30`; the two are not interchangeable |
| `MetroidPrime/Player/CGameStateMemcardCtor.cpp` (DOL unit) | **1 function, 184 bytes, 100.00%, unit `Matching`**, lane `cal2` 2026-09-26 | `fn_80009DBC` over `.text 0x80009DBC..0x80009E74`, `flip_test.sh` PASS, `unit_fit.sh` "fits / no extra functions". The constructor of the 0xE8-byte `SGameStateMemcard` at `CGameState+0x204`. **The finding worth keeping is that the four `lbz` per loop iteration are decided by the missing `const` on the two globals it fills from, and nothing else.** Retail re-reads the same byte before each of its four byte stores because it cannot rule out that the buffer it is writing *is* that global; declared `const`, mwcceppc hoists the load out of the loop and emits one `lbz` before the `mtctr` - measured with `tools/try_batch.py`, 22 differing instructions against 0. **`const volatile` does not work either** (same hoisted load, same 22), so it is specifically the absent `const` that keeps the reload and not the reload itself. The two globals are `lbl_80417D90` and `lbl_80417D91` - `.sdata` one-byte objects, values 1 and 0 - found by resolving `lbz r0,-32752(r13)` against **`_SDA_BASE_` = 0x8041FD80** (`tools/sda.py`): 0x8041FD80 - 32752 = 0x80417D90. A `Matching` unit may reference such a global by name with `extern "C" unsigned char lbl_80417D90;`; `dtk` already defines it in `auto_09_80417D80_sdata.o`, so no unit has to claim `.sdata`. Also load-bearing: the destination pointer is **stepped**, `for (int i = 0; i < 19; i++, p += 4)`, never re-indexed, and `fn_80009DBC` must **return `this`** - its own last act is `fn_80009898(self)` and the epilogue's `mr r3,r31` comes *after* the `bl`, so the callee's result is discarded. A nested `for (j = 0; j < 4; j++) p[j] = k;` is byte-exact too (mwcceppc unrolls the inner); a loop over a byte count is not. **`fn_80009898` is called, never defined, and is not harmless**: its inner `fn_800098CC` stores 72 at `+0x50` and fills `+0x54..+0x9B` from a third byte at `lbl_80417D8D`, so the 76 this constructor writes there does not survive its own last call. `include/MetroidPrime/Player/CGameState.hpp` now records that, and its two ends - `xA0` = 0 and `xE4` = the flag - are named members where the header had said "no function in the DOL writes it" |
| `CGameGlobalObjects` integration: `CInGameTweakManagerCtor.cpp`, `CGameGlobalObjectsTailCtor.cpp`, `Factories/CCharacterFactoryBuilder.cpp` (DOL units) | **Two `Matching`, one written and `NonMatching`, and the integration measured and not landed** - lane `frame`, 2026-09-26. `fn_8016C230` (0x8016C230, 0x14, `CInGameTweakManager`'s constructor) and `fn_801F0A44` (0x801F0A44, 0x30, the +0x150 member's) are 100.00% with `flip_test.sh` PASS; the second is the `volatile u32 w[2]` uninitialised-frame-byte spelling from `CPersistentOptionsCtor.cpp`, first try. `CCharacterFactoryBuilder` (0x80031E60..0x80032230) is 8 of 10 functions at 100% (80.33%): the constructor was renamed in `symbols.txt` (`fn_80032008` -> `__ct__24CCharacterFactoryBuilderFv`) with seven siblings so objdiff pairs them, and `CGameGlobalObjects.hpp`'s 0x28-byte stand-in became the real class. It cannot flip: it emits `CDummyFactory`'s vtable into unclaimed `.data`, and `fn_80031F68` inside its range is referenced by name from another lane's unit. **Two findings.** (a) `CDummyFactory::Build` returns `CFactoryFnReturn(CFactoryFnReturn(p))` - retail builds the result in a frame temporary and copies it into the return slot the way `rstl::auto_ptr` copies (owned flag loaded, not recomputed); `return CFactoryFnReturn(p)` is 43 instructions out, a named local 20, the double construction 0. (b) **On the host, g++ asks for `CCharacterFactory::~CCharacterFactory()` although nothing calls it by name**: `-O2` speculatively devirtualises the `delete` in `TObjOwnerDerivedFromIObj<CCharacterFactory>::~` and emits a guarded direct call. So a declared-only class with a virtual destructor still costs its destructor on the port. The integration itself: `docs/research/cgameglobalobjects_ctor.md`. |
| `SGameStateBlock`'s `rstl::vector<unsigned char>` operations: `CGameStateBlockCopyCtor.cpp`, `CGameStateBlockConstruct.cpp`, `CGameStateBlockClear.cpp`, `CGameStateBlockFill.cpp`, `CGameStateBlockReserve.cpp` (DOL units) | **Three `Matching`, two `NonMatching`** - lane `frame`, 2026-09-26. `fn_80004D5C` (null-guarded construct, 0x28), `fn_80142914` (clear, 0xC) and `fn_80142BA4` (fill, 0x154) are 100.00% with `flip_test.sh` PASS; `fn_80004AA0` (copy constructor, 0xFC) is 94.05% and `fn_801465EC` (reserve, 0x108) 91.44%. They are the tree's own `rstl/vector.hpp` bodies written out over `SGameStateBlock`, and they were written because `tools/boot_probe.sh` reached them inside `new CGameState`. The fill's loop has to form the element address before the store (`p = data + count++; *p = *src`): indexing `data[count++]` is 49 instructions out. `reserve` is left where retail keeps two iterator objects on the stack. **`tools/try_batch.py` cannot find a definition that starts `extern "C"`** on the same line (its regex has no `"`), so wrap such functions in an `extern "C" { }` block. |
| `SGameStateMemcardFill.cpp` (fix) | **98.30% back to 99.55%, and a host segfault removed** - lane `frame`, 2026-09-26. `reinterpret_cast<SMemcardA0*>(self->xa0_unk)` was written when the header's +0xA0 was a `u8` array; when the header made it `u32 xa0_unk`, the same cast became a cast of the count *value* to a pointer, on both compilers. objdiff showed a 1.25-point drop nobody chased; the port showed `new CGameState` segfaulting storing through it. `&self->xa0_unk` restores both. **A header change can silently rewrite a `reinterpret_cast` in a unit that still compiles.** |
| `CMain::RsMain` (0x80005C6C, 0x864 = 2148 B) via splitting `main.cpp` | **Split accepted, 0 gain, 2 regressions, and NOT collected** - lane `rsmain`, 2026-09-26. The `mainTail.cpp` recipe generalised: cuts at 0x800053B8-0x80005C6C / 0x80005C6C-0x800064D0 / 0x800064D0-0x8000848C, both new boundaries function boundaries that are **not** another unit's boundary, `dtk dol split` with no link-order cycle, **38 functions moved**. `RsMain` stayed `NonMatching` at 0.26% (`unit_fit`: claimed 2148, ours 8, **short by 2140**) and `CheckReset` (0x80006BA4, 0x49C) stayed at 0.47% in `mainMid`. `matched 3957` and `linked 2534` **both identical to baseline** - the port link is unchanged because `CMainRsMain.cpp` keeps `#ifndef TARGET_PC` and the host body is `PortBoot.cpp`. **Two moved functions regressed and it is not avoidable: `__ct__24CGameArchitectureSupport` 93.10% -> 87.99% and `AddPaksAndFactories` 57.15% -> 57.04%**, because mwcceppc's `@stringBase0` moved (the placement string `??(??)..` from 0 to 0x76) and two of seven references change shape. Both cut directions give 87.99%, and single-removal bisection needs the whole set, so it is not one function's placement. **Left uncollected on purpose** - see carve-vein rule 2c. The patch is preserved at `/tmp/lane-keepers/rsmain.patch`. **The real blocker is a header job, not the split:** `CMain`+0x18..+0x48 holds two 20-byte frame-time histories that `include/MetroidPrime/CMain.hpp` does not model (they sit inside `char x10_pad[0x38]` at line 137), and `fn_800069AC` - the bounded, insertion-sorted float push `RsMain` calls **six times** - is 308 bytes and unwritten. Writing a partial body *lowers* the score, because the empty 8-byte frame already matches retail's prologue exactly. |
| `Kyoto/Particles/CVectorElement` (DOL unit) | **Landed, 2026-09-28** - `Matching` 100.00% **92 / 92**, `flip_test` PASS, `main.dol` bit-identical (`6ef9b491...`), `matched` 8640 -> 8641, `linked` 3497 -> 3589, DOL units 7976 -> 7977. The one short function was `CVEKEYF::GetValue(int, CVector3f&) const` at 99.90%, and it was **two instructions in the wrong order** - the register assignment already agreed, only the emission order of two hoisted loads differed. One 9-line wrapper fixes it; the mechanism and the three sibling TUs that want the identical change are in the hoisted-load-order section below. **Superseded in part, 2026-09-28: of those three, `CRealElement` took the same change and landed; `CIntElement` should; `CColorElement` cannot - it is a register-allocation difference, not a hoist order.** Landed again three further times on later bases after `git reset`; `configure.py` was `NonMatching` and the source edit gone each time, everything else reproduced exactly. |
| `Kyoto/Particles/CRealElement` (DOL unit) | **Landed, 2026-09-28** - `Matching` 100.00% **151 / 151**, `flip_test` PASS, `main.dol` bit-identical (`6ef9b491...`), `matched` 8641 -> 8642, `linked` 3589 -> 3740, DOL units 7977 -> 7978. The one short function was `CREKEYF::GetValue(int, float&) const` (0x802F0854, 396 B) at 99.88%, and it was **two instructions in the wrong order** - the same defect and the same one-line fix as `CVectorElement` above, applied to this TU's own `*KEYF::GetValue` call site; the mechanism is in the hoisted-load-order section below. The emitter call site in the same file already matched and was left on the original helper, which is the point: retail disagrees with itself between the two callers. **This unit was diagnosed and verified twice before it landed, and both earlier runs' edits were lost to a `git reset` by the driver** - so the run had to re-measure, re-apply and re-verify from scratch; the third application reproduced the earlier numbers exactly (`bytescmp` 2 real diffs -> 0, 151/151, DOL sha1 held). When a `match` item comes back with a clean tree, treat its notes as the recipe and spend the time on re-measuring, not on re-diagnosing. |

### Two compiler facts this tree keeps rediscovering the hard way

**mwcceppc reserves r3 for `this` in a non-static member function.** `rstl/rc_ptr_copy` sat at
97.22% for a session on the belief that the body was wrong - *"MWCC allocates the AddRef r5/r4
where retail uses r4/r3"*. The body was right. Holding `this` in r3 pushes the first temporary
after it into r5, and the **same three statements** as a `static` member function get r4/r3, byte
for byte. The ABI is unchanged (r3 = dest, r4 = src) so no call site moved. This generalises to
every `rc_ptr` instantiation, and it took one unit from `NonMatching` to `Matching` to prove.

**mwcceppc 2.7 masks every `!` applied to a `bool`-typed operand.** `CErrorOutputWindow`'s
`cntlzw` is unreachable from source: `clrlwi r0,r31,24` is always emitted first, and that is what
causes the `srwi` that follows. Nine operand spellings and three destination types were measured.
When a comparison of a bool cannot be spelled two ways, it is a compiler version, not a puzzle.

**mwcceppc 2.7 also pairs apostrophes *and* backticks inside `//` comments.** An odd count on a
line is a compile error, and a dropped `//` prefix in a block comment does the same. Two hours went
into this in one lane; it is cheap to know.

### A `.data` range triggers the link-order cycle, not just a `.text` one

The rule everywhere in this file is *one discontiguous range per unit per section*. It is stated
for `.text` and reads as a `.text` rule. It is not: claiming
`.data 0x803B0D5C..0x803B0D68` for `__vt__15CMemoryInStream` fails
`dtk dol split` with the same link-order cycle, **before anything compiles**, so the error looks
unrelated to the section you touched. And that particular one is unfixable rather than merely
awkward: a vtable is emitted only by the key-function TU, so it cannot be moved to a carved unit at
all.

### `tools/offset_shift.py` - the tool that finds a layout bug by its uniformity

A class laid out wrongly by one constant makes every touching function land at 96-99% with a
**uniform operand delta**, and a codegen difference never produces that uniformity. It found
`CStateManager::pad2_2` (0x34, retail 0x2C) and `CAnimData::x120_unk` (0x58, retail 0x48), and
**nineteen functions went to 100.00%** on those two constants. It scans the DOL; a REL-side
extension is in flight.

**A negative result is a result.** Run over `src/Kyoto/Streams/`, `src/LZO/`, `src/rstl/`,
`src/Kyoto/Audio/`, `CActor` and the Enemies units, it printed nothing - and the reason is
instructive: *no function in those areas is between 90 and 100% at all*, because they are all
either exactly right or genuinely unwritten. The tool can only see a layout bug in a class that is
nearly matching, so "nothing found" is usually a statement about the areas, not about the tool.

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
`CColorElement.cpp` does not**, and the reason is measured, not guessed: its
`CCEKEYF::GetValue` (0x802cf66c, 392 B) is 96.408% with **28 differing instructions of 98**, and
only two of those are the hoist pair. The other 26 are a **one-slot register shift across the
whole inlined `GetKeyframeIndex` expansion** - retail puts the index in `r6`
(`and r6,r3,r0` / `cmpw r6,r5` / `subf r6,r4,r6` / `divw r0,r6,r3`) where ours has `r5` - plus the
`CColor::Lerp` argument block built in the opposite order (ours `slwi r4,r5,2` then `add r4,r6,r4`
then `slwi r0,r0,2`; retail `slwi r0,r0,2` then `slwi r5,r5,2` then `add r5,r6,r0`). **Retail calls
`CColor::Lerp` out of line here too** (`bl 48050f05`, and ours carries the matching undefined
`Lerp__6CColorFRC6CColorRC6CColorf`), so this is *not* an inlining difference. Note also that this
TU hoists the pair into `r4`/`r5` rather than `r4`/`r6` as `CRealElement` does, which is why the
wrapper cannot reach it: the register assignment itself has to move first. Generalises to any unit
whose near-miss is a pure instruction-order difference inside a hoisted argument group: read the
diff for whether the *values* are already right, because if they are, the lever is the call's
argument spelling and not the body.

### Ruled out here (each changed the register allocation or made it worse)

- spelling the index computation inline instead of calling the helper - the loads stop being hoisted
  at all, 25 differing instructions;
- hoisting `mLoopStart` / `mLoopEnd` into `const int` locals, in either definition order;
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
./tools/probe_sources.sh  ->  741 files, 0 failed; LINKED (313 undefined, 0 duplicates)
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
./tools/probe_sources.sh  ->  741 files, 0 failed; LINKED (313 undefined, 0 duplicates)
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
