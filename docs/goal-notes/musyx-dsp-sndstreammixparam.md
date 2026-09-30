# musyx-dsp-sndstreammixparam — already resolved on this branch, nothing to change

`kind: match`, `target: musyx/runtime/stream.c`. **The unit is `Matching` and `flip_test` PASSes
before I touched anything.** The premise in `item.json` was stale: it was recorded against a tree
before `ef9e308` and that fix is already an ancestor of this lane's HEAD.

**I changed no source file. `git status --porcelain --untracked-files=all` is empty.**

## Re-measured (not recalled)

The premise: "`sndStreamMixParameter` (0x8038BBF0) is compiled out by `#if MUSY_VERSION <= 2.0.2`
in `extern/musyx/src/musyx/runtime/stream.c:759`, so main.elf fails to link and every REL module's
sha1 becomes uncheckable." Every part of that is false here:

```
configure.py:1297                       Object(Matching, "musyx/runtime/stream.c")
stream.c:758                            #if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 3)   <- not 2.0.2
git merge-base --is-ancestor ef9e308 HEAD   -> true
git branch -a --contains ef9e308       goal/decomp, master, goal/lane-1..9
```

`ef9e308` ("fix: commit the MusyX stream.c guards match-stream flipped against", 2026-09-30
00:07:56) is the fix `item.json` is describing, and it is on `goal/decomp` and `master`, not just
on a lane. Its message says exactly this failure had happened before: the guards live in `extern/`,
which the goal loop's `stage_change` pathspec never staged, so `goal/decomp` was unlinkable until
that commit. `item.json` was queued at 02:38:59Z, after it.

Symbol-level proof, `build/binutils/powerpc-eabi-nm -n`:

```
build/G2ME01/src/musyx/runtime/stream.o   00001e4c T sndStreamMixParameter
build/G2ME01/main.elf                     8038bbf0 T sndStreamMixParameter     <- defined, not undefined
grep sndStream build/goal/judge/undef.base.txt -> no match (was never undefined at HEAD)
```

Both objects define the same 13 `.text` symbols at identical offsets (`sndStreamMixParameter`
@0x1e4c, `sndStreamFree` @0x23b0, `sndStreamActivate` @0x2a84, ...).

`build/report.json` for `main/musyx/runtime/stream`: 100.0 fuzzy, 100.0 matched code,
**18/18 functions**, `complete: true`.

## Verified

```
./tools/decomp_build.sh musyx/runtime/stream.c
All:  30.61% fuzzy, 22.73% matched, 11.74% linked (9938 / 28465 functions)

./tools/flip_test.sh musyx/runtime/stream.c
  PASS  -> kept as Matching
kept: 1 / 1   failed: 0   skipped: 0

./tools/gate.sh build/goal/judge/report.base.json
  GATE PASS  c749b00+0 changed          (matched 9938 -> 9938, linked 4896 -> 4896)
  every line ok; DOL sha1 6ef9b491..., 86/86 RELs cmp-equal

sha1sum build/G2ME01/main.dol          6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
all 86 orig/G2ME01/files/RelProd/*.rel cmp -> all equal
./tools/unit_fit.sh musyx/runtime/stream.c
  .text claimed 14320 ours 14320 retail 14320 fits; no extra functions
python3 tools/check_decl_order.py --unit main/musyx/runtime/stream -> none out of retail order

tools/goal_check.sh build/goal/item.json
  ok  no judge-owned path touched
  ok  gate.sh
  ok  counts: matched 9938 -> 9938   linked 4896 -> 4896
  ok  check_symbol_names.py
  ok  All:  30.61% fuzzy (9938 / 28465)
  ok  flip_test musyx/runtime/stream.c: PASS, Object(Matching) in configure.py
  goal_check: PASS musyx-dsp-sndstreammixparam
```

`.sbss SHORT by 2` / `.sdata2 SHORT by 4` from `unit_fit.sh` are the pre-existing retail-derived
trailing padding (`docs/goal-notes/match-stream.md`); the DOL hashes retail.

## Lesson

For a `match` item whose `reason` names a *link* symptom, check `git merge-base --is-ancestor
<suspected-fix> HEAD` before editing anything. A queued reason is a measurement taken when the item
was filed, and this one predates the commit that fixed it — the queue outlived the fix. A second
guard-mangling attempt here would have re-broken 84 RELs, which is what `ef9e308`'s message warns
about. Re-measure first; if the unit already flips, the correct diff is empty.

## NEW

Filed from a survey I ran while checking whether the same defect class bites elsewhere: comparing
`nm -n build/G2ME01/src/<unit>.o` against `build/G2ME01/obj/<unit>.o` for all 31 `main/musyx/*` units.
19 differ, but 18 are benign (extra functions our object defines that the linker drops, all on
already-`Matching` units). One is a real instance of exactly this item's defect class:

NEW: match-musyx-hw-dspctrl-saladdstudioinput | match | musyx/runtime/hw_dspctrl.c | unbalanced `#if MUSY_VERSION <= 2.0.2` at hw_dspctrl.c:2081 closes at 2142 and swallows salAddStudioInput (2113) and salRemoveStudioInput (2127), which retail defines; .text is 0x3518 (14104) vs claimed 0x3718 and our salHandleAuxProcessing sits at retail's salAddStudioInput offset 0x3518.

### Evidence for that NEW (measured out-of-tree, repo untouched)

`extern/musyx/src/musyx/runtime/hw_dspctrl.c` guard at 2081 is the 3-line
`#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)` (false at 2.0.3). Inside it:
2084 `#if MUSY_TARGET != MUSY_TARGET_PC` / `salReconnectVoice` (2085) / `#endif` (2111), then
`salAddStudioInput` (2113) and `salRemoveStudioInput` (2127) are left inside the dead 2.0.2 guard,
which closes only at 2142. So both are compiled out. Retail defines `salAddStudioInput` @0x3518
(`nm build/G2ME01/obj/musyx/runtime/hw_dspctrl.o`); ours does not define it at all, and ours puts
`salHandleAuxProcessing` at 0x3518 — exactly retail's `salAddStudioInput` slot.

The fix is to close the 2.0.2 guard right after line 2111 and reopen it before `salRemoveStudioInput`
at 2127, so only `salAddStudioInput` escapes. I compiled that variant in a scratch copy
(`.tmp/opencode/dspprobe/`, since deleted) with the exact `build.ninja` mwcc_sjis flags:

```
ours   (after fix): salInitDspCtrl 0x0000 ... salDeactivateVoice 0x34b8
                     salAddStudioInput 0x3518   salHandleAuxProcessing 0x35c0
retail:              identical, symbol for symbol and offset for offset
.text ours 0x3718 = retail 0x3718 = 14104 = exactly the claimed split 0x80397EF8..0x8039B610
objcopy --only-section=.text, then cmp -> TEXT IDENTICAL
```

`.rodata` 50 vs 56 and `.data` 6738 vs 6744 are 6 bytes of trailing retail gap padding each (ours
is a byte-identical prefix), the same artifact `unit_fit.sh` reports on `Matching` units. The unit is
`Object(NonMatching, ...)` at `configure.py:1306`, 15/16 functions, 98.81%. Next run should apply
the guard edit, then `unit_fit.sh` + `flip_test.sh musyx/runtime/hw_dspctrl.c`; a byte-identical
`.text` at the exact claimed size makes a flip plausible. Note this one is a genuine `#if`/`#endif`
**rebalance**, not a version-number edit — the 2.0.2 guard is legitimately meant to exclude
`salReconnectVoice` and `salRemoveStudioInput` (neither is in retail), only `salAddStudioInput`
belongs outside it.
