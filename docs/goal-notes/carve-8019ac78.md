# carve-8019ac78 - `MetroidPrime/Carve8019AC78` (match)

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS carve-8019ac78`
(gate.sh green, counts `matched 12672 -> 12675   linked 6031 -> 6034`, `check_symbol_names.py`
clean, `All: 35.53% fuzzy, 29.38% matched, 13.05% linked (12675 / 28465 functions)`,
`flip_test MetroidPrime/Carve8019AC78.c: PASS, Object(Matching) in configure.py`).

## The four files

| file | what |
|---|---|
| `src/MetroidPrime/Carve8019AC78.c` | new, 3 functions, descending by address |
| `config/G2ME01/splits.txt:984-985` | `MetroidPrime/Carve8019AC78.c:` / `.text start:0x8019AC78 end:0x8019AD24` |
| `files.cmake:539` | `src/MetroidPrime/Carve8019AC78.c` |
| `configure.py:729` | `Object(Matching, "MetroidPrime/Carve8019AC78.c")` (one line) |

Each entry sits between `Carve801997B0.c` (0x801997B0..0x801997B4) and `Carve8019CE6C.c`
(0x8019CE6C..0x8019CE70) - address order in all three manifests. No `PortLinkStubs.cpp` duplicate
existed: `grep -rn fn_8019AC78 src/ include/ configure.py config/ files.cmake` found only
`config/G2ME01/symbols.txt:6845`. Nothing else in the diff.

## What was written

`.text 0x8019AC78..0x8019AD24, 0xAC = 172 bytes, 3 functions`, all `fn_` placeholders from
`symbols.txt:6845-6847`, definitions in descending address order (`fn_8019ACC0`, `fn_8019AC98`,
`fn_8019AC78` - mwcceppc emits in reverse source order, so this is what puts `.text` in retail
order). The two functions the item named are the ones it named:

- `fn_8019AC78` (0x20, 8 instructions): frame + one unconditional `bl fn_8019AC98`. Twin
  `fn_80004D3C` (`src/MetroidPrime/Player/Carve80004C4C.c`, `Matching`) is these 8 instructions
  word for word with the `bl` retargeted.
- `fn_8019AC98` (0x28, 10 instructions): `cmplwi r3,0x0` on the **destination**, then
  `bl fn_8019ACC0` with both arguments untouched. Twin `fn_80004D5C`
  (`src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`, `Matching`) is the same 10 instructions
  with `bl fn_80004AA0` where the `bl` here is, including retail's own `cmplwi`-before-`stw`
  order.
- `fn_8019ACC0` (0x64, 25 instructions): the element's implicit copy constructor - `lwz/stw` for
  the words, `lfs/stfs` for the `float` at +0x08, `lbz/stb` for the bytes at +0x20, +0x21, +0x24
  and +0x25. This is the one function the item did **not** name; see below.

## Why the claim is 0x8019AC78..0x8019AD24 and not 0x8019AC78..0x8019ACC0

`fn_8019AC98` calls `fn_8019ACC0`, and nothing in the tree defined `fn_8019ACC0` before this
change (measured: `grep -rn fn_8019ACC0 src/ include/ configure.py config/ files.cmake` returned
only `config/G2ME01/symbols.txt:6847`). A `Matching` unit's object *is* linked, so a claim that
stopped at 0x8019ACC0 would add `fn_8019ACC0` to the port's link gap, and
`tools/link_check.sh --strict` - run by `tools/probe_sources.sh` inside `gate.sh` - fails when
that count grows. The alternative, leaving the unit `NonMatching`, forfeits the flip entirely, so
a two-function claim could never be more than `progress`. `fn_8019ACC0` is contiguous with the
item's range, so the claim is the whole contiguous run; measured after the change:
`probe: 804 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)` against the
recorded 291 in `docs/research/port_link_baseline.txt:3`. Same reasoning as
`src/MetroidPrime/Player/Carve80004C4C.c:66-77`, which took the whole tail rather than the three
functions its item named.

## The one thing that did not match the first time (measured, so nobody repeats it)

`fn_8019AC78` and `fn_8019AC98` matched on the first build; `fn_8019ACC0` came out at 99.84% and
`main.dol` hashed `28ead22fd4668cf1aab02d9f8161b33c54ed44c3` against the retail
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. `cmp -l` between the NonMatching-linked and
Matching-linked DOLs localised it to **four bytes, at 0x8019AD0C, 0x8019AD14, 0x8019AD18 and
0x8019AD1C** - the last two `lbz`/`stb` pairs, retail `lbz r5,0x24(r4)` / `lbz r0,0x25(r4)` /
`stb r5,0x24(r3)` / `stb r0,0x25(r3)` against ours at +0x23 and +0x24.

**Cause: the struct was declared without the two padding bytes.** Four consecutive
`unsigned char` members at `+0x20,+0x21,+0x24,+0x25` pack to `+0x20,+0x21,+0x22,+0x23`, and
mwcceppc happily copied the right *values* to the wrong *offsets* - so nothing about the source
looked wrong and objdiff said 99.84% rather than failing. The fix is two named pad bytes
(`x22Pad`, `x23Pad`) in `struct SUnknown8019`; the source comment records this, because it is the
kind of defect a `100%`-looking body hides. Lesson, not just for this file: when a carve's
copy-constructor body names non-contiguous offsets, the struct needs the gaps spelled out, and
`main.dol`'s sha1 is what finds it - not objdiff's fuzzy number.

## Measurements

- `build/G2ME01/report.json`: `total_functions` **28465** (unchanged), `matched_functions`
  **12672 -> 12675**, `complete_units` 808 -> 809. All three functions `fuzzy_match_percent`
  100.0, unit `complete: true`.
- `tools/unit_fit.sh MetroidPrime/Carve8019AC78.c`: `.text claimed 172 ours 172 retail 172 fits`,
  "no extra functions: our object defines only what the retail unit object does".
- `tools/check_files_cmake.py`, `tools/check_decl_order.py`, `tools/check_symbol_names.py`
  (532 units, 0 missing names) all clean.
- `./tools/flip_test.sh MetroidPrime/Carve8019AC78.c`: `PASS -> kept as Matching`, `kept: 1 / 1`.
- `report_diff` reports `main/auto_03_801997B4_text: 10 function(s) moved into
  main/MetroidPrime/Carve8019AC78, main/auto_03_8019AD24_text (exact count match - a split, not a
  loss)` - dtk re-split the auto run around the new claim, which is why `build/G2ME01/asm/` now
  holds `auto_03_801997B4_text.s` (ending at fn_8019ABDC) and a new `auto_03_8019AD24_text.s`.

## Not filed as `NEW:`

Nothing was blocked. The neighbouring `fn_8019AD24` (0x8019AD24, 0x5C, 23 instructions) is this
same member-wise copy with `r7`/`r0` instead of `r5`/`r0` and is the obvious next function, but it
belongs to the run the next item in the queue will seed, so filing it here would be a restatement
of an existing carve item rather than new work.