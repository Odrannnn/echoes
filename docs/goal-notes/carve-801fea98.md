# carve-801fea98 - `MetroidPrime/ScriptObjects/Carve801FEA98` (match, lane 10)

**Result: PASS.** `./tools/goal_check.sh <abs>/build/goal/item.json` ->
`goal_check: PASS carve-801fea98` (gate.sh ok - DOL sha1, 86 RELs, report diff, wiring, docs
claims, port probe; `counts: matched 12691 -> 12693   linked 6050 -> 6052`;
`check_symbol_names.py` ok; `All:  35.55% fuzzy, 29.39% matched, 13.06% linked
(12693 / 28465 functions)`; `flip_test MetroidPrime/ScriptObjects/Carve801FEA98.c: PASS,
Object(Matching) in configure.py`). The judge was re-run on the final tree (the last two edits
were comment-only and the verdict repeated).

## What was done

Wrote the 2 unsourced functions at `.text 0x801FEA98..0x801FEAE0` (0x48 = 72 bytes) as
`src/MetroidPrime/ScriptObjects/Carve801FEA98.c`, out of dtk's `main/auto_03_801FDC88_text` gap.
A carve is four files, all four in this change:

- `configure.py` - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEA98.c"),` (one line)
  between `ScriptObjects/Carve801FD8E0.c` and `ScriptObjects/Carve801FEEF0.c` (address order).
- `config/G2ME01/splits.txt` - `MetroidPrime/ScriptObjects/Carve801FEA98.c:` /
  `.text start:0x801FEA98 end:0x801FEAE0`, placed after `ScriptObjects/Carve801FDB5C.c`
  (0x801FDBE0..0x801FDC88) and before `ScriptObjects/Carve801FEEF0.c` (0x801FEEF0..0x801FEEF8);
  `total_functions` is still **28465** (`found 28465 functions` in the gate's dtk split).
- `files.cmake` - `src/MetroidPrime/ScriptObjects/Carve801FEA98.c`, same position.
- `src/MetroidPrime/ScriptObjects/Carve801FEA98.c` - definitions **descending by address**:
  `fn_801FEAB8`, `fn_801FEA98`.

One more file, and it is not decorative - see "The port side" below:

- `src/MetroidPrime/PortLinkStubs.cpp` - retired `stub_183` (`fn_801FEA98`, now defined for real
  by the carve) and added `stub_199` (`fn_801FEAE0`) in its place, plus the header's counts
  (165 supplied / 4 data objects unchanged - one function out, one in).

The claim is exactly the item's range and nothing else. The two functions are `rstl::construct<T>`'s
two halves for the 0x24-byte script-object element whose copy constructor is `fn_801FEAE0`:
`fn_801FEA98` is the in-class `construct` forwarder (`include/rstl/construct.hpp:74-77`) and
`fn_801FEAB8` is the placement-new guard (`new (dest) T(src)`, `:53-56`) - `cmplwi r3,0x0` on the
destination, then one call with both arguments untouched.

The twins are exact, re-measured this run with
`python3 tools/dol_read.py <addr> <size>` and `build/binutils/powerpc-eabi-objdump -d`:

| function | addr | size/insns | twin | bytes vs twin | body written |
|---|---|---|---|---|---|
| `fn_801FEA98` | 0x801FEA98 | 0x20 / 8 | `fn_80004D3C` 0x80004D3C 0x20, `src/MetroidPrime/Player/Carve80004C4C.c` | **8 of 8**, `bl` included (`48000015` both - each callee is 0x14 past its own `bl`) | `fn_801FEAB8(self, src);` |
| `fn_801FEAB8` | 0x801FEAB8 | 0x28 / 10 | `fn_80004D5C` 0x80004D5C 0x28, `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp` | **9 of 10**, the `bl` at +0x14 the one difference (`48000015` -> 0x801FEAE0 here, `4bfffd31` -> 0x80004AA0 there) | `if (self != 0) fn_801FEAE0(self, src);` |

Retail's own bytes at the range, read from the disc this run:
`94 21 ff f0 7c 08 02 a6 90 01 00 14 48 00 00 15 80 01 00 14 7c 08 03 a6 38 21 00 10 4e 80 00 20
94 21 ff f0 7c 08 02 a6 28 03 00 00 90 01 00 14 41 82 00 08 48 00 00 15 80 01 00 14 7c 08 03 a6
38 21 00 10 4e 80 00 20` (`tools/dol_read.py 0x801FEA98 0x48`).

## What the pair is, from the callers

- `fn_801FF838` (0x801FF838, `ScriptObjects/Carve801FF720.cpp`, a `Matching` unit) calls
  `fn_801FEA98` at **0x801FF868** - confirmed on the linked binary - with `addi r31,r31,36` right
  after, so the pair constructs the 0x24-byte element that `Carve801FF720.cpp`'s walk copies.
- `fn_801FEAE0` (0x801FEAE0, 0x68, unclaimed) is the element's real copy constructor: it stores the
  vtable pair `lbl_803B7BCC` / `lbl_803B7BF0` into `+0x00`, copies the `rstl::basic_string` at
  `+0x04` through `__ct__Q24rstl66basic_string<...>`, and calls `fn_801FE8B8` (0x801FE8B8, 0xC4) on
  the member at `+0x14`.
- Neighbours: `fn_801FEA2C` (0x801FEA2C, 0x6C) ends exactly where this claim starts; `fn_801FEAE0`
  starts exactly where it ends. That is why the range is what it is - a claim may not span an
  unclaimed gap.

## The port side (this is the part that is easy to miss)

`fn_801FEAB8`'s `bl` names `fn_801FEAE0`, which no unit claims, so the DOL link resolves it from
dtk's own `auto_*` object but the port's link has nothing. Measured both ways in this tree:

- without a stand-in: `python3 tools/link_gap.py --rebuild` exits 1 and prints
  `286  MISSING` + `gap grew: fn_801FEAE0 is not in port_link_gap_list.md`;
- with `stub_199`: the same command prints `285  MISSING`, `all accounted for in
  port_link_gap_list.md` (exit 0), and the gate's strict probe prints
  `LINKED (291 undefined, 0 duplicates)` - the baseline count, unchanged.

So the carve has to hand `PortLinkStubs.cpp` a function stub for its callee, exactly as
`Carve801FD8E0.c` did the day before (`fn_801FD8E0`'s stub out, `fn_801FD924`'s in). The stub is
announced, empty, and `PortLinkStubs.cpp` is not in `configure.py`, so it cannot reach `main.dol`.
`fn_801FEAE0` is **not** carved here: its 0x68 bytes need the `.data` vtable pair, the
`basic_string` copy and `fn_801FE8B8`'s body, none of which this claim touches.

## Verification (all measured in this lane's tree, nothing recalled)

- `./tools/decomp_build.sh` -> `All:  35.55% fuzzy, 29.39% matched, 13.06% linked
  (12693 / 28465 functions)`; `sha1sum build/G2ME01/main.dol` =
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, the expected one.
- `build/report.json`: `main/MetroidPrime/ScriptObjects/Carve801FEA98` is **2 / 2**, 100.00%
  (`total_code 72`, `matched_code 72`, `complete_units 1`). The old gap split cleanly:
  `main/auto_03_801FDC88_text` 32 -> **19**, new `main/auto_03_801FEAE0_text` **11**
  (19 + 11 + 2 = 32), and the gate's diff says
  `SPLIT main/auto_03_801FDC88_text: 13 function(s) moved into main/MetroidPrime/ScriptObjects/Carve801FEA98, main/auto_03_801FEAE0_text (exact count match - a split, not a loss)`.
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FEA98.c` -> `PASS  -> kept as Matching`.
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FEA98.c` -> `.text claimed 72, ours 72,
  retail 72, fits`; `no extra functions`.
- `python3 tools/check_decl_order.py` -> `ok: 1042 unit(s) checked, 28 permuted, all 28 accounted
  for in decl_order.md` (this unit is not among the 28).
- `python3 tools/check_symbol_names.py` -> `checked 532 units; 0 declared names are missing`.
- `python3 tools/check_files_cmake.py` -> `every configured DOL object is either in files.cmake or
  excluded with a reason`.
- `python3 tools/check_module_wiring.py`, `check_docs_claims.py`, `check_raw_offsets.py` and the
  86-REL `cmp` pass inside `gate.sh` (`GATE PASS  e11b7d32+7 changed`); this unit has no raw
  offsets to record.

## Notes for the next run

- **`tools/carve_diff.sh 0x801FEA98 0x48 <obj>` reports `NOT byte-exact` with 2 differing
  instructions for this unit, and that is the expected reading, not a failure.** It compares
  retail's *linked* bytes (out of `main.elf`) against the raw `.o`, so every `bl` is an unresolved
  relocation there. The two differences here are exactly the two call words, and both resolve to
  this unit's own functions (`fn_801FEA98+0xc` -> 0x801FEAB8, `fn_801FEAB8+0x14` -> 0x801FEAE0).
  `flip_test.sh` and the DOL sha1 are the verdict; `carve_diff.sh` is only a locator.
- `flip_test.sh` and `unit_fit.sh` take the unit **as `configure.py` writes it**
  (`MetroidPrime/ScriptObjects/Carve801FEA98.c`); passing `src/...` prints
  `SKIP ... not listed in configure.py` / `not declared in any splits.txt`, which reads like a
  broken carve and is only a wrong argument.
- The callee-exchange measurement costs two `link_gap.py --rebuild` runs (~7 s and ~2 s here once
  the port tree was warm) and is worth taking: the number in the `PortLinkStubs.cpp` comment is
  what the next reader checks the trade against.

No `WALL:`, no `STALE:` - both functions matched and the unit flipped. No `NEW:` blocker: the one
function left behind, `fn_801FEAE0`, cannot reach 100% on its own (it needs the `.data` vtable pair
and two unclaimed bodies), so it is not a candidate lane.

## Run 2 - lane 10, worktree `wt-mp2-goal-L10`, 2026-10-02 (independent re-do)

**Result: PASS again, in a different tree state with a different diff.** `./tools/goal_check.sh
build/goal/item.json` -> `goal_check: PASS carve-801fea98`, `counts: matched 12699 -> 12701
linked 6058 -> 6060`. This tree (`git log -1` = `7196a62c match: carve-80004438`) did **not** have
the carve: no `Carve801FEA98.c`, no `Object(Matching, ...)`, no splits entry. Early
`Carve801FDAA4.c` and `Carve801FEE40.c` had landed, so the port-stub numbering differs from run 1's
(here `stub_183` was still `fn_801FEA98`, `stub_199` was `fn_801FDAE8`, highest stub was
`stub_200`), and the split arithmetic differs too.

### Re-measured before writing anything (not recalled from run 1)

- Retail bytes at 0x801FEA98..0x801FEAE0, `python3 tools/dol_read.py 0x801FEA98 0x48`: the same 72
  bytes run 1 recorded (`94 21 ff f0 ... 38 21 00 10 4e 80 00 20` twice, the two `bl`s `48 00 00
  15`).
- Twin `fn_80004D3C` (0x80004D3C, 0x20): **8 of 8 words identical**, `bl` included.
- Twin `fn_80004D5C` (0x80004D5C, 0x28): **9 of 10 words**; the one difference is the `bl` at
  +0x14, `4b ff fd 31` -> 0x80004AA0 there, `48 00 00 15` -> 0x801FEAE0 here.
- Element stride 0x24 = 36, re-derived from **bytes this run**, not from run 1's note:
  `fn_801FF838` in `build/G2ME01/main.elf` does `bl fn_801FEA98` at 0x801FF868 with `mr r3,r30` /
  `mr r4,r31` at 0x801FF860-0x801FF864 and steps both cursors afterwards (`addi r31,r31,36` at
  0x801FF86C, `addi r30,r30,36` at 0x801FF870); the unclaimed walk `fn_801FEA2C` calls it at
  0x801FEA64 and steps its destination cursor by 0x24 (`addi r31,r31,0x24`, 0x801FEA6C).

### The diff (5 files, same shape as run 1)

- `src/MetroidPrime/ScriptObjects/Carve801FEA98.c` (new) - `fn_801FEAB8` then `fn_801FEA98`
  (descending), bodies `if (self != 0) fn_801FEAE0(self, src);` and
  `fn_801FEAB8(dest, src);`. One `extern` callee, `fn_801FEAE0`.
- `configure.py` - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEA98.c"),` on one line,
  inserted between `Carve801FDAA4.c` and `Carve801FEE40.c` (the ascending run in this file; note
  `Carve801FDB5C.c` itself sits out of address order just above, which predates this item).
- `config/G2ME01/splits.txt` - `MetroidPrime/ScriptObjects/Carve801FEA98.c:` /
  `.text start:0x801FEA98 end:0x801FEAE0`, between `Carve801FDB5C.c` and `Carve801FEE40.c`;
  `total_functions` still **28465**.
- `files.cmake` - same position.
- `src/MetroidPrime/PortLinkStubs.cpp` - **retired `stub_183`** (`fn_801FEA98`, now defined for
  real by the carve) and added **`stub_201`** (`fn_801FEAE0`, the callee the carve's own bytes
  name). One function stub out, one in; the header counts stay 165 functions + 4 data objects
  (measured: `grep -cE 'asm\("'` = 169, of which `grep -cE 'asm\("(fn_|lbl_)'` = 33 - both
  unchanged).

### Measurements of this run

- `./tools/decomp_build.sh` -> `All:  35.55% fuzzy, 29.39% matched, 13.07% linked (12701 / 28465
  functions)`; `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `build/report.json`: `main/MetroidPrime/ScriptObjects/Carve801FEA98` **2 / 2**, 72/72 bytes;
  the split went `main/auto_03_801FDC88_text` **29 -> 19** functions (4536 -> 3600 bytes) with a
  new `main/auto_03_801FEAE0_text` of **8** functions / 864 bytes (0x801FEAE0..0x801FEE40;
  19 + 8 + 2 = 29). Run 1 recorded **11** for that auto unit - in that tree the range still ran to
  0x801FEEF0, which is the tail `Carve801FEE40.c` has since claimed here. Do not carry the 11.
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FEA98.c` -> `PASS -> kept as Matching`.
- `./tools/unit_fit.sh ...` -> `.text claimed 72, ours 72, retail 72, fits`; `no extra functions`.
- `python3 tools/check_decl_order.py` -> `1046 unit(s) checked, 28 permuted, all accounted for`
  (this unit not among the 28). `check_symbol_names.py` -> `532 units; 0 missing`.
  `check_files_cmake.py` -> every configured DOL object in `files.cmake` or excluded with a reason.
- Port side, both directions measured: with the carve in `files.cmake` and `stub_183` still
  present, `tools/link_check.sh --rebuild` prints `unique undefined symbols 292`, `duplicate
  definitions   1`, `DUP fn_801FEA98`, `FAIL duplicates went 0 -> 1` and
  `FAIL undefined went 291 -> 292`; `python3 tools/link_gap.py --rebuild` exits 1 with
  `286  MISSING` and `gap grew: fn_801FEAE0 is not in port_link_gap_list.md`. After the exchange:
  `291 unique undefined symbols, 0 duplicate definitions`, `unchanged from baseline`, and
  `link_gap.py --rebuild` exits 0 with `285  MISSING ... all accounted for`.
- `tools/carve_diff.sh 0x801FEA98 0x48 <obj>` -> `NOT byte-exact`, **2 differing instructions, both
  `bl` words** (`fn_801FEA98+0xc` -> 0x801FEAB8, `fn_801FEAB8+0x14` -> 0x801FEAE0). Expected: it
  compares retail's *linked* bytes against the raw `.o`, where those are the relocations. The
  linked bytes are right: `objdump -s -j .text --start-address=0x801FEA98 --stop-address=0x801FEAE0
  build/G2ME01/main.elf` shows retail's 72 bytes exactly, both `bl`s included.
- `python3 tools/check_docs_claims.py` -> `docs claims agree with the tree`.
- `goal_check.sh` also rewrites the derived counts in `docs/HANDOFF.md` and
  `docs/RUNNING_THE_DECOMP.md`; both show as modified in `git status` for that reason alone (the
  twin state-block diff, nothing else).

### One stale-artifact warning for the next run

`build/G2ME01/asm/auto_03_801FEAE0_text.s` already existed in this worktree before this run
(mtime 16:12, an earlier reset attempt's ignored build output). Its header reads
`0x801FEAE0..0x801FEEF0 | size: 0x410` and it holds 11 functions - i.e. it predates
`Carve801FEE40.c`'s claim and does **not** describe the split this carve produces (8 functions,
864 bytes). `build/report.json` before this run had no `auto_03_801FEAE0_text` unit at all, so the
artifact was not part of the judged tree state. Read the split out of `report.json`, never out of a
pre-existing `.s` in `build/`.

### Verdict for this run

No `WALL:` and no `STALE:`: both functions matched, the unit flipped, and the flip is `Matching`
with the judge green. No `NEW:` blocker - unchanged from run 1: the one function left behind,
`fn_801FEAE0` (0x801FEAE0, 0x68), cannot reach 100% on its own (it needs the `.data` vtable pair
`lbl_803B7BCC`/`lbl_803B7BF0` plus the `basic_string` copy constructor and `fn_801FE8B8`'s 0xC4
bytes, none of them claimed), so it is not a candidate lane.

## Run 3 - lane 11, worktree `wt-mp2-goal-L11`, 2026-10-02 (third independent re-do)

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS carve-801fea98`
on the final tree; `counts: matched 13020 -> 13022   linked 6124 -> 6126`; every gate step `ok`
(`GATE PASS  7347718e+7 changed`); `flip_test ... PASS, Object(Matching) in configure.py`.

### Tree state at the start (measured, not recalled from runs 1-2)

- `git log -1` = `7347718e docs: record the docs-bloat repair and the history rewrite`, `git status`
  clean. The carve was **absent**: no `Carve801FEA98.c`, no `Object(...)`, no splits entry, no
  `files.cmake` entry. `Carve801FD5E8/638/8E0/DAA4/FEE40/FEEF0` had landed.
- The ninth upstream sync is in, and this tree's `PortLinkStubs.cpp` supplies **193** symbols
  (187 functions + 6 data objects), not run 2's 165/4 - so the port-side numbering and arithmetic
  are different again. `stub_183` was still `fn_801FEA98`; the highest stub was `stub_224`.
- `build/report.json` before the change: `main/auto_03_801FDC88_text` **29** functions / 4536 bytes
  (0x801FDC88..0x801FEE40), and **no** `main/auto_03_801FEAE0_text` unit. The recorded port
  baseline (`docs/research/port_link_baseline.txt`) is `undefined 287`, and
  `build-port-link/link_summary.txt` read `287 0 0 1` before the change - the two agree.

### Re-measured this run before writing anything

- Retail bytes, `python3 tools/dol_read.py 0x801FEA98 0x48`: `94 21 ff f0 7c 08 02 a6 90 01 00 14
  48 00 00 15 80 01 00 14 7c 08 03 a6 38 21 00 10 4e 80 00 20` (fn_801FEA98, 8 insns) then
  `94 21 ff f0 7c 08 02 a6 28 03 00 00 90 01 00 14 41 82 00 08 48 00 00 15 ... 4e 80 00 20`
  (fn_801FEAB8, 10 insns). Both `bl`s are `48000015`, i.e. +0x14 past themselves - the twins' words.
- Twin `fn_80004D3C` (0x80004D3C, 0x20, `src/MetroidPrime/Player/Carve80004C4C.c`): **8 of 8 words
  identical**, `bl` included. Twin `fn_80004D5C` (0x80004D5C, 0x28,
  `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`): **9 of 10**, the one difference the `bl`
  at +0x14 (`4bfffd31` -> 0x80004AA0 there, `48 00 00 15` -> 0x801FEAE0 here). The absence of any
  `li r4,-1` is what makes this pair `construct` and not `destroy_impl`.
- Element 0x24 = 36, re-derived from **bytes this run**: `fn_801FEA2C` (0x801FEA2C, 0x6C,
  unclaimed) is a count-first loop that `bl fn_801FEA98` at 0x801FEA64 with `mr r3,r31` /
  `mr r4,r29` and `addi r31,r31,0x24` at 0x801FEA6C; `fn_801FF838` (in `Carve801FF720.cpp`,
  Matching) does `bl fn_801FEA98` at 0x801FF868 with `mr r3,r30` / `mr r4,r31` and steps both
  cursors by 36. Callee `fn_801FEAE0` (0x801FEAE0, 0x68) stores `lbl_803B7BCC` then `lbl_803B7BF0`
  into +0x0, copies the `basic_string` at +0x4 through
  `__ct__Q24rstl66basic_string<...>` and calls `fn_801FE8B8` (0x801FE8B8, 0xC4) on +0x14.
- Boundaries: `fn_801FEA2C` + 0x6C = 0x801FEA98 exactly (claim start), `fn_801FEAE0` starts exactly
  at the claim end. `symbols.txt:8311-8313`.

### The diff (5 files, same shape as runs 1-2)

- `src/MetroidPrime/ScriptObjects/Carve801FEA98.c` (new) - `fn_801FEAB8` then `fn_801FEA98`
  (descending), bodies `if (dest != 0) fn_801FEAE0(dest, src);` and `fn_801FEAB8(dest, src);`, one
  `extern` callee `fn_801FEAE0`.
- `configure.py` - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEA98.c"),` on one line,
  between `Carve801FDAA4.c` and `Carve801FEE40.c` (that run in this file is **descending**, unlike
  splits.txt's ascending order).
- `config/G2ME01/splits.txt` - `MetroidPrime/ScriptObjects/Carve801FEA98.c:` /
  `.text start:0x801FEA98 end:0x801FEAE0`, between `Carve801FDB5C.c` and `Carve801FEE40.c`;
  `total_functions` still **28465** (`report.json` measures `total_functions 28465`).
- `files.cmake` - same position as configure.py.
- `src/MetroidPrime/PortLinkStubs.cpp` - **retired `stub_183`** (`fn_801FEA98`) and added
  **`stub_225`** (`fn_801FEAE0`, the callee the carve's own bytes name), plus the header's
  narrative line. One function stub out, one in; measured `grep -cE 'asm\("'` = **193** and
  `grep -cE 'asm\("(fn_|lbl_)'` = **33** before and after, so the header's 187 functions + 6 data
  objects still holds.

### Measurements of this run

- `./tools/decomp_build.sh` -> `All:  36.96% fuzzy, 30.40% matched, 13.39% linked
  (13022 / 28465 functions)`; `sha1sum build/G2ME01/main.dol` =
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (the expected one; the gate's own `hashes vs
  config.yml` step, which covers the DOL and all 86 RELs, prints `ok`).
- `build/report.json`: `main/MetroidPrime/ScriptObjects/Carve801FEA98` **2 / 2**, 72/72 bytes,
  `complete_code_percent 100.0`; the gap split exactly as runs 1-2 saw it -
  `main/auto_03_801FDC88_text` **29 -> 19** functions (4536 -> 3600 bytes) and a new
  `main/auto_03_801FEAE0_text` of **8** functions / 864 bytes (0x801FEAE0..0x801FEE40;
  19 + 8 + 2 = 29, 3600 + 864 + 72 = 4536). The gate's diff line is
  `SPLIT main/auto_03_801FDC88_text: 10 function(s) moved into
  main/MetroidPrime/ScriptObjects/Carve801FEA98, main/auto_03_801FEAE0_text (exact count match - a
  split, not a loss)`, with no `WORSE`/`GONE`/`FELL` anywhere in `build/gate-diff.log`.
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FEA98.c` -> `PASS -> kept as Matching`.
  `./tools/unit_fit.sh ...` -> `.text claimed 72, ours 72, retail 72, fits`; `no extra functions`.
  `check_decl_order.py` -> `1053 unit(s) checked, 36 permuted, all accounted for` (this unit not
  among them). `check_symbol_names.py` -> `575 units; 0 declared names are missing`.
- Port side, **both directions measured** on this tree's 287-undefined baseline:
  - carve in, `stub_183` retired, no `fn_801FEAE0` stub: `tools/link_check.sh` prints
    `unique undefined symbols 288`, `duplicate definitions 0`, `FAIL undefined went 287 -> 288`,
    and `build-port-link/link_undefined.txt` names exactly one new symbol, `fn_801FEAE0`;
    `python3 tools/link_gap.py --rebuild` exits 1 with `282  MISSING` and
    `gap grew: fn_801FEAE0 is not in port_link_gap_list.md`.
  - with `stub_225`: `287 undefined, 0 duplicates`, `unchanged from baseline`; link_gap.py exits 0
    with `281  MISSING ... all accounted for in port_link_gap_list.md`.
  - **So this tree's port gap baseline is 281, not run 2's 285/286** - do not carry those.
- Gate: `probe: 808 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0 duplicates)`;
  `GATE PASS  7347718e+7 changed`; `check_docs_claims.py --write` -> `docs claims agree with the
  tree`.
- `tools/carve_diff.sh 0x801FEA98 0x48 <obj>` -> `NOT byte-exact`, **2 differing instructions, both
  `bl` words** (`fn_801FEA98+0xc` -> fn_801FEAB8, `fn_801FEAB8+0x14` -> fn_801FEAE0). That is the
  expected reading, not a failure: it compares retail's *linked* bytes against the raw `.o`, where
  those two words are relocations. The linked bytes are right:
  `objdump -s -j .text --start-address=0x801FEA98 --stop-address=0x801FEAE0 build/G2ME01/main.elf`
  shows retail's 72 bytes exactly, both `48000015` included.

### Notes for the next run

- The stub number to use is tree-dependent: this tree's highest was `stub_224`, so the new one is
  `stub_225`. Read it out of the file, never from an earlier run's note.
- `build/G2ME01/asm/` in this tree held no stale `auto_03_801FEAE0_text.s` (run 2 was bitten by
  one), but the rule stands: read the split out of `build/report.json`, never out of a
  pre-existing `.s` under `build/`.
- `link_gap.py`'s MISSING count and `link_check.sh`'s undefined count are different instruments on
  different numbers (281 vs 287 here). Quote the one you measured, and say which.
- One pre-existing inconsistency in a file this item touches, left alone because it is not this
  item's business: `PortLinkStubs.cpp`'s header `Breakdown:` line (86 REL loader + 45 game method +
  33 unmangled + 1 allocator + 4 vtable/typeinfo = 169) does not sum to the 193 total the same
  header states, and by the header's own residual rule the game-method term is 69. Measured this
  run: 193 `asm("`, 187 function stubs, 6 data objects, 33 `fn_`+`lbl_`. A docs-only fix, so no
  `NEW:`.

### Verdict for this run

No `WALL:` and no `STALE:`: both functions matched, the unit flipped, the judge is green on the
final tree. No `NEW:` blocker, unchanged from runs 1-2: the one function left behind, `fn_801FEAE0`
(0x801FEAE0, 0x68), cannot reach 100% on its own (it needs the `.data` pair
`lbl_803B7BCC`/`lbl_803B7BF0`, the `basic_string` copy constructor and `fn_801FE8B8`'s 0xC4 bytes,
none of them claimed), so it is not a candidate lane.

## Run 3 - lane 11, worktree `wt-mp2-goal-L11`, 2026-10-02 (third independent re-do)

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` ->
`goal_check: PASS carve-801fea98`; `counts: matched 13020 -> 13022   linked 6124 -> 6126`;
`gate.sh` ok on every step (`GATE PASS  7347718e+7 changed`); `flip_test` PASS with
`Object(Matching)` in `configure.py`.

### Tree state before this run (measured, not carried from runs 1-2)

- `git log -1` = `7347718e docs: record the docs-bloat repair and the history rewrite`, tree clean.
- The carve was **absent**: no `src/MetroidPrime/ScriptObjects/Carve801FEA98.c`, no `Object(...)`, no
  splits entry, no `files.cmake` entry. `Carve801FD5E8/638/8E0/DAA4/FEE40/FEEF0` had landed.
- This tree is well past run 2's: the ninth upstream sync is in and `PortLinkStubs.cpp` supplies
  **193** symbols (187 functions + 6 data objects), not run 2's 165/4 - so the stub numbering and
  every port-side number below differ from both earlier runs. Highest stub number here was `224`.
- `build/report.json` before the change: `main/auto_03_801FDC88_text` **29** functions / 4536 bytes
  (0x801FDC88..0x801FEE40) and **no** `main/auto_03_801FEAE0_text` unit. The stale-artifact warning
  from run 2 did not apply: `build/G2ME01/asm/` held no `auto_03_801FEAE0_text.s`.

### Re-measured before writing anything

- Retail bytes at 0x801FEA98..0x801FEAE0, `python3 tools/dol_read.py 0x801FEA98 0x48`: the same 72
  bytes runs 1-2 recorded - two 8/10-instruction frames, both `bl`s `48 00 00 15`.
- Twin `fn_80004D3C` (0x80004D3C, 0x20, `Player/Carve80004C4C.c`): **8 of 8 words identical**, `bl`
  included (each callee is 0x14 past its own `bl`). Twin `fn_80004D5C` (0x80004D5C, 0x28,
  `Player/CGameStateBlockConstruct.cpp`): **9 of 10**; the one difference is the `bl` at +0x14
  (`4b ff fd 31` -> 0x80004AA0 there, `48 00 00 15` -> 0x801FEAE0 here).
- Element stride 0x24 = 36, re-derived from bytes this run, not from a note: `fn_801FEA2C`
  (0x801FEA2C, 0x6C, unclaimed) is a count-first loop - `bl fn_801FEA98` at 0x801FEA64 with
  `mr r3,r31` / `mr r4,r29`, then `addi r30,r30,0x1` / `addi r31,r31,0x24`; and `fn_801FF838` in
  `Carve801FF720.cpp` does `bl fn_801FEA98` at 0x801FF868 with `mr r3,r30` / `mr r4,r31` and steps
  both cursors by 36 afterwards. `fn_801FEAE0` is the element's copy constructor (vtable pair
  `lbl_803B7BCC`/`lbl_803B7BF0` into +0x0, the `basic_string` at +0x4, `fn_801FE8B8` on +0x14).
- Claim boundaries: `fn_801FEA2C` ends exactly at 0x801FEA98 (0x801FEA2C + 0x6C) and is unclaimed;
  `fn_801FEAE0` starts exactly at the claim's end and is unclaimed. No gap is spanned.

### The diff (5 files)

- `src/MetroidPrime/ScriptObjects/Carve801FEA98.c` (new) - `fn_801FEAB8` then `fn_801FEA98`
  (descending), bodies `if (dest != 0) fn_801FEAE0(dest, src);` and `fn_801FEAB8(dest, src);`. One
  `extern` callee, `fn_801FEAE0`, declared and never defined.
- `configure.py` - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEA98.c"),` on one line,
  between `Carve801FDAA4.c` and `Carve801FEE40.c` (that run is descending by address).
- `config/G2ME01/splits.txt` - `MetroidPrime/ScriptObjects/Carve801FEA98.c:` /
  `.text start:0x801FEA98 end:0x801FEAE0`, between `Carve801FDB5C.c` and `Carve801FEE40.c`;
  `total_functions` still **28465** (`total_functions 28465` in `build/report.json`).
- `files.cmake` - same position as `configure.py`.
- `src/MetroidPrime/PortLinkStubs.cpp` - **retired `stub_183`** (`fn_801FEA98`, now defined for real
  by the carve) and added **`stub_225`** (`fn_801FEAE0`, the callee the carve's own bytes name),
  plus the header's exchange note. Measured after the edit: `grep -cE 'asm\("'` = 193 and
  `grep -cE 'asm\("(fn_|lbl_)'` = 33, both unchanged - one function stub out, one in.

### Measurements of this run

- `./tools/decomp_build.sh` -> `All:  36.96% fuzzy, 30.40% matched, 13.39% linked
  (13022 / 28465 functions)`; `sha1sum build/G2ME01/main.dol` =
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, the expected one.
- `build/report.json`: `main/MetroidPrime/ScriptObjects/Carve801FEA98` **2 / 2**, 72/72 bytes,
  `complete_code_percent` 100.0. The split: `main/auto_03_801FDC88_text` **29 -> 19** functions
  (4536 -> 3600 bytes) and a new `main/auto_03_801FEAE0_text` of **8** functions / 864 bytes
  (0x801FEAE0..0x801FEE40); 19 + 8 + 2 = 29, 3600 + 864 + 72 = 4536. The gate's diff says
  `SPLIT main/auto_03_801FDC88_text: 10 function(s) moved into main/MetroidPrime/ScriptObjects/Carve801FEA98, main/auto_03_801FEAE0_text (exact count match - a split, not a loss)`, with no
  `WORSE`/`GONE`/`FELL` line anywhere.
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FEA98.c` -> `PASS -> kept as Matching`.
- `./tools/unit_fit.sh ...` -> `.text claimed 72, ours 72, retail 72, fits`; `no extra functions`.
- `python3 tools/check_decl_order.py` -> `1053 unit(s) checked, 36 permuted, all 36 accounted for`
  (this unit is not among them). `check_symbol_names.py` -> `575 units; 0 declared names missing`.
  `check_files_cmake.py` -> every configured DOL object in `files.cmake` or excluded with a reason.
- Port side, **both directions measured in this tree** (its baseline is 287 undefined, 0 duplicates):
  - carve in `files.cmake`, `stub_183` retired, no stub for `fn_801FEAE0`:
    `./tools/link_check.sh` prints `unique undefined symbols 288`, `duplicate definitions 0`,
    `FAIL undefined went 287 -> 288`, and `build-port-link/link_undefined.txt` gains exactly
    `fn_801FEAE0`; `python3 tools/link_gap.py --rebuild` exits 1 with `282  MISSING` and
    `gap grew: fn_801FEAE0 is not in port_link_gap_list.md`.
  - with `stub_225`: `287 undefined, 0 duplicates`, `unchanged from baseline`, and
    `link_gap.py --rebuild` exits 0 with `281  MISSING`, all accounted for. So **this tree's gap
    baseline is 281**, not runs 1-2's 285/286 - do not carry those.
- `./tools/probe_sources.sh` -> `probe: 808 files, 0 failed, 0 errors; link: LINKED (287 undefined,
  0 duplicates)`.
- `tools/carve_diff.sh 0x801FEA98 0x48 <obj>` -> `NOT byte-exact`, **2 differing instructions, both
  `bl` words** (`fn_801FEA98+0xc` -> 0x801FEAB8, `fn_801FEAB8+0x14` -> 0x801FEAE0). Expected: it
  compares retail's *linked* bytes against the raw `.o`, where those two are relocations. The linked
  bytes are right: `objdump -s -j .text --start-address=0x801FEA98 --stop-address=0x801FEAE0
  build/G2ME01/main.elf` shows retail's 72 bytes exactly, both `48 00 00 15` included.

### Notes for the next run

- `flip_test.sh` and `unit_fit.sh` take the unit **as `configure.py` writes it**
  (`MetroidPrime/ScriptObjects/Carve801FEA98.c`); passing `src/...` prints
  `SKIP ... not listed in configure.py`, which reads like a broken carve and is only a wrong
  argument.
- `goal_check.sh` rewrites the derived counts in `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md`
  (`MP_GATE_DOCS_WRITE=1`), so both show as modified in `git status` for that reason alone. The
  un-rewritten state block fails `check_docs_claims.py` (measured: 4 `missing:`/`stale:` lines);
  with `--write` it prints `docs claims agree with the tree`.
- The stub number to use next in this tree's file is 226 (225 was the highest free one this run).
- Observation, pre-existing and **not** fixed (not this item's business): `PortLinkStubs.cpp`'s own
  `Breakdown:` line sums to 169 (86 + 45 + 33 + 1 + 4) while the same header states 193. Measured
  this run: total `asm("` = 193, function stubs = 187, data objects = 6, `fn_`/`lbl_` = 33; by the
  header's own residual formula the game-method term is 69, not 45.

### Verdict for this run

No `WALL:` and no `STALE:`: both functions matched, the unit flipped, and the flip is `Matching`
with the judge green. No `NEW:` blocker - unchanged from runs 1-2: the one function left behind,
`fn_801FEAE0` (0x801FEAE0, 0x68), cannot reach 100% on its own (it needs the `.data` vtable pair
`lbl_803B7BCC`/`lbl_803B7BF0`, the `basic_string` copy constructor and `fn_801FE8B8`'s 0xC4 bytes,
none of them claimed), so it is not a candidate lane.

## Run 3 - lane 11, worktree `wt-mp2-goal-L11`, 2026-10-02 (third independent re-do)

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS carve-801fea98`
on the final tree; `counts: matched 13020 -> 13022   linked 6124 -> 6126`; every gate step `ok`
(`GATE PASS  7347718e+7 changed`); `flip_test ... PASS, Object(Matching) in configure.py`.

### Tree state at the start (measured, not recalled from runs 1-2)

- `git log -1` = `7347718e docs: record the docs-bloat repair and the history rewrite`, `git status`
  clean. The carve was **absent**: no `Carve801FEA98.c`, no `Object(...)`, no splits entry, no
  `files.cmake` entry. `Carve801FD5E8/638/8E0/DAA4/FEE40/FEEF0` had landed.
- The ninth upstream sync is in, and this tree's `PortLinkStubs.cpp` supplies **193** symbols
  (187 functions + 6 data objects), not run 2's 165/4 - so the port-side numbering and arithmetic
  are different again. `stub_183` was still `fn_801FEA98`; the highest stub was `stub_224`.
- `build/report.json` before the change: `main/auto_03_801FDC88_text` **29** functions / 4536 bytes
  (0x801FDC88..0x801FEE40), and **no** `main/auto_03_801FEAE0_text` unit. The recorded port
  baseline (`docs/research/port_link_baseline.txt`) is `undefined 287`, and
  `build-port-link/link_summary.txt` read `287 0 0 1` before the change - the two agree.

### Re-measured this run before writing anything

- Retail bytes, `python3 tools/dol_read.py 0x801FEA98 0x48`: `94 21 ff f0 7c 08 02 a6 90 01 00 14
  48 00 00 15 80 01 00 14 7c 08 03 a6 38 21 00 10 4e 80 00 20` (fn_801FEA98, 8 insns) then
  `94 21 ff f0 7c 08 02 a6 28 03 00 00 90 01 00 14 41 82 00 08 48 00 00 15 ... 4e 80 00 20`
  (fn_801FEAB8, 10 insns). Both `bl`s are `48000015`, i.e. +0x14 past themselves - the twins' words.
- Twin `fn_80004D3C` (0x80004D3C, 0x20, `src/MetroidPrime/Player/Carve80004C4C.c`): **8 of 8 words
  identical**, `bl` included. Twin `fn_80004D5C` (0x80004D5C, 0x28,
  `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`): **9 of 10**, the one difference the `bl`
  at +0x14 (`4bfffd31` -> 0x80004AA0 there, `48 00 00 15` -> 0x801FEAE0 here). The absence of any
  `li r4,-1` is what makes this pair `construct` and not `destroy_impl`.
- Element 0x24 = 36, re-derived from **bytes this run**: `fn_801FEA2C` (0x801FEA2C, 0x6C,
  unclaimed) is a count-first loop that `bl fn_801FEA98` at 0x801FEA64 with `mr r3,r31` /
  `mr r4,r29` and `addi r31,r31,0x24` at 0x801FEA6C; `fn_801FF838` (in `Carve801FF720.cpp`,
  Matching) does `bl fn_801FEA98` at 0x801FF868 with `mr r3,r30` / `mr r4,r31` and steps both
  cursors by 36. Callee `fn_801FEAE0` (0x801FEAE0, 0x68) stores `lbl_803B7BCC` then `lbl_803B7BF0`
  into +0x0, copies the `basic_string` at +0x4 through
  `__ct__Q24rstl66basic_string<...>` and calls `fn_801FE8B8` (0x801FE8B8, 0xC4) on +0x14.
- Boundaries: `fn_801FEA2C` + 0x6C = 0x801FEA98 exactly (claim start), `fn_801FEAE0` starts exactly
  at the claim end. `symbols.txt:8311-8313`.

### The diff (5 files, same shape as runs 1-2)

- `src/MetroidPrime/ScriptObjects/Carve801FEA98.c` (new) - `fn_801FEAB8` then `fn_801FEA98`
  (descending), bodies `if (dest != 0) fn_801FEAE0(dest, src);` and `fn_801FEAB8(dest, src);`, one
  `extern` callee `fn_801FEAE0`.
- `configure.py` - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEA98.c"),` on one line,
  between `Carve801FDAA4.c` and `Carve801FEE40.c` (that run in this file is **descending**, unlike
  splits.txt's ascending order).
- `config/G2ME01/splits.txt` - `MetroidPrime/ScriptObjects/Carve801FEA98.c:` /
  `.text start:0x801FEA98 end:0x801FEAE0`, between `Carve801FDB5C.c` and `Carve801FEE40.c`;
  `total_functions` still **28465** (`report.json` measures `total_functions 28465`).
- `files.cmake` - same position as configure.py.
- `src/MetroidPrime/PortLinkStubs.cpp` - **retired `stub_183`** (`fn_801FEA98`) and added
  **`stub_225`** (`fn_801FEAE0`, the callee the carve's own bytes name), plus the header's
  narrative line. One function stub out, one in; measured `grep -cE 'asm\("'` = **193** and
  `grep -cE 'asm\("(fn_|lbl_)'` = **33** before and after, so the header's 187 functions + 6 data
  objects still holds.

### Measurements of this run

- `./tools/decomp_build.sh` -> `All:  36.96% fuzzy, 30.40% matched, 13.39% linked
  (13022 / 28465 functions)`; `sha1sum build/G2ME01/main.dol` =
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (the expected one; the gate's own `hashes vs
  config.yml` step, which covers the DOL and all 86 RELs, prints `ok`).
- `build/report.json`: `main/MetroidPrime/ScriptObjects/Carve801FEA98` **2 / 2**, 72/72 bytes,
  `complete_code_percent 100.0`; the gap split exactly as runs 1-2 saw it -
  `main/auto_03_801FDC88_text` **29 -> 19** functions (4536 -> 3600 bytes) and a new
  `main/auto_03_801FEAE0_text` of **8** functions / 864 bytes (0x801FEAE0..0x801FEE40;
  19 + 8 + 2 = 29, 3600 + 864 + 72 = 4536). The gate's diff line is
  `SPLIT main/auto_03_801FDC88_text: 10 function(s) moved into
  main/MetroidPrime/ScriptObjects/Carve801FEA98, main/auto_03_801FEAE0_text (exact count match - a
  split, not a loss)`, with no `WORSE`/`GONE`/`FELL` anywhere in `build/gate-diff.log`.
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FEA98.c` -> `PASS -> kept as Matching`.
  `./tools/unit_fit.sh ...` -> `.text claimed 72, ours 72, retail 72, fits`; `no extra functions`.
  `check_decl_order.py` -> `1053 unit(s) checked, 36 permuted, all accounted for` (this unit not
  among them). `check_symbol_names.py` -> `575 units; 0 declared names are missing`.
- Port side, **both directions measured** on this tree's 287-undefined baseline:
  - carve in, `stub_183` retired, no `fn_801FEAE0` stub: `tools/link_check.sh` prints
    `unique undefined symbols 288`, `duplicate definitions 0`, `FAIL undefined went 287 -> 288`,
    and `build-port-link/link_undefined.txt` names exactly one new symbol, `fn_801FEAE0`;
    `python3 tools/link_gap.py --rebuild` exits 1 with `282  MISSING` and
    `gap grew: fn_801FEAE0 is not in port_link_gap_list.md`.
  - with `stub_225`: `287 undefined, 0 duplicates`, `unchanged from baseline`; link_gap.py exits 0
    with `281  MISSING ... all accounted for in port_link_gap_list.md`.
  - **So this tree's port gap baseline is 281, not run 2's 285/286** - do not carry those.
- Gate: `probe: 808 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0 duplicates)`;
  `GATE PASS  7347718e+7 changed`; `check_docs_claims.py --write` -> `docs claims agree with the
  tree`.
- `tools/carve_diff.sh 0x801FEA98 0x48 <obj>` -> `NOT byte-exact`, **2 differing instructions, both
  `bl` words** (`fn_801FEA98+0xc` -> fn_801FEAB8, `fn_801FEAB8+0x14` -> fn_801FEAE0). That is the
  expected reading, not a failure: it compares retail's *linked* bytes against the raw `.o`, where
  those two words are relocations. The linked bytes are right:
  `objdump -s -j .text --start-address=0x801FEA98 --stop-address=0x801FEAE0 build/G2ME01/main.elf`
  shows retail's 72 bytes exactly, both `48000015` included.

### Notes for the next run

- The stub number to use is tree-dependent: this tree's highest was `stub_224`, so the new one is
  `stub_225`. Read it out of the file, never from an earlier run's note.
- `build/G2ME01/asm/` in this tree held no stale `auto_03_801FEAE0_text.s` (run 2 was bitten by
  one), but the rule stands: read the split out of `build/report.json`, never out of a
  pre-existing `.s` under `build/`.
- `link_gap.py`'s MISSING count and `link_check.sh`'s undefined count are different instruments on
  different numbers (281 vs 287 here). Quote the one you measured, and say which.
- One pre-existing inconsistency in a file this item touches, left alone because it is not this
  item's business: `PortLinkStubs.cpp`'s header `Breakdown:` line (86 REL loader + 45 game method +
  33 unmangled + 1 allocator + 4 vtable/typeinfo = 169) does not sum to the 193 total the same
  header states, and by the header's own residual rule the game-method term is 69. Measured this
  run: 193 `asm("`, 187 function stubs, 6 data objects, 33 `fn_`+`lbl_`. A docs-only fix, so no
  `NEW:`.

### Verdict for this run

No `WALL:` and no `STALE:`: both functions matched, the unit flipped, the judge is green on the
final tree. No `NEW:` blocker, unchanged from runs 1-2: the one function left behind, `fn_801FEAE0`
(0x801FEAE0, 0x68), cannot reach 100% on its own (it needs the `.data` pair
`lbl_803B7BCC`/`lbl_803B7BF0`, the `basic_string` copy constructor and `fn_801FE8B8`'s 0xC4 bytes,
none of them claimed), so it is not a candidate lane.
