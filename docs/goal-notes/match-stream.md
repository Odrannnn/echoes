# match-stream — DONE 2026-09-29

`musyx/runtime/stream` (DOL) is now **18/18 at 100.00%**, `Object(Matching, "musyx/runtime/stream.c")`,
`tools/flip_test.sh musyx/runtime/stream.c` → **PASS** (kept as Matching).

## What was actually wrong

Not the two functions the item named. Both were symptoms of one thing: the vendored
`extern/musyx/src/musyx/runtime/stream.c` is a **later MusyX revision than the one Metroid
Prime 2 shipped**, so at the `MUSY_VERSION=2.0.3` this tree configures, it emits three
functions the DOL does not contain and suppresses one the DOL does contain.

Measured, from `build/binutils/powerpc-eabi-nm` on each object (`-n`, `.text` addresses):

| | our object @2.0.3 (before) | retail `build/G2ME01/obj/...` |
|---|---|---|
| after `sndStreamADPCMParameter` @0x1750 | `sndStreamMixParameterEx` @0x1e60 | `sndStreamMixParameter` @0x1e4c |
| | `sndStreamFrq` @0x23c8 | `sndStreamFree` @0x23b0 |
| | `sndStreamLPFParameter` @0x2a30 | `sndStreamActivate` @0x2a84 |

`.text` was 17316 bytes against retail's 14320. The 1380-byte `sndStreamMixParameter` the
DOL defines was guarded out by `#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)`, so objdiff
had no function at 0x1e4c to pair with and reported 0%. `streamKill` had the same cause from
the other side: retail's body is the direct-index form `si = &streamInfo[voice]`, which sat
behind the same `<= 2.0.2` guard, so the compiler emitted the 2.0.3 search loop instead
(62.88%).

Corroboration that the DOL really is 2.0.3 for this library, not 2.0.2: the DOL defines
**none** of `GeneratePublicID`, `sndStreamCallbackFrq`, `sndStreamGetARAMAddress`,
`sndStreamAllocStereo`, `sndStreamMixParameterVolume`, `sndStreamLPFDefaultParameter` — all
of which a real `MUSY_VERSION=2.0.2` compile does emit. I measured that directly: compiling
the file with `-DMUSY_VERSION=((2<<16)|(0<<8)|2)` produces **28** text functions against
retail's 18, so 2.0.2 is not the answer either. (Note for the next run: `-DMUSY_VERSION_PATCH=2`
fails outright — `musyx/version.h` is `#ifndef`-guarded, and MWCC treats the duplicate `-D` as
a hard error, "macro 'MUSY_VERSION_PATCH' redefined". `-DMUSY_VERSION=MUSY_VERSION_CHECK(2,0,2)`
also fails — MWCC's `-D` parser splits on the comma. `((2<<16)|(0<<8)|2)` is the spelling
that works.)

The three absent functions are referenced by nothing anywhere in the tree except
`extern/musyx-port`, which is the port's own separate copy and is not built for the DOL.
`src/Kyoto/Audio/CDSPStreamManager.cpp:426` is the only caller of the API, and it calls
`sndStreamMixParameter` — the function the flip had been hiding.

## The change

Four guards in `extern/musyx/src/musyx/runtime/stream.c`, one comment, and the
`NonMatching` → `Matching` flip in `configure.py:1275`:

1. `streamKill`, line 336: `#elif MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)` → `2, 0, 3`,
   so the direct-index switch body is the one compiled.
2. `sndStreamMixParameter`, line 758: `#if ... <= MUSY_VERSION_CHECK(2, 0, 2)` → `2, 0, 3`.
3. `sndStreamMixParameterEx` (line 798) and `sndStreamFrq` (line 855) wrapped in a new
   `#if MUSY_VERSION > MUSY_VERSION_CHECK(2, 0, 3)`.
4. `sndStreamLPFParameter` (line 881): `#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)` →
   `> MUSY_VERSION_CHECK(2, 0, 3)`. This also covers the nested `sndStreamLPFDefaultParameter`.

A version above 2.0.3 is the only expressible cut, and it is the literal truth about the
vendored file: these four functions post-date the snapshot the game shipped. I left the inner
`#if MUSY_VERSION != MUSY_VERSION_CHECK(2, 0, 3)` on `sndStreamLPFDefaultParameter` alone —
now always true under the new outer guard, but it records upstream's own condition.

No function body was altered; the diff is guards and a comment. `extern/musyx-port` is a
separate copy and is untouched, so the PC port still gets all of these.

## Verified

```
./tools/flip_test.sh musyx/runtime/stream.c      -> PASS -> kept as Matching
sha1sum build/G2ME01/main.dol                    -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
all 86 orig/G2ME01/files/RelProd/*.rel cmp       -> all equal
./tools/probe_sources.sh                        -> probe: 736 files, 0 failed, 0 errors
python3 tools/check_symbol_names.py             -> checked 484 units; 0 declared names missing
./tools/unit_fit.sh musyx/runtime/stream.c      -> .text 14320 = 14320 fits; no extra functions
python3 tools/check_decl_order.py --unit main/musyx/runtime/stream -> none out of retail order
python3 tools/check_files_cmake.py              -> every configured DOL object accounted for
```

`build/report.json` for `main/musyx/runtime/stream`: 100.00% fuzzy, 100.00% matched code,
18/18 functions, `metadata.complete: true`, no function below 100%. The `All:` line went
**29.12% → 29.14% fuzzy, 9479 → 9481 matched functions, 714 → 715 linked files**; SDK Code
175/179 → 176/179 linked. Nothing anywhere got worse.

`unit_fit.sh` still reports `.sbss SHORT by 2` and `.sdata2 SHORT by 4`; that is pre-existing
trailing padding in the retail-derived object (it carries dtk gap symbols
`gap_10_8041A24E_sbss` and zero fill), not a content difference, and it does not stop the
flip — the DOL hashes retail. Most `Matching` units show the same.

## Lesson worth keeping

For a vendored-SDK unit the first question is not "which function is sub-100%". It is
**"does my object define the same set of functions as retail's"** — compare
`nm -n build/G2ME01/src/<unit>.o` against `nm -n build/G2ME01/obj/<unit>.o`. A function
objdiff reports at 0% with no score key is usually *absent*, not wrong, and every offset
after it is shifted. Here one missing definition explained both symptoms and the 3 KB size
gap. `MUSY_VERSION` in `configure.py`'s `MusyX()` helper is the lever for these units, and
`MUSY_TARGET`/`MUSY_VERSION` guards are where a later vendored revision shows up first.

## NEW

None filed. The item is complete, nothing is blocked, and there is no wall: the two
sub-100% functions needed no spelling work at all.

---

# match-stream — re-run 2026-09-29, lane 1 (wt-mp2-goal-L1)

The item was requeued on lane 1 with the original reason still in `item.json`, and the L1 worktree
at `8039914` had `configure.py:1275` still `NonMatching` and `report.json` still showing
`streamKill` at 62.878788% and `sndStreamMixParameter` with no score key at all. So the previous
run's edits were never in this tree. **The diagnosis above was correct and the fix reproduces
exactly**; nothing below contradicts it. This entry records what I re-measured in the lane.

## Re-measured here, not recalled

`build/binutils/powerpc-eabi-nm -n` on both objects, `.text` addresses — identical to the table
above, ours @2.0.3 vs retail:

| ours | retail |
|---|---|
| `sndStreamMixParameterEx` @0x1e60 | `sndStreamMixParameter` @0x1e4c |
| `sndStreamFrq` @0x23c8 | `sndStreamFree` @0x23b0 |
| `sndStreamLPFParameter` @0x2a30 | `sndStreamActivate` @0x2a84 |

`report.json` for `main/musyx/runtime/stream` before: 90.02095% fuzzy, **16/18** functions,
`complete: false`.

## The change (same four guards, same four lines)

`extern/musyx/src/musyx/runtime/stream.c`:

- 336 `#elif MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)` -> `2, 0, 3` (streamKill)
- 760 `#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)` -> `2, 0, 3` (sndStreamMixParameter)
- 795 new `#if MUSY_VERSION > MUSY_VERSION_CHECK(2, 0, 3)` around `sndStreamMixParameterEx`,
  closed at 832
- 855 new `#if MUSY_VERSION > MUSY_VERSION_CHECK(2, 0, 3)` around the `#pragma push` /
  `sndStreamFrq` / `#pragma pop` block, closed after the pop
- 882 `#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)` -> `> MUSY_VERSION_CHECK(2, 0, 3)`
  (sndStreamLPFParameter; also covers the nested sndStreamLPFDefaultParameter)

Two short comments added explaining the cut. Preprocessor nesting re-checked with a depth pass:
final depth 0, never negative.

`configure.py:1275` `NonMatching` -> `Matching`.

No function body was edited. `extern/musyx-port` is a separate copy and is untouched.

## Verified in the lane

```
./tools/decomp_build.sh musyx/runtime/stream -> main/musyx/runtime/stream: 100.00% fuzzy, 100.00% matched (18 / 18 functions)
./tools/flip_test.sh musyx/runtime/stream.c  -> PASS -> kept as Matching
./tools/unit_fit.sh musyx/runtime/stream.c  -> .text 14320 = 14320 fits; no extra functions
python3 tools/check_decl_order.py --unit main/musyx/runtime/stream -> none out of retail order
./tools/probe_sources.sh  -> probe: 736 files, 0 failed, 0 errors; link: LINKED (254 undefined, 0 duplicates)
python3 tools/check_symbol_names.py -> ok (goal_check)
sha1sum build/G2ME01/main.dol -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
tools/goal_check.sh build/goal/item.json -> goal_check: PASS match-stream
```

The judge, verbatim:

```
ok  no judge-owned path touched
ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 9505 -> 9507   linked 4818 -> 4836
ok  check_symbol_names.py
ok  All:  29.18% fuzzy, 21.33% matched, 11.60% linked (9507 / 28465 functions)
ok  flip_test musyx/runtime/stream.c: PASS, Object(Matching) in configure.py
```

`unit_fit.sh` still reports `.sbss SHORT by 2` and `.sdata2 SHORT by 4` — pre-existing
retail-derived trailing padding (dtk gap symbols), same as most Matching units, and the DOL
hashes retail.

`gate.sh` rewrote the `docs/HANDOFF.md` state block (9505 -> 9507, linked 4818 -> 4836). I
reverted that file: the driver rewrites those counts itself and an edit to it is discarded.

## NEW

None. The item is complete in this tree; there is no wall and nothing is blocked.
