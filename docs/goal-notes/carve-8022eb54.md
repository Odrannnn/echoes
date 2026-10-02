# carve-8022eb54

**Kind:** `match` **Target:** `MetroidPrime/ScriptLoader/Carve8022EB54` **Result: PASS.**

`tools/goal_check.sh build/goal/item.json` printed, verbatim, on the final (post-comment-fix) tree:

```
goal_check: item carve-8022eb54 (match) target=MetroidPrime/ScriptLoader/Carve8022EB54
goal_check: baseline /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal-L13/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12693 -> 12695   linked 6052 -> 6054
  ok    check_symbol_names.py
  ok    All:  35.55% fuzzy, 29.39% matched, 13.06% linked (12695 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve8022EB54.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8022eb54
```

## What I did

The carve, four files, each entry in address order (the claim sits between
`IngBlobSwarmLoaderSet.cpp` at 0x8022E134..0x8022E13C and `EmperorIngStage3.cpp` at
0x8022EB9C):

- **`src/MetroidPrime/ScriptLoader/Carve8022EB54.c`** (new, 82 lines, plain C, 2 functions,
  definitions **descending by address**).
- **`configure.py:849`** - `Object(Matching, "MetroidPrime/ScriptLoader/Carve8022EB54.c"),`
  between `IngBlobSwarmLoaderSet.cpp` (848) and `EmperorIngStage3.cpp` (now 850). One line.
- **`config/G2ME01/splits.txt:1612-1613`** -
  `MetroidPrime/ScriptLoader/Carve8022EB54.c:` / `.text start:0x8022EB54 end:0x8022EB9C`.
- **`files.cmake:619`** - after `Carve8022DA40.c` (618), before `Carve80232834.c`, so the carve
  list stays in address order.

Plus the two tree comments the carve made false, in the same change (the edit
`carve-80232834` made for the same class of sentence one address along):

- **`include/MetroidPrime/Player/CPlayer.hpp:839-847`** said `fn_8022EA5C` sits in "the
  **unclaimed** gap 0x8022E13C..0x8022EB9C". The gap is now 0x8022E13C..**0x8022EB54** and the
  header records the carve as the next claim.
- **`src/MetroidPrime/PortGlobals.cpp:1734-1737`** carried the identical sentence for the
  identical stand-in; same correction. `fn_8022EA5C` (0x8022EA5C, 0x64 B) is still inside the
  unclaimed part, so both stand-ins are still needed - the carve touches neither.

No `PortLinkStubs.cpp` duplicate and no host stand-in: `grep -rn "fn_8022EB54\|fn_8022EB90"`
outside the new file finds nothing, and the one callee `Free__7CMemoryFPCv` (0x802CE388,
`symbols.txt:12992`) is claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL (`splits.txt:2379`) and
defined for the host link by `src/Kyoto/Alloc/PortMwccNew.cpp:38`.

## What the two functions are (measured, not recalled)

Both are **byte-for-byte twins of named retail functions**, which is what the item said and what
the disassembly confirms - `fn_8022EB54` even has the same `bl` target:

- `fn_8022EB54` (0x8022EB54, `size:0x3C`, `symbols.txt:9934`) is `__dt__5CMainFv`
  (0x800087DC, `size:0x3C`, `symbols.txt:162`, source `src/MetroidPrime/main.cpp:517`, the empty
  `CMain::~CMain()`) word for word: frame, `mr. r31,r3 / beq`, `extsh. r0,r4 / ble` (so the flag
  is a **short**), `bl Free__7CMemoryFPCv` only on a positive flag, receiver returned in r3. The
  deleting-destructor shape of a class with nothing to destroy.
- `fn_8022EB90` (0x8022EB90, `size:0xC`, `symbols.txt:9935`) is
  `DisableFog__Q29CGameArea8CAreaFogFv` (0x80056DE8, `size:0xC`, `symbols.txt:1712`, source
  `src/MetroidPrime/CGameArea.cpp:1592`, `mFogMode = kRFM_None` with `GX_FOG_NONE = 0`) word for
  word: `li r0,0 / stw r0,0(r3) / blr`.

Byte evidence against the **pristine disc**, not against our own build:

```
python3 tools/dol_read.py 0x8022EB54 0x48 orig/G2ME01/sys/main.dol
```

72 bytes, and 69 of them are equal to our object's `.text`; the three that differ are the `bl`
displacement word (object placeholder `48 00 00 01`, retail `48 09 f8 15`), which the linker
resolves. Everything else, ordering included, is equal.

## The one thing that cost time: the object's function order

Written ascending, the build failed `CHECK config/G2ME01/build.sha1` with
`build/G2ME01/main.dol: FAILED` and `86 files OK`: mwcceppc had emitted the definitions in
**reverse source order**, so the linked DOL carried `fn_8022EB90` at 0x8022EB54 (nm of the linked
ELF), and the DOL hash caught it before `flip_test` was even needed. Definitions rewritten
descending, the object's `.text` comes out ascending - `nm -n
build/G2ME01/src/MetroidPrime/ScriptLoader/Carve8022EB54.o` gives `00000000 T fn_8022EB54` /
`0000003c T fn_8022EB90` - and the link puts them at 0x8022EB54 and 0x8022EB90. This is the rule
in the item, re-measured rather than inherited.

**Two traps for the next carve, both hit here:**

1. The object the link uses is **`build/G2ME01/src/<path>.o`** (`build.ninja:10191`, the
   `mwcc_sjis` output). `build/G2ME01/obj/<path>.o` is a *different* file - its `nm`/`carve_diff`
   showed the opposite function order and sent me chasing a linker that was not doing anything
   wrong. Diff the `src/` object.
2. `tools/carve_diff.sh` reports **"differing instructions: 1 / NOT byte-exact"** for a correct
   carve, necessarily: it reads its "retail" side from `build/G2ME01/main.elf`, which once the
   unit is `Matching` is *our own* linked build, and an unlinked object's `bl` is still an
   unresolved relocation. "The only difference is the call word" is the pass; the DOL sha1 is the
   verdict.

## Verified, measured

- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86 RELs `OK`.
- `tools/flip_test.sh MetroidPrime/ScriptLoader/Carve8022EB54.c` -> `PASS -> kept as Matching`.
- `tools/unit_fit.sh` -> `.text claimed 72 / ours 72 / retail 72, fits`; `no extra functions`.
- `python3 tools/check_decl_order.py --unit ...` -> `ok: 0 unit(s) checked, none emits its
  functions out of retail order`.
- `python3 tools/check_symbol_names.py` -> `0 declared names are missing from their object`.
- `build/report.json`, unit `main/MetroidPrime/ScriptLoader/Carve8022EB54`: 2/2 functions at
  100.00%; `total_functions` still 28465.

## Caveats / observations (nothing filed as `NEW:`)

- A `progress`/`match` reviewer note, not a work item: nothing about this item is blocked, so no
  `NEW:` line. The two traps above are lessons and stay here.
- **`docs/HANDOFF.md` (478 MB) and `docs/RUNNING_THE_DECOMP.md` (186 MB) show as modified in this
  worktree** (mtimes 16:27:58 and 16:22:03, during this run) though `git status` was clean when I
  started and I only ran `decomp_build.sh`, `flip_test.sh`, `goal_check.sh` and read-only tools;
  none of them names those files. `git hash-object` differs from HEAD's blob while byte size and
  line count are identical, so it is not a hand edit or a diff I can read - it looks like another
  process writing into this worktree. I left both files alone, as the item requires; flagging it
  only so the driver knows the churn is not mine.

---

## Run 2 - 2026-10-02, lane 13, on `7196a62c` (goal/decomp). PASS, carve re-created from scratch.

**Why there was a second run.** Run 1 (on 0291b134) was judged PASS at 14:37:40 and then could not
be carried onto 7196a62c: `build/goal/run.log` records `error: patch too large` at 14:38:09 and
`carve-8022eb54 does not apply on 7196a62 - releasing it for a fresh attempt` (the failed carry was
the 1.3 GB `build/goal/rebase.patch` the driver had just written, not this change). So the item came
back **with the tree reset**: `src/MetroidPrime/ScriptLoader/Carve8022EB54.c` was absent and
`grep -rn "fn_8022EB54\|fn_8022EB90"` over `src/ include/ config/` found only the two placeholder
lines, `config/G2ME01/symbols.txt:9934-9935`. Nothing of run 1 was in the tree; this run wrote the
whole change again, and re-measured every number below on this tree.

**Same four files, same two comment corrections, all in one change** (line numbers as they now are):

- `src/MetroidPrime/ScriptLoader/Carve8022EB54.c` (new, **102 lines**, plain C, 2 functions,
  definitions **DESCENDING**: `fn_8022EB90` is defined first, `fn_8022EB54` second - the reverse of
  the address order, which is the point).
- `configure.py:852` - `Object(Matching, "MetroidPrime/ScriptLoader/Carve8022EB54.c"),`, one line,
  between `IngBlobSwarmLoaderSet.cpp` (851) and `EmperorIngStage3.cpp` (853).
- `config/G2ME01/splits.txt:1621-1622` - `MetroidPrime/ScriptLoader/Carve8022EB54.c:` /
  `.text start:0x8022EB54 end:0x8022EB9C`, in address order between `IngBlobSwarmLoaderSet.cpp`
  (1619-1620) and `EmperorIngStage3.cpp` (1624-1626).
- `files.cmake:622` - after `Carve8022DA40.c` (621), before `Carve80232834.c` (623).
- The two now-false "gap 0x8022E13C..0x8022EB9C" sentences, same corrections run 1 made:
  `include/MetroidPrime/Player/CPlayer.hpp:840-842` (gap end is now 0x8022EB54, and the claim is
  named between the two neighbours) and `src/MetroidPrime/PortGlobals.cpp:1736-1737` (for the
  `fn_8022EA5C` stand-in). `fn_8022EA5C` (0x8022EA5C, 0x64 B) is still inside the remaining
  unclaimed 0x8022E13C..0x8022EB54, so both stand-ins are still needed and neither was touched.

No `PortLinkStubs.cpp` duplicate: nothing else in `src/` or `include/` names either function.

**The one measurement this run added that run 1 did not have.** The header's "descending by address"
paragraph is now a measurement, not an inheritance: after the carve was verified I rewrote the same
file **ascending** (definitions swapped, nothing else changed), rebuilt and read the result -
`CHECK config/G2ME01/build.sha1` fails with `build/G2ME01/main.dol: FAILED` / `86 files OK`,
`powerpc-eabi-nm -n build/G2ME01/src/MetroidPrime/ScriptLoader/Carve8022EB54.o` then reads
`00000000 T fn_8022EB90` / `0000000c T fn_8022EB54`, the **linked** ELF carries `8022eb54 T
fn_8022EB90` / `8022eb60 T fn_8022EB54` (the link itself succeeds - only the sha1 catches it), and
`sha1sum build/G2ME01/main.dol` is `6735d32d0bf4527d6d0230beecd67ec15d053040` instead of retail's
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. Restored descending, the object reads `00000000 T
fn_8022EB54` / `0000003c T fn_8022EB90` and the sha1 is retail's again.

**Byte evidence, measured on this tree.** `tools/carve_diff.sh 0x8022EB54 0x48
build/G2ME01/src/MetroidPrime/ScriptLoader/Carve8022EB54.o` prints `retail: 18 instructions, 72
bytes` / `ours : 18 instructions, 72 bytes` and `differing instructions: 1` - the `bl` at +0x20,
which is an unresolved `R_PPC_REL24 Free__7CMemoryFPCv` in the unlinked object (objdump of the
object) and resolves to `802ce388 <Free__7CMemoryFPCv>` in the linked ELF, i.e. exactly retail's
`48 09 f8 15` at 0x8022EB74 (`python3 tools/dol_read.py 0x8022EB54 0x48 orig/G2ME01/sys/main.dol`;
LI = 0x27E05, 0x8022EB74 + 0x9F814 = 0x802CE388). So `NOT byte-exact` is the tool's verdict on a
correct carve, and the DOL sha1 is the real one. The twins were re-confirmed rather than recalled:
`./tools/dis.sh 0x800087DC 0x3C` is `__dt__5CMainFv` (`symbols.txt:162`, `main.cpp:517`, the empty
`CMain::~CMain()`) - the same 15 instructions, `bl` word apart - and `./tools/dis.sh 0x80056DE8
0xC` is `DisableFog__Q29CGameArea8CAreaFogFv` (`symbols.txt:1712`, `CGameArea.cpp:1592`), the same
`li r0,0 / stw r0,0(r3) / blr`.

**Verdict, verbatim, on the final tree:**

```
goal_check: item carve-8022eb54 (match) target=MetroidPrime/ScriptLoader/Carve8022EB54
goal_check: baseline .../wt-mp2-goal-L13/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12699 -> 12701   linked 6058 -> 6060
  ok    check_symbol_names.py
  ok    All:  35.55% fuzzy, 29.39% matched, 13.07% linked (12701 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve8022EB54.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8022eb54
```

Other gates this run: `sha1sum build/G2ME01/main.dol` =
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `tools/decomp_build.sh` `All: 35.55% fuzzy, 29.39%
matched, 13.07% linked (12701 / 28465 functions)`, unit line `main/MetroidPrime/ScriptLoader/
Carve8022EB54: 100.00% fuzzy, 100.00% matched (2 / 2 functions)`; `tools/unit_fit.sh` `.text claimed
72 / ours 72 / retail 72, fits` + `no extra functions`; `python3 tools/check_decl_order.py --unit
main/MetroidPrime/ScriptLoader/Carve8022EB54` -> `ok: 1 unit(s) checked, none emits its functions
out of retail order`; `python3 tools/check_symbol_names.py` -> `checked 532 units; 0 declared names
are missing from their object`; `tools/probe_sources.sh` -> `818 files, 0 failed, 0 errors; link:
LINKED (291 undefined, 0 duplicates)`. `total_functions` still 28465.

**No `NEW:` line.** Nothing is blocked; the item is done. The two traps run 1 recorded (diff
`build/G2ME01/src/<path>.o`, not `build/G2ME01/obj/<path>.o`; `carve_diff` says `NOT byte-exact`
for a correct carve) both held this run as well. Run 1's `docs/HANDOFF.md` /
`docs/RUNNING_THE_DECOMP.md` churn did **not** recur: `git status --short` in this run showed only
the five changed files and the new `.c`.

### Correction + one hand-off note (run 2, end of session)

The paragraph above says run 1's `docs/HANDOFF.md` / `docs/RUNNING_THE_DECOMP.md` churn "did not
recur". It **did**, and the cause is now measured rather than guessed: something in the normal build
path (`tools/gate.sh` / `tools/decomp_build.sh`, almost certainly the `sync_state_block.py --dedupe`
step `tools/run_goal.sh:658` wraps) rewrites both files whenever they are built against, bumping the
state block (`matched 12699 -> 12701`, `linked 6058 -> 6060`, `13.06% -> 13.07%`). It is not a hand
edit by me and not another process: `git status --short` was clean, `git checkout --` both files,
`git status` was clean again for 5 s, and then one `./tools/goal_check.sh` dirtied exactly those two
files again. Line counts stay identical to HEAD; the diff reads as ~8.6 M lines each
(`git diff --stat` -> `8622363 insertions(+), 8622363 deletions(-)` for a four-line state-block
edit, i.e. the writer rewrites nearly every line's bytes).

**I left both files reverted to HEAD**, deliberately, because run 1's change died in the carry with
`error: patch too large` (`build/goal/run.log` 14:38:09) and an 8.6 M-line diff in the patch is the
obvious candidate. `git status --short` in this tree therefore shows exactly six entries: the five
modified files and the new `src/MetroidPrime/ScriptLoader/Carve8022EB54.c`. The judge passes either
way - the final `goal_check` run that printed `PASS carve-8022eb54` above ran with both docs **at
HEAD** (stale state block) and printed `ok gate.sh (... docs claims ...)`, so `check_docs_claims.py`
does not fail on a stale-but-lower state block and the driver's own sync re-derives it in the
destination. Nothing here needs a `NEW:` line; it is a tooling observation.

---

## Run 3 - 2026-10-02, lane 3, on `7347718e` (goal/lane-3). PASS, carve re-created from scratch again.

**Third run for this item, same outcome as runs 1 and 2: the carve had to be written again from
nothing.** Same cause as run 2 (the carry failed and the driver released the item with the tree
reset). Measured on this tree before touching anything: `git status --short` clean;
`src/MetroidPrime/ScriptLoader/Carve8022EB54.c` absent; `grep -rn "fn_8022EB54\|fn_8022EB90\|Carve8022EB54"
src/ include/ config/ files.cmake configure.py` matched **only** the two placeholder lines,
`config/G2ME01/symbols.txt:9934-9935`; and `build/report.json` had **no** `Carve8022EB54` unit at all
(its `units` list is keyed by name, and `main/auto_03_8022E13C_text` still carried all 10 functions
including `fn_8022EB54` at offset 2584 and `fn_8022EB90` at offset 2644). So this is **not** `STALE:`
- the work is genuinely absent here - but it is also **not** new work: it is the same 72 bytes runs 1
and 2 landed. The only thing that could make this run worth its cost is measuring it again.

**The carve, four files, each entry in address order** (line numbers on this tree):

- `src/MetroidPrime/ScriptLoader/Carve8022EB54.c` - **new, 113 lines**, plain C, 2 functions,
  definitions **DESCENDING by address**: `fn_8022EB90` (line 96) is defined first, `fn_8022EB54`
  (line 106) second.
- `configure.py:859` - `Object(Matching, "MetroidPrime/ScriptLoader/Carve8022EB54.c"),` on one line,
  between `IngBlobSwarmLoaderSet.cpp` (858) and `EmperorIngStage3.cpp` (860).
- `config/G2ME01/splits.txt:1749-1750` - `MetroidPrime/ScriptLoader/Carve8022EB54.c:` /
  `.text start:0x8022EB54 end:0x8022EB9C`, in address order between `IngBlobSwarmLoaderSet.cpp`
  (1746-1747) and `EmperorIngStage3.cpp` (1752-1754).
- `files.cmake:614` - after `Carve8022DA40.c` (613), before `Carve80232834.c` (615).

Plus the two tree comments the carve makes false, same correction runs 1 and 2 made: the
"unclaimed gap 0x8022E13C..0x8022EB9C" sentences at `include/MetroidPrime/Player/CPlayer.hpp:847-854`
and `src/MetroidPrime/PortGlobals.cpp:1723-1726`, both now naming the carve and giving the gap's new
end 0x8022EB54. `fn_8022EA5C` (0x8022EA5C, 0x64) is still inside the remaining unclaimed
0x8022E13C..0x8022EB54, so both `fn_8022EA5C` stand-ins are still needed and neither was touched.

No `PortLinkStubs.cpp` duplicate: `grep -rn "fn_8022EB54\|fn_8022EB90" src/ include/` outside the new
file finds nothing. The one callee `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`) is declared
only; it is claimed in the DOL by `Kyoto/Alloc/CMemory.cpp` and defined for the host link by
`src/Kyoto/Alloc/PortMwccNew.cpp`, so the port's undefined count cannot move.

## What this run re-measured (rather than inherited)

**The twins, from the twins' own disassembly.** `./tools/dis.sh 0x800087DC 0x3C` is
`__dt__5CMainFv` (`symbols.txt:162`, `src/MetroidPrime/main.cpp:517`, the empty `CMain::~CMain()`):
the same 15 instructions as `fn_8022EB54`, **including the `bl` target** - both go to
`802ce388 <Free__7CMemoryFPCv>`. `./tools/dis.sh 0x80056DE8 0xC` is
`DisableFog__Q29CGameArea8CAreaFogFv` (`symbols.txt:1712`, `src/MetroidPrime/CGameArea.cpp:1592`,
`mFogMode = kRFM_None` with `GX_FOG_NONE = 0`): the same `li r0,0 / stw r0,0(r3) / blr`. Retail's own
bytes for the carve, from the **pristine disc** and not from our build:
`python3 tools/dol_read.py 0x8022EB54 0x48 orig/G2ME01/sys/main.dol` ->
`94 21 ff f0 7c 08 02 a6 90 01 00 14 93 e1 00 0c 7c 7f 1b 79 41 82 00 10 7c 80 07 35 40 81 00 08
48 09 f8 15 80 01 00 14 7f e3 fb 78 83 e1 00 0c 7c 08 03 a6 38 21 00 10 4e 80 00 20 38 00 00 00
90 03 00 00 4e 80 00 20`, i.e. `li = 0x27E05` and `0x8022EB74 + 0x9F814 = 0x802CE388`, the same
callee the twin calls.

**The call sites, which runs 1 and 2 did not measure.** `grep -rn 'bl fn_8022EB54\|bl fn_8022EB90'
build/G2ME01/asm/` finds four calls, all in
`build/G2ME01/asm/MetroidPrime/Player/CPlayer.s`, and they are what the bodies are *for*:

- `fn_8022EB54` at 0x8001A798 and 0x8001A80C, on `r30+0x1300` and `r30+0x1168`, both with `li r4,-1`,
  sitting between the neighbouring members' own destructor calls (`__dt__10CModelDataFv` at
  0x8001A7D4, `__dt__10CMorphBallFv` at 0x8001A800, `__dt__20CDamageVulnerabilityFv` at 0x8001A818).
  `li r4,-1` is this tree's call-site convention for "destroy, do not free me afterwards"
  (`src/MetroidPrime/Cameras/Carve801E7C14.c:29-31` documents it), so no caller of these four ever
  reaches the `Free` - the member has no members of its own, which is the whole body.
- `fn_8022EB90` at 0x8001B73C and 0x8001BB6C, each in the middle of a member initialiser run: the
  sequence at 0x8001B6CC is `addi r3, r31,0x1168`, then a run of `stfs`/`stb` writes to `r31+0x1140..0x1164`,
  then `bl fn_8022EB90` at 0x8001B73C. The one word it clears is the head of the object being set up.

So the two are the teardown step and the "clear my first word" step of one empty class. That is why
the shapes are the ones they are, and it is the evidence the file's header now cites.

**Both traps from run 1 held again**, and both are now measured on this tree:

1. The object the link uses is `build/G2ME01/src/<path>.o`, not `build/G2ME01/obj/<path>.o`. `nm -n`
   on the `src/` object reads `00000000 T fn_8022EB54` / `0000003c T fn_8022EB90` - ascending,
   correct - and the **linked** ELF carries `8022eb54 T fn_8022EB54` / `8022eb90 T fn_8022EB90`,
   i.e. descending in the source produced the right `.text`.
2. `tools/carve_diff.sh 0x8022EB54 0x48 build/G2ME01/src/MetroidPrime/ScriptLoader/Carve8022EB54.o`
   prints `retail: 18 instructions, 72 bytes` / `ours : 18 instructions, 72 bytes`,
   `differing instructions: 1`, `NOT byte-exact` - and the one difference it prints is
   `+8 retail: 8022eb74 bl 802ce388 <Free__7CMemoryFPCv> ours: 00000020 bl 20 <fn_8022EB54+0x20>`.
   `objdump -r` on the same object shows why: `00000020 R_PPC_REL24 Free__7CMemoryFPCv`, an
   unresolved relocation in an unlinked object. **The tool's "NOT byte-exact" is its verdict on a
   correct carve**; the DOL sha1 is the real one. Nothing new here, but it cost run 1 time, so it
   stays.

## Verified, measured on this tree

- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `decomp_build.sh`'s
  `CHECK config/G2ME01/build.sha1` clean with all 86 RELs.
- `./tools/decomp_build.sh` -> `All: 36.96% fuzzy, 30.40% matched, 13.39% linked (13022 / 28465 functions)`.
- `./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve8022EB54.c` -> `PASS -> kept as Matching`
  (`kept: 1 / 1  failed: 0  skipped: 0`).
- `./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve8022EB54.c` -> `.text claimed 72 / ours 72 /
  retail 72, fits` + `no extra functions`.
- `python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptLoader/Carve8022EB54` ->
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `python3 tools/check_symbol_names.py` -> `checked 575 units; 0 declared names are missing from their object`.
- `build/report.json`: unit `main/MetroidPrime/ScriptLoader/Carve8022EB54` **2 / 2** functions
  matched, and `main/auto_03_8022E13C_text` dropped from 10 functions to **8** - the two carved
  functions leave the auto unit as they should. `total_functions` still **28465**.
- `./tools/goal_check.sh build/goal/item.json`, verbatim:

```
goal_check: item carve-8022eb54 (match) target=MetroidPrime/ScriptLoader/Carve8022EB54
goal_check: baseline .../wt-mp2-goal-L3/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13020 -> 13022   linked 6124 -> 6126
  ok    check_symbol_names.py
  ok    All:  36.96% fuzzy, 30.40% matched, 13.39% linked (13022 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve8022EB54.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8022eb54
```

## The docs churn, run 3: same cause, **much smaller diff** - and it is a lesson worth keeping

Run 2 measured this as an 8.6 M-line rewrite and suspected `sync_state_block.py --dedupe`. It
recurred twice in this run - once per `goal_check.sh` invocation, each time after `git checkout --`
left both files clean - so run 2's cause is right: it is the build/gate path rewriting the state
block, not a hand edit and not another process. **But this time the rewrite is 7 lines, not 8.6 M:**
`git diff --numstat -- docs/` reads `5 5 docs/HANDOFF.md` and `2 2 docs/RUNNING_THE_DECOMP.md`, and
`git diff --stat` for the whole change is 18 insertions / 11 deletions. The content is the derived
numbers only: the state block `matched 13020 -> 13022`, `linked 6124 -> 6126`, `DOL units 11419 ->
11421`, `36.95% -> 36.96%`, and the probe count `807 -> 808` in three places (one in each file, plus
two in HANDOFF). So run 2's "the writer rewrites nearly every line's bytes" **did not recur** on
this tree, and run 2's fear - that this diff is the `error: patch too large` candidate - is not
reproduced by run 3's numbers.

The goal-unit prompt forbids editing those two files and says the driver discards such edits, so I
left **both at HEAD** (`git status --short` shows exactly six entries: the five modified files and
the new `.c`). Confirmed on this tree, as run 2 also found: the `goal_check` run printed above ran
with both docs **reverted to HEAD**, i.e. with a stale state block, and still printed
`ok gate.sh (... docs claims ...)`. `check_docs_claims.py` does not fail on a stale-but-lower state
block, and the driver's own sync re-derives it in the destination.

## Caveats / observations (nothing filed as `NEW:`)

- **No `NEW:` line.** Nothing about this item is blocked; it is done. Runs 1 and 2 were also not
  blocked - the only thing that went wrong in both was the **carry** (`error: patch too large`,
  `carve-8022eb54 does not apply on <sha> - releasing it for a fresh attempt`), which is a driver-side
  event and not a defect in this change. Three runs of a verified-identical 72-byte carve is the
  cost of that one carry failure, so the thing worth the driver's attention is the carry, not this
  item.
- Not tried, deliberately: nothing in this item's range was left on the table. The claim is exactly
  `0x8022EB54..0x8022EB9C` and both functions in it are matched, so there is no residual.
- The two call sites of `fn_8022EB90` mean it is a member initialiser's "clear my first word"; if
  someone later decompiles the `CPlayer` member they name, that is the reader who should say what
  the class is. The carve does not guess at it and the file says so.
