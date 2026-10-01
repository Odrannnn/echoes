
## Lane 1: passed, then failed on the moved tip (2026-10-01 20:27:31Z)

The judged change failed goal_check.sh (exit 1) once rebased onto ecde8b308286; re-do it against the current tip.

## Lane 1, second attempt on ecde8b308286: PASS (2026-10-01)

`./tools/goal_check.sh build/goal/item.json` exits 0 on this tip. The change is the previous
attempt's, re-applied from `build/goal/rebase.patch`, which `git apply --check` accepts unchanged;
`git log 366622b..HEAD -- files.cmake src/MetroidPrime/PortReachStubs.cpp
src/MetroidPrime/Player/CGameState.cpp include/MetroidPrime/Player/SPersistentOptionsValue.hpp
docs/research/port_link_gap.md docs/research/port_link_gap_list.md` is **empty**, so nothing about
the code had to be re-derived.

### Why the re-judge failed on ecde8b3 - the baseline, not the change

The only failing line in `build/goal/run.log` is the one the previous lane's notes predicted:

    verify: the boot baseline is for 366622b, the branch head is ecde8b3 - BOOT_PROGRESS FAIL

`tools/goal_verify/boot_progress.py:339` refuses before booting when `boot.base.json`'s `head` is
not `git rev-parse HEAD`, and `run_goal.sh` only re-records that baseline between *items*. The
driver re-recorded it at the start of this attempt (`run.log`: `recording the boot baseline at
ecde8b3`; `build/goal/judge/boot.base.json` now carries `ecde8b3082860ff5cc670152b58f989920f34eec`).
**The lesson the previous lane wrote down is confirmed: if the re-judge fails with the
baseline/head mismatch line, the change is not implicated - re-apply and re-judge.** Everything else
had already passed on 366622b (`build/goal/check.out` still on disk, `goal_check: PASS`).

### Measured on ecde8b3 (all re-derived on this tree, none recalled)

- `./tools/goal_check.sh build/goal/item.json`: `PASS`, every step ok. `gate.sh`: `GATE PASS
  ecde8b30+7 changed`, `hashes vs config.yml ok` (all 86 RELs), `docs claims ok`, `files.cmake ok`,
  `port link gap ok`, `reach stubs ok`, `per-function diff matched 12108 -> 12108 linked 5860 ->
  5860 (+0 functions at 100%, 0 units newly linked)`. `sha1sum build/G2ME01/main.dol` =
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/check_symbol_names.py`: `checked 525 units; 0 declared names are missing`.
  `python3 tools/check_decl_order.py`: `ok: 981 unit(s) checked, 29 permuted, all 29 accounted for`.
- `./tools/link_check.sh`: `compile errors 0`, `unique undefined symbols 289`, `duplicate
  definitions 0`. `./tools/probe_sources.sh`: `748 files, 0 failed, 0 errors; link: LINKED (289
  undefined, 0 duplicates)`.
- `python3 tools/link_gap.py --rebuild`: **284 MISSING**, `all accounted for in
  port_link_gap_list.md` with the applied edit. The `other game methods` row and heading are
  **213 -> 212** here, and the regenerated list sums to 284 against the table's 284 - checked by
  re-deriving the group counts from the list the way `check_docs_claims.py` 4b2 does.
- `./tools/goal_verify/boot-progress.sh`: `BOOT_PROGRESS PASS: all 2 runs got further than all 2
  head runs`, every marker kept, exit code 0. The head baseline is a clean-exit head whose stub set
  has the target first; the candidate's set has **zero** occurrences of it, and the newly reached
  stubs are unchanged from the head's minus that one - `fn_80145ACC` (11 per run) and the rest.

### Two claims in the carried-over header were wrong on this tree, and are corrected in the file

The previous attempt's `PortSPersistentOptionsValue.cpp` header asserted things that do not measure
here. I verified each rather than inheriting it, and rewrote the header:

- **"that unit is `Matching` at 100.00%".** Neither address is a `configure.py` unit on this tree:
  `grep -c SPersistentOptionsValue configure.py` is **0**, `build.ninja` does not mention either
  file, `build/report.json` has no unit for either, and `tools/flip_test.sh
  src/MetroidPrime/Player/SPersistentOptionsValueCtor.cpp` prints `SKIP - not listed in
  configure.py`. The `Matching at 100.00%, flip_test PASS, 1/1` figures in
  `tools/check_files_cmake.py`'s `EXCLUDED` table and in those two files' own headers predate the
  units leaving `configure.py`. The header now says this and does not claim a match.
- **"retail's symbol table calls the constructor `__ct__23SPersistentOptionsValueFiii`".**
  `config/G2ME01/symbols.txt` carries **no** `SPersistentOptionsValue` name at all
  (`grep -c` is 0). The two addresses are named `ClampToMinMax__20CEnvironmentVariableFv`
  (0x801461AC, size 0x44) and `__ct__20CEnvironmentVariableFiii` (0x801462DC, size 0x3C) - so the
  class this tree spells `SPersistentOptionsValue` is retail's `CEnvironmentVariable`. This does not
  change the fix (the port needs the host compiler's mangling to resolve to retail's bytes) and the
  header now records it. **`include/MetroidPrime/Player/SPersistentOptionsValue.hpp:7` still says
  "retail 0x801462DC" for this class and is now the one file that disagrees** - correcting it is not
  this item's work, and the class is reachable from the port link, so it is noted here rather than
  edited.
- **The quoted `[reach-stub 0008]` line.** `mpReachStub` numbers from one counter shared by every
  stub, so the number is per *call*, not per stub: measured on the head at ecde8b3, the eleven hits
  are 0006, 0008, ... 0026. The header now quotes the first and says what the sequence is. (How the
  eleven were measured: the head's `PortReachStubs.cpp` restored, `PortSPersistentOptionsValue.cpp`
  removed from `files.cmake`, `MP_BOOT_RUNS=1 ./tools/boot_probe.sh`, then both files put back.)

### Retail bytes, measured rather than quoted from a sibling file

`tools/dol_read.py 0x801462DC 0x3C` and `0x801461AC 0x44` out of `orig/G2ME01/sys/main.dol`, piped
through `build/binutils/powerpc-eabi-objdump -D -b binary -m powerpc:common -EB`
(`-m powerpc:7500` is **not** supported by the bundled objdump - "can't use supplied machine" - so
the sibling files' `powerpc:7500` spelling cannot have been run as written):

    0x801462dc  stwu r1,-16(r1)   0x801461ac  lwz  r4,8(r3)     v = +0x08
    0x801462ec  mr   r31,r3       0x801461b0  lwz  r5,0(r3)     lo = +0x00
    0x801462f0  stw  r4,0(r3)     0x801461b4  cmpw r4,r5
    0x801462f4  stw  r5,4(r3)     0x801461b8  blt  801461c8
    0x801462f8  stw  r6,8(r3)     0x801461bc  lwz  r0,4(r3)     hi = +0x04
    0x801462fc  bl   801461ac     0x801461c4  blelr            in range -> return
    0x80146304  mr   r3,r31       0x801461e8  stw  r5,8(r3)     the one store

So the constructor is three stores and the clamp call, and the clamp is a clamp of `value` into
`[lo, hi]` - which is what both port bodies say. A DOL's section table puts the file offsets at
`0x00`, the addresses at `0x48` and the sizes at `0x6c`; reading the addresses at `0x20` yields
plausible-looking nonsense, and `tools/dol_read.py` is the reader to use instead of hand-parsing.

### Next, unchanged and re-measured here

`fn_80145ACC` (the map insert, `CPersistentOptionsMapInsert.cpp`, 0x80145ACC) is now the only body
the eleven rows run into: **11 hits per run**, the same count the target had. It relocates against
`fn_80146338` (retail 0x80146338, `size:0x1B8`, the rbtree node insert), which `symbols.txt` still
names `fn_80146338` and which nothing in the tree implements, so closing it needs that first.
**No `NEW:` filed:** `build/goal/queue.json` already holds four items for it -
`port-boot-stub-fn-80145acc-38068b1`, `-9f2d849`, `-847cb312`, `-2c9e52ca` - one per tip it was
named at, so a fifth would be a duplicate, and the driver also re-derives the most-hit stub itself
(`run_goal.sh`'s `queue_boot_blocker` calls `boot_progress.py blocker`).

### For whoever owns `tools/`, not queued here

`tools/check_files_cmake.py`'s `EXCLUDED` table is stale for the two `SPersistentOptionsValue`
files, and not only in the wording this item ran into. Measured: neither path is in `configure.py`,
so the entries' `Matching at 100.00%, flip_test PASS, 1/1` claims (and the "becomes worth listing
when the port constructs a CPersistentOptions" ones) describe units that are no longer configured.
The tool cannot catch this itself: it only reports an exclusion as stale when the file is **listed**
or **absent**, never when the unit has left `configure.py`, and both files are still on disk. That
is a lesson about the tool rather than countable work, so it is a note and not a `NEW:`.

## Lane 1: passed, then failed on the moved tip (2026-10-01 20:58:46Z)

The judged change failed goal_check.sh (exit 1) once rebased onto d44a455a4dfc; re-do it against the current tip.

## Lane 1, third attempt on d44a455a4dfc: PASS (2026-10-01)

`./tools/goal_check.sh build/goal/item.json` **exits 0 on d44a455a4dfc**, the tip the driver released
this onto. This is the same change the two earlier lanes landed, re-applied - `git apply` accepted
its `files.cmake`, `PortReachStubs.cpp`, `port_link_gap_list.md` and new-source hunks with only a
line offset; the `docs/HANDOFF.md` / `docs/RUNNING_THE_DECOMP.md` hunks were dropped (the driver
rewrites the probe count itself) and the `port_link_gap.md` hunk had to be re-applied by hand,
because that file moved since ecde8b3.

**So the previous lane's lesson holds a third time: a `BOOT_PROGRESS FAIL` reading
`the boot baseline is for <old>, the branch head is <new>` is the baseline, not the change.** This
time the driver had re-recorded the baseline before releasing the item (`run.log`: `recording the
boot baseline at d44a455`), so the verify step actually booted and passed on its own merits.

### What the diff is

Four files: a new `src/MetroidPrime/Player/PortSPersistentOptionsValue.cpp` (retail's constructor
under the class's own name, plus `fn_801461AC`), one `files.cmake` entry, `reachstub_523` deleted
from `PortReachStubs.cpp`, and the two `port_link_gap` docs rows. No `configure.py`, no
`splits.txt`, no DOL unit - `grep -c SPersistentOptionsValue configure.py` is **0**, so main.dol
cannot move.

`build/goal/check-verify.log` reads the result exactly as the item asks:

    vs head run 1: further - same markers, both exit with code 0, and stubs the head hit are no
    longer hit: ['_ZN23SPersistentOptionsValueC1Eiii']
    ...
    BOOT_PROGRESS PASS: all 2 runs got further than all 2 head runs

### Measured on this tree, all re-derived (nothing inherited)

- `goal_check.sh`: `PASS`, every step ok - `gate.sh` green, `matched 12132 -> 12132`,
  `linked 5860 -> 5860`, `All: 34.28% fuzzy, 27.50% matched, 12.89% linked (12132 / 28465
  functions)`, `port undefined 289 -> 288`, `probe: 749 files, 0 failed, 0 errors; link: LINKED
  (288 undefined, 0 duplicates)`.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/check_symbol_names.py`: `checked 525 units; 0 declared names are missing`.
- `python3 tools/check_decl_order.py`: `ok: 981 unit(s) checked, 29 permuted, all 29 accounted for`.
- `python3 tools/link_gap.py --rebuild`: **283 MISSING** (213 -> 212 `other game methods`),
  `all accounted for in port_link_gap_list.md`. The table and the regenerated list now agree at
  212 each. Note the absolute total is **283 here, not the 284 the ecde8b3 notes recorded** - one
  fewer gap on this tip, from 40dd2fc9, which is why the list's `unmangled` group also reads 56
  rather than 57. Both are the tree's own numbers; the 284 figure is superseded.
- `./tools/link_check.sh` standalone: `compile errors 0`, `unique undefined symbols 288`,
  `duplicate definitions 0` (the trailing `NOT LINKED` is the script's own verdict line and is
  about the *diagnostic* stub link, not the port link - `probe_sources.sh`'s real link says
  `LINKED`).
- `python3 tools/check_files_cmake.py`: `every configured DOL object is either in files.cmake or
  excluded with a reason`. The two `SPersistentOptionsValue*.cpp` files stay unlisted; adding
  either would report `stale: ... is in EXCLUDED but is now listed in files.cmake` and fail the
  gate's own step, which is why this is a third file rather than the obvious one.
- **The definition is real, not a stub.** `nm` on the linked object
  `build-port-link/CMakeFiles/mp_game.dir/src/MetroidPrime/Player/PortSPersistentOptionsValue.cpp.o`:
  `T fn_801461AC` and `T _ZN23SPersistentOptionsValueC1Eiii` (plus the `C2` base-object alias).
  Nothing prints a line; the body stores three arguments and clamps.

### Two carried-over header claims were STALE on this tip, and are corrected in the file

Both were measured here, not assumed, and both had to change because **the tip moved**:

- **The eleven hits are 0006..0016, consecutive - not "0006, 0008, ... 0026, interleaved with
  `fn_80145ACC`".** `fn_80145ACC` became a real body in 40dd2fc9
  (`port: port-boot-stub-fn-80145acc-9f2d849`), so it is no longer reached at all: on the head at
  d44a455, `grep -c fn_80145ACC build-boot-probe/run.log` is **0**, while
  `grep -c _ZN23SPersistentOptionsValueC1Eiii` on the same file is **11**, numbered 0006 to 0016.
  Measured with the head's `PortReachStubs.cpp` and `files.cmake` restored and the new file
  removed, `MP_BOOT_RUNS=1 ./tools/boot_probe.sh`, then both put back.
- **`fn_80145ACC` is no longer the next blocker**, and nothing new is: the boot still exits 0 at
  `frame: 300` with every marker unchanged (`check-verify.log`), so this item closes the last
  stub the eleven rows run into. **No `NEW:` filed** - `build/goal/queue.json` already holds the
  four `port-boot-stub-fn-80145acc-*` items the earlier notes listed, and the driver re-derives
  the most-hit stub itself (`run_goal.sh`'s `queue_boot_blocker`).

Everything else in the header re-measured the same: `tools/flip_test.sh
src/MetroidPrime/Player/SPersistentOptionsValueCtor.cpp` still prints `SKIP - not listed in
configure.py`, `grep -c SPersistentOptionsValue config/G2ME01/symbols.txt` is still **0**, and
`symbols.txt:5434,5438` still name the two addresses `ClampToMinMax__20CEnvironmentVariableFv`
and `__ct__20CEnvironmentVariableFiii`.

### Retail bytes, re-read here rather than copied from the sibling header

`tools/dol_read.py 0x801462DC 0x3C` and `0x801461AC 0x44` (it prints a `hex :` line, not raw
bytes - parse that line and write the binary yourself; redirecting its stdout straight into
`objdump -b binary` disassembles the *text*, which reads as plausible nonsense and is how a
stale figure gets carried forward). Through `build/binutils/powerpc-eabi-objdump -D -b binary -m
powerpc:common -EB`:

    ctor   stwu r1,-16(r1) / mflr / stw r0,20(r1) / stw r31,12(r1) / mr r31,r3
           stw r4,0(r3) / stw r5,4(r3) / stw r6,8(r3) / bl <clamp>
           lwz r0,20(r1) / mr r3,r31 / lwz r31,12(r1) / mtlr / addi r1,r1,16 / blr
    clamp  lwz r4,8(r3) / lwz r5,0(r3) / cmpw r4,r5 / blt +0x1c      v < lo -> lo
           lwz r0,4(r3) / cmpw r4,r0 / blelr                        in range -> return
           cmpw r5,r4 / lwz r0,4(r3) / ble +0x2c ... / stw r5,8(r3) v > hi -> hi

Three stores plus the clamp call, and a clamp with exactly one store, both reached through
`+0x00`/`+0x04`/`+0x08` - which is what the two port bodies say, so no spelling was tried and
none was needed.

`build/goal/review/port-boot-stub-zn23spersistentoptionsvaluec1eiii-366622b-L1-21.patch` (line
224) is the previous lane's whole patch, verbatim, if a later run needs it; it still does not
apply whole because the two docs files it touches have since moved.

## Lane 1: passed, then failed on the moved tip (2026-10-01 21:12:14Z)

The judged change failed goal_check.sh (exit 1) once rebased onto ef6584342af5; re-do it against the current tip.

## Lane 1, fourth attempt on ef658434: PASS (2026-10-01 23:2xZ)

`./tools/goal_check.sh build/goal/item.json` **exits 0 on ef658434**, run three times on this tree
(once after applying the patch, once after a full restore, once after the comment corrections
below). This is the same change the three earlier lanes landed: `git apply --check` accepts
`build/goal/rebase.patch` (byte-identical to
`review/port-boot-stub-zn23spersistentoptionsvaluec1eiii-366622b-L1-22.patch`) against this tip
once `docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md` and `docs/goal-notes/*` are excluded, and
`git diff d44a455a..HEAD --stat` shows the only commit since d44a455 is `ef658434`, which touches
`config/G2ME01/symbols.txt`, `docs/HANDOFF.md`, `docs/goal-notes/...`,
`include/Kyoto/Animation/CCharacterInfo.hpp` and `src/Kyoto/Animation/CCharacterInfo.cpp` - none
of them a port file, so nothing about this change had to be re-derived.

**The three earlier `BOOT_PROGRESS FAIL`s were the driver, not the change, and that is now
confirmed a fourth time rather than assumed.** The judge recorded the boot baseline for `ef658434`
at 21:12:15Z (`build/goal/judge/boot.base.json` carries
`ef6584342af504a9a93885bceb680dfcea561e24`, matching `git rev-parse HEAD`), so the verify step
booted both trees for real this time and passed on its own merits.

### Measured on this tree, all re-derived (nothing inherited)

- `goal_check.sh`: `PASS`, every step ok - `GATE PASS ef658434+7 changed`, `matched 12149 ->
  12149`, `linked 5860 -> 5860`, `All: 34.31% fuzzy, 27.50% matched, 12.89% linked (12149 /
  28465 functions)`, `port undefined 289 -> 288`, `probe: 749 files, 0 failed, 0 errors; link:
  LINKED (288 undefined, 0 duplicates)`.
- `build/goal/check-verify.log`: both candidate runs `exit: exit; last marker: frame: 300; main
  thread, innermost first: no frames`, and against all four head comparisons
  `further - same markers, both exit with code 0, and stubs the head hit are no longer hit:
  ['_ZN23SPersistentOptionsValueC1Eiii']`, then `BOOT_PROGRESS PASS: all 2 runs got further than
  all 2 head runs`.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/check_symbol_names.py`: `checked 525 units; 0 declared names are missing`.
- `python3 tools/check_decl_order.py`: `ok: 981 unit(s) checked, 29 permuted, all 29 accounted
  for`.
- `python3 tools/link_gap.py --rebuild`: **283 MISSING**, `all accounted for in
  port_link_gap_list.md` - the same 283 the d44a455 attempt recorded, `other game methods`
  213 -> 212. **The ecde8b3 attempt's 284 is superseded.**
- `link_check` (from the gate's `check-link.log`): `compile errors 0`, `unique undefined symbols
  288`, `duplicate definitions 0`.
- **The target left the undefined list**, which is this item's acceptance test beyond the boot:
  the baseline recorded it demangled as
  `SPersistentOptionsValue::SPersistentOptionsValue(int, int, int)  CGameState.cpp.o` at
  `build/goal/judge/undef.base.txt:228`, and after the change
  `grep -c SPersistentOptionsValue build/goal/undef_by_obj.txt` is **0**.
- **The definition is real, not a stub.** `build/binutils/powerpc-eabi-nm` on the linked object
  `build-port-link/CMakeFiles/mp_game.dir/src/MetroidPrime/Player/PortSPersistentOptionsValue.cpp.o`:
  `T fn_801461AC`, `T _ZN23SPersistentOptionsValueC1Eiii`, `T _ZN23SPersistentOptionsValueC2Eiii`.
  Nothing prints a line; the body stores three arguments and clamps.

### Three numbers in the carried-over diff were stale on this tip and are corrected

The patch is the previous lanes' verbatim, so its comments quoted two earlier tips. All three were
re-measured here rather than carried:

- **`files.cmake`'s "the port's undefined count 290 -> 289"** is **289 -> 288** on ef658434 (the
  gate's own `port undefined 289 -> 288`). 290 was already stale at d44a455, whose notes recorded
  289. Corrected in the comment.
- **The clamp body is now spelled exactly as `SPersistentOptionsValueClamp.cpp` spells it.** The
  patch's version wrote
  `self->x08_value = self->x08_value < self->x00_lo ? self->x00_lo : self->x04_hi;`, which is
  equivalent C++ but is a *second dialect* of a body that is already written twice in the tree.
  It is now the sibling's `p` alias, the same three locals in the same order and the same nested
  conditional - the spelling that file's header records as the mwcceppc sweep winner at 0
  differing instructions. Behaviour is unchanged (the gate re-ran clean after the edit); the point
  is that the two copies cannot drift.
- **The header's stub numbering now names this tip.** It reads "measured on the head at
  **ef658434**", measured by reverting the four tracked files and moving the new file aside, then
  `MP_BOOT_RUNS=1 ./tools/boot_probe.sh`, then restoring - the same procedure the earlier notes
  record.

### A trap the earlier notes walked into, found here and worth keeping

The head's boot hits **19** distinct stand-in symbols per run, the candidate's **19** as well -
but a `grep "reach-stub"` on the candidate's `build-boot-probe/run.log` shows only **12**. The
other seven (`fn_80270A64`, `fn_80270BB4`, `fn_80270D44`, `fn_80270EC8`, `fn_80271104`,
`fn_802711A4`, `fn_80272624`) are printed as **`[auto-stub]`, not `[reach-stub]`** - bodies
`tools/boot_probe.sh` appended to `PortReachStubs.cpp:974-980` for symbols the *diagnostic* link
asked for, referenced only by `Carve80271238.cpp.o` (all seven) and `Carve80270848.cpp.o`
(`fn_80272624`). They print between `Initializing renderer...` and `boot: step 21c returned -
CCubeRenderer's constructor completed, 8 pool tokens`. A run that counted only `reach-stub` lines
would conclude this change made the boot hit seven fewer stubs, which is false and would have
looked like a regression. `tools/goal_verify/boot_progress.py:55` matches both spellings, which is
why the judge never saw it; `STUB = re.compile(r"^\[(?:reach-stub \d+|auto-stub)\] (\S+)")`.
Count both, or read `boot.base.json`'s `stubs` dict, which the judge records for the head.

Confirmed on this tip, as the d44a455 attempt recorded: the eleven `SPersistentOptionsValue` hits
are **`[reach-stub 0006]` .. `[reach-stub 0016]`, one per call and consecutive**, and
`fn_80145ACC` is **not hit at all** (real body since `40dd2fc9`).

### Retail bytes, re-read on this tree rather than copied from the sibling header

`python3 tools/dol_read.py 0x801462DC 0x3C` and `0x801461AC 0x44`, taking the `u32 :` line (not
the `hex :` line - piping `dol_read.py`'s stdout straight into `objdump -b binary` disassembles the
*text*, which reads as plausible nonsense and is how a stale figure gets carried forward):

    ctor   0x9421fff0 stwu r1,-16(r1) / 0x7c0802a6 mflr r0 / 0x90010014 stw r0,20(r1)
           0x93e1000c stw r31,12(r1) / 0x7c7f1b78 mr r31,r3 / 0x90830000 stw r4,0(r3)
           0x90a30004 stw r5,4(r3) / 0x90c30008 stw r6,8(r3) / 0x4bfffeb1 bl 0x801461ac
           0x80010014 lwz r0,20(r1) / 0x7fe3fb78 mr r3,r31 / 0x83e1000c lwz r31,12(r1)
           0x7c0803a6 mtlr r0 / 0x38210010 addi r1,r1,16 / 0x4e800020 blr      = 15 instrs, 0x3C
    clamp  0x80830008 lwz r4,8(r3) / 0x80a30000 lwz r5,0(r3) / 0x7c042800 cmpw r4,r5
           0x41800010 blt / 0x80030004 lwz r0,4(r3) / 0x7c040000 cmpw r4,r0 / 0x4c810020 blelr
           ... / 0x90a30008 stw r5,8(r3) / 0x4e800020 blr                    = 17 instrs, 0x44

Three stores plus the clamp call, and a clamp whose only store is 0x801461E8 reached through the
`blelr` return path - identical to what the sibling units' headers record, so no spelling was
tried and none was needed. `grep -c SPersistentOptionsValue config/G2ME01/symbols.txt` is still
**0** and `symbols.txt:5434,5438` still name the two addresses
`ClampToMinMax__20CEnvironmentVariableFv` and `__ct__20CEnvironmentVariableFiii`;
`grep -c SPersistentOptionsValue configure.py` is still **0**, so main.dol cannot move.

### New in the header this run: the tree spells retail's one class under two names

Recorded because a reader of the new file would otherwise ask why it cannot call the clamp that
already exists. `include/MetroidPrime/Player/CEnvironmentVariable.hpp` declares a *separate*
`CEnvironmentVariable` ("Guessed name" in its own header) whose **private** `ClampToMinMax()` is
retail's 0x801461AC and which `CGameState.cpp` defines; its symbol is
`_ZN20CEnvironmentVariable14ClampToMinMaxEv`, a third name for the same 68 bytes. The two classes
never meet: `LoadFields` builds `SPersistentOptionsValue` temporaries (CGameState.cpp:325-335)
and the map they land in is a `rstl::map<rstl::string, CEnvironmentVariable>` that
`PortCPersistentOptionsMap.cpp:79-80` fills by copying the three words across by hand. So the
clamp is private, differently mangled, and unusable from the new file - and the port link reports
**0 duplicate definitions** with both bodies present.

### Next, re-measured here: nothing

The candidate still exits 0 at `frame: 300` with all 315 markers and the same stub set as the
head minus `_ZN23SPersistentOptionsValueC1Eiii`, so this item closes the last stub the eleven
rows run into. **No `NEW:` filed** - `build/goal/queue.json` already holds the four
`port-boot-stub-fn-80145acc-*` items the earlier notes listed, and the driver re-derives the
most-hit stub itself (`run_goal.sh`'s `queue_boot_blocker` calls `boot_progress.py blocker`).

### For whoever owns `tools/`, still not queued here

Unchanged from the earlier attempts and still true: `tools/check_files_cmake.py`'s `EXCLUDED`
table is stale for the two `SPersistentOptionsValue*.cpp` files. Neither path is in
`configure.py`, so their `Matching at 100.00%, flip_test PASS, 1/1` entries describe units that
have left `configure.py`, and the tool cannot notice: it reports an exclusion as stale only when
the file is **listed** or **absent**, never when its unit left `configure.py`. It also cannot be
listed while the entry stands - adding either path makes the tool print
`stale: <path> is in EXCLUDED but is now listed in files.cmake` and exit 1, which is the gate's
own `files.cmake` step. That is why this item needs a **third** file rather than the obvious one.
A lesson about the tool, not countable work, so a note and not a `NEW:`.

### Two things the driver does that are worth knowing before the next attempt

- **`gate.sh` rewrites `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` itself.** With
  `MP_GATE_DOCS_WRITE` set, `tools/gate.sh:115` runs `check_docs_claims.py --write`, which
  changed the probe claim `748 files` -> `749 files` in four places during `goal_check.sh`. That is
  why the previous lanes' committed diffs contain those two files despite the brief telling them
  not to edit them: the hunks are the judge's own output, not the lane's. Excluding them from the
  patch is correct and the tree ends up identical.
- **`build/goal/rebase.patch` in this worktree is byte-identical to
  `review/...-L1-22.patch`**, so a later run has the whole previous patch in two places. It still
  applies only with the three doc paths excluded.

## Lane 1: passed, then failed on the moved tip (2026-10-01 21:26:28Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 0c47802d51d3; re-do it against the current tip.

## Lane 1, fifth attempt on 0c47802d: PASS (2026-10-01 23:3xZ)

`./tools/goal_check.sh build/goal/item.json` **exits 0 on 0c47802d51d3230dec3d1a228b343d06360d283f**,
the tip the driver released this onto - and this time the `boot-progress.sh` step **booted both trees
for real and passed on its own merits**, which is the first time that has happened on a *released*
attempt (the four before it were judged on a baseline recorded for the previous tip, and refused
before booting). Root cause, fourth confirmation: `run.log` 21:25:32-21:26:29 shows the tip moved
`ef65843 -> 0c47802` at 21:25:32, the judge carried the patch onto it and re-judged **at 21:25:45,
using the boot baseline still recorded at ef65843**, and only re-recorded that baseline at 21:26:29 -
after the re-judge. So the failure line `verify: the boot baseline is for ef65843, the branch head is
0c47802` is a race between the two, not a property of the change. **No attempt should spend time on
the change because of that line; re-apply and re-judge.**

### What the diff is (identical to the four earlier attempts, re-applied from `build/goal/rebase.patch`)

`git apply --check --exclude='docs/HANDOFF.md' --exclude='docs/RUNNING_THE_DECOMP.md'
--exclude='docs/goal-notes/*' build/goal/rebase.patch` accepts it unchanged on this tip. Six files
as the lane's own change, two of them the gate's output:

    files.cmake                                     +12   one entry + its comment
    src/MetroidPrime/Player/PortSPersistentOptionsValue.cpp   +123   new: retail's ctor + clamp
    src/MetroidPrime/PortReachStubs.cpp              -5/+5  reachstub_523 alias removed
    docs/research/port_link_gap.md                   -1/+1  other game methods 213 -> 212
    docs/research/port_link_gap_list.md              -1/+1  the _ZN23SPersistentOptionsValueC1Eiii line
    docs/HANDOFF.md, docs/RUNNING_THE_DECOMP.md      -2/+2  the GATE's own probe-count rewrite

The last two are **not this item's change**: `gate.sh` runs `check_docs_claims.py --write` under
`MP_GATE_DOCS_WRITE` and rewrote the probe claim `748 files` -> `749 files` in four places during
`goal_check.sh`, exactly as the fourth attempt recorded. No `configure.py`, no `splits.txt`, no DOL
unit: `grep -c SPersistentOptionsValue configure.py` is **0**, so `main.dol` cannot move.

### Measured on this tree, all re-derived (nothing inherited)

- `goal_check.sh`: `PASS`, every step ok - `GATE PASS 0c47802d+7 changed`, `matched 12158 -> 12158`,
  `linked 5860 -> 5860`, `All: 34.33% fuzzy, 27.57% matched, 12.89% linked (12158 / 28465
  functions)`, `port undefined 289 -> 288`, `probe: 749 files, 0 failed, 0 errors; link: LINKED
  (288 undefined, 0 duplicates)`.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/check_symbol_names.py`: `checked 525 units; 0 declared names are missing from their
  object`. `python3 tools/check_decl_order.py`: `ok: 981 unit(s) checked, 29 permuted, all 29
  accounted for in decl_order.md`.
- `link_check` (from the gate's `check-link.log`): `compile errors 0`, `unique undefined symbols 288`,
  `duplicate definitions 0`. (The trailing `NOT LINKED` is that script's own verdict line about the
  *diagnostic* stub link; `probe_sources.sh`'s real link says `LINKED`.)
- `python3 tools/link_gap.py --rebuild`: **283 MISSING**, `all accounted for in port_link_gap_list.md`.
  Table and regenerated list agree at 212 + 11 + 56 + 4 = 283. **The 284 the ecde8b3 attempt recorded
  is superseded, as the third and fourth attempts said** - 283 here and at d44a455 and ef658434.
- `tools/flip_test.sh src/MetroidPrime/Player/SPersistentOptionsValueCtor.cpp` -> `SKIP - not listed
  in configure.py`; `tools/unit_fit.sh src/MetroidPrime/Player/PortSPersistentOptionsValue.cpp` ->
  `not declared in any splits.txt`. Neither is a `configure.py` unit, so no unit can be claimed here.
- `grep -c SPersistentOptionsValue config/G2ME01/symbols.txt` is **0**;
  `symbols.txt:5434` `ClampToMinMax__20CEnvironmentVariableFv = .text:0x801461AC; // size:0x44` and
  `symbols.txt:5438` `__ct__20CEnvironmentVariableFiii = .text:0x801462DC; // size:0x3C` - so retail's
  class at these two addresses is `CEnvironmentVariable`, and `SPersistentOptionsValue` is this tree's
  own spelling. Unchanged from the four earlier attempts.
- **The target left the undefined list**, which is the item's acceptance test beyond the boot:
  `build/goal/judge/undef.base.txt:228` records it demangled as
  `SPersistentOptionsValue::SPersistentOptionsValue(int, int, int)	CGameState.cpp.o` at a head count of
  **289**; after the change `grep -c SPersistentOptionsValue build/goal/undef_by_obj.txt` is **0**
  against **288**.
- **The definition is real, not a stand-in.** `build/binutils/powerpc-eabi-nm` on
  `build-port-link/CMakeFiles/mp_game.dir/src/MetroidPrime/Player/PortSPersistentOptionsValue.cpp.o`:
  `T _ZN23SPersistentOptionsValueC1Eiii`, `T _ZN23SPersistentOptionsValueC2Eiii` (the base-object
  alias), `T fn_801461AC`, and `nm -u` prints **nothing** - no callee the link has to invent. Nothing
  in the file prints a line.

### The stub evidence, taken from the judge's own record instead of a hand-run probe

The fourth attempt spent a head boot (`git revert` the four files, `MP_BOOT_RUNS=1
./tools/boot_probe.sh`, restore) to re-measure the hit count. **There is a cheaper measurement and
it is the authoritative one**: the driver records the head's boot itself, before the agent starts,
into `build/goal/judge/boot.base.json` plus `build-boot-probe/run.log`, and it is that record - not a
lone run of ours - the judge compares against. Read at 23:26, before `goal_check.sh` overwrote the
log, on tip 0c47802d:

- `boot.base.json`'s `head` is `0c47802d51d3230dec3d1a228b343d06360d283f`, i.e. **the baseline is for
  this tip** - which is exactly why this attempt's verify booted instead of refusing.
- Its `stubs` map, per run: **20 distinct symbols, 30 total hits**, of which
  `_ZN23SPersistentOptionsValueC1Eiii` is **11** - identical in both runs.
- `build-boot-probe/run.log` (the head's, two runs): `reach-stub` lines **46** = 23 x 2, `auto-stub`
  lines **14** = 7 x 2, `frame:` lines **600** = 300 x 2, and the eleven target hits are
  **`[reach-stub 0006]` .. `[reach-stub 0016]`, consecutive, one per call** - so the header's `0006`
  is confirmed on this tip without a run of our own. `grep -c fn_80145ACC` is **0**: it has been a real
  body since 40dd2fc9 and the eleven rows no longer run into it, as the third and fourth attempts said.
- The seven `auto-stub` lines sit between `Initializing renderer...` and `boot: step 21c returned -
  CCubeRenderer's constructor completed` (lines 4199-4207), the bodies `tools/boot_probe.sh` appends
  at `PortReachStubs.cpp:974-980`. Unchanged in shape at this tip.

After `goal_check.sh` the same file holds the **candidate's** two runs: `SPersistentOptionsValueC1Eiii`
hits **0**, `reach-stub` lines **24** = 12 x 2, `auto-stub` **14** = 7 x 2, `frame:` **600**. So 19
distinct stand-ins against the head's 20, the difference being exactly the target - which is what
`check-verify.log` reports four times over:

    vs head run 1: further - same markers, both exit with code 0, and stubs the head hit are no longer
    hit: ['_ZN23SPersistentOptionsValueC1Eiii']          (x2 runs)
    BOOT_PROGRESS PASS: all 2 runs got further than all 2 head runs

**One thing that log settles that the ordinary port link cannot.** `gate.sh`'s own last line reads
`reach stubs   not in a real build  ok` - the gate never builds with `MP_BOOT_STUBS=ON`, so it cannot
see a duplicate definition between the removed `reachstub_523` alias and the new file. The boot probe
*does* build that way (it is the only configuration in which those `[reach-stub]` lines print at all),
and it linked and ran with the alias gone. So the alias removal is verified, not assumed.

### Retail bytes, re-read on this tree rather than copied from the sibling header

`python3 tools/dol_read.py 0x801462DC 0x3C` and `0x801461AC 0x44`, taking the **`u32 :`** line (the
`hex :` line is the same bytes and also works; what does *not* work is redirecting `dol_read.py`'s
stdout into `objdump -b binary`, which disassembles the text and reads as plausible nonsense):

    ctor   0x9421fff0 stwu r1,-16(r1) / 0x7c0802a6 mflr r0 / 0x90010014 stw r0,20(r1)
           0x93e1000c stw r31,12(r1) / 0x7c7f1b78 mr r31,r3 / 0x90830000 stw r4,0(r3)
           0x90a30004 stw r5,4(r3) / 0x90c30008 stw r6,8(r3) / 0x4bfffeb1 bl <0x801461ac>
           0x80010014 lwz r0,20(r1) / 0x7fe3fb78 mr r3,r31 / 0x83e1000c lwz r31,12(r1)
           0x7c0803a6 mtlr r0 / 0x38210010 addi r1,r1,16 / 0x4e800020 blr        = 15 instrs, 0x3C
    clamp  lwz r4,8(r3) / lwz r5,0(r3) / cmpw r4,r5 / blt +0x1c        value <  lo -> clamp
           lwz r0,4(r3) / cmpw r4,r0 / blelr                          value <= hi -> return, no store
           cmpw r5,r4 / lwz r0,4(r3) / ble +0xc / cmpw r0,r4 / bge +0xc / mr r4,r0 / mr r5,r4
           0x90a30008 stw r5,8(r3) / blr                              one store, at 0x801461E8

Three stores plus the clamp call, and a clamp with exactly one store that both arms reach. **The `bl`'s
target was worth deriving rather than trusting the header:** `0x4bfffeb1`'s 24-bit field is `0xFFFFAC`,
i.e. -84 words from `0x801462FC`, which lands on `0x801461AC` exactly. (Getting that field out of the
word is easy to get wrong - mask `0x03FFFFFC`, shift right 2 - and the `bl` carries `LK=1`, so a
one's-complement on the whole 32-bit word gives `0x831461ac`, which looks like a real address and is
not one.) This is what both port bodies say, so no spelling was tried and none was needed: the work is
re-doing a fix that has now landed four times, not finding one.

### Two stale numbers in the patch, corrected in the files on this tip

Both are the previous tips' names for this measurement, and both were re-measured here:

- The new file's header said "Measured on the head at **ef658434** with `MP_BOOT_RUNS=1
  ./tools/boot_probe.sh`". Corrected to name **0c47802d** and to say where the measurement came from -
  `build/goal/judge/boot.base.json` and the `build-boot-probe/run.log` that `--record` produced - so the
  next reader does not repeat a hand-run probe to re-check a number the judge already recorded.
- `files.cmake`'s comment said "Measured on ef658434: the port's undefined count 289 -> 288". The
  counts are right; the tip name was not, so it now reads 0c47802d. (290 was already stale at
  d44a455, whose notes recorded 289.)

Every other citation in the new file's header was checked against this tree rather than carried:
`CGameState.cpp:325-335` are the eleven `fn_80145ACC` rows, `SPersistentOptionsValue.hpp`'s fields are
still `x00_lo`/`x04_hi`/`x08_value`, `CEnvironmentVariable.hpp` still declares a *private*
`ClampToMinMax` that `CGameState.cpp:178` defines, and the map is still filled word-by-word in
`src/MetroidPrime/PortCPersistentOptionsMap.cpp:79-80` (note: that file is under `src/MetroidPrime/`,
not `src/MetroidPrime/Player/`, so a path quoted with the `Player/` segment is wrong - the header does
not quote it).

### Next, re-measured here: nothing

The candidate still exits 0 at `frame: 300` with all 315 markers and the same stub set as the head
minus the target, so this item closes the last stub the eleven rows run into. **No `NEW:` filed** -
`build/goal/queue.json` already holds the four `port-boot-stub-fn-80145acc-*` items the earlier notes
listed, and the driver re-derives the most-hit stub itself (`run_goal.sh`'s `queue_boot_blocker` calls
`boot_progress.py blocker`).

### For whoever owns `tools/`, still not queued here (unchanged across all five attempts)

`tools/check_files_cmake.py`'s `EXCLUDED` table is stale for the two `SPersistentOptionsValue*.cpp`
files: neither path is in `configure.py`, so their `Matching at 100.00%, flip_test PASS, 1/1` entries
describe units that have left `configure.py`, and the tool cannot notice - it reports an exclusion as
stale only when the file is **listed** or **absent**, never when its unit left `configure.py`. It also
cannot be listed while the entry stands: adding either path makes the tool print `stale: <path> is in
EXCLUDED but is now listed in files.cmake` and exit 1, which is the gate's own `files.cmake` step
(`ok` today only because neither is listed). That is why this item needs a **third** file rather than
the obvious one. A lesson about the tool, not countable work, so a note and not a `NEW:`.

## Lane 1: passed, then failed on the moved tip (2026-10-01 21:34:23Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 8a327b18ce59; re-do it against the current tip.

## Lane 1: passed, then failed on the moved tip (2026-10-01 21:39:02Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 8d908cd8da81; re-do it against the current tip.

## Lane 1, sixth attempt on 8d908cd8da81: PASS (2026-10-02)

`./tools/goal_check.sh build/goal/item.json` **exits 0 on 8d908cd8da81150bf47d630484464c0c452872c5**,
run twice on this tree (once after applying the patch, once after the two comment corrections
below). The change is the one the five earlier lanes landed, re-applied from
`build/goal/rebase.patch`; `git apply --check --exclude='docs/HANDOFF.md'
--exclude='docs/RUNNING_THE_DECOMP.md' --exclude='docs/goal-notes/*' build/goal/rebase.patch`
accepts it **unchanged** on this tip, so nothing about the code had to be re-derived.

**The baseline/head race did not recur this time, and the reason is worth one line for the next
attempt:** the driver recorded the boot baseline for `8d908cd8` at 23:39, i.e. *before* this lane
started, and `build/goal/judge/boot.base.json`'s `head` field carries
`8d908cd8da81150bf47d630484464c0c452872c5` = `git rev-parse HEAD`. So `boot_progress.py:339` did
not refuse before booting, and the verify step **booted both trees for real and passed on its own
merits** - six confirmations of the earlier notes' lesson that the
`the boot baseline is for <old>, the branch head is <new>` line is a race between the driver and
the lane, never a property of the change. Check that one field first; if it matches HEAD, the
verify step is real and its result means something.

### Measured on this tree, all re-derived (nothing inherited)

- `goal_check.sh`: `PASS`, every step ok - `GATE PASS 8d908cd8+7 changed`, `matched 12164 ->
  12164`, `linked 5860 -> 5860`, `All: 34.36% fuzzy, 27.60% matched, 12.89% linked (12164 /
  28465 functions)`, `port undefined 290 -> 289`, `probe: 749 files, 0 failed, 0 errors; link:
  LINKED (289 undefined, 0 duplicates)`.
- `build/goal/check-verify.log`: both candidate runs `exit: exit; last marker: frame: 300; main
  thread, innermost first: no frames`, and against all four head comparisons
  `further - same markers, both exit with code 0, and stubs the head hit are no longer hit:
  ['_ZN23SPersistentOptionsValueC1Eiii']`, then `BOOT_PROGRESS PASS: all 2 runs got further than
  all 2 head runs`.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/check_symbol_names.py`: `checked 525 units; 0 declared names are missing from
  their object`. `python3 tools/check_decl_order.py`: `ok: 981 unit(s) checked, 29 permuted, all
  29 accounted for in decl_order.md`.
- `python3 tools/check_files_cmake.py`: `every configured DOL object is either in files.cmake or
  excluded with a reason`, exit 0, `742 sources; configure.py declares 648 DOL objects`. Still
  `ok` only because neither `SPersistentOptionsValue*.cpp` is listed - see the unchanged note
  below on why they cannot be.
- `./tools/link_check.sh` standalone: `compile errors 0`, `unique undefined symbols 289`,
  `duplicate definitions 0`. (The trailing `NOT LINKED` is that script's own verdict line about
  the *diagnostic* stub link; `probe_sources.sh`'s real link says `LINKED`.)
- `tools/flip_test.sh src/MetroidPrime/Player/SPersistentOptionsValueCtor.cpp` ->
  `kept: 0 / 1  failed: 0  skipped: 1` / `SKIP ... not listed in configure.py`;
  `tools/unit_fit.sh src/MetroidPrime/Player/PortSPersistentOptionsValue.cpp` ->
  `not declared in any splits.txt`. Neither is a `configure.py` unit, so no unit can be claimed
  here and `main.dol` cannot move.
- `grep -c SPersistentOptionsValue configure.py` = **0**; `grep -c SPersistentOptionsValue
  config/G2ME01/symbols.txt` = **0**; `symbols.txt:5434,5438` still
  `ClampToMinMax__20CEnvironmentVariableFv = .text:0x801461AC; // type:function size:0x44` and
  `__ct__20CEnvironmentVariableFiii = .text:0x801462DC; // type:function size:0x3C`. Unchanged
  across all six attempts: retail's class at these two addresses is `CEnvironmentVariable`, and
  `SPersistentOptionsValue` is this tree's own spelling.
- **The target left the undefined list**, which is the item's acceptance test beyond the boot:
  `build/goal/judge/undef.base.count` is **290** at the head, and after the change
  `grep -c SPersistentOptionsValue build/goal/undef_by_obj.txt` is **0**; the judge prints
  `port undefined 290 -> 289`. **`290 -> 289` is the figure on this tip** - the fifth attempt's
  `289 -> 288` was already stale here, because the four commits since `0c47802d`
  (`9fe02042`, `d79065b7`, `8a327b18`, `8d908cd8`) each added port undefined symbols.
- **The definition is real, not a stand-in.** `build/binutils/powerpc-eabi-nm` on the linked
  object `build-port-link/CMakeFiles/mp_game.dir/src/MetroidPrime/Player/PortSPersistentOptionsValue.cpp.o`:
  `T fn_801461AC`, `T _ZN23SPersistentOptionsValueC1Eiii`, `T _ZN23SPersistentOptionsValueC2Eiii`
  (the base-object alias), and `nm -u` prints **nothing**. Nothing in the file prints a line; the
  body stores three arguments and clamps.

### The gap total is 284 on this tip, and the 283 of the last four attempts is superseded

`python3 tools/link_gap.py` (no `--rebuild`; the link is current from `goal_check.sh`):
**284 MISSING**, `all accounted for in port_link_gap_list.md`, and the group headings now read
`other game methods (212)`, `unmangled: fn_/lbl_/globals (56)`, `REL module loaders (11)`,
`static data members (5)` = 284, matching the table in `port_link_gap.md`. The extra one is
`_ZN9CGraphics12mModelMatrixE`, added by `9fe02042` (`progress-prime1-cactormodelparticles`) and
deliberately *listed* rather than defined, which that commit's own note in `port_link_gap.md`
explains. So: **284 here; the 283 the third, fourth and fifth attempts recorded is superseded,
and the 284 the second attempt recorded happens to agree by coincidence, from a different gap.**

### The head's boot evidence, read from the judge's record rather than a hand-run probe

The fourth attempt spent a head boot to re-measure the hit count and the fifth noted there is a
cheaper way. Confirmed cheaper here - `build/goal/judge/boot.base.json` and the
`build-boot-probe/run.log` its `--record` produced, read **before** `goal_check.sh` overwrote the
log with the candidate's runs:

- Two runs, each: **20 distinct stand-in symbols, 30 total hits, 315 markers, `exited: True`,
  `stop: frame: 300`, `frames: []`**, and `_ZN23SPersistentOptionsValueC1Eiii` **11**. Identical
  in both runs.
- That head log's `reach-stub` lines **46** = 23 x 2, `auto-stub` **14** = 7 x 2, `frame:`
  **600** = 300 x 2, and the eleven target hits are **`[reach-stub 0006]` .. `[reach-stub 0016]`,
  consecutive, one per call** - so the header's `0006` holds on this tip with no run of our own.
  `grep -c fn_80145ACC` is **0**: it has been a real body since `40dd2fc9`, as the third, fourth
  and fifth attempts said.
- After `goal_check.sh` the same file holds the **candidate's** two runs: target hits **0**,
  `reach-stub` **24** = 12 x 2, `auto-stub` **14** = 7 x 2, `frame:` **600**. So 19 distinct
  stand-ins against the head's 20, the difference being exactly the target.
- **Count both stub spellings.** The seven `fn_8027xxxx` names print as `[auto-stub]`, not
  `[reach-stub]` - bodies `tools/boot_probe.sh` appends to `PortReachStubs.cpp` for symbols the
  diagnostic link asked for - and they sit between `Initializing renderer...` and `boot: step 21c
  returned - CCubeRenderer's constructor completed`. A run counting only `reach-stub` lines
  reports 12 against the head's 23 and reads as a 7-stub regression, which is false.
  `tools/goal_verify/boot_progress.py:55` matches both: `STUB = re.compile(r"^\[(?:reach-stub
  \d+|auto-stub)\] (\S+)")`.

**One thing only the boot probe can see, re-confirmed:** `gate.sh`'s own last line is
`reach stubs   not in a real build  ok` - the gate never builds with `MP_BOOT_STUBS=ON`, so it
cannot detect a duplicate definition between the removed `reachstub_523` alias and the new file.
The boot probe *does* build that way (the only configuration in which `[reach-stub]` lines print
at all) and it linked and ran with the alias gone. So the alias removal is verified, not assumed.

### Retail bytes, re-read on this tree rather than copied from the sibling header

`python3 tools/dol_read.py 0x801462DC 0x3C` and `0x801461AC 0x44`, taking the **`u32 :`** line
(the `hex :` line is the same bytes and also works; what does *not* work is redirecting
`dol_read.py`'s stdout into `objdump -b binary`, which disassembles the text and reads as
plausible nonsense - that is how a stale figure gets carried forward):

    ctor   0x9421fff0 stwu r1,-16(r1) / 0x7c0802a6 mflr r0 / 0x90010014 stw r0,20(r1)
           0x93e1000c stw r31,12(r1) / 0x7c7f1b78 mr r31,r3 / 0x90830000 stw r4,0(r3)
           0x90a30004 stw r5,4(r3) / 0x90c30008 stw r6,8(r3) / 0x4bfffeb1 bl <0x801461ac>
           0x80010014 lwz r0,20(r1) / 0x7fe3fb78 mr r3,r31 / 0x83e1000c lwz r31,12(r1)
           0x7c0803a6 mtlr r0 / 0x38210010 addi r1,r1,16 / 0x4e800020 blr      15 instrs, 0x3C
    clamp  0x80830008 lwz r4,8(r3) / 0x80a30000 lwz r5,0(r3) / 0x7c042800 cmpw r4,r5
           0x41800010 blt / 0x80030004 lwz r0,4(r3) / 0x7c040000 cmpw r4,r0 / 0x4c810020 blelr
           ... / 0x90a30008 stw r5,8(r3) / 0x4e800020 blr                       17 instrs, 0x44

Three stores plus the clamp call, and a clamp with exactly one store (at 0x801461E8) that both arms
reach, with a `blelr` and no store at all for the in-range case. **The `bl` target was derived
rather than trusted again**: `0x4bfffeb1`'s 24-bit field is `0xFFFFAC`, i.e. -84 words from
`0x801462FC`, which lands on `0x801461AC` exactly. (Mask `0x03FFFFFC`, shift right 2; the `bl`
carries `LK=1`, so a one's-complement on the whole 32-bit word gives `0x831461ac`, which looks
like a real address and is not one.) That is what both port bodies say, so no spelling was tried
and none was needed - this is the sixth re-doing of a fix that has landed five times, not a
search.

Every other citation in the new file's header was checked against this tree rather than carried:
`CGameState.cpp:325-335` are the eleven `fn_80145ACC` rows, each constructing
`SPersistentOptionsValue(lo, hi, value)`; `SPersistentOptionsValue.hpp:33-35` are still
`x00_lo` / `x04_hi` / `x08_value`; `CEnvironmentVariable.hpp:23` still declares a *private*
`ClampToMinMax` that `CGameState.cpp:178` defines, and the header still says "Guessed name" with
`CHECK_SIZEOF(CEnvironmentVariable, 0xc)`.

### Two stale names in the patch, corrected in the files on this tip

Both are previous tips' names for a measurement whose *values* are still right, and both were
re-measured before editing:

- The new file's header said "Measured on the head at **0c47802d**" and quoted
  `boot.base.json`'s `head` as `0c47802d51d3...`. Corrected to **8d908cd8** /
  `8d908cd8da81150bf47d630484464c0c452872c5`, the value actually in the file on this tip. The
  sentence explaining that the measurement came from the judge's own `--record`, not a hand-run
  probe, is kept - it is the reason the next run does not spend a boot re-deriving it.
- `files.cmake`'s comment said "Measured on 8a327b1: the port's undefined count 290 -> 289". The
  counts are right; the tip name was one commit behind, so it now reads 8d908cd8. (The chain of
  stale tip names in this comment across attempts is 8a327b1 -> 0c47802d -> 8d908cd8; the counts
  went 290 -> 289 -> 289 -> 289 and only the first is right for the tip it names.)

`goal_check.sh` was re-run after both edits and still exits 0.

### What the diff is

Five files as the lane's own change, two of them the gate's own output:

    files.cmake                                                        +12   one entry + its comment
    src/MetroidPrime/Player/PortSPersistentOptionsValue.cpp           +123   new: ctor + fn_801461AC
    src/MetroidPrime/PortReachStubs.cpp                             -5/+5    reachstub_523 alias removed
    docs/research/port_link_gap.md                                     -1/+1 other game methods 213 -> 212
    docs/research/port_link_gap_list.md                               -1/+1 the target's line, and the heading
    docs/HANDOFF.md, docs/RUNNING_THE_DECOMP.md                       -2/+2 THE GATE's probe-count rewrite

The last two are **not this item's change**: `tools/gate.sh:115` runs `check_docs_claims.py
--write` under `MP_GATE_DOCS_WRITE` and rewrote the probe claim `748 files` -> `749 files` in four
places during `goal_check.sh` - the same mechanism the fourth and fifth attempts recorded, now
reproduced a sixth time, which is enough to call it expected rather than surprising. No
`configure.py`, no `splits.txt`, no DOL unit: `grep -c SPersistentOptionsValue configure.py` is
**0**, so `main.dol` cannot move, and the gate's per-function diff confirms it
(`+0 functions at 100%, 0 units newly linked` is the expected reading for a port item: the win is
the boot and the undefined count, not a report count).

### Next, re-measured here: nothing

The candidate still exits 0 at `frame: 300` with all 315 markers and the same stand-in set as the
head minus the target, so this item closes the last stub the eleven rows run into. **No `NEW:`
filed** - `build/goal/queue.json` already holds the four `port-boot-stub-fn-80145acc-*` items the
earlier notes listed (and `fn_80145ACC` is a real body since `40dd2fc9` anyway), and the driver
re-derives the most-hit stub itself (`run_goal.sh`'s `queue_boot_blocker` calls
`boot_progress.py blocker`).

### For whoever owns `tools/`, still not queued here (unchanged across all six attempts)

`tools/check_files_cmake.py`'s `EXCLUDED` table is stale for the two `SPersistentOptionsValue*.cpp`
files: neither path is in `configure.py`, so their `Matching at 100.00%, flip_test PASS, 1/1`
entries describe units that have left `configure.py`, and the tool cannot notice - it reports an
exclusion as stale only when the file is **listed** or **absent**, never when its unit left
`configure.py`. It also cannot be listed while the entry stands: adding either path makes the tool
print `stale: <path> is in EXCLUDED but is now listed in files.cmake` and exit 1, which is the
gate's own `files.cmake` step (its step reads `ok` today only because neither is listed). That is
why this item needs a **third** file rather than the obvious one - re-measured here, still true.
`tools/sync_files_cmake_excluded.py --check` exists and is wired into a gate step, so the fix is
probably a prune rule rather than new code, but this is a lesson about the tool rather than
countable work: a note and not a `NEW:`.
