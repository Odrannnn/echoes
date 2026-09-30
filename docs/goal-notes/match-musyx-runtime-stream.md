# match-musyx-runtime-stream — DONE 2026-09-30, lane 7 (wt-mp2-goal-L7)

**No source change was needed or made.** The item was queued from a stale observation: the
breakage it describes (a flipped `Matching` unit whose `extern/` guards were never committed) has
already been repaired on this branch by **`ef9e308` "fix: commit the MusyX stream.c guards
match-stream flipped against"**, which is an ancestor of HEAD `c749b00`. I re-measured instead of
trusting that, and the unit is genuinely complete.

## Measured in this tree (not recalled)

`git merge-base --is-ancestor ef9e308 HEAD` → true. Tree clean at HEAD `c749b00`.

`configure.py:1297` reads `Object(Matching, "musyx/runtime/stream.c")`. The four guards the
item's reason asks for are all present in `extern/musyx/src/musyx/runtime/stream.c`:

| line | guard | covers |
|---|---|---|
| 336 | `#elif MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 3)` | `streamKill` direct-index body |
| 758 | `#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 3)` | `sndStreamMixParameter` |
| 797 (closed 878) | `#if MUSY_VERSION > MUSY_VERSION_CHECK(2, 0, 3)` | `sndStreamMixParameterEx` … `sndStreamFrq`, one guard spanning the `#pragma pop` |
| 880 | `#if MUSY_VERSION > MUSY_VERSION_CHECK(2, 0, 3)` | `sndStreamLPFParameter` (+ nested `sndStreamLPFDefaultParameter`) |

`build/report.json` for `main/musyx/runtime/stream`: **100.00% fuzzy, 100.00% matched code,
18/18 functions**, `metadata.complete: true`, no function below 100%.

The symbol sets agree exactly — the thing the whole item was about:

```
diff <(nm -n build/G2ME01/src/musyx/runtime/stream.o | grep ' T ') \
     <(nm -n build/G2ME01/obj/musyx/runtime/stream.o | grep ' T ')   ->  no differences
```

i.e. same 18 `T` symbols at the same addresses, `sndStreamMixParameter @0x1e4c` present, and none
of `sndStreamMixParameterEx` / `sndStreamFrq` / `sndStreamLPFParameter` invented. `.text` 14320 =
14320. So the undefined-symbol break the reason describes cannot occur from this tree.

## The acceptance test, run here

```
./tools/flip_test.sh musyx/runtime/stream.c   ->  PASS -> kept as Matching   (kept: 1 / 1)
sha1sum build/G2ME01/main.dol                 ->  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
86 x cmp vs orig/G2ME01/files/RelProd/*.rel   ->  86 equal, 0 different
./tools/unit_fit.sh musyx/runtime/stream.c    ->  .text 14320 = 14320 fits; no extra functions
python3 tools/check_decl_order.py --unit main/musyx/runtime/stream -> none out of retail order
grep -c undefined build/flip-ninja.log        ->  0
./tools/goal_check.sh build/goal/item.json    ->  goal_check: PASS match-musyx-runtime-stream
```

The judge, verbatim, with an empty diff:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9938 -> 9938   linked 4896 -> 4896
  ok    check_symbol_names.py
  ok    All:  30.61% fuzzy, 22.73% matched, 11.74% linked (9938 / 28465 functions)
  ok    flip_test musyx/runtime/stream.c: PASS, Object(Matching) in configure.py
goal_check: PASS match-musyx-runtime-stream
```

I ran the judge to get a real verdict rather than asserting one; it left `git status` clean
(`build/` is gitignored; `gate.sh` rewrote nothing tracked).

`.sbss SHORT by 2` / `.sdata2 SHORT by 4` from `unit_fit.sh` are the pre-existing retail-derived
trailing padding (dtk gap symbols) that most `Matching` units report; the DOL hashes retail, so
they do not stop the flip.

## Notes for the driver

- `docs/goal-notes/match-stream.md` is still accurate but was written before the repair and does
  not say the guards are now committed in-tree; this entry is the follow-up record.
- `unit_fit.sh` flags `.line` and `.mwcats.text` as not claimed by `splits.txt` for this unit.
  Pre-existing, consistent with every other unit, and the DOL hashes retail — not filed as a
  `NEW:` item, since it raises no count.
- The durable fix the reason asks for (verify a carried flip commit contains the source files its
  notes claim) is driver work and out of scope for a worker lane: the affected file lives in
  `extern/`, which the goal loop's `stage_change` pathspec does not stage.

## NEW

None filed. Nothing is blocked and there is no wall: the unit is already `Matching`,
18/18 at 100.00%, `complete: true`, and `flip_test.sh` passes it in place.

WALL: none.