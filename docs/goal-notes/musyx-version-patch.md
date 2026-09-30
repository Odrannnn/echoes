# musyx-version-patch — DONE 2026-09-30, lane 5 (wt-mp2-goal-L5): stale item, no change needed or made

**The breakage the reason describes was already repaired on this branch by `ef9e308` ("fix: commit the
MusyX stream.c guards match-stream flipped against")**, an ancestor of my HEAD `fc16aa3`. I
re-measured every claim in the reason instead of trusting it, including the reason's own proposed root
cause, and that root cause is now **measured to be wrong**: `-DMUSY_VERSION_PATCH=3` does *not* compile
out `sndStreamMixParameter` in this tree, and setting the pin to 2 makes the unit strictly worse.
`git status --porcelain --untracked-files=all` is empty before and after everything below.
`./tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS musyx-version-patch`** (19.6 s, exit 0).

This is the third item in a row on this target (`match-stream`, `match-musyx-runtime-stream`,
`fix-musyx-stream-link`, now this one). The queue entry should be retired; the derivation is below so
the next reader does not repeat it.

## The reason's claim, point by point

`reason` = *"`MusyX()` defaults `patch=3` (configure.py:358) so `-DMUSY_VERSION_PATCH=3` makes
MUSY_VERSION 2.0.3 and compiles out `sndStreamMixParameter` (stream.c:759), which retail has and which
CDSPStreamManager.cpp's retail object calls — main.elf will not link, so no REL module's sha1 is
checkable on this branch"*.

| claim | measured here | verdict |
|---|---|---|
| `MusyX()` defaults `patch=3` | `configure.py:358` `def MusyX(objects, mw_version="GC/1.3.2", major=2, minor=0, patch=3)`; the single `MusyX([...])` group at `configure.py:1291` holds all ~179 MusyX objects, `stream.c` at `configure.py:1297` | true |
| `patch=3` compiles out `sndStreamMixParameter` | **false.** `stream.c:758` is now `#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 3)`, which is *true* at 2.0.3, and the object we build defines `sndStreamMixParameter @0x1e4c` | false (was true at `ef9e308^`) |
| retail `CDSPStreamManager.o` calls it | true and load-bearing: `powerpc-eabi-nm -u build/G2ME01/obj/Kyoto/Audio/CDSPStreamManager.o` → `U sndStreamMixParameter` | true |
| `main.elf` will not link | false. Forced `rm -f build/G2ME01/main.dol build/G2ME01/main.elf build/G2ME01/src/musyx/runtime/stream.o && ninja` → `[6/7] CHECK config/G2ME01/build.sha1` / `87 files OK` / `All: 30.61% fuzzy, 22.74% matched, 11.74% linked (724 / 2042 files)`, exit 0, `real 0m8.513s` | false |
| no REL module's sha1 is checkable | false. All 86 are checkable and all match (below) | false |

The line numbers in the reason (`stream.c:759` = the `void sndStreamMixParameter(` line, i.e. the body
just inside the guard) date it to the pre-`ef9e308` tree, when the guard read
`MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)`. `ef9e308` changed the three thresholds to `(2, 0, 3)`,
so 2.0.3 now selects the 2.0.2-shaped code retail actually ships and excludes the 2.0.3-only functions
retail does not define.

## The reason's own root cause, measured three ways: `patch=2` is a wall

I compiled `extern/musyx/src/musyx/runtime/stream.c` by hand at three pins (the exact `mwcc_sjis`
command from `build.ninja:18990`, only `-DMUSY_VERSION_PATCH` changed) and compared the function sets to
`build/G2ME01/obj/musyx/runtime/stream.o`:

```
== PATCH=3 (configured) ==   .text 14320   .bss 6670
streamInit SetHWMix streamHandle streamCorrectLoops streamKill GetPrivateIndex sndStreamARAMUpdate
CheckOutputMode SetupVolume SetupVolumeAndPan streamOutputModeChanged sndStreamAllocEx
sndStreamAllocLength sndStreamADPCMParameter sndStreamMixParameter sndStreamFree sndStreamActivate
sndStreamDeactivate

== PATCH=2 ==                .text 15848   .bss 6672
... same 18, PLUS GeneratePublicID, sndStreamCallbackFrq, sndStreamGetARAMAddress,
sndStreamAllocStereo

== PATCH=4 ==                .text 17388   .bss 6670
... same 18 MINUS sndStreamMixParameter, PLUS sndStreamMixParameterEx, sndStreamFrq,
sndStreamLPFParameter, sndStreamLPFDefaultParameter

== RETAIL ==                 .text 14344   .bss 6672
the 18, exactly
```

(`size` on the retail object reports `.text 14344`, 24 more than ours; that is the trailing dtk padding
`unit_fit.sh` reports as the pre-existing `.sbss SHORT by 2` / `.sdata2 SHORT by 4`.)

So:

* **`patch=3` is the correct pin and it is already producing retail's exact function set.** The item's
  premise is inverted: 2.0.3 is what the *guards* make correct, not what breaks it.
* **`patch=2` cannot work.** The `streamKill` / `sndStreamMixParameter` bodies are the 2.0.2 *spelling*,
  but `stream.c` also has decomp-added `#elif MUSY_VERSION == MUSY_VERSION_CHECK(2, 0, 3)` (line 350),
  `#if == …(2, 0, 2)` (431, 832) and `#if != …(2, 0, 3)` (910) guards, and at 2.0.2 those select the
  wrong side: four functions retail does not define appear in the object. By `unit_fit.sh`'s own rule
  ("a unit whose object emits functions the retail object does not define can never be `Matching`")
  that is disqualifying, and `.text` grows by 1528 bytes.
* **`patch=4` reproduces the original breakage** — the same shape as the pre-`ef9e308` tree, dropping
  `sndStreamMixParameter` and adding the three functions retail lacks.
* There is no per-file escape hatch for the version: all MusyX objects sit in one `MusyX([...])` group
  with one set of `-D`s, so `patch` cannot be lowered for `stream.c` alone without splitting the
  library (which would reorder the DOL link). The guards in the vendored source are the right shape of
  fix, and they are committed.

`MUSY_VERSION` 2.0.3 is also load-bearing for the rest of the SDK, which is why the pin stays:
`ninja`'s SDK line reads `98.67% fuzzy, 98.64% matched, 94.78% linked (176 / 179 files)` at 2.0.3.

## The unit, measured

```
configure.py:1297                       Object(Matching, "musyx/runtime/stream.c")
build/report.json main/musyx/runtime/stream
    18 / 18 functions, fuzzy 100.0, matched code 100.0, metadata.complete: true
    no sub-100% function
diff <(powerpc-eabi-nm -n build/G2ME01/src/musyx/runtime/stream.o | grep ' T ') \
     <(powerpc-eabi-nm -n build/G2ME01/obj/musyx/runtime/stream.o  | grep ' T ')   -> no differences
```

Same 18 `T` symbols at the same offsets on both sides, `sndStreamMixParameter @0x1e4c` defined, and
none of `sndStreamMixParameterEx` / `sndStreamFrq` / `sndStreamLPFParameter` invented.

## The acceptance test, and the RELs the reason says are uncheckable

```
./tools/flip_test.sh musyx/runtime/stream.c   ->  PASS -> kept as Matching  (kept: 1/1, exit 0)
```
`flip_test` finished in 0.94 s, which is too fast to have recompiled — `tools/flip_test.sh`'s `case`
tests `matching` lower case against a value `unit_info` spells `Matching`, so an already-`Matching`
unit takes the no-op substitution branch and `ninja` has nothing to do. That is the defect already
recorded in `docs/goal-notes/fix-musyx-stream-link.md`; it is why the forced `rm -f … && ninja` above is
the measurement that counts. `tools/` is out of bounds for a lane, so I did not touch it.

Independently of the gate, in this tree:

```
config.yml REL entries: 86   sha1 mismatches: 0 []
cmp -equal to orig/G2ME01/files/RelProd: 86   different: 0
main.dol 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
```

86 for 86, so "no REL module's sha1 is checkable on this branch" does not hold on `fc16aa3`.

Also clean:

```
./tools/unit_fit.sh musyx/runtime/stream.c    .text 14320/14320 fits; .bss 6656/6656 fits;
                                              no extra functions
python3 tools/check_decl_order.py --unit main/musyx/runtime/stream
                                              ok: 1 unit(s) checked, none emits its functions out of
                                              retail order
python3 tools/check_symbol_names.py            checked 503 units; 0 declared names are missing
```

`.sbss SHORT by 2` / `.sdata2 SHORT by 4` are the pre-existing retail-derived trailing padding every
`Matching` SDK unit reports; the DOL hashes retail, so they do not stop the flip.

## The judge, verbatim

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item musyx-version-patch (match) target=musyx/runtime/stream.c
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9939 -> 9939   linked 4896 -> 4896
  ok    check_symbol_names.py
  ok    All:  30.61% fuzzy, 22.74% matched, 11.74% linked (9939 / 28465 functions)
  ok    flip_test musyx/runtime/stream.c: PASS, Object(Matching) in configure.py
goal_check: PASS musyx-version-patch
```

`matched 9939 -> 9939`: nothing to raise, because this item's work was counted by `ef9e308` and the
`progress-prime1-cscriptactor` commit above it. The baseline at `build/goal/judge/HEAD` is `fc16aa3` —
this lane's own HEAD — and `build/goal/judge/record-gate.log` already reads
`configure ok / ninja + build.sha1 ok / hashes vs config.yml ok / report ok` at that commit.

## Recorded rather than filed (none of these raise a count)

* **Queue hygiene, driver work:** this target has now drawn four items in a row, all of which the
  `ef9e308` guards made moot, and each was re-derived from an inherited `reason` string rather than from
  the tree. The `reason` here is a `NEW:` line that `progress-rel-head-lumite` filed against a branch
  head that predates `ef9e308`. `docs/PROCESS_LESSONS.md`'s "inherited reasons" lesson, third instance.
* **Lesson for the next MusyX item:** the vendored `extern/musyx` is a *later* revision than the
  snapshot MP2 shipped, so at a given `MUSY_VERSION` a vendored file can both compile out a function
  retail defines and compile in ones retail does not. The way to settle it is the three-pin compile
  above — do not reason from the `#if` text alone; a threshold that looks wrong can be exactly what makes
  the object byte-equal, and the decomp-added `==`/`!= 2.0.3` guards mean "lower the pin" is not a fix.
* **Cosmetic, not touched:** the comment at `stream.c:793-796` explaining these guards has a very
  over-long third line (two sentences run together). Re-wrapping it is unrelated to this item, so I left
  it; `tools/`-adjacent tidying of vendored comments is noise in a reviewer's diff.
* `unit_fit.sh` flags `.line` (2798) and `.mwcats.text` (144) as not claimed by `splits.txt` for this
  unit. Pre-existing, consistent with every other unit, and the DOL hashes retail.

## NEW

None filed. Nothing is blocked: the unit is `Matching`, 18/18 at 100.0%, `complete: true`, the branch
links, and all 86 REL module sha1s check. The two real units this work touches the neighbourhood of
(`Kyoto/Audio/CDSPStreamManager.cpp`, still `NonMatching`, and `musyx/runtime/hw_dspctrl.c`, the last
`NonMatching` object in the SDK group) are known work, not something this item discovered, and naming
them would only duplicate the queue.

WALL: none — every function in the unit is at 100.0% and the flip passes in place. (The `patch=2`
spelling measured above is a wall on the *item's hypothesis*, not on a function, so it is recorded here
rather than as a `WALL:` line.)
