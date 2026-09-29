# match-cmetaanimrandom — DONE, unit flipped to `Matching`

**Target:** `Kyoto/Animation/CMetaAnimRandom` (DOL unit, `.text` 0x80295428..0x80295B20 = 1784 B,
`.data` 0x803B93D0..0x803B93F0 = 32 B). 13 functions.
**Result:** 13/13 functions, 100.0000% fuzzy, `complete: true`; `tools/flip_test.sh` **PASS**,
kept as `Matching`.

## What the two open functions were

`build/report.json` said `uninitialized_copy_n<pair<rc_ptr<IMetaAnim>,int>*,
pair<rc_ptr<IMetaAnim>,int>*>` was at **0%** and the `vector` copy constructor at **46.36%**. Both
were the same defect, and it was not a body problem:

- Retail's object defines the 80-byte `rstl::uninitialized_copy_n` **out of line**, immediately
  after the vector copy constructor that calls it (`0x802956DC` copy ctor, 132 B -> `0x80295760`).
- Ours had **no such symbol at all**: the project-wide `-pragma "inline_max_size(125)"` inlines an
  80-byte callee, so the loop was inlined into the copy constructor. That made the copy ctor 200
  bytes with the loop in it and dropped the 80-byte function, which is exactly the 0% + 46.36%
  pair. The inlined loop body was already byte-identical to retail's out-of-line one — only the
  call and the function were missing.

## The fix: `#pragma inline_max_size(121)` at the top of the unit

`src/Kyoto/Animation/CMetaAnimRandom.cpp`, one pragma + comment, before the includes. Nothing else
in the source changed; no shared header was touched, so no other unit moves.

`configure.py:888` is `Object(Matching, "Kyoto/Animation/CMetaAnimRandom.cpp")` — that line is
`flip_test.sh`'s own output, not a hand edit.

### Measured window (every value a full rebuild + objdiff of the unit)

| `#pragma inline_max_size` | .text | `uninitialized_copy_n` emitted | functions | unit fuzzy | not-100% |
|---|---|---|---|---|---|
| 0    | 0x1144 | yes | — | — | many extras; everything stops being inlined |
| 64   | 0xa6c  | yes | 8/13  | 78.73%  | `uninitialized_copy` 0%, `reserve` 66.7%, `CreateRandomData` 71.57%, `destroy` 40.25%, copy ctor not listed |
| 78–84| 0x9fc  | yes | 10/13 | 87.92%  | `reserve` 66.7%, `CreateRandomData` 71.57%, `destroy` 40.25% |
| 100–120 | 0x984 | yes | 12/13 | 96.78% | `destroy` 40.25% (became a forwarder) |
| **121** | **0x950** | **yes** | **13/13** | **100.0000%** | **none** |
| 122–125 (project default) | 0x944 | no | 11/13 | 91.55% | copy ctor 46.36%, `uninitialized_copy_n` 0% |

The window is one value wide on the high side and ~21 values wide on the low side. Below 121 the
twin `destroy_impl(It,It)` also stops being inlined, so `destroy<pointer_iterator<...>>` collapses
to a 0x38-byte forwarder and drops to 40.25% — the same COMDAT-forwarder failure
`docs/RUNNING_THE_DECOMP.md` records for `Kyoto/CResLoaderInsert.cpp` and `CSequenceHelper.cpp`.
**So `inline_max_size` is a two-sided knife here: 121 is the only value, and 120 loses a different
function than 122 does.** If a later edit to this unit breaks it, re-measure the window rather than
assuming 121 still holds.

## Things tried that did NOT work (do not repeat)

- **The `template <> inline void rstl::vector<T>::clear()` trick** from
  `docs/RUNNING_THE_DECOMP.md` ("inline caller -> specialisation in the unit"), applied to
  `rstl::uninitialized_copy_n` in `namespace rstl` in this unit. mwcceppc 2.7 **accepts** the
  explicit specialisation of the `static` function template (proved: changing the body to
  `cur = dest + 1` showed up in the copy ctor) but then inlines it **whatever the pragma says** —
  `#pragma inline_max_size(0)` around it changed nothing. An explicit `inline` specialisation is
  always inlined, so it can never produce retail's out-of-line copy. Not a wall, just the wrong tool.
- `#pragma inline_max_size(0)` scoped around the specialisation: no effect on the call.
- The inlined loop body itself: already byte-exact. No body edit was needed or made.

## Gates (all measured on the final tree)

```
sha1sum build/G2ME01/main.dol        6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (expected, unchanged)
./tools/probe_sources.sh             probe: 736 files, 0 failed, 0 errors; link: LINKED (254 undefined, 0 duplicates)
python3 tools/check_symbol_names.py  checked 484 units; 0 declared names are missing from their object
./tools/flip_test.sh Kyoto/Animation/CMetaAnimRandom.cpp   PASS -> kept as Matching
```

`report.json` for the unit: `fuzzy_match_percent 100.0`, `13/13 functions`, `complete_code 1784/1784`,
`complete_data 32/32`, `metadata.complete true`.

`./tools/decomp_build.sh` full run — the All line did not fall:

```
before   All: 29.16% fuzzy, 21.31% matched, 11.38% linked (715 / 2025 files)  Code 9505 / 28465 functions
after    All: 29.16% fuzzy, 21.31% matched, 11.41% linked (716 / 2025 files)  Code 9507 / 28465 functions
         DOL  matched 32.25% -> 32.26%, 8094 -> 8096 / 16726 functions;  Game Code 36.19% -> 36.20%
         Modules 1411 / 11739 and SDK 1293 / 1308 unchanged
```

RELs: all 86 `cmp`-equal to `orig/G2ME01/files/RelProd/`, and all 87 `hash:` entries in
`config/G2ME01/config.yml` (DOL + 86 modules) reproduced by `sha1sum` — 0 mismatches.

`python3 tools/check_decl_order.py --unit Kyoto/Animation/CMetaAnimRandom.cpp` -> `ok: 0 unit(s)
checked` (no new declarations were added, so nothing to order; retail's source-function order was
already right and is unchanged).

## Note for the driver / next lane

- `tools/unit_fit.sh` still reports the unit **600 bytes over** with 7 extras (`__dt__` of
  `pair`/`rc_ptr`/`ncrc_ptr`/`IMetaAnim`, `ReleaseData` x2). All 7 are **weak** out-of-line copies
  of inline functions, they were there before this change, and `flip_test` PASSes, so mwldeppc is
  discarding them as the tool's own comment predicts. Not a blocker.
- `python3 tools/check_docs_claims.py` now reports the `docs/HANDOFF.md` state block as stale
  (it wants `matched 9507 / 28465`, `DOL 8096 / 16726`). I did not edit `docs/HANDOFF.md` — the
  item brief forbids it and says the judge rewrites the derived counts itself.
- Lesson worth reusing: **a function at 0% in a unit whose other functions are at 100% is very
  often a function that is not emitted at all, not one with a wrong body.** Check
  `nm` on our object before reading the asm.
- Lesson: **`inline_max_size` can be two-sided.** A value that fixes the callee you are chasing can
  break an unrelated *caller* in the same TU by outlining a second template. Sweep downward and
  upward from the default, and read the whole unit's not-100% list at each value, not just the
  function you came for.

---

# match-cmetaanimrandom — run 2 (lane L3, `goal/lane-3` @ `ada6d97`)

**Result: the unit is flipped and `flip_test.sh` PASSes — but the tree arrived broken and had to
be repaired first. The repair is a prerequisite, not scope creep; the real finding is below.**

## The finding: `ada6d97` ("match: match-stream") shipped half a commit and broke the DOL link

This worktree is `goal/lane-3` at `ada6d97`, not the `wt-mp2-goal` tree the first run worked in.
The first run's change was already judged PASS there and carried forward, so **the CMetaAnimRandom
work itself was a one-line re-apply**. The lane instead hit a build that could not link at all,
before any edit of mine:

```
$ ./tools/decomp_build.sh
[3/8] LINK build/G2ME01/main.elf
### mwldeppc.exe Linker Error:
#   undefined: 'sndStreamMixParameter'
#   Referenced from 'CDSPStreamManager::UpdateVolume(int,int)' in CDSPStreamManager.o
```

`git show ada6d97 --stat` is the whole story — **3 files**: `configure.py`, `docs/HANDOFF.md`,
`docs/goal-notes/match-stream.md`. It flipped `musyx/runtime/stream.c` to `Matching` and committed
**none of the `extern/musyx/src/musyx/runtime/stream.c` guard edits its own notes describe**
(`git log -1 ada6d97 -- extern/musyx/src/musyx/runtime/stream.c` is empty; the file's last commit is
`e44c426`). So our object replaced retail's while still compiling the wrong function set, and
`CDSPStreamManager.o:426` — the one caller, which needs `sndStreamMixParameter` — lost it.

**Every item on this lane would have failed this way.** `probe_sources.sh`, `gate.sh` and
`flip_test.sh` all link, so nothing passes until it is fixed. It is not a pre-existing condition of
the branch: the link is green at `8039914` and red at its child `ada6d97`.

Ground truth, retail's own object vs ours at `ada6d97` (`nm --defined-only ... | grep " T "`):

```
$ diff <retail syms> <our syms>
< sndStreamMixParameter          (retail defines it; we did not)
> sndStreamFrq
> sndStreamLPFParameter
> sndStreamMixParameterEx         (we defined three retail does not)
```

That 1-against-3 signature is exactly the four guards. I re-applied them from the committed
`docs/goal-notes/match-stream.md`, which already specified all five edits precisely, so the repair is
that commit's own change and not new work:

- `streamKill` 336 `#elif ... <= 2,0,2` -> `2,0,3`
- `sndStreamMixParameter` 758 `#if ... <= 2,0,2` -> `2,0,3`  (this is the one that fixes the link)
- new `#if MUSY_VERSION > 2,0,3` around `sndStreamMixParameterEx`, closed after it
- new `#if MUSY_VERSION > 2,0,3` around the `#pragma push`/`sndStreamFrq`/`#pragma pop` block
- `sndStreamLPFParameter` 881 `#if ... >= 2,0,2` -> `> 2,0,3`

Preprocessor nesting re-checked: final depth 0, min depth 0. After the repair the symbol sets are
**identical** (`diff` empty, 13 = 13) and `stream` is 18/18 at 100%, `complete: true`.

## The unit itself (unchanged conclusion, re-measured in this tree)

`src/Kyoto/Animation/CMetaAnimRandom.cpp`: the `#pragma inline_max_size(121)` + comment before the
includes. `configure.py:888` now reads `Object(Matching, ...)` — written by `flip_test.sh` itself.

`build/report.json`: **13/13 functions, 100.0% fuzzy, `.text` 1784/1784, `.data` 32/32,
`complete: true`.** No function body was touched; the inlined loop body was already byte-exact. The
measured `inline_max_size` window in the first run's table (121 only, 120 and 122 each losing a
*different* function) held unchanged on this branch.

## Gates, all measured on this final tree

```
sha1sum build/G2ME01/main.dol        6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (expected)
./tools/flip_test.sh Kyoto/Animation/CMetaAnimRandom.cpp   PASS -> kept as Matching
python3 tools/check_symbol_names.py  checked 484 units; 0 declared names are missing
./tools/probe_sources.sh             probe: 736 files, 0 failed, 0 errors;
                                     link: LINKED (254 undefined, 0 duplicates)
```

- All 86 RELs `cmp`-equal to `orig/G2ME01/files/RelProd/`; all 87 `hash:` entries in
  `config/G2ME01/config.yml` (DOL + 86) reproduced by sha1 — **0 mismatches** (parsed with a YAML
  walk, not a shell one-liner: my first attempt mis-paired the fields and reported 0 ok / 1 bogus
  "mismatch" on `splits:`. Parse the file, do not paste it.)
- `check_decl_order.py --unit Kyoto/Animation/CMetaAnimRandom.cpp` -> ok, no reordering needed.
- Per-function vs the judge baseline `build/goal/judge/report.base.json`: **0 functions worse, 3
  better** — `uninitialized_copy_n` 0 -> 100, the vector copy ctor 46.36 -> 100, and `streamKill`
  62.88 -> 100 (the stream repair, not this unit).

```
baseline  All: 29.18% fuzzy, 21.33% matched, 11.60% linked (9506 / 28465 functions)
final     All: 29.18% fuzzy, 21.34% matched, 11.63% linked (9509 / 28465 functions)
```

`check_docs_claims.py` now wants `matched 9509`, `linked 4849`, `DOL 8098` in the HANDOFF state
block. Left alone deliberately: the brief forbids editing `docs/HANDOFF.md` and says the judge
rewrites derived counts.

## For the driver — this is the part that matters

- **`ada6d97` must not be trusted as a green baseline, and the same failure will recur.** A commit
  that flips a unit to `Matching` while dropping the source half of its own change is invisible to
  `goal_check.sh` (it judged `match-stream` PASS on the lane where the uncommitted edit was still
  in the working tree) and only shows up once the change is *carried onto another branch*. The
  lesson generalises past this repo: **a flip commit is only sound if the source change travelled
  with it — verify the commit's diff contains the file the notes claim it edits, not just the
  `configure.py` line.**
- `build/goal/rebase.patch` (from the release) still describes the CMetaAnimRandom change; the
  driver had already applied it cleanly to `configure.py` and the source, then released the item
  because the *rebuild* failed on this unrelated link error. So the release reason was the broken
  `stream.c`, not anything wrong with the unit.
- Cheap check that would have caught it: after any carry, `nm --defined-only` the flipped unit's
  object against the retail object and diff the symbol sets, before trusting the build.

NEW: match-musyx-runtime-stream | match | musyx/runtime/stream.c | commit ada6d97 flipped this unit to Matching but committed none of its stream.c guard edits, so the DOL no longer links (undefined sndStreamMixParameter) and every item on the lane fails; the four guards are re-applied in this item's diff and stream is 18/18 at 100%, so this target needs no further work - the durable fix is for the driver to verify a carried flip commit contains the source files its notes claim
