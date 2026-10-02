# carve-801fec64 - `MetroidPrime/ScriptObjects/Carve801FEC64` is `Matching` and flipped

**Result: PASS.**  `./tools/goal_check.sh build/goal/item.json` (run in `wt-mp2-goal-L10`) exits 0,
last line `goal_check: PASS carve-801fec64`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13020 -> 13022   linked 6124 -> 6126
  ok    check_symbol_names.py
  ok    All:  36.96% fuzzy, 30.40% matched, 13.39% linked (13022 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FEC64.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fec64
```

`total_functions` is still **28465** after the `splits.txt` edit.  `build/report.json` has
`main/MetroidPrime/ScriptObjects/Carve801FEC64` at `fuzzy_match_percent 100.0`, `matched_code
72 / 72`, `matched_functions 2 / 2`, `matched_functions_percent 100.0`, `complete_code 72`,
`complete: True`, and its two functions at 100.00% (`fn_801FEC84` 40 B, `fn_801FEC64` 32 B).

Other measurements on this tree, all taken this run:

| check | result |
|---|---|
| `tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FEC64.c` | `PASS  -> kept as Matching` |
| `tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FEC64.c` | `.text claimed 72 ours 72 retail 72 fits`, no extra functions |
| `python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FEC64` | `1 unit(s) checked, none emits its functions out of retail order` |
| `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail, with our object in the link) |
| `grep -cE 'asm\("' src/MetroidPrime/PortLinkStubs.cpp` | 193 before and after (`fn_`/`lbl_` term 33 before and after, `stub_data_*` 6) |
| `build/goal/check-gate.log` | `per-function diff SPLIT main/auto_03_801FDC88_text: 5 function(s) moved into main/MetroidPrime/ScriptObjects/Carve801FEC64, main/auto_03_801FECAC_text (exact count match - a split, not a loss)`, `port probe ok`, `port link gap ok`, `decl order ok`, `GATE PASS` |

## The six files

| file | change |
|---|---|
| `src/MetroidPrime/ScriptObjects/Carve801FEC64.c` | **new**, 119 lines, the claim's source: `fn_801FEC84` then `fn_801FEC64`, descending |
| `config/G2ME01/splits.txt:1371-1372` | `MetroidPrime/ScriptObjects/Carve801FEC64.c: .text start:0x801FEC64 end:0x801FECAC`, between `Carve801FDB5C.c` (0x801FDBE0..0x801FDC88) and `Carve801FEE40.c` (0x801FEE40..0x801FEE88) |
| `configure.py:773` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEC64.c"),` on one line, same neighbours |
| `files.cmake:580` | `src/MetroidPrime/ScriptObjects/Carve801FEC64.c`, same neighbours |
| `src/MetroidPrime/PortLinkStubs.cpp` | **`stub_189` (`fn_801FEC64`) deleted** (a tombstone note stands at :934-943), **`stub_225` (`fn_801FECAC`) added** at :1253-1278, header count line annotated |
| `src/MetroidPrime/ScriptObjects/Carve801FF8A0.cpp:59-64` | the paragraph that said `fn_801FEC64` "remains unclaimed and still needs `stub_189`" is marked **superseded again** in place (comment only) |

## The twins, re-measured here rather than recalled

Both read out of `build/G2ME01/main.elf` this run, at the addresses the item names:

- **`fn_801FEC64` (0x801FEC64, 0x20 = 32 B, 8 instructions) is `fn_80004D3C`'s 8 of 8 words, the
  `bl` included** (`Player/Carve80004C4C.c`, a `Matching` unit, `symbols.txt:97`), and the same 8 of
  8 against `fn_801FEE40` (`ScriptObjects/Carve801FEE40.c`, also `Matching`).  All three `bl` words
  read `48 00 00 15`, because in each copy the callee sits exactly 0x14 bytes past its own `bl`
  (0x801FEC70 + 0x14 = 0x801FEC84 here).
- **`fn_801FEC84` (0x801FEC84, 0x28 = 40 B, 9 instructions) is `fn_80004D5C`'s 8 of 9 words**
  (`Player/CGameStateBlockConstruct.cpp`, `symbols.txt:98`), and `fn_801FEE60`'s 9 of 9
  (`Carve801FEE40.c`).  The one differing word against `fn_80004D5C` is the `bl` at offset 0x14:
  `4b ff fd 31` (-> 0x80004AA0) against `48 00 00 15` (-> 0x801FECAC).  The twin's body is
  `if (self != 0) { fn_80004AA0(self, src); }`, and the `beq` target here, 0x18 past the test, is
  the epilogue - a null destination copies nothing and still returns.

**The whole claim is byte-identical to `Carve801FEE40.c`'s 72 bytes.**  `objdump -s` over both
ranges of `main.elf` prints the same 18 words twice:
`9421fff0 7c0802a6 90010014 48000015 80010014 7c0803a6 38210010 4e800020` then
`9421fff0 7c0802a6 28030000 90010014 41820008 48000015 80010014 7c0803a6 38210010 4e800020`.
And the compiled objects agree: `objcopy -O binary --only-section=.text` on
`build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801FEC64.o` `cmp`s equal to `Carve801FEE40.o`'s
(the one difference between the two functions' *sources* is which element size the header comments
talk about; no byte either body reads mentions the element at all).

**What the two are:** `rstl::construct< T >(T*, T const*)` for the 0x2C-byte element.  The element
size is measured, not assumed: the retail caller `fn_801FEBF8` (0x801FEBF8, 0x6C) keeps the
destination cursor in `r31`, the count in `r28` and the source in `r29`, calls `fn_801FEC64` once
per element at 0x801FEC30-0x801FEC34 and steps the cursor by 44 (`addi r31,r31,0x2c` at
0x801FEC38); its loop is entered at the bottom test (`b .L_801FEC3C` first), so a zero count
constructs nothing and returns.

## The link side: `stub_189` out, `stub_225` in, count unmoved

`fn_801FEC64` was **already** a stand-in: `stub_189` in `src/MetroidPrime/PortLinkStubs.cpp`, added
by hand on 2026-10-02 for `Carve801FF8A0.cpp` (its `fn_801FF9B8` calls it at 0x801FF9E8).  Listing
the new unit while leaving it would be two definitions of one symbol in the port's flat link, which
is `tools/gate.sh`'s `port link dups` step, so it is deleted.

Its callee `fn_801FECAC` (0x801FECAC, 0x78 = 120 bytes, `symbols.txt:8318`) is the other half of the
same trade and needed a stand-in of its own: measured, it was referenced by nothing in the tree
before this change (`grep -rn fn_801FECAC` outside `build/`: `symbols.txt`, a `Carve801FEE40.c`
comment and a `PortLinkStubs.cpp` comment only), so nothing was undefined for it and the carve is
what makes the linker ask.  Hence `stub_225`, with the same empty body and the same "this is not a
claim that fn_801FECAC is decompiled" note every other stub in that file carries.

The file's derived counts do not move - measured, not recalled: `grep -cE 'asm\("'` is **193**
before and after, `grep -cE 'asm\("(fn_|lbl_)'` is **33** before and after, `stub_data_*` is **6**
before and after.  So the header sentence keeps its `187 functions, 6 data objects` and gains only a
parenthetical recording the exchange.  `port probe ok` and `port link gap ok` in `check-gate.log`.

## The dtk split this claim makes

The gate's per-function diff reports it as a split, not a loss: `main/auto_03_801FDC88_text` loses 5
functions and 0x1C4 bytes, and they land in `main/MetroidPrime/ScriptObjects/Carve801FEC64` (2,
0x48) and `main/auto_03_801FECAC_text` (3, 0x194).  Measured in `build/report.json` this run:

| unit | range | functions |
|---|---|---|
| `main/auto_03_801FDC88_text` | 0x801FDC88 + 4060 B = 0xFDC (ends at 0x801FEC64) | 24 |
| `main/MetroidPrime/ScriptObjects/Carve801FEC64` | 0x801FEC64..0x801FECAC | 2 (both 100.00%) |
| `main/auto_03_801FECAC_text` | 0x801FECAC + 404 B = 0x194 (ends at 0x801FEE40) | 3 (`fn_801FECAC`, `fn_801FED24`, `fn_801FEDD4`) |

So the claim stops exactly where the item said: `fn_801FEBF8` (0x801FEBF8 + 0x6C) ends where it
starts, and `fn_801FECAC` is the first function past its end.

## Superseded claims corrected in place

- `Carve801FF8A0.cpp` said `fn_801FEC64` "remains unclaimed and still needs `stub_189`"; it and
  `fn_801FEC84` are now claimed for real, so that half is marked **superseded again** and the
  paragraph points at the new unit and at `stub_225`.
- `PortLinkStubs.cpp`'s `stub_189` block is replaced by a tombstone note that keeps its measurement
  (`288 MISSING`, both `gap grew:` lines, `286 MISSING` with the blocks in place) and records the
  exchange.

## Process facts, re-confirmed here rather than recalled

- **`tools/goal_check.sh` rewrites `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md`** (this run:
  `7 insertions, 7 deletions` - the state block's `13022`, `6126`, probe `808 files`).  `git
  checkout --` on both after the PASS leaves the tree carrying only the six files above.
- **`tools/carve_diff.sh` on the pre-link object always shows the two `bl`s differing** (`+3` and
  `+13`, `48000001` + an `R_PPC_REL24` relocation against retail's `48000015`) - a relocatable
  object's calls are not resolved yet.  Byte-exactness is decided by `flip_test.sh` plus the DOL
  sha1, and by the linked-word and object comparison quoted above, not by that script on the `.o`.

NEW: carve-801fecac | match | MetroidPrime/ScriptObjects/Carve801FECAC | fn_801FECAC (0x801FECAC, 0x78 = 120 B) is the 0x2C-byte element's copy constructor, unclaimed with clean boundaries (the rest of auto_03_801FECAC_text is fn_801FED24 and fn_801FEDD4, ending at Carve801FEE40.c's 0x801FEE40); its bytes need the two .data vtables lbl_803B7BCC/lbl_803B7BE4 and the bodies of the rstl::basic_string copy constructor and fn_801FE8B8 (0xC4), and retiring stub_225 is the payoff

## Second run (lane 10, `wt-mp2-goal-L10`, 2026-10-02) - re-done from scratch on a tree that did not have it

**Result: PASS, measured here.** `./tools/goal_check.sh build/goal/item.json` (run in this worktree,
twice - once mid-run and once on the final tree after the port-link measurement below) exits 0, last
line `goal_check: PASS carve-801fec64`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13026 -> 13028   linked 6130 -> 6132
  ok    check_symbol_names.py
  ok    All:  36.96% fuzzy, 30.40% matched, 13.39% linked (13028 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FEC64.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fec64
```

The first section above is the *earlier* run's result on its own tree; this run had **nothing of it
on disk** (`ls src/MetroidPrime/ScriptObjects/Carve801FEC64.c` -> no such file, no
`Object(Matching, ...Carve801FEC64...)` in `configure.py`, the unit absent from `build/report.json`),
so it was re-written rather than reapplied. The numbers came out the same, but two of the earlier
section's statements are **tree-specific and were re-measured**: the auto range this claim splits
was `auto_03_801FEAE0_text` here (not `auto_03_801FDC88_text` - this tree already had
`Carve801FEA98.c`, which moved that split), and the stub numbers here are `stub_189` (the retired
`fn_801FEC64`) / `stub_226` (the new `fn_801FECAC`), not the earlier tree's `stub_225`.

### The six files, this tree

| file | change |
|---|---|
| `src/MetroidPrime/ScriptObjects/Carve801FEC64.c` | **new**, 90 lines: header + `fn_801FEC84` then `fn_801FEC64`, descending, `extern void fn_801FECAC(void*, const void*)` declared at the top |
| `config/G2ME01/splits.txt` | `MetroidPrime/ScriptObjects/Carve801FEC64.c: .text start:0x801FEC64 end:0x801FECAC`, between `Carve801FEA98.c` and `Carve801FEE40.c` |
| `configure.py:775` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEC64.c"),` on one line, same neighbours |
| `files.cmake:582` | `src/MetroidPrime/ScriptObjects/Carve801FEC64.c`, same neighbours |
| `src/MetroidPrime/PortLinkStubs.cpp` | `stub_189` (`fn_801FEC64`) block replaced by a tombstone; `stub_226` (`fn_801FECAC`) added at :986-1007; header count clause annotated (exchange, total unmoved) |
| `src/MetroidPrime/ScriptObjects/Carve801FF8A0.cpp:55-63` | the paragraph saying `fn_801FEC64` "remains unclaimed and still needs `stub_189`" marked **superseded again** in place (comment only) |

### Measured this run, on this tree

| check | result |
|---|---|
| `build/report.json` unit `main/MetroidPrime/ScriptObjects/Carve801FEC64` | `fuzzy 100.0`, `matched_code 72 / 72`, `matched_functions 2 / 2`, `complete: True`; `fn_801FEC84` 40 B 100.00%, `fn_801FEC64` 32 B 100.00% |
| `tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FEC64.c` | `PASS  -> kept as Matching` |
| `tools/unit_fit.sh ...` | `.text claimed 72 ours 72 retail 72 fits`, no extra functions |
| `python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FEC64` | `1 unit(s) checked, none emits its functions out of retail order` |
| `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (with our object in the link) |
| `python3 tools/check_symbol_names.py` | `checked 575 units; 0 declared names are missing from their object` |
| `tools/carve_diff.sh 801FEC64 48 build/G2ME01/src/.../Carve801FEC64.o` | 18 instructions / 72 bytes both sides, 2 differing (`+3`, `+13` - the two `bl`s, unresolved in a relocatable object; the earlier section's caveat, re-confirmed) |
| `grep -cE 'asm\("' PortLinkStubs.cpp` / `asm\("(fn_\|lbl_)'` / `stub_data_` | **193 / 33 / 7** before and after (the exchange moves neither total) |
| `build/goal/check-gate.log` | `per-function diff SPLIT main/auto_03_801FEAE0_text: 5 function(s) moved into main/MetroidPrime/ScriptObjects/Carve801FEC64, main/auto_03_801FECAC_text (exact count match - a split, not a loss)`; `decl order ok`, `port probe ok`, `port link gap ok`, `GATE PASS edb784ec+8 changed` |

Byte twins, re-read out of `build/G2ME01/main.elf` this run: `0x801FEC64` is `9421fff0 7c0802a6
90010014 48000015 80010014 7c0803a6 38210010 4e800020`, the same 8 words as `0x801FEE40` and
`0x80004D3C`; `0x801FEC84` is `9421fff0 7c0802a6 28030000 90010014 41820008 48000015 80010014
7c0803a6 38210010 4e800020`, the same 9 words as `0x801FEE60` (the `4bfffd31` at `0x80004D5C`'s
offset 0x14 is the only word of difference against that twin). Element stride 0x2C re-read from the
retail caller in the same range: `fn_801FEBF8` steps `addi r31,r31,0x2c` at `0x801FEC38`.

### The port link, measured in the "without" direction this time

With `stub_226`'s body temporarily commented out and the same `extern` line left in place
(restored immediately after; the file's sha1 was taken before and after, `708158d0...`):

```
python3 tools/link_gap.py --rebuild
...
    282  MISSING
link gap not accounted for:
  gap grew: fn_801FECAC is not in port_link_gap_list.md
```

With the block back in place, `tools/gate.sh`'s `port link gap` step is `ok` - so `fn_801FECAC` is
what the carve makes the flat link ask for, exactly as the earlier section's `NEW:` line said.
Before this change the tree referenced `fn_801FECAC` nowhere outside `symbols.txt` and comments
(`grep -rn "fn_801FECAC" src/ include/` was empty), and `fn_801FEC84` nowhere at all.

### The dtk split, measured in `build/report.json`

| unit | range | functions |
|---|---|---|
| `main/auto_03_801FEAE0_text` | 0x801FEAE0 + 388 B = 0x184 (ends at 0x801FEC64) | 3 (`fn_801FEAE0`, `fn_801FEB48`, `fn_801FEBF8`) |
| `main/MetroidPrime/ScriptObjects/Carve801FEC64` | 0x801FEC64..0x801FECAC | 2 (both 100.00%) |
| `main/auto_03_801FECAC_text` | 0x801FECAC + 404 B = 0x194 (ends at 0x801FEE40) | 3 (`fn_801FECAC`, `fn_801FED24`, `fn_801FEDD4`) |

`total_functions` is **28465** after the `splits.txt` edit, and the claim starts exactly where
`fn_801FEBF8` (0x801FEBF8 + 0x6C) ends.

### Process facts, re-confirmed

- `tools/goal_check.sh` rewrites `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` (this run:
  `7 insertions, 7 deletions`); `git checkout --` on both after the PASS leaves the tree carrying
  only the six files above. The judge-owned-path check is `git status --porcelain --untracked-files=all`
  grepping `^(tools/|docs/research/port_link_baseline.txt$|build/goal/)`, so a scratch file under
  this tree's `build/goal/` - including a copy of these notes - would fail the item outright.
- No new blockers: both functions reached 100.00% on the first spelling, so no `WALL:` line. The
  `NEW:` line at the end of the section above (`carve-801fecac`) is still the only follow-up this
  range offers and is **not re-filed here** - it was filed by the first run.
