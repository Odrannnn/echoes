# carve-801fdaa4 - `MetroidPrime/ScriptObjects/Carve801FDAA4` is `Matching` and flipped

**Result: PASS.** `tools/goal_check.sh build/goal/item.json` exits 0, last line
`goal_check: PASS carve-801fdaa4`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12667 -> 12669   linked 6026 -> 6028
  ok    check_symbol_names.py
  ok    All:  35.53% fuzzy, 29.37% matched, 13.04% linked (12669 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FDAA4.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fdaa4
```

`total_functions` is still **28465** after the `splits.txt` edit.  `build/report.json` has the unit
at `fuzzy_match_percent 100.0`, `matched_code 68 / 68`, `matched_functions 2 / 2`, `complete: True`:

```
fn_801FDAC4  0x801FDAC4  0x24   36 B  100.0
fn_801FDAA4  0x801FDAA4  0x20   32 B  100.0
```

`build/G2ME01/main.dol` is `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, so retail is reproduced with
this unit's own object in the link.

## The four files

| file | change |
|---|---|
| `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c` | **new**, the claim's source: `fn_801FDAC4` then `fn_801FDAA4`, descending |
| `config/G2ME01/splits.txt` | `MetroidPrime/ScriptObjects/Carve801FDAA4.c: .text start:0x801FDAA4 end:0x801FDAE8`, between `Carve801F97C8.c` (0x801F9848) and `Carve801FDB5C.c` (0x801FDBE0) |
| `configure.py` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FDAA4.c"),` on one line, same neighbours |
| `files.cmake` | `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c`, same neighbours |

`tools/check_files_cmake.py` reports *"every configured DOL object is either in files.cmake or
excluded with a reason"*, and `python3 tools/check_decl_order.py --unit
main/MetroidPrime/ScriptObjects/Carve801FDAA4` reports *"1 unit(s) checked, none emits its functions
out of retail order"*.  Independently, our object's own `.text` order is ascending, which is the
check that actually matters:

```
00000000 <fn_801FDAA4>:
00000020 <fn_801FDAC4>:
```

`tools/carve_diff.sh 0x801FDAA4 0x44 build/G2ME01/obj/MetroidPrime/ScriptObjects/Carve801FDAA4.o`
reports 17 instructions / 68 bytes on both sides and **2 differing instructions**, both `bl`s -
i.e. only the unresolved call displacements, which the link fills.  (Run it on the whole range, not
with a symbol argument: the symbol filter restarts the offsets and prints `ours: <none>`.)

## What the two functions are

`.text 0x801FDAA4..0x801FDAE8`, `0x44` = 68 bytes, **2 functions**, carved from dtk's
`auto_03_801FA3CC_text` (`config/G2ME01/symbols.txt:8285-8286`) exactly as the item's range said -
nothing was extended.  `rstl::destroy<T>` and the `rstl::destroy_impl<T>` under it, for the
0x2C-byte element of the block `ScriptObjects/Carve801FF8A0.cpp` walks:

- `fn_801FDAA4` - 8 instructions, a 0x10 frame and one `bl fn_801FDAC4` (0x801FDAB0).  Its measured
  twin is `fn_80004D3C` (0x80004D3C, 0x20) in `Player/Carve80004C4C.c`, a `Matching` unit: the same
  eight instructions with a different `bl`.  Also the same eight as `__sys_free` (0x80008A28).
- `fn_801FDAC4` - 9 instructions, `li r4,-1` at 0x801FDACC (**before** the `stw`, which is where
  MWCC puts a constant argument) and `bl fn_801FDAE8` at 0x801FDAD4.  Its measured twin is
  `fn_80004C6C` (0x80004C6C, 0x24), same nine instructions.  `-1` is an `int`, not a `short`.

So the bodies are one line each and they matched first try; no spelling search was needed.

**The caller is what identifies them.**  `Carve801FF8A0.cpp`'s `fn_801FF96C` (a `Matching` unit)
walks that block's elements 0x2C at a time and calls `fn_801FDAA4` on each (`bl fn_801FDAA4` at
0x801FF990), so these two are that element's teardown - and `fn_801FDAE8` is the *deleting*
destructor: it stores the vtable word `lbl_803B7BE4` at `+0`, destroys a `rstl::basic_string` at
`+4` and another member at `+0x18`, and calls `CMemory::Free` only when the flag is positive
(`extsh. r0,r31` / `ble`).  That is exactly what `destroy_impl`'s `-1` is there to suppress.

## The link side: `stub_190` out, `stub_195` in, count unmoved

`fn_801FDAA4` was a hand stand-in in `src/MetroidPrime/PortLinkStubs.cpp` (`stub_190`), added for
`Carve801FF8A0.cpp`, so the block is **deleted** - a carve listed while the stub file also defines
the symbol is a duplicate the moment it is listed, and `tools/gate.sh`'s `port link dups` step
exists for that.

Its replacement is one function deeper, and that is the whole cost of this item:

- `fn_801FDAC4` is defined by the new unit, so it never enters the gap.
- `fn_801FDAE8` (0x801FDAE8, 0x74) is **not** claimed by anything, and the carve cannot drop the
  `bl` - it is in retail's bytes.  It therefore needs `stub_195` in `PortLinkStubs.cpp`, the same
  trade `stub_189` records for the neighbouring `Carve801FF8A0.cpp`'s `fn_801FEC64`, and the same
  trade `stub_190` itself recorded.  Claiming `fn_801FDAE8` instead would only move the gap along
  to its own callee `fn_801FD6F0` (0x801FD6F0, 0x84), which is in the same range, so the item's
  range was kept as written.

Net: 163 function stubs before and after (one retired, one added), 31 unmangled `fn_`/`lbl_`
stubs before and after; the header counts in `PortLinkStubs.cpp` were updated to say so rather than
left stale.  Measured on this tree:

```
probe: 802 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)
link_check: unique undefined symbols 291
link_check: duplicate definitions   0
link_check: unchanged from baseline (291 undefined, 0 duplicates)
ok: 285 MISSING symbol(s), all accounted for in port_link_gap_list.md
```

## Superseded claim corrected in place

`src/MetroidPrime/ScriptObjects/Carve801FF8A0.cpp:48-51` said *"Neither is claimed by anything in
this tree (measured: `grep -rn "fn_801FEC64\|fn_801FDAA4" src/ include/` is empty), so both need a
stand-in ... see `stub_189`/`stub_190` there"*.  This change makes half of it false, so it now
records what happened and says which part was superseded.

## Not done, deliberately

- **`fn_801FDAE8` (0x74 = 116 bytes) is not claimed.**  It is the obvious next carve here - it is
  the next contiguous function, the range above it is unclaimed to 0x801FDBE0, and it is the
  deleting destructor, so its body is mostly readable off the listing.  It is *not* a twin of
  anything matched in this tree, it stores a data word (`lbl_803B7BE4`) that would itself have to
  be claimed or stubbed, and it would add `fn_801FD6F0` to the port's gap - so it needs its own
  item and its own spelling search.  See the `NEW:` line below.
- `fn_801FDA54` (0x801FDA54, 0x50) and `fn_801FDB5C` (0x801FDB5C, 0x84) stay retail's.
  `fn_801FDB5C` is the function `docs/goal-notes/carve-801fdb5c.md` already recorded as not
  matchable by any spelling tried; nothing here reopens it.

NEW: carve-801fdae8 | match | MetroidPrime/ScriptObjects/Carve801FDAE8 | `fn_801FDAE8` 0x801FDAE8 0x74 B is the next contiguous unclaimed function after this carve, is the element's deleting destructor (vtable word `lbl_803B7BE4` at +0, `rstl::basic_string` at +4, member at +0x18, `CMemory::Free` gated on `extsh. r0,r31`/`ble`), and retiring `stub_195` needs no port-link change

## Lesson worth keeping (not a `NEW:`)

**A carve's own callee is where its link cost lands, and it is one function deeper than the item
suggests.**  The item named a pair of twins that both matched on the first spelling; the only real
work was the third function neither twin mentions, and the reason the run is green rather than
`STRICT FAIL ... (GREW)` is the hand stub that retires when the carve replaces the stub beside it.
The rule generalises: for a carve at depth *n* from a stubbed symbol, expect to write (or explicitly
carry) the stand-in for depth *n+1*, and measure `link_gap.py` before assuming the item's range is
the whole cost.

## Two things found while running the judge, for the driver

**1. `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` are already 137x duplicated in the committed
tree.**  Measured at `HEAD`, with no edit of mine in place:

```
$ git show HEAD:docs/HANDOFF.md | wc -l              # 53598
$ git show HEAD:docs/HANDOFF.md | sort -u | wc -l     #   391
$ git show HEAD:docs/RUNNING_THE_DECOMP.md | wc -l    # 60008
```

`AGENTS.md` says `HANDOFF.md` is under ~150 lines; it is 53,598.  `tools/check_docs_claims.py`
`--write` (`DOCS`, the loop at `check_docs_claims.py:121-126`) rewrites `\d{3} files\b` across
the whole file, so on every `goal_check.sh` run the gate touches ~106,000 lines of these two files
and leaves a diff that dwarfs the real change.  I reverted them (`git checkout -- docs/...`) after
the run, so the tree carries only the six files the item needs; the judge had already passed.
**This is a docs fix, so per the brief it is not a `NEW:` line** - but it is why the gate's own diff
is enormous, and it will be enormous for every lane until the duplication is collapsed.

**2. `tools/carve_diff.sh`'s symbol argument is wrong** - run on the whole range, not with a symbol.
`./tools/carve_diff.sh 0x801FDAA4 0x20 <obj> fn_801FDAA4` reports *"differing instructions: 10"* and
prints `ours: <none>` against retail instructions that are identical in ours, because the symbol
filter restarts our instruction stream at offset 0 instead of skipping to the symbol.  Without the
argument the same range reports 0 differing (only the two `bl` displacements, which are
relocations).  A lane that trusts the per-symbol form will read a byte-exact carve as broken.

---

# Second attempt, lane 8 (`goal/lane-8`, HEAD `dc8b2938` "match: carve-801fbc58")

**Result: PASS again, re-measured from scratch on this tree.** `tools/goal_check.sh
build/goal/item.json` exits 0, last line `goal_check: PASS carve-801fdaa4`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12675 -> 12677   linked 6034 -> 6036
  ok    check_symbol_names.py
  ok    All:  35.53% fuzzy, 29.38% matched, 13.05% linked (12677 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FDAA4.c: PASS, Object(Matching) in configure.py
```

The previous run's change was **not on disk anywhere** when this lane started, so this is a re-do,
not a duplicate: `ls wt-mp2-goal/src/MetroidPrime/ScriptObjects/Carve801FDAA4.c` -> "No such file",
`git -C wt-mp2-goal status --short` -> only `?? publish.lock`, and this worktree started clean with no
`Carve801FDAA4` in `configure.py`, `files.cmake`, `splits.txt` or `build/report.json`.  What *was* left
was the untracked `build/` from that run: `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801FDAA4.s`
and `build/G2ME01/obj/.../Carve801FDAA4.o`.  Those are build products of a tree state that no longer
exists; every byte quoted below was re-read from `build/G2ME01/main.elf` after a fresh build.

## Measured on this tree

| check | command | result |
|---|---|---|
| not stale | `build/report.json` before the change | no `Carve801FDAA4` unit; `matched_functions 12675`, `total_functions 28465` |
| the unit | `decomp_build.sh main/MetroidPrime/ScriptObjects/Carve801FDAA4` | `100.00% fuzzy, 100.00% matched (2 / 2 functions)`; report.json `complete: True`, `fn_801FDAC4` 100.0 / 36 B, `fn_801FDAA4` 100.0 / 32 B |
| the one rule | `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` |
| flip | `tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FDAA4.c` | `PASS -> kept as Matching`, `kept: 1 / 1` |
| fit | `tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FDAA4.c` | `.text claimed 68 ours 68 retail 68 fits`; "no extra functions" |
| bytes | `tools/carve_diff.sh 0x801FDAA4 0x44 build/G2ME01/obj/.../Carve801FDAA4.o` | 17 instructions / 68 bytes on both sides, 2 differing instructions, both `bl` (relocations the link fills) |
| order | `check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FDAA4` | "1 unit(s) checked, none emits its functions out of retail order"; the object's own `.text` is `fn_801FDAA4` at +0 then `fn_801FDAC4` at +0x20 |
| totals | `build/report.json` | `total_functions` still 28465; `check_symbol_names.py`: "checked 532 units; 0 declared names are missing" |
| cmake | `check_files_cmake.py` | "every configured DOL object is either in files.cmake or excluded with a reason" |
| port gap | `python3 tools/link_gap.py --rebuild` | `285 MISSING`, "all accounted for in port_link_gap_list.md" - same as the baseline |

The stub comment claims the gap measurement, so it was measured rather than asserted: with the
`stub_197` block deleted from `PortLinkStubs.cpp` the same command prints `286 MISSING` and
`gap grew: fn_801FDAE8 is not in port_link_gap_list.md`; with the block restored it prints `285 MISSING`,
all accounted for.  (Measured, then the file was restored from a copy.)

## The four files, and the fifth and sixth

Unchanged in plan from the first attempt, so the two functions still matched on the first spelling -
`fn_801FDAC4` = `fn_801FDAE8(self, -1)`, which is its twin `fn_80004C6C`'s body `fn_80004A4C(self, -1)`
with the callee swapped, and `fn_801FDAA4` = `fn_801FDAC4(self)`, the twin `fn_80004D3C`'s
`fn_80004D5C(dest, src)` forwarder:

| file | change |
|---|---|
| `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c` | **new**: `fn_801FDAC4` then `fn_801FDAA4`, descending |
| `config/G2ME01/splits.txt` | `Carve801FDAA4.c: .text start:0x801FDAA4 end:0x801FDAE8`, between `Carve801FBC58.c` and `Carve801FDB5C.c` |
| `configure.py` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FDAA4.c"),` on one line |
| `files.cmake` | `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c`, same neighbours |
| `src/MetroidPrime/PortLinkStubs.cpp` | `stub_190` (**fn_801FDAA4**) retired, `stub_197` (**fn_801FDAE8**) added |
| `src/MetroidPrime/ScriptObjects/Carve801FF8A0.cpp:47-55` | its "neither is claimed by anything in this tree" claim is half false now; marked superseded |

## Two corrections to the item's own reason, measured here

1. **The dtk range is `auto_03_801FBD68_text`, not `auto_03_801FA3CC_text`.**  The item (and the
   retired `stub_190` comment) name `auto_03_801FA3CC_text`; HEAD's own previous commit,
   `dc8b2938 match: carve-801fbc58`, claimed 0x801FBC58..0x801FBD68 out of that file and dtk now emits
   `build/G2ME01/asm/auto_03_801FBD68_text.s`, `# 0x801FBD68..0x801FDBE0 | size: 0x1E78`.  `fn_801FDAA4`
   is a `.fn` in that file.  The new `stub_197` comment names the right object; nothing else in the
   tree needed it.
2. **`stub_195` was free when the first run wrote it; it is taken now.**  This tree already has
   `stub_195` = `fn_80008C28` (`Carve800052A0.c`) and `stub_196` = `fn_801FBD68`
   (`Carve801FBC58.c`), so this run's new block is `stub_197`.  A lane that copies the earlier
   naming will collide.

## Carve counts did not move, so no header numeral changed

`grep -cE 'asm\("' src/MetroidPrime/PortLinkStubs.cpp` = **169** before and after, of which 4 are data
stubs and 33 are unmangled `fn_`/`lbl_` - this run's change is an exact exchange (one unmangled stub
retired, one added), so every term in the file's header paragraph is unchanged by it.  Left alone on
purpose: the header's own numerals (164 functions / 32 unmangled, at lines 12 and 34) are one below
what the file's stated derivation gives today (169 - 4 = 165 functions, 33 unmangled).  That
off-by-one is pre-existing and not this item's business.

## Still outstanding: `fn_801FDAE8` (0x74 bytes) - the earlier `NEW:` line in this file stands

Re-measured here, so it is not stale: `fn_801FDAE8 = .text:0x801FDAE8; size:0x74` at
`config/G2ME01/symbols.txt:8287` is unclaimed, its body is `mr. r30,r3` / `beq`,
`lis r4,-32709` / `addi r0,r4,31716` (the `lbl_803B7BE4` vtable word, `symbols.txt:18345`) / `stw r0,0(r30)`,
`addi r3,r30,24` / `li r4,-1` / `bl fn_801FD6F0`, `addic. r0,r30,4` / `beq` /
`bl internal_dereference__Q24rstl66basic_string...` (0x802FE9B8), then `extsh. r0,r31` / `ble` /
`mr r3,r30` / `bl Free__7CMemoryFPCv`.  Its boundaries are clean (below this unit's claim end,
above `fn_801FDB5C` at 0x801FDB5C, which `docs/goal-notes/carve-801fdb5c.md` already recorded as not
matchable), so a carve 0x801FDAE8..0x801FDB5C is available.  Two things it costs that the current
claim avoids: the data word `lbl_803B7BE4` (nothing in this tree claims it) and `fn_801FD6F0`
(0x801FD6F0, 0x84, `symbols.txt:8274`, unclaimed), either of which may need its own stand-in.
Retiring `stub_197` is the payoff.  Not filed again - the `NEW:` line is already in this file.

## Two process facts, re-confirmed rather than recalled

- **`tools/goal_check.sh` leaves `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` rewritten**:
  `git diff --stat docs/` right after a PASS in this lane prints `191614` and `191608` changed lines.
  Those two files are 96254 and 102664 lines long at `HEAD` (`git show HEAD:... | wc -l`) - the same
  duplication the first attempt measured, and it grows.  `git checkout --` on both after the judge
  leaves the tree carrying only the six files above.  A lane that does not revert them commits a
  191k-line diff.
- **`tools/carve_diff.sh`'s symbol argument is still wrong** (first attempt, item 2 above): re-measured
  here only in the whole-range form, which is the correct one.

---

# Third attempt, lane 8 (`goal/lane-8`, HEAD `55021a40` "match: carve-8019ac78")

**Result: PASS again, every number re-measured on this tree.** `tools/goal_check.sh
build/goal/item.json` exits 0, last line `goal_check: PASS carve-801fdaa4`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12683 -> 12685   linked 6042 -> 6044
  ok    check_symbol_names.py
  ok    All:  35.54% fuzzy, 29.38% matched, 13.05% linked (12685 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FDAA4.c: PASS, Object(Matching) in configure.py
```

**Third time this item has been re-queued with the change absent from the tree** (first attempt:
"Result: PASS"; second: "PASS again, re-measured from scratch"; this one is the third). Nothing of
the earlier runs' work was on disk: `ls src/MetroidPrime/ScriptObjects/Carve801FDAA4.c` -> "No such
file", no `Carve801FDAA4` in `configure.py` / `files.cmake` / `splits.txt` / `build/report.json`,
and this worktree started clean at `55021a40`. So this is a third independent re-do, not a
duplicate, and everything below was re-read from `build/G2ME01/main.elf` and `build/report.json`
after a fresh build rather than carried over.

## Measured on this tree

| check | command | result |
|---|---|---|
| not stale | `build/report.json` before the change | no `Carve801FDAA4` unit; `matched_functions 12683`, `total_functions 28465` |
| the unit | `decomp_build.sh main/MetroidPrime/ScriptObjects/Carve801FDAA4` | `100.00% fuzzy, 100.00% matched (2 / 2 functions)`; report.json `fn_801FDAC4` 100.0 / 36 B, `fn_801FDAA4` 100.0 / 32 B, `complete: True` |
| the one rule | `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` |
| flip | `tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FDAA4.c` | `PASS -> kept as Matching`, `kept: 1 / 1` |
| fit | `tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FDAA4.c` | `.text claimed 68 ours 68 retail 68 fits`; "no extra functions: our object defines only what the retail unit object does" |
| bytes | `tools/carve_diff.sh 0x801FDAA4 0x44 build/G2ME01/obj/.../Carve801FDAA4.o` | 17 instructions / 68 bytes on both sides, 2 differing instructions, both `bl` (relocations the link fills) |
| order | `check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FDAA4` | "1 unit(s) checked, none emits its functions out of retail order"; the object's own `.text` is `fn_801FDAA4` at +0 then `fn_801FDAC4` at +0x20 |
| totals | `build/report.json` | `total_functions` still 28465; `check_symbol_names.py`: "checked 532 units; 0 declared names are missing" |
| cmake | `check_files_cmake.py` | "every configured DOL object is either in files.cmake or excluded with a reason" |
| port link, carve only | `tools/link_check.sh` before touching `PortLinkStubs.cpp` | `unique undefined symbols 292`, `duplicate definitions 1 / DUP fn_801FDAA4`, `FAIL duplicates went 0 -> 1`, `FAIL undefined went 291 -> 292` |
| port link, done | `tools/link_check.sh` after | `291 undefined, 0 duplicates`, "unchanged from baseline" |
| port gap | `python3 tools/link_gap.py --rebuild` | carve only: `286 MISSING` + `gap grew: fn_801FDAE8 is not in port_link_gap_list.md`; with `stub_198`: `285 MISSING`, "all accounted for" |

The two stub-comment measurements are measured, not asserted: with the `stub_198` block deleted the
same `link_gap.py` command prints `286 MISSING` and names `fn_801FDAE8`; with the block in place it
prints `285 MISSING`, all accounted for.

## The six files

Unchanged in plan from the two earlier attempts, so the bodies still matched on the first spelling -
`fn_801FDAC4` = `fn_801FDAE8(self, -1)` and `fn_801FDAA4` = `fn_801FDAC4(self)`, the twins
`fn_80004C6C` / `fn_80004D3C` in `Player/Carve80004C4C.c` with the callee swapped:

| file | change |
|---|---|
| `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c` | **new**: `fn_801FDAC4` then `fn_801FDAA4`, descending (97 lines) |
| `config/G2ME01/splits.txt` | `Carve801FDAA4.c: .text start:0x801FDAA4 end:0x801FDAE8`, between `Carve801FD638.c` (ends 0x801FD67C) and `Carve801FDB5C.c` (starts 0x801FDBE0) |
| `configure.py` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FDAA4.c"),` on one line, after `Carve801FD638.c` |
| `files.cmake` | `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c`, same neighbours |
| `src/MetroidPrime/PortLinkStubs.cpp` | the `stub_190` (**fn_801FDAA4**) block **deleted**; `stub_198` (**fn_801FDAE8**) added after `stub_197` |
| `src/MetroidPrime/ScriptObjects/Carve801FF8A0.cpp:47-58` | its "neither is claimed by anything in this tree" claim is half false now; marked superseded in place |

`splits.txt`, `configure.py` and `files.cmake` neighbours differ from the second attempt's, because
other lanes have landed carves in between: `ScriptObjects/Carve801FD638.c` (0x801FD638..0x801FD67C,
`Matching`, 2/2) now exists and is this unit's immediate predecessor.

## Two facts this tree has changed since the second attempt

1. **The dtk auto object that held `fn_801FDAA4` is now `auto_03_801FD67C_text.o`
   (0x801FD67C..0x801FDAA4, 0x428), not `auto_03_801FA3CC_text.o`.**  Measured:
   `grep -ln fn_801FDAA4 build/G2ME01/asm/*.s` -> `auto_03_801FD67C_text.s` (plus the two units that
   call it).  `Carve801FD638.c` claiming 0x801FD638..0x801FD67C started a new auto range at
   0x801FD67C, so the file name in the item's reason and in both earlier runs' stub comments is
   stale here.  `fn_801FDAE8` likewise now has an auto object of its own,
   `auto_03_801FDAE8_text.o` (0x801FDAE8..0x801FDBE0), so the new stub comment names that one.
2. **`stub_198` is the free name on this tree; `stub_197` is taken by `fn_801FD67C`**
   (`Carve801FD638.c`'s callee, added at HEAD~).  The second attempt's new block was `stub_197` for
   the same reason; a fourth attempt must re-measure.  `grep -c stub_198` is 0 before this run.

## Carve counts did not move, so no header numeral changed

`grep -cE 'asm\("' src/MetroidPrime/PortLinkStubs.cpp` = **169** before and after, of which **33**
are unmangled `fn_`/`lbl_`: this run's change is an exact exchange (one unmangled stub retired, one
added), so every term in the file's header paragraph is unchanged by it.  Left alone on purpose: the
header's own numerals (164 functions / 32 unmangled, lines 12 and 34) are one below what the file's
stated derivation gives today (169 - 4 = 165 functions, 33 unmangled).  That off-by-one is
pre-existing, was already recorded by the second attempt as not this item's business, and is not
created or moved by this change.

## Still outstanding: `fn_801FDAE8` (0x74 bytes) - the earlier `NEW:` line in this file stands

Re-measured here, so it is not stale.  Boundaries are clean and available: `fn_801FDAE8 =
.text:0x801FDAE8; size:0x74` (`symbols.txt:8287`), its claim end above is exactly this unit's end,
and `fn_801FDB5C` (0x801FDB5C, 0x84) is still unclaimed (`grep -rn 0x801FDB5C config/G2ME01/splits.txt`
is empty - `ScriptObjects/Carve801FDB5C.c` claims 0x801FDBE0..0x801FDC88, *past* `fn_801FDB5C`).  So
a carve 0x801FDAE8..0x801FDB5C is available and retiring `stub_198` is the payoff.  It costs the
`.data` word `lbl_803B7BE4` (0x803B7BE4, `symbols.txt:18345`, unclaimed) and `fn_801FD6F0`
(0x801FD6F0, 0x84, unclaimed) - the same two as the `stub_197` comment records for `fn_801FD67C`,
whose body at `build/G2ME01/asm/auto_03_801FDAE8_text.s:9-41` is instruction-for-instruction the
same shape with `lbl_803B7BE4` in place of `lbl_803B7BFC`.  **Not re-filed**: the `NEW:` line for
`carve-801fdae8` is already in this file and a second copy would only duplicate the queue entry.

## Process facts, re-confirmed here rather than recalled

- **`tools/goal_check.sh` still leaves `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md`
  rewritten**: `git diff --stat docs/` immediately after the PASS printed `574830` and `574824`
  changed lines.  `git checkout --` on both leaves the tree carrying only the six files above.  Both
  earlier attempts measured the same thing (191k lines then, 575k now - the duplication at HEAD has
  grown).  It is a docs fix, so per the brief it is not a `NEW:` line.
- **`tools/carve_diff.sh`'s symbol argument is still wrong** (first attempt, item 2 above).  Only
  the whole-range form was used here, which is the correct one.
- The gap really does land one function deeper than the item's range, on this tree too: with only
  the carve in place, `tools/link_check.sh` reports **both** `FAIL duplicates went 0 -> 1 /
  DUP fn_801FDAA4` and `FAIL undefined went 291 -> 292`.  Deleting `stub_190` without adding
  `stub_198` would have failed the first and fixed the second; the two edits only work as a pair.

---

# Fourth attempt, lane 8 (`goal/lane-8`, HEAD `5dd4446f` "match: carve-801fd8e0")

**Result: PASS again, every number re-measured on this tree.** `tools/goal_check.sh
build/goal/item.json` exits 0, last line `goal_check: PASS carve-801fdaa4`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12687 -> 12689   linked 6046 -> 6048
  ok    check_symbol_names.py
  ok    All:  35.54% fuzzy, 29.39% matched, 13.06% linked (12689 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FDAA4.c: PASS, Object(Matching) in configure.py
```

**Fourth time the change was absent from the tree** (first / second / third attempts above each
found it gone). Measured before touching anything: `ls src/MetroidPrime/ScriptObjects/Carve801FDAA4.c`
-> "No such file"; `grep -c Carve801FDAA4 configure.py files.cmake config/G2ME01/splits.txt` -> 0,0,0;
`build/report.json` has no unit by that name; the worktree started clean at `5dd4446f`. So this is a
fourth independent re-do and nothing below is carried over - the disassembly, the twin word counts
and every gate number were re-derived from `orig/G2ME01/sys/main.dol` and a fresh build.

## Measured on this tree

| check | command | result |
|---|---|---|
| not stale | `build/report.json` before the change | no `Carve801FDAA4` unit; `matched_functions 12687`, `total_functions 28465` |
| retail bytes | `objdump -D -b binary --adjust-vma=0x801FDAA4` on the 68 disc bytes at file offset `0x640 + (addr - 0x80003840)` | 17 instructions, `fn_801FDAA4` 8 at +0x00, `fn_801FDAC4` 9 at +0x20 |
| the unit | `decomp_build.sh main/MetroidPrime/ScriptObjects/Carve801FDAA4` | `100.00% fuzzy, 100.00% matched (2 / 2 functions)`; report.json `fn_801FDAA4` 100.0 / 32 B, `fn_801FDAC4` 100.0 / 36 B, `complete: true` |
| the one rule | `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` |
| flip | `tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FDAA4.c` | `PASS -> kept as Matching`, `kept: 1 / 1`, `failed: 0`, and `configure.py:761` is still `Object(Matching, ...)` afterwards |
| fit | `tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FDAA4.c` | `.text claimed 68 ours 68 retail 68 fits`; "no extra functions" |
| bytes | `tools/carve_diff.sh 0x801FDAA4 0x44 build/G2ME01/obj/.../Carve801FDAA4.o` | 17 instructions / 68 bytes both sides, `differing instructions: 2`, both `bl` (relocations the link fills) |
| order | `check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FDAA4` | "1 unit(s) checked, none emits its functions out of retail order"; the object's own `.text` is `T fn_801FDAA4` at +0 then `T fn_801FDAC4` at +0x20, `U fn_801FDAE8` |
| totals | `build/report.json` | `total_functions` still 28465; `matched 12687 -> 12689`, `complete_code 853424 -> 853492` (+68); `check_symbol_names.py`: "checked 532 units; 0 declared names are missing" |
| cmake | `check_files_cmake.py` | "every configured DOL object is either in files.cmake or excluded with a reason" |
| port link, carve only | `tools/link_check.sh` with the carve in and `stub_190` still present | `FAIL duplicates went 0 -> 1 / DUP fn_801FDAA4`, `FAIL undefined went 291 -> 292` |
| port link, done | `tools/link_check.sh` after the stub swap | `compile errors 0`, `291 undefined`, `0 duplicates`, "unchanged from baseline" |
| port gap | `python3 tools/link_gap.py --rebuild` | with the `stub_199` block removed: `286  MISSING` + `gap grew: fn_801FDAE8 is not in port_link_gap_list.md`; with it in place: `285  MISSING`, "ok: all accounted for" |

The two stub-comment measurements are measured, not asserted: the `stub_199` block was deleted, the
gap command re-run and the number quoted, then the file restored from a copy
(`git diff --stat` after the restore matches the intended edit).

## The six files

Unchanged in plan from the three earlier attempts, so the bodies still matched on the first
spelling - `fn_801FDAC4` = `fn_801FDAE8(self, -1)` and `fn_801FDAA4` = `fn_801FDAC4(self)`, the
twins `fn_80004C6C` / `fn_80004D3C` in `Player/Carve80004C4C.c` with the callee swapped:

| file | change |
|---|---|
| `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c` | **new**: `fn_801FDAC4` then `fn_801FDAA4`, descending (100 lines) |
| `config/G2ME01/splits.txt` | `Carve801FDAA4.c: .text start:0x801FDAA4 end:0x801FDAE8`, between `Carve801FD8E0.c` (0x801FD8E0..0x801FD924) and `Carve801FDB5C.c` (0x801FDBE0..0x801FDC88) |
| `configure.py` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FDAA4.c"),` on one line, after `Carve801FD8E0.c` |
| `files.cmake` | `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c`, same neighbours |
| `src/MetroidPrime/PortLinkStubs.cpp` | the `stub_190` (**fn_801FDAA4**) block **deleted**; `stub_199` (**fn_801FDAE8**) added after `stub_198` |
| `src/MetroidPrime/ScriptObjects/Carve801FF8A0.cpp:47-56` | its "neither is claimed by anything in this tree" paragraph is half false now; marked superseded in place, with the still-true half about `fn_801FEC64` kept |

## The twin measurements, re-derived here rather than recalled

Disassembled off the disc this run (`-b binary --adjust-vma`), so the word counts are facts about
*this* tree's bytes:

- `fn_801FDAA4` vs `fn_80004D3C` (0x80004D3C, 0x20): **8 words, 0 differ** - the `bl` word included.
  Both read `48000015`, because each callee sits 0x14 past its own `bl` (0x801FDAB0 + 0x14 =
  0x801FDAC4 here; 0x80004D48 + 0x14 = 0x80004D5C there).  `__sys_free` (0x80008A28) is the same
  eight apart from its `bl` (`482c5955`).
- `fn_801FDAC4` vs `fn_80004C6C` (0x80004C6C, 0x24): **9 words, 1 differs** - offset 0x10, the
  `bl` (`48000158` -> 0x80004A4C there, `48000015` -> 0x801FDAE8 here).  `li r4,-1` is at 0x801FDACC,
  **before** the `stw` at 0x801FDAD0, which is where MWCC puts a constant argument.
- **Element 0x2C = 44 bytes**, from both retail callers, read off the disc this run:
  `fn_801FDA54` (0x801FDA54, 0x50, unclaimed) - `bl fn_801FDAA4` at 0x801FDA78, `addi r31,r31,0x2C`;
  `fn_801FF96C` (0x801FF96C, in the `Matching` unit `ScriptObjects/Carve801FF8A0.cpp`) - `bl
  fn_801FDAA4` at 0x801FF990, `addi r31,r31,0x2C`.  In both, `r4` is dead at the call site, so the
  argument list is one pointer and no struct is spelled.

## Free stub name on this tree: `stub_199`

`stub_197` = `fn_801FD67C`, `stub_198` = `fn_801FD924` (HEAD's own `Carve801FD8E0.c` carve); the
highest number in the file is `stub_198` and `grep -c stub_199` was 0 before this run. **A fifth
attempt must re-measure again** - three of the four names (`stub_195`, `stub_197`, `stub_198`) were
each taken by a lane landing between runs.

Stub counts did not move: `grep -cE 'asm\("'` = **169** before and after, of which **33** are
unmangled `fn_`/`lbl_`, so this run's change is an exact exchange and no header numeral changed.
The file's own header numerals (164 functions / 32 unmangled) are still one below what its stated
derivation gives (169 - 4 = 165, 33) - pre-existing, recorded as not this item's business by the
second attempt, and not created or moved here.

## The link side, unchanged in kind from the three earlier runs

The gap lands **one function deeper than the item's range**, on this tree too, for the same reason
the second attempt gave: `fn_801FDAA4` was a hand stand-in (`stub_190`) added for
`Carve801FF8A0.cpp`, so retiring it is mandatory (a carve listed while the stub file also defines
the symbol is a duplicate), and `fn_801FDAE8` is in retail's bytes with nothing claiming it, so it
needs `stub_199`.  One stub out, one in.  `tools/gate.sh`'s `port link dups` step is what catches
the half-done state.

## Still outstanding: `fn_801FDAE8` (0x74 bytes) - the `NEW:` line from the first attempt stands

Re-measured here, so it is not stale.  `fn_801FDAE8 = .text:0x801FDAE8; size:0x74`
(`symbols.txt:8287`) is unclaimed and its boundaries are clean: its start is exactly this unit's
claim end, and `fn_801FDB5C` (0x801FDB5C, 0x84) is still unclaimed too, so a carve
0x801FDAE8..0x801FDB5C is available and retiring `stub_199` is the payoff.  Its body, re-read this
run from `build/G2ME01/asm/auto_03_801FDAE8_text.s:10-41`: `mr. r30,r3` / `beq`, `lis r4,
lbl_803B7BE4@ha` / `addi r0,r4,lbl_803B7BE4@l` / `stw r0,0x0(r30)`, `addi r3,r30,0x18` / `li r4,-1` /
`bl fn_801FD6F0` (0x801FD6F0, 0x84, also unclaimed), `addic. r0,r30,0x4` / `beq` / `addi r3,r30,0x4` /
`bl "internal_dereference__Q24rstl66basic_string<...>"` (0x802FE9B8), then `extsh. r0,r31` / `ble` /
`mr r3,r30` / `bl Free__7CMemoryFPCv`.  It costs the `.data` word `lbl_803B7BE4` (0x803B7BE4,
`symbols.txt:18345`, size 0xC, unclaimed) as well as `fn_801FD6F0`, either of which may need its
own stand-in - so it needs its own item and its own spelling search.  **Not re-filed**: the `NEW:`
line for `carve-801fdae8` is already in this file and a second copy would only duplicate the queue
entry.

## Process facts, re-confirmed here rather than recalled

- **`tools/goal_check.sh` still leaves `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` rewritten**:
  `git diff --stat -- docs/` immediately after the PASS printed `1149651` changed lines across the
  two files - the same duplication the three earlier attempts measured (191k, then 575k, now 1.15M
  lines; it grows every run).  `git checkout --` on both leaves the tree carrying only the six files
  above.  A lane that does not revert them commits a million-line diff.  It is a docs fix, so per
  the brief it is not a `NEW:` line.
- **`tools/carve_diff.sh`'s symbol argument is still wrong** (first attempt, item 2 above).  Only the
  whole-range form was used here, which is the correct one.
