# carve-801fd638 - `MetroidPrime/ScriptObjects/Carve801FD638` is `Matching` and flipped

**Result: PASS.** `tools/goal_check.sh build/goal/item.json` exits 0, last line
`goal_check: PASS carve-801fd638`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12665 -> 12667   linked 6024 -> 6026
  ok    check_symbol_names.py
  ok    All:  35.52% fuzzy, 29.37% matched, 13.04% linked (12667 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FD638.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fd638
```

`total_functions` is still **28465** after the `splits.txt` edit.  `build/report.json` has the unit
at `fuzzy_match_percent 100.0`, `matched_code 68 / 68`, `matched_functions 2 / 2`,
`complete: True`, and both functions at `fuzzy_match_percent 100.0`:

```
fn_801FD658  0x801FD658  0x24   36 B  100.0
fn_801FD638  0x801FD638  0x20   32 B  100.0
```

Both twins landed on the first spelling tried - there is no wall and no spelling list to record.

## The four files, all in address order

| file | entry |
| --- | --- |
| `configure.py:753` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD638.c"),` between `Carve801FDB5C.c` and `Carve801FEEF0.c` |
| `config/G2ME01/splits.txt:1201-1202` | `MetroidPrime/ScriptObjects/Carve801FD638.c:` / `.text start:0x801FD638 end:0x801FD67C` |
| `files.cmake:575` | `src/MetroidPrime/ScriptObjects/Carve801FD638.c` |
| `src/MetroidPrime/ScriptObjects/Carve801FD638.c` | new, `.text` 0x801FD638..0x801FD67C, `0x44` = 68 bytes, 2 functions |

`python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FD638` ->
`ok: 1 unit(s) checked, none emits its functions out of retail order` (definitions descend:
`fn_801FD658` is written before `fn_801FD638`).

`tools/carve_diff.sh 0x801FD638 0x44 build/G2ME01/obj/MetroidPrime/ScriptObjects/Carve801FD638.o`
-> `retail: 17 instructions, 68 bytes` / `ours: 17 instructions, 68 bytes`, `differing
instructions: 2`, both of them the `bl` displacements the linker fills in
(0x801FD644 -> `fn_801FD658`, 0x801FD668 -> `fn_801FD67C`); `tools/flip_test.sh` is what decided it.

## What the two functions are

`.text 0x801FD638..0x801FD67C`, out of dtk's `auto_03_801FA3CC_text`
(`config/G2ME01/symbols.txt:8271-8272`, instructions read from
`build/G2ME01/asm/auto_03_801FA3CC_text.s:3744-3767`).  `rstl::destroy`'s two halves for one
element type:

| function | retail | what it is | its measured twin (both `Matching`, `src/MetroidPrime/Player/Carve80004C4C.c`) |
| --- | --- | --- | --- |
| `fn_801FD658` | 0x801FD658, 0x24 | `destroy_impl<T>(T*)` = `in->~T()`; materialises `li r4,-1`, the "do not free me" flag | `fn_80004C6C` (0x80004C6C, 0x24), these nine instructions word for word |
| `fn_801FD638` | 0x801FD638, 0x20 | `destroy<T>(T*)`, whose whole body is `destroy_impl(in)` - one `bl`, no load, no test | `fn_80004D3C` (0x80004D3C, 0x20), byte for byte apart from the `bl` |

**The element is 0x24 = 36 bytes, measured from two retail loops that call only `fn_801FD638`:**
`fn_801FD5E8` (0x801FD5E8, 0x50, unclaimed) walks its array with `addi r31,r31,0x24` and
`bl fn_801FD638` at 0x801FD60C, and `fn_801FF66C` (already ported, in
`src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp`, `Matching`) walks another with
`addi r31,r31,36` and `bl fn_801FD638` at 0x801FF690 - the second verified with
`build/binutils/powerpc-eabi-objdump -d --start-address=0x801FF660 --stop-address=0x801FF6A0
build/G2ME01/main.elf`.  Nothing is asserted about the class itself; the receiver never appears in
either body.

## The port's link: one stub deleted, one added, and the count does not move

`fn_801FD638` was already defined - as an **empty stand-in**, `stub_181` in
`src/MetroidPrime/PortLinkStubs.cpp`, added for `Carve801FF5A0.cpp`'s call.  That stand-in is
**deleted** by this change: leaving it would be two definitions of one symbol in the port's flat
link, which `tools/link_check.sh --strict` reports as a duplicate.

`fn_801FD658` calls `fn_801FD67C`, which is **outside this claim** (0x801FD67C, 0x74 = 116 bytes,
`symbols.txt:8273`) and unclaimed, so the carve would add one name to the port's undefined set and
`link_check.sh --strict` fails on a rise against `docs/research/port_link_baseline.txt`
(`undefined 291`).  It is therefore supplied by a new empty stand-in, `stub_195`, in the same file -
the identical trade `stub_178` already makes for the identical shape one range below
(`Carve801FDB5C.c`'s `fn_801FDC68` is retail's byte for byte and retail's is one `bl fn_801FDC88`).
**It is not a claim that `fn_801FD67C` is decompiled; it is not.**  Claiming it instead only moves
the same gap one function along: those 0x74 bytes call `fn_801FD6F0` (0x84), which calls
`fn_801FD774`, and `fn_801FD67C` stores a vtable address out of `.data`.

Measured after the change, from the gate's own artefacts:

```
build-port-link/link_summary.txt            291 0 0 1        # 291 undefined, 0 duplicates, link ran
build-port-link/link_undefined.txt          no fn_801FD67C / fn_801FD638 / fn_801FD658
build-port-link/.../Carve801FD638.c.o       built from the new files.cmake line
```

so the carve's real definitions satisfy the port's call and the one stand-in covers the one callee
that leaves the claim, with the count at the baseline rather than above it.

## Two things worth flagging to whoever owns the tooling

1. **`tools/check_docs_claims.py --write` rewrote `docs/HANDOFF.md` and
   `docs/RUNNING_THE_DECOMP.md` into a 63 646-line diff** as a side effect of the judge
   (`gate.sh` runs it with `MP_GATE_DOCS_WRITE=1`).  The state's three lines moved correctly
   (12665 -> 12667, 6024 -> 6026, DOL 11065 -> 11067), and the rest was this item's own probe count:
   `probe 800 files` -> `probe 801 files`.  **`docs/HANDOFF.md` contains one single line duplicated
   15 910 times** (measured: `grep -c "^byte-identical to \`orig/G2ME01/files/RelProd/\`"` is 15910
   both in `HEAD` and in the worktree, in a 32 270-line file), so any probe-count change rewrites
   15 910 lines.  That duplication is already committed and predates this item - this change did
   **not** clean it and should not.  Both docs files were reverted with `git checkout --` after the
   judge ran, as the brief requires; the judge itself still passes with them reverted, because it
   never looks at them.
2. **The same step left `AGENTS.md`'s gate list stale**: it quotes `./tools/probe_sources.sh` as
   "329 files, 0 failures" where the judge-written `docs/HANDOFF.md` records the same tool at 801.
   `AGENTS.md` is not one of the files `--write` rewrites, so nothing corrects it.

## Notes for the next run

- **`stub_178` / `stub_195` are the general rule for a forwarder-shaped carve.** A carve whose body
  is one `bl` to an unclaimed function cannot drop the call (it is in the matched bytes), so it
  needs a stand-in for the callee; deleting the call to keep the link count down loses the match.
  Two instances now, and `stub_194` makes the same trade for `Carve80213320.cpp`.
- **`fn_801FD5E8` (0x801FD5E8, 0x50) is the natural next carve** and needs no stand-in:
  it is `destroy_impl(It,It)` over the 0x24 element, the same shape as `fn_801FDC18` in the already
  `Matching` `Carve801FDB5C.c`, and its only callee (`fn_801FD638`) is defined for real by this item.
  It ends exactly where this claim starts (0x801FD5E8 + 0x50 = 0x801FD638), so the two could be one
  contiguous unit if a later item wants them together.
- **`fn_801FD67C` is a real piece of work, not a stub target.** Its bytes name the element's shape:
  a vtable pointer at +0, an `rstl::basic_string` at +0x4 (released through
  `internal_dereference__Q24rstl66basic_string<...>`), a member at +0x14 destroyed by
  `fn_801FD6F0`, and `Free__7CMemoryFPCv` only when the caller's flag is positive. Matching its
  0x74 bytes needs the `.data` vtable `lbl_803B7BFC` as well as the bodies of `fn_801FD6F0` and
  `fn_801FD774`.

---

# Second run, 2026-10-02, lane 1 (`goal/lane-1`, HEAD `dc8b2938`)

**Result: PASS again, from a clean lane tree. The first run's change had not landed** - there is no
commit for it anywhere (`git log --all --grep=801fd638` is empty, `wt-mp2-goal` is on
`ff29e7f2`, and `MetroidPrime/ScriptObjects/Carve801FD638.c` did not exist). So this is not
`STALE:` in the sense the brief means: the item was **queued again**, and everything below was
re-measured on this tree rather than taken from the run above.

## What I measured, before writing anything

| what | how | result |
| --- | --- | --- |
| clean-tree baseline | `build/report.json` `measures` | `total_functions 28465`, `matched_functions 12675`, `matched_functions_percent 44.52837` |
| the two functions exist and are unclaimed | `config/G2ME01/symbols.txt:8271-8272` | `fn_801FD638` 0x801FD638 size 0x20, `fn_801FD658` 0x801FD658 size 0x24 |
| the bytes are the twins' bytes | `powerpc-eabi-objdump -d` on `build/G2ME01/main.elf`, 0x801FD638..0x801FD680 vs 0x80004C4C..0x80004C90 and 0x80004D3C..0x80004D5C | `fn_801FD638` == `fn_80004D3C` and `fn_801FD658` == `fn_80004C6C`, instruction for instruction, only the `bl` displacement differs |
| element size 0x24 | two retail loops that call **only** `fn_801FD638` | `fn_801FD5E8` (`addi r31,r31,36` + `bl fn_801FD638` at 0x801FD60C) and `fn_801FF66C` (`addi r31,r31,36` + `bl fn_801FD638` at 0x801FF690) - re-read both this run, they agree |
| `fn_801FD67C` shape for the stub comment | objdump 0x801FD67C..0x801FD6F0 | 0x74 bytes; vtable `lis r4,-32709` + `addi r0,r4,31740` = **0x803B7BFC** into +0x0, `fn_801FD6F0` over +0x14 with `li r4,-1`, `internal_dereference__Q24rstl66basic_string<...>` on +0x4, `Free__7CMemoryFPCv` only under `extsh. r0,r31 / ble` |

**Verdict on the run above: both of its conclusions re-derived, and both hold.** The twin relation is
not inherited - I disassembled all four functions and compared. The first run's spelling (copy
`fn_80004C6C`/`fn_80004D3C`'s bodies, callees `extern`) is the only one I tried, and it matched
byte for byte first time, so there is no spelling list and no `WALL:`.

## The four files, all in address order (as they are on this tree)

| file | entry |
| --- | --- |
| `configure.py:756` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD638.c"),` between `Carve801FDB5C.c` and `Carve801FEEF0.c` |
| `config/G2ME01/splits.txt:1213-1214` | `MetroidPrime/ScriptObjects/Carve801FD638.c:` / `.text start:0x801FD638 end:0x801FD67C` |
| `files.cmake:578` | `src/MetroidPrime/ScriptObjects/Carve801FD638.c` |
| `src/MetroidPrime/ScriptObjects/Carve801FD638.c` | new, `.text` 0x801FD638..0x801FD67C, `0x44` = 68 bytes, 2 functions |

Definitions descend (`fn_801FD658` written before `fn_801FD638`):
`python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FD638` ->
`ok: 1 unit(s) checked, none emits its functions out of retail order`.
`tools/carve_diff.sh 0x801FD638 0x44 build/G2ME01/obj/MetroidPrime/ScriptObjects/Carve801FD638.o`
-> `retail: 17 instructions, 68 bytes` / `ours : 17 instructions, 68 bytes`, `differing
instructions: 2` (0x801FD644 -> `fn_801FD658`, 0x801FD668 -> `fn_801FD67C` - the two `bl`
displacements the linker fills in). `tools/unit_fit.sh` -> `claimed 68 ours 68 retail 68 fits`,
`no extra functions`.

## Result, measured

`./tools/goal_check.sh build/goal/item.json` exits 0, last line `goal_check: PASS carve-801fd638`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12675 -> 12677   linked 6034 -> 6036
  ok    check_symbol_names.py
  ok    All:  35.53% fuzzy, 29.38% matched, 13.05% linked (12677 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FD638.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fd638
```

`total_functions` is still **28465** after the `splits.txt` edit.  `build/report.json` has
`main/MetroidPrime/ScriptObjects/Carve801FD638` at `fuzzy_match_percent 100.0`,
`matched_code 68 / 68`, `matched_functions 2 / 2`, `complete_units 1`, and both functions at
`fuzzy_match_percent 100.0`: `fn_801FD658` (36 B, vaddr 2149570136 = 0x801FD658) and `fn_801FD638`
(32 B, vaddr 2149570104 = 0x801FD638).

Standalone gates, run outside the judge too:
`sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`./tools/probe_sources.sh` -> `probe: 805 files, 0 failed, 0 errors; link: LINKED (291 undefined,
0 duplicates)`; `python3 tools/check_symbol_names.py` -> `checked 532 units; 0 declared names are
missing from their object`.

## The port's link: one stub retired, one added, count unmoved

Same trade as the run above, and it is the part of this item that is not obvious. `fn_801FD638` was
already defined in the port as an **empty stand-in** - `stub_181`, `asm("fn_801FD638")` at
`src/MetroidPrime/PortLinkStubs.cpp:781`, added on 2026-10-02 for `Carve801FF5A0.cpp`'s call.
**It is deleted by this change**: the new unit is in `files.cmake`, so both objects would reach the
port's flat link and `fn_801FD638` would have two definitions.

`fn_801FD658` calls `fn_801FD67C`, which is **outside this claim** (0x801FD67C, 0x74 bytes,
`symbols.txt:8273`) and unclaimed, so the carve adds one name to the port's undefined set and
`tools/link_check.sh --strict` fails on a rise against `docs/research/port_link_baseline.txt`
(`undefined 291`).  It is therefore supplied by a new empty stand-in, **`stub_197`**, appended after
`stub_196` in the same file.  Measured after the change, from the gate's own artefacts:

```
build-port-link/link_summary.txt            291 0 0 1     # 291 undefined, 0 duplicates, link ran
build-port-link/link_undefined.txt          no fn_801FD67C / fn_801FD638 / fn_801FD658
```

so the carve's real definitions satisfy the port's call and the one stand-in covers the one callee
that leaves the claim, with the count **at** the baseline rather than above it.

**It is not a claim that `fn_801FD67C` is decompiled; it is not.**  Claiming it instead only moves
the same gap one function along: those 0x74 bytes store the `.data` vtable `lbl_803B7BFC` and call
`fn_801FD6F0` (0x801FD6F0, 0x84), which calls `fn_801FD774` - both unclaimed too.

## Confirmed again for whoever owns the tooling

`tools/check_docs_claims.py --write`, run by the judge (`gate.sh` with `MP_GATE_DOCS_WRITE=1`),
rewrites `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md`.  Measured this run the same way as
above: the state's counts moved correctly (matched 12675 -> 12677, linked 6034 -> 6036, DOL
11075 -> 11077) and the rest of the diff was again this item's own probe count (`probe 804 files` in
the committed `docs/HANDOFF.md` -> `probe 805 files` measured).  **`docs/HANDOFF.md` still has one
line duplicated 15 910 times** (`grep -c "^byte-identical to \`orig/G2ME01/files/RelProd/\`"` = 15910
in `HEAD`), so any probe-count change rewrites 15 910 lines.  That duplication is pre-existing and
this change did **not** touch it.  Both docs files were `git checkout --`-ed back after the judge
ran, as the brief requires; the judge still passes with them reverted because it never reads them.

NEW: carve-801fd5e8 | match | MetroidPrime/ScriptObjects/Carve801FD5E8 | fn_801FD5E8 (0x801FD5E8, 0x50, unclaimed, ends exactly at this item's claim start 0x801FD638) is destroy_impl(first,last) over the 0x24 element, the same shape as fn_801FDC18 in the already Matching Carve801FDB5C.c, and its only callee fn_801FD638 is now defined for real by Carve801FD638.c - so it needs no stand-in and would flip as one unit.
