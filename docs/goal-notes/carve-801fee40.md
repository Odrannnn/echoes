# carve-801fee40 - `MetroidPrime/ScriptObjects/Carve801FEE40` is `Matching` and flipped

**Result: PASS.**  `./tools/goal_check.sh build/goal/item.json` (run in `wt-mp2-goal-L8`, from
HEAD `0291b134` "match: carve-801fdaa4") exits 0, last line `goal_check: PASS carve-801fee40`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12693 -> 12695   linked 6052 -> 6054
  ok    check_symbol_names.py
  ok    All:  35.55% fuzzy, 29.39% matched, 13.06% linked (12695 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FEE40.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fee40
```

`total_functions` is still **28465** after the `splits.txt` edit.  `build/report.json` has the unit
at `fuzzy_match_percent 100.0`, `matched_code 72 / 72`, `matched_functions 2 / 2`,
`matched_functions_percent 100.0`, `complete: True`.

Other measurements on this tree, all taken this run:

| check | result |
|---|---|
| `tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FEE40.c` | `PASS  -> kept as Matching` |
| `tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FEE40.c` | `.text claimed 72 ours 72 retail 72 fits`, no extra functions |
| `python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FEE40` | `1 unit(s) checked, none emits its functions out of retail order` |
| our object's own `.text` order | ascending: `fn_801FEE40` at `0x0`, `fn_801FEE60` at `0x20` |
| `python3 tools/check_files_cmake.py` | every configured DOL object is either in files.cmake or excluded with a reason |
| `python3 tools/check_symbol_names.py` | `checked 532 units; 0 declared names are missing from their object` |
| `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail, with our object in the link) |
| port link (`build/gate-probe.log`) | `815 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)` - baseline `undef.base.count` is **291**, so unmoved |
| `build/gate-link.log` | `285  MISSING`, all accounted for in `port_link_gap_list.md` |

## The six files

| file | change |
|---|---|
| `src/MetroidPrime/ScriptObjects/Carve801FEE40.c` | **new**, the claim's source: `fn_801FEE60` then `fn_801FEE40`, descending |
| `config/G2ME01/splits.txt` | `MetroidPrime/ScriptObjects/Carve801FEE40.c: .text start:0x801FEE40 end:0x801FEE88`, between `Carve801FDB5C.c` (0x801FDBE0..0x801FDC88) and `Carve801FEEF0.c` (0x801FEEF0..0x801FEEF8) |
| `configure.py` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEE40.c"),` on one line, same neighbours |
| `files.cmake` | `src/MetroidPrime/ScriptObjects/Carve801FEE40.c`, same neighbours |
| `src/MetroidPrime/PortLinkStubs.cpp` | **`stub_180` (`fn_801FEE40`) deleted**, `stub_200` (`fn_801FEE88`) added, two comment blocks corrected, header count line annotated |
| `src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp` | the paragraph that called `fn_801FEE40` a stub is marked superseded in place (comment only) |

## The twins, re-measured here rather than recalled

Both read out of `build/G2ME01/main.elf` this run, at the addresses the item names:

- **`fn_801FEE40` (0x801FEE40, 0x20 = 32 B, 8 instructions) is `fn_80004D3C`'s 8 of 8 words,
  the `bl` included** (`Player/Carve80004C4C.c`, a `Matching` unit, `symbols.txt:97`).  Both `bl`
  words read `48 00 00 15`, because in each copy the callee sits exactly 0x14 bytes past its own
  `bl` (0x801FEE4C + 0x14 = 0x801FEE60; 0x80004D48 + 0x14 = 0x80004D5C).
- **`fn_801FEE60` (0x801FEE60, 0x28 = 40 B, 9 instructions) is `fn_80004D5C`'s 8 of 9 words**
  (`Player/CGameStateBlockConstruct.cpp`, `symbols.txt:98`).  The one differing word is the `bl`
  at offset 0x14: `4b ff fd 31` (-> 0x80004AA0) against `48 00 00 15` (-> 0x801FEE88).  The twin's
  body is `if (self != 0) { fn_80004AA0(self, src); }`, and the `beq` target here, 0x18 past the
  test, is the epilogue - a null destination copies nothing and still returns.

**What the two are:** `rstl::construct< T >(T*, T const*)` for the 0x24-byte element of the block
`Carve801FF5A0.cpp` walks.  The element size is measured, not assumed: `fn_801FF6B8` (0x801FF6B8,
in that `Matching` unit) does `mr r3,r30` / `mr r4,r31` / `bl fn_801FEE40` at 0x801FF6E0-0x801FF6E8
and steps both cursors by 36 at 0x801FF6F0-0x801FF6F8.  `fn_801FEE88`'s own body agrees: it copies
an `rstl::basic_string` at +0x4 (0x14 bytes) and calls `fn_801FE8B8` on +0x14.

**Independent byte check** (not the judge): the 18 words `build/G2ME01/main.elf` now holds at
0x801FEE40..0x801FEE88 are identical to the 18 words dtk emitted into
`build/G2ME01/asm/auto_03_801FDC88_text.s` from the disc before the claim existed -
`9421fff0 7c0802a6 90010014 48000015 80010014 7c0803a6 38210010 4e800020` twice, with
`28030000 90010014 41820008` for the null test in the second half.

## The link side: `stub_180` out, `stub_200` in, count unmoved

`fn_801FEE40` was **already** a stand-in: `stub_180` in `src/MetroidPrime/PortLinkStubs.cpp`, added
by hand on 2026-10-02 for `Carve801FF5A0.cpp`.  Listing the new unit while leaving it would be two
definitions of one symbol in the port's flat link, which is `tools/gate.sh`'s `port link dups`
step, so it is deleted.

Its callee `fn_801FEE88` is the other half of the same trade and needed a stand-in of its own:
measured, it was referenced by nothing in the tree before this change
(`grep -rn fn_801FEE88` outside `build/`: `config/G2ME01/symbols.txt`, `PortLinkStubs.cpp`,
`Carve801FF5A0.cpp` comment, and this run's new unit only), so nothing was undefined for it and
the carve is what makes the linker ask.  Hence `stub_200`, with the same empty body and the same
"this is not a claim that fn_801FEE88 is decompiled" note every other stub in that file carries.

The file's derived counts do not move - measured, not recalled: `grep -cE 'asm\("'` is **169**
before and after, `grep -cE 'asm\("(fn_|lbl_)'` is **33** before and after, `stub_data_*` is **4**
before and after.  So the header sentence keeps its `165 functions, 4 data objects` and gains only a
parenthetical recording the exchange.

## Superseded claims corrected in place

- `Carve801FF5A0.cpp` said `fn_801FEE40` needed a stand-in; it is now claimed for real, so that
  half is marked **superseded** and the paragraph points at the new unit, at `stub_200` for
  `fn_801FEE88`, and at the half about `fn_801FD638` that is still true.
- `PortLinkStubs.cpp`'s `stub_179` block said the `Carve801FF5A0.cpp` trade "now stands for two of
  the three symbols"; it stands for the allocator alone from this commit.

## Not done, deliberately

**`fn_801FEE88` (0x801FEE88, 0x68 = 104 bytes) is left to dtk and is not claimed.**  Its bytes are a
copy constructor: the `.data` vtable `lbl_803B7BCC` (0x803B7BCC, `symbols.txt:18343`) stored at
+0x0 and immediately overwritten by `lbl_803B7BFC` (0x803B7BFC, `:18347`) - the base constructor
inside the derived one - then `__ct__Q24rstl66basic_string<...>(this+4, src+4)` and
`fn_801FE8B8(this+0x14, src+0x14)`, `this` back in `r3`.  Matching those 0x68 bytes needs both
vtables plus the bodies of that string constructor and of `fn_801FE8B8` (0x801FE8B8, 0xC4), none of
which any unit claims.  Claiming it here would also have made the unit's flip depend on three
bodies instead of two, for no gain in this item.

NEW: carve-801fee88 | match | MetroidPrime/ScriptObjects/Carve801FEE88 | fn_801FEE88 (0x68 bytes) is the 0x24-byte element's copy constructor, unclaimed with clean boundaries on both sides; matching it needs the two .data vtables lbl_803B7BCC/lbl_803B7BFC and the bodies of the basic_string copy ctor and fn_801FE8B8 (0xC4), and retiring stub_200 is the payoff

**The claim stops exactly at the item's range.**  `fn_801FEDD4` (0x801FEDD4, 0x6C) ends at
0x801FEE40 and is still unclaimed in front of it; `fn_801FEEF0` (0x801FEEF0, 0x8) is
`Carve801FEEF0.c`'s claim immediately behind `fn_801FEE88`, so a later claim of `fn_801FEE88` would
be bounded by this unit's end on one side and by `Carve801FEEF0.c` on the other - no gap, no cycle.

## Process facts, re-confirmed here rather than recalled

- **`tools/goal_check.sh` still rewrites the two load-bearing docs**: `git diff --stat -- docs/`
  immediately after the PASS printed `4406987 insertions, 4406987 deletions` across
  `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md`.  `git checkout --` on both leaves the tree
  carrying only the six files above.  A lane that does not revert them commits a 4.4M-line diff.
  It is a docs fix, so per the brief it is not a `NEW:` line.
- **`tools/carve_diff.sh` on the pre-link object always shows the two `bl`s differing** - `+3` and
  `+13` here, `48000015` against an unrelocated displacement - because the object's calls are not
  yet relocated.  Byte-exactness is decided by `flip_test.sh` plus the DOL sha1, and by the linked
  word comparison quoted above, not by that script on the `.o`.
