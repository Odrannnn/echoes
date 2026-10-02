# carve-801ffa20

**Kind:** `match` **Target:** `MetroidPrime/ScriptObjects/Carve801FFA20` **Result: PASS.**

`tools/goal_check.sh build/goal/item.json` printed, verbatim:

```
goal_check: item carve-801ffa20 (match) target=MetroidPrime/ScriptObjects/Carve801FFA20
goal_check: baseline .../wt-mp2-goal-L7/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12579 -> 12583   linked 5941 -> 5945
  ok    check_symbol_names.py
  ok    All:  35.45% fuzzy, 29.26% matched, 12.97% linked (12583 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FFA20.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801ffa20
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

## What I did

The carve shape, four files, each entry placed in address order:

- **`src/MetroidPrime/ScriptObjects/Carve801FFA20.cpp`** (new, `extern "C"`, 4 functions).
- **`configure.py:745`** - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FFA20.cpp"),`
  between `Carve801FF720.cpp` (744) and `SpacePirate.cpp` (746), one line.
- **`config/G2ME01/splits.txt:1180-1181`** -
  `MetroidPrime/ScriptObjects/Carve801FFA20.cpp:` / `.text start:0x801FFA20 end:0x801FFBA0`,
  after `Carve801FF720.cpp` (which ends at 0x801FF8A0) and before
  `MetroidPrime/ScriptLoader/SpacePirate.cpp`. Nothing else claimed; the range is exactly
  retail's, contiguous, and spans no unclaimed gap.
- **`files.cmake:567`** - added after `Carve801FF720.cpp`.

Plus the one thing the carve needs to keep the port link green (see below):

- **`src/MetroidPrime/PortLinkStubs.cpp:790-807`** - `stub_185() asm("fn_801FE7E8")`,
  immediately after `stub_184`, following the pattern already used there for the three
  `Carve801FF5A0.cpp` callees (`stub_179`/`_180`/`_181`) and the two `Carve801FF720.cpp` ones
  (`stub_183`/`_184`).

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` are modified in my tree only because
`goal_check.sh` rewrites the derived state block from the tree itself; the driver discards those.

## What the four functions are, measured

`.text 0x801FFA20..0x801FFBA0`, 0x180 = 384 bytes, 96 instructions, 4 functions:

```
fn_801FFA20  0x801FFA20  0xAC  43 instructions  the block's reserve
fn_801FFACC  0x801FFACC  0x20   8 instructions  forwarder to fn_801FFAEC
fn_801FFAEC  0x801FFAEC  0x4C  19 instructions  destroy_impl(begin, end)
fn_801FFB38  0x801FFB38  0x68  26 instructions  uninitialized_copy(begin, end, dst)
```

Read out of `build/G2ME01/asm/auto_03_801FF8A0_text.s` (lines 125-240) before the claim existed;
still readable with
`build/binutils/powerpc-eabi-objdump -d --start-address=0x801FFA20 --stop-address=0x801FFBA0 build/G2ME01/main.elf`.

This is one instantiation of the same template the neighbouring carves already hold, at element
stride **0x30 = 48**. Each of the four is byte-shape identical to a symbol retail *does* name, in
`src/MetroidPrime/CGameHintInfo.cpp` (Matching):

| this copy | named twin | size |
| --- | --- | --- |
| `fn_801FFA20` | `reserve<rstl::vector<CGameHintInfo::CGameHint, ...> >::int` at 0x80180D14 | 0xAC |
| `fn_801FFACC` | `destroy<CGameHintInfo::CGameHint*>` at 0x80180DC0 | 0x20 |
| `fn_801FFAEC` | `destroy_impl<CGameHintInfo::CGameHint*>` at 0x80180DE0 | 0x4C |
| `fn_801FFB38` | `uninitialized_copy<rstl::pointer_iterator<CGameHintInfo::CGameHint, ...>, ...>` at 0x80180E2C | 0x68 |

The callees are retail's own, all `extern`: `allocate__Q24rstl17rmemory_allocatorFi` (`bl` at
0x801FFA50), `Free__7CMemoryFPCv` (0x801FFAA4), the 0x20-byte element destructor `fn_801FDC68`
(0x801FFB10, already claimed by `Carve801FDB5C.c` in the same directory) and the 0x20-byte element
copy `fn_801FE7E8` (0x801FFB68, `symbols.txt:8305`).

`fn_801FFA20`'s block has the same three words at the same offsets as the twin's vector -
`x04` element count, `x08` capacity compared with a **signed** `cmpw` (`ble` at 0x801FFA48),
`x0c` base pointer - and the loop bound is `x0c + x04 * 0x30`.

## Verification

`tools/carve_diff.sh 0x801FFA20 0x180 build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801FFA20.o`:

```
retail: 96 instructions, 384 bytes
ours  : 96 instructions, 384 bytes
differing instructions: 7
```

All **7** differences are `bl` displacements, i.e. relocations resolved by the linker
(`allocate__Q24rstl17rmemory_allocatorFi`, `fn_801FFB38`, `fn_801FFACC`, `Free__7CMemoryFPCv`,
`fn_801FFAEC`, `fn_801FDC68`, `fn_801FE7E8`); no opcode, offset or frame displacement differs.
`build/report.json` for the unit: `matched_functions 4/4`, `total_code`/`matched_code` 384/384,
`fuzzy_match_percent 100.0`, `metadata.complete true`. `tools/flip_test.sh` **PASS**, which is the
only thing that decides it.

## Why this unit is C++ and not `.c`

The item's rule of thumb is "plain C so the `fn_` names do not mangle". The requirement behind it is
that `powerpc-eabi-nm` on the object shows four unmangled `T fn_801FF<addr>` and nothing else, which
`extern "C"` achieves. Plain C does **not** produce these bytes, and that is measured, not assumed:
the sibling `Carve801FF5A0.cpp` in this directory holds the identical 0xAC/0x20/0x4C/0x68 run and
records that written as C with a one-word struct passed by value, `fn_801FF5A0` came out **27 of 43
words** different (42 instructions instead of 43 - C mode common-subexpressions the two `x0c` reads,
so it never reloads, and the two argument slots land interleaved rather than in two contiguous
8-byte slots) and `fn_801FF66C` **6 of 19** words different (C mode puts the walked cursor in r30 and
the bound in r31 where retail has them the other way round). I did not re-measure that for this
range; I reused the sibling's measurement, which is why the source comment attributes it there.

The load-bearing detail is that `fn_801FFA20` materialises its two by-value one-word iterators as
8-byte stack slots holding the same pointer twice - 0x8/0xc for `end`, 0x10/0x14 for `begin`, handed
over as `addi r4,r1,0xc` / `addi r3,r1,0x14` (0x801FFA60-0x801FFA6C) - and reads `x0c` twice, once
per temporary (0x801FFA5C and 0x801FFA78). A converting constructor on the iterator struct is what
makes mwcceppc build those and refuse to common the loads; `Carve801FF5A0.cpp` and `Carve801FDB5C.c`
argue the same point on the neighbouring ranges.

## The `stub_185` addition, and why it is a stub rather than another carve

With the carve listed and nothing else, the port probe gained an undefined symbol and the gate
failed:

```
port probe      probe: 775 files, 0 failed, 0 errors; link: NOT LINKED (292 undefined, 0 duplicates)
port link gap   287  MISSING
  gap grew: fn_801FE7E8 is not in port_link_gap_list.md
GATE FAIL: probe link-gap
```

`fn_801FE7E8` is unclaimed retail code at 0x801FE7E8, 0x20 bytes, and the carve's bytes *are* the
`bl` to it, so the call cannot be dropped without losing the match. `PortLinkStubs.cpp` is the
established home for exactly this: `stub_179`/`_180`/`_181` for the three `Carve801FF5A0.cpp` callees
and `stub_183`/`_184` for the two `Carve801FF720.cpp` ones, each with a comment recording the
measured gap growth. `fn_801FE7E8` is itself a forwarder (`stwu/mflr/stw` / `bl fn_801FE808` / epilogue,
`objdump -d --start-address=0x801FE7E8 --stop-address=0x801FE808`), so carving it would only move the
gap one function along - the trade `stub_178`'s comment already argues for this family. I took the
stub, with an empty body and no claim that anything there is decompiled.

After it: `link_check: unique undefined symbols 291` and
`STRICT PASS - regression gate: 291 undefined against a baseline of 291 (no growth), 0 duplicate(s)`.
`fn_801FE7E8` is deliberately **not** added to `docs/research/port_link_gap_list.md`: the stub
defines it, so it never reaches the gap list - the same as its five siblings, none of which is listed.

No `PortLinkStubs.cpp` duplicate of the four target symbols exists (`grep` over `src/` finds
`fn_801FFA20`/`_ACC`/`_AEC`/`_B38` only in the new file), which is the check `gate.sh`'s `port link
dups` step exists for; it reports `0 duplicates`.

## Order and the four-file rule

`python3 tools/check_decl_order.py` -> `ok: 1002 unit(s) checked, 28 permuted, all 28 accounted for
in decl_order.md` (28 both before and after my unit; mine is not among them). Definitions are in
descending address order: `fn_801FFB38`, `fn_801FFAEC`, `fn_801FFACC`, `fn_801FFA20`.

## Lessons worth keeping (not filed as `NEW:`)

- The `It`-by-value argument is what decides a carve of this template family, and it is *invisible*
  in the item's twin list: the twins are named after their callers' element type, but the reason the
  body reproduces is the 8-byte by-value iterator slot plus the un-hoisted `lwz` of the bound. Any
  future carve of the 0xAC/0x20/0x4C/0x68 shape should copy `Carve801FF5A0.cpp` wholesale and change
  only the stride and the two element callees - that is exactly what this item needed.
- `gate.sh`'s `probe link-gap` is the only thing that catches a carve whose *callees* are unclaimed;
  `carve_diff.sh` and objdiff are both blind to it, because the `bl` target is a relocation in our
  object and the callee exists in the DOL. The count is 291 before and after.

## NEW:

NEW: carve-801ff8a0 | match | MetroidPrime/ScriptObjects/Carve801FF8A0 | the 0xAC/0x20/0x4C/0x68 round at 0x801FF8A0..0x801FFA20 is the same template at stride 0x2C (mulli/addi 0x2C at 0x801FF8CC/0x801FF8E4/0x801FF914/0x801FF994/0x801FF9EC); copy Carve801FF5A0.cpp and change only the stride and the two element callees fn_801FEC64 (bl 0x801FF9E8) and fn_801FDAA4 (bl 0x801FF990), both 0x20 bytes, both unclaimed, and expect one stub_186 plus one stub_187 in PortLinkStubs.cpp.
## Lane 7: passed, then failed on the moved tip (2026-10-02 11:30:44Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 337d276fcaf1; re-do it against the current tip.

## Run 2 on tip 337d276fcaf1 (2026-10-02): PASS

The tip moved under run 1 (`Lane 7: passed, then failed on the moved tip`), so the change was redone
from scratch against `337d276fcaf1` ("match: carve-80004c4c"). **This run re-measured everything;
run 1's conclusions all held, but two of its numbers were stale.** Nothing in the tree had the carve
(`git status` clean, no `Carve801FFA20` in `configure.py`, `splits.txt`, `files.cmake`), so this was
not a `STALE:` item - `build/report.json` had no such unit.

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
goal_check: item carve-801ffa20 (match) target=MetroidPrime/ScriptObjects/Carve801FFA20
goal_check: baseline .../wt-mp2-goal-L7/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12595 -> 12599   linked 5957 -> 5961
  ok    check_symbol_names.py
  ok    All:  35.46% fuzzy, 29.28% matched, 12.99% linked (12599 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FFA20.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801ffa20
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

### What changed, five files

- **`src/MetroidPrime/ScriptObjects/Carve801FFA20.cpp`** (new, 181 lines, `extern "C"`, 4 functions)
  - `Carve801FF720.cpp`'s body with the stride 36 -> 48 and the two element callees swapped
    (`fn_801FEA98` -> `fn_801FE7E8`, `fn_801FD8E0` -> `fn_801FDC68`). Nothing else differs.
- **`configure.py:750`** - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FFA20.cpp"),`
  immediately after `Carve801FF720.cpp` (749), one line.
- **`config/G2ME01/splits.txt:1194-1195`** - the unit header plus
  `.text start:0x801FFA20 end:0x801FFBA0`, after `Carve801FF720.cpp` (ends 0x801FF8A0) and before
  `MetroidPrime/ScriptLoader/SpacePirate.cpp`. Contiguous, retail's own, spans no unclaimed gap:
  `splits.txt` has no other entry between `start:0x801FE...` and `start:0x801FD...`, i.e. nothing
  claims 0x801FF8A0..0x801FFBA0.
- **`files.cmake:572`** - after `Carve801FF720.cpp`.
- **`src/MetroidPrime/PortLinkStubs.cpp`** - `stub_187` for `fn_801FE7E8` (with its comment), plus
  the header counts 160->161 / 156->157 / 23->24 unmangled `fn_`/`lbl_` and their history clauses.
  The count is a claim my own change moves and the file's convention is to keep it honest, so it is
  updated in the same commit.

### What was measured this run

`build/binutils/powerpc-eabi-objdump -d --start-address=0x801FFA20 --stop-address=0x801FFBA0
build/G2ME01/main.elf` -> 96 instructions, 384 bytes, 4 functions
(`fn_801FFA20` 0xAC/43, `fn_801FFACC` 0x20/8, `fn_801FFAEC` 0x4C/19, `fn_801FFB38` 0x68/26), which
confirms run 1's table. Compared word by word against 0x801FF720..0x801FF8A0 the same way run 1
compared against 0x801FF5A0: **96/96 both, 16 differing words, every one the stride (36->48) or a
call/branch target** - `mulli` 0x801FF74C/764/794 -> 0x801FFA4C/A64/A94, `addi` 0x801FF814/86C/870 ->
0x801FFB14/B6C/B70, `bl fn_801FD8E0` -> `bl fn_801FDC68` (0x801FFB10), `bl fn_801FEA98` ->
`bl fn_801FE7E8` (0x801FFB68), and the seven internal `bl`/`b`/`bne` targets relabelled. The four
`bl`s identical on both sides are `allocate__Q24rstl17rmemory_allocatorFi` (0x801FFA50),
`Free__7CMemoryFPCv` (0x801FFAA4) and the two internal calls. `fn_801FE7E8` is a forwarder
(`stwu/mflr/stw` / `bl fn_801FE808` / epilogue), `symbols.txt:8305`, unclaimed; `fn_801FDC68` is
claimed by `Carve801FDB5C.c`, so it needs no stub.

`./tools/decomp_build.sh MetroidPrime/ScriptObjects/Carve801FFA20` ->
`main/MetroidPrime/ScriptObjects/Carve801FFA20: 100.00% fuzzy, 100.00% matched (4 / 4 functions)`
and `All: ... (12599 / 28465 functions)`, so `total_functions` is still 28465.
`build/report.json` for the unit: `matched_functions 4/4`, `total_code`/`matched_code` 384/384,
`fuzzy_match_percent 100.0`, `metadata.complete true`.

`./tools/carve_diff.sh 0x801FFA20 0x180 build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801FFA20.o`:

```
retail: 96 instructions, 384 bytes
ours  : 96 instructions, 384 bytes
differing instructions: 7
```

All 7 are `bl` displacements (relocations the linker resolves) - no opcode, offset or frame
displacement differs. `powerpc-eabi-nm` on the object: four `T fn_801FF<addr>` and nothing else.
`python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FFA20` ->
`ok: 1 unit(s) checked, none emits its functions out of retail order`; definitions are descending
(`fn_801FFB38`, `fn_801FFAEC`, `fn_801FFACC`, `fn_801FFA20`). `./tools/flip_test.sh
MetroidPrime/ScriptObjects/Carve801FFA20.cpp` -> **PASS, kept as Matching** (the full check, both
directions, not just the build).

The stub, measured with it absent: `python3 tools/link_gap.py --rebuild` exits 1 and prints
`287 MISSING` then `gap grew: fn_801FE7E8 is not in port_link_gap_list.md`. With `stub_187` in
place: exits 0, `286 MISSING`, `ok: 286 MISSING symbol(s), all accounted for in
port_link_gap_list.md`. `gate.sh`'s `port link dups` reports 0 duplicates.
`fn_801FE7E8` is deliberately **not** added to `docs/research/port_link_gap_list.md`: the stub
defines it, so it never reaches the gap list, the same as its five siblings.

### Where run 1's record was stale (measured, not guessed)

- **`stub_185` is taken.** Run 1 wrote `stub_185` for `fn_801FE7E8`; on this tip `stub_185` is
  `fn_801F9848` and `stub_186` is `fn_8000408C` (both added after run 1 was written), so this run's
  stub is **`stub_187`**. A future attempt that copies the run-1 number gets a duplicate definition.
- **The undefined count moved.** Run 1 measured `unique undefined symbols 291` before and after;
  this tip's `link_gap.py` reports `286 MISSING` and the gate's probe count is a different number
  again. The invariant that matters is *no growth*, and it held in both directions here.
- Run 1's claim that the last `stub_NNN` was `stub_184` is likewise superseded.

### Lessons (not filed as `NEW:`)

- Confirming a carve against the **immediately preceding claimed range** rather than the first one
  is cheaper than confirming against a matched twin in another unit, and it is the check that
  catches the stride: this template's only variable across emissions is the element stride
  (0x24, 0x24, 0x2C, 0x30) plus the two element callees, so "N differing words, all of them the
  stride or a `bl`" is the whole delta. It held at 4 words for 0x24->0x24 and 16 for 0x24->0x30.
- `PortLinkStubs.cpp`'s stub counter is shared state with every other lane. Read the last
  `stub_NNN` in the file in this run; never take the number from a note.
- The generator's header text (`tools/gen_link_stubs.py:146-160`) prints `len(uniq_f)` /
  `len(uniq_d)` / `by_class` from the tsv only, so every hand-added stub silently makes the header's
  own numbers wrong. Nothing derives them (`grep -rn PortLinkStubs tools/*.py` finds only prose in
  `check_files_cmake.py`), so the header is a claim to maintain by hand and `gate.sh` will not
  catch a miss. That is also why this file has a duplicated `Breakdown:` paragraph, the second of
  which still reads `22 unmangled fn_/lbl_` - left alone here as pre-existing and unrelated to this
  item, but it is wrong and the next generator run should collapse the duplication.

## Run 3 on tip 3fcf5f2e (`match: carve-801b9be0`), 2026-10-02: PASS

The item was requeued a second time for the same reason as after run 1: **run 2's pass was never
landed.** Measured, not assumed - `git log --oneline --all | grep 801ffa20` is empty, there is no
commit for this item id anywhere, `git status` was clean, `ls src/MetroidPrime/ScriptObjects/
Carve801FFA20.*` failed, and `configure.py` / `splits.txt` / `files.cmake` had no `Carve801FFA20`
entry. So not a `STALE:` item; `build/report.json` had no such unit. HEAD is now two commits past
run 2's tip (`2f134b64`, `3fcf5f2e`), both of which touched `PortLinkStubs.cpp`.

**This run re-measured everything rather than replaying run 2's numbers.** All of run 2's structural
conclusions held; two of its numbers were stale again (below).

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
goal_check: item carve-801ffa20 (match) target=MetroidPrime/ScriptObjects/Carve801FFA20
goal_check: baseline .../wt-mp2-goal-L7/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12599 -> 12603   linked 5960 -> 5964
  ok    check_symbol_names.py
  ok    All:  35.46% fuzzy, 29.28% matched, 12.99% linked (12603 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FFA20.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801ffa20
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
`python3 tools/check_symbol_names.py` -> `checked 529 units; 0 declared names are missing`.

### What changed, five files

- **`src/MetroidPrime/ScriptObjects/Carve801FFA20.cpp`** (new, `extern "C"`, 4 functions) -
  `Carve801FF720.cpp`'s body with the element stride 36 -> 48 and the two element callees swapped
  (`fn_801FD8E0` -> `fn_801FDC68`, `fn_801FEA98` -> `fn_801FE7E8`). Nothing else in the bodies
  differs.
- **`configure.py:751`** - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FFA20.cpp"),`
  immediately after `Carve801FF720.cpp` (750), one line, twelve-space indent as its neighbours.
- **`config/G2ME01/splits.txt:1199-1200`** - the unit header plus
  `.text start:0x801FFA20 end:0x801FFBA0`, after `Carve801FF720.cpp` (ends 0x801FF8A0) and before
  `MetroidPrime/ScriptLoader/SpacePirate.cpp` (starts 0x80200E10). Contiguous, retail's own, spans
  no unclaimed gap: `splits.txt` has nothing at all between `start:0x801FF720` and
  `start:0x80200E10` other than the two carve headers.
- **`files.cmake:573`** - after `Carve801FF720.cpp`.
- **`src/MetroidPrime/PortLinkStubs.cpp`** - `stub_188` for `fn_801FE7E8` (with its comment), plus
  the header counts 161 -> 162 / 157 -> 158 / 24 -> 25 unmangled `fn_`/`lbl_` in the *maintained*
  header block and its clause history.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status` but I did not
touch them: `tools/check_docs_claims.py:80` rewrites the derived state block from the tree on
every run, and the diff is exactly that (12599 -> 12603 matched, 5960 -> 5964 linked, 11020 ->
11024 DOL units). The driver discards them; `goal_check.sh` reported `no judge-owned path touched`.

### What was measured this run

`build/binutils/powerpc-eabi-objdump -d` over 0x801FFA20..0x801FFBA0 and over the preceding claim
0x801FF720..0x801FF8A0, compared **at relative offsets** (this is the check that decides, and it is
the one run 2 describes): **96 instructions each, 384 bytes each, and every instruction at the same
relative offset in both**, so both ranges also carry the same four function starts
(+0x000 / +0x0AC / +0x0CC / +0x118). **10 differing words**, all of them accounted for:

```
  +0x02c  mulli r3,r30,36  -> mulli r3,r30,48     the allocation's size
  +0x044  mulli r0,r0,36   -> mulli r0,r0,48      the loop bound, first copy call
  +0x074  mulli r0,r0,36   -> mulli r0,r0,48      the loop bound, destroy call
  +0x0f0  bl     801fd8e0  -> bl     801fdc68     the element destructor
  +0x0f4  addi   r31,r31,36 -> addi r31,r31,48
  +0x148  bl     801fea98  -> bl     801fe7e8     the element copy
  +0x14c  addi   r31,r31,36 -> addi r31,r31,48
  +0x150  addi   r30,r30,36 -> addi r30,r30,48
  +0x030  bl     802fdab8  -> bl     802fdab8     allocate: same target, displacement only
  +0x084  bl     802ce388  -> bl     802ce388     Free:     same target, displacement only
```

The last two encode the *same* absolute addresses and differ only because this range sits 0x300
bytes higher. No opcode, frame displacement or internal `bl`/`b`/`bne` target differs anywhere else.
That is what makes this file `Carve801FF720.cpp` with two constants changed.

`./tools/decomp_build.sh MetroidPrime/ScriptObjects/Carve801FFA20` ->
`main/MetroidPrime/ScriptObjects/Carve801FFA20: 100.00% fuzzy, 100.00% matched (4 / 4 functions)`
and `All: ... (12603 / 28465 functions)`, so `total_functions` is still 28465, and `87 files OK`
(DOL sha1 still retail's). `build/report.json` for the unit: `matched_functions 4/4`,
`total_code`/`matched_code` 384/384, `matched_code_percent 100.0`, `complete_units 1`,
`metadata.complete true`.

`./tools/carve_diff.sh 0x801FFA20 0x180 build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801FFA20.o`
-> `retail: 96 instructions, 384 bytes / ours: 96 instructions, 384 bytes / differing
instructions: 7`, and all 7 are `bl` displacements shown as unresolved relocations in our object
(`00000030 bl 30` etc.). The script's closing `NOT byte-exact` is its own wording for "there are
relocations", which is the normal carve state; `flip_test.sh` is what decides.
`powerpc-eabi-nm` on the object: exactly four `T fn_801FFA*` at 0/0xac/0xcc/0x118, and four `U` for
the externs - nothing else defined. `python3 tools/check_decl_order.py --unit
MetroidPrime/ScriptObjects/Carve801FFA20` -> `ok: 1 unit(s) checked, none emits its functions out of
retail order`; definitions are descending (`fn_801FFB38`, `fn_801FFAEC`, `fn_801FFACC`,
`fn_801FFA20`). `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FFA20.cpp` -> **PASS,
kept as Matching** (the full check, both directions, not just the build).

The stub, measured both ways in this tree: with the carve claimed and the stub absent,
`python3 tools/link_gap.py --rebuild` prints `287 MISSING` and
`gap grew: fn_801FE7E8 is not in port_link_gap_list.md`; with `stub_188` in place the same command
prints `286 MISSING` and `ok: 286 MISSING symbol(s), all accounted for in port_link_gap_list.md`.
`fn_801FDC68` needs no stub - it is already defined by the neighbouring
`src/MetroidPrime/ScriptObjects/Carve801FDB5C.c:87`. `gate.sh`'s `port link dups` reports
`0 duplicates`, and `grep -rn "fn_801FFA20\|fn_801FFACC\|fn_801FFAEC\|fn_801FFB38" src/ include/`
finds the four names only in the new file. `fn_801FE7E8` is deliberately **not** added to
`docs/research/port_link_gap_list.md`: the stub defines it, so it never reaches the gap list, the
same as its seven siblings.

### Where run 2's record was stale (measured, not guessed)

- **The stub number moved again.** Run 2 wrote `stub_187`; on this tip `stub_187` is `fn_801B9C68`
  (added by `Carve801B9BE0.c` in `3fcf5f2e`) and `stub_186` is `fn_8000408C`, so this run's stub is
  **`stub_188`**. Copying run 2's number gives a duplicate definition.
- **The linked baseline moved.** Run 2 reported `matched 12595 -> 12599, linked 5957 -> 5961`; this
  tip starts at `12599 / 5960` and ends at `12603 / 5964`. Only the per-item delta is meaningful.
- **The header counts moved again**, 161/157/24 -> 162/158/25. Run 2's `161 / 157 / 24` is
  superseded. Run 1's `291 undefined` is now a third number for the same quantity; the invariant
  that matters is *no growth*, and it held.
- Run 1 and run 2 both quote the differing-word count as **16**; this run's word-by-word comparison
  at relative offsets finds **10** (8 real: 6 stride + 2 callees, plus 2 displacement-only `bl`s).
  16 is probably the count from a comparison that also relabelled the internal branch targets or
  compared the text section rather than the instruction words. Not load-bearing - the conclusion
  ("all differences are the stride or a `bl`") is the same - but the number itself is now measured.

### Lessons (not filed as `NEW:`)

- **The relative-offset comparison is the check worth repeating.** Comparing the target against
  the *immediately preceding claimed range* at offsets relative to each range's start, rather than
  against a matched twin in another unit, is what proves "same template, two constants changed":
  if every instruction sits at the same relative offset, the four function starts, the frame size,
  the register assignment and the internal call edges are all identical for free and the only
  words left to explain are constants and `bl`s. It is also the cheapest way to catch a wrong
  stride, which is the only thing that really varies across emissions of this template.
- **`PortLinkStubs.cpp`'s stub counter is shared state with every other lane.** Read the last
  `stub_NNN` in the file in this run; never take the number from a note. Three runs of this item
  used three different numbers (185, 187, 188) because the tip moved between them.
- **`config/G2ME01/symbols.txt` sizes are not trustworthy for function boundaries outside a claimed
  range.** For this range they agreed with `objdump` (0xAC/0x20/0x4C/0x68). For the next round at
  0x801FFBA0 they do **not**: it lists `fn_801FFBA0` 0xB4, `fn_801FFC54` 0xC8, `fn_801FFD1C` 0x54,
  `fn_801FFE4C` 0x64 - so starts at +0x000/+0x0B4/+0x17C/+0x2AC, i.e. the run is 0x360 bytes, not
  0x180, and 0x801FFBA0..0x801FFD20 (96 instructions, stride 0x0C) contains `bl fn_801FFD1C` at
  +0x17C and `bl fn_801FFE4C` at +0x2AC, **both outside itself**. That range is therefore not a
  straight copy of this file's shape, and I did not measure it further - so deliberately **no
  `NEW:` for it**: a queue entry has to name a real unit, and I cannot yet name the claimable
  contiguous range there. Its callees `fn_801FFD1C` and `fn_801FFE4C` are both unclaimed
  (`grep -rn` over `src/` and `include/` finds neither), so whoever takes it needs two
  `PortLinkStubs.cpp` stubs. Recorded here so a future run does not assume 0x801FFBA0 is a
  four-constant edit away.
- Run 1's `NEW: carve-801ff8a0` is still valid and still correctly scoped. Re-measured:
  0x801FF8A0..0x801FFBA0 is **two** rounds, not one - 192 instructions, with `mulli` strides 44
  **and** 48, and `bl`s to `fn_801FEC64`, `fn_801FDAA4` (the 44-stride half run 1 describes,
  0x801FF8A0..0x801FFA20) as well as to `fn_801FE7E8`, `fn_801FDC68` and this unit's four functions
  (the 48-stride half, 0x801FFA20..0x801FFBA0, which is this item). Run 1's NEW claims only the
  first half, so it needs no correction - but note it will need its *own* `stub_189` for
  `fn_801FEC64`, not a reuse of the `stub_188` this run adds for `fn_801FE7E8`.
