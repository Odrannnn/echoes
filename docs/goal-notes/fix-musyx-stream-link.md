# fix-musyx-stream-link — DONE 2026-09-30, lane 5 (wt-mp2-goal-L5): already in the tree, no change

**The item's work is already committed here and the item passes with an empty diff.**
`tools/goal_check.sh build/goal/item.json` → `goal_check: PASS fix-musyx-stream-link`
(20.5 s, exit 0). `git status --porcelain --untracked-files=all` is empty before and after
every check below. Nothing in `src/`, `extern/`, `configure.py` or `config/` needed changing,
so nothing was changed.

`build/goal/item.json`'s `reason` is the truncated string
`"found by match-cfidget: commit ada6d97 flipped"` — it is the tail of the `NEW:` line filed at
the end of `build/goal/notes/match-cfidget.md`, and it describes a piece of work two earlier runs
already delivered. The fix is commit `ef9e308` ("fix: commit the MusyX stream.c guards
match-stream flipped against"), which **is an ancestor of this lane's HEAD** `6c11880`
(`git merge-base --is-ancestor ef9e308 HEAD` → 0). `ada6d97` is the earlier commit that flipped
`configure.py` without the guards, which is what broke the link and what this item existed to
repair.

## The fix that is already in the tree

`ef9e308` is 9 insertions / 3 deletions in one file, `extern/musyx/src/musyx/runtime/stream.c` —
guards only, no function body touched:

```
 336  #elif MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)  ->  (2, 0, 3)   streamKill
 758  #if    MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)  ->  (2, 0, 3)   sndStreamMixParameter
 793  new  #if MUSY_VERSION >  MUSY_VERSION_CHECK(2, 0, 3)  around sndStreamMixParameterEx
 850  new  #if MUSY_VERSION >  MUSY_VERSION_CHECK(2, 0, 3)  around #pragma push / sndStreamFrq / #pragma pop
 875  new  #endif
 878  #if    MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)  ->  > (2, 0, 3)  sndStreamLPFParameter
```

The vendored MusyX in `extern/musyx/src` is a later revision than the snapshot MP2 shipped, so at
the `MUSY_VERSION` this tree configures it emitted three functions the DOL does not define
(`sndStreamMixParameterEx`, `sndStreamFrq`, `sndStreamLPFParameter`) and compiled out one it does
(`sndStreamMixParameter`, behind `#if <= 2.0.2`). With `configure.py:1297` reading
`Object(Matching, "musyx/runtime/stream.c")` and the guards absent, our object went into the link
without `sndStreamMixParameter`, which `Kyoto/Audio/CDSPStreamManager.o` (still `NonMatching`,
retail's object) references — so the whole branch did not link. The full derivation is in
`docs/goal-notes/match-stream.md`; nothing here contradicts it.

The second half of the original blocker is also fixed: `tools/run_goal.sh:454` now reads

```
( cd "$WT" && git add -A -- src include config docs configure.py files.cmake CMakeLists.txt extern/musyx extern/musyx-port ) || true
```

`extern` is in the staging list, which is why `ef9e308` survived to the branch head this time
instead of being dropped the way it was for `ada6d97`.

## Re-measured here, not recalled

```
configure.py:1297                  Object(Matching, "musyx/runtime/stream.c")
report.json main/musyx/runtime/stream
                                   100.0% fuzzy, 100.0% matched code, 18/18 functions,
                                   metadata.complete: true; every function 100.0%
                                   .text 14320/14320, .bss 6656/6656
build/report.json measures         All: 30.61% fuzzy, 22.73% matched, 11.74% linked
                                   9938 / 28465 functions, 724 / 2042 files, total_functions 28465
build/goal/judge/HEAD              6c11880  (== this lane's HEAD; the baseline is this tree)
build/goal/judge/report.base.json  main/musyx/runtime/stream 18/18, complete: true
sha1sum build/G2ME01/main.dol      6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
```

`nm -n` on both objects — identical name list *and* identical offsets, so the fix is load-bearing
for every entry, not just for the symbol the linker asked for:

```
ours  build/G2ME01/src/musyx/runtime/stream.o     retail  build/G2ME01/obj/musyx/runtime/stream.o
00000000 streamInit                                00000000 streamInit
000000c8 SetHWMix                                  000000c8 SetHWMix
00000154 streamHandle                              00000154 streamHandle
00000a38 streamCorrectLoops                         00000a38 streamCorrectLoops
00000a3c streamKill                                00000a3c streamKill
00000ac0 GetPrivateIndex                           00000ac0 GetPrivateIndex
00000c20 sndStreamARAMUpdate                       00000c20 sndStreamARAMUpdate
000010b4 CheckOutputMode                           000010b4 CheckOutputMode
000010e8 SetupVolume                               000010e8 SetupVolume
000010f8 SetupVolumeAndPan                         000010f8 SetupVolumeAndPan
0000113c streamOutputModeChanged                   0000113c streamOutputModeChanged
0000126c sndStreamAllocEx                          0000126c sndStreamAllocEx
0000170c sndStreamAllocLength                      0000170c sndStreamAllocLength
00001750 sndStreamADPCMParameter                   00001750 sndStreamADPCMParameter
00001e4c sndStreamMixParameter                     00001e4c sndStreamMixParameter
000023b0 sndStreamFree                             000023b0 sndStreamFree
00002a84 sndStreamActivate                         00002a84 sndStreamActivate
000030dc sndStreamDeactivate                       000030dc sndStreamDeactivate
```

## Forcing a real rebuild — `flip_test` alone is not enough here

`./tools/flip_test.sh musyx/runtime/stream.c` printed `PASS -> kept as Matching` in **0.996 s**.
That is too fast to have recompiled and relinked (a forced rebuild below takes 11 s), and the
reason is in `tools/flip_test.sh`: `unit_info` prints the state as it is spelled in `configure.py`
(`Matching`, capital M) while the `case` at ~line 148 tests `matching`, lower case. So the
"already Matching - verifying in place" branch is dead, control falls to `*`, and the
`Object(NonMatching, "u"` → `Object(Matching, "u"` substitution is a no-op on an already-`Matching`
unit. `check()` then re-runs `configure.py` and `ninja`, but ninja has nothing to do, so the
sha1 comparison validates **the previous run's binary**. That is `docs/PROCESS_LESSONS.md`'s
stale-derived-input lesson again, one level down. `tools/` is out of bounds for a lane, so I did
not touch it; the measurement below is what actually proves the item.

Genuine recompile + relink, from a tree with the guards present:

```
$ rm -f build/G2ME01/main.dol build/G2ME01/main.elf build/G2ME01/src/musyx/runtime/stream.o
$ ninja
[1/7] MWCC build/G2ME01/src/musyx/runtime/stream.o
[3/7] LINK build/G2ME01/main.elf
real 0m10.951s ; exit 0
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
```

Negative control — the same two lines with `stream.c` reverted to `ef9e308^`, i.e. **without** the
four guards, everything else identical:

```
$ git show ef9e308^:extern/musyx/src/musyx/runtime/stream.c > extern/musyx/src/musyx/runtime/stream.c
$ rm -f build/G2ME01/src/musyx/runtime/stream.o && ninja
FAILED: [code=1] build/G2ME01/main.elf
### mwldeppc.exe Linker Error:
#   undefined: 'sndStreamMixParameter'
exit 1
```

then restored (`cp` of the backup, recompiled, relinked) → sha1 back to
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `git status --porcelain` empty. So the guards are
exactly what stands between this branch and a link failure, and they are present.

## The judge, verbatim

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item fix-musyx-stream-link (match) target=musyx/runtime/stream
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9938 -> 9938   linked 4896 -> 4896
  ok    check_symbol_names.py
  ok    All:  30.61% fuzzy, 22.73% matched, 11.74% linked (9938 / 28465 functions)
  ok    flip_test musyx/runtime/stream.c: PASS, Object(Matching) in configure.py
goal_check: PASS fix-musyx-stream-link
```

The gate's own diff line is `matched 9938 -> 9938  linked 4896 -> 4896`: nothing to raise, because
the work this item asked for is already counted. `build/gate-docs.log` ends `docs claims agree
with the tree`, so `docs/HANDOFF.md` is not stale either, and no doc needed editing.

Also clean in this tree:

```
./tools/unit_fit.sh musyx/runtime/stream.c
    .text claimed 14320 ours 14320 retail 14320 fits
    .bss  claimed  6656 ours  6656 retail  6656 fits
    .sbss claimed    16 ours    14 retail    16 SHORT by 2
    .sdata2 claimed  24 ours    20 retail    24 SHORT by 4
    no extra functions: our object defines only what the retail unit object does
python3 tools/check_decl_order.py --unit main/musyx/runtime/stream   ok, none out of retail order
python3 tools/check_symbol_names.py    checked 503 units; 0 declared names are missing
python3 tools/check_files_cmake.py     every configured DOL object is either in files.cmake
                                       or excluded with a reason
python3 tools/check_raw_offsets.py     ok: 152 raw-offset site(s) in 61 file(s), all documented
build/report.json total_functions     28465
```

`.sbss SHORT by 2` / `.sdata2 SHORT by 4` are the pre-existing trailing padding both prior
`match-stream` runs recorded (dtk gap symbols in the retail-derived object); `gate.sh` and
`goal_check` both pass with them and the DOL hashes retail.

## NEW

None filed. The item is complete; there is no sub-100% function to spell, no wall, and no port
symbol to define. Two things recorded here rather than filed, because the brief excludes them
from `NEW:`:

* the queue entry for this item is stale — it re-issues work that `ef9e308` and `ada6d97` already
  did, and it will keep doing so off that truncated `reason` string until the entry is retired
  (a driver/queue matter);
* `tools/flip_test.sh`'s `case` tests `matching`/`absent` lower case against a value that
  `unit_info` emits as `Matching`/`absent`…`Matching`, so an already-`Matching` unit takes the
  substitution branch with a no-op replace and never rebuilds (a tooling defect, in a path a lane
  may not edit). Until it is fixed, a lane verifying an already-`Matching` unit should
  `rm -f build/G2ME01/main.dol build/G2ME01/main.elf build/G2ME01/src/<unit>.o` first, as above.
