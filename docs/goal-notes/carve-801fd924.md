# carve-801fd924 - `MetroidPrime/ScriptObjects/Carve801FD924` (match, lane 10)

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` exits 0, last line
`goal_check: PASS carve-801fd924`:

```
goal_check: item carve-801fd924 (match) target=MetroidPrime/ScriptObjects/Carve801FD924
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13028 -> 13029   linked 6132 -> 6133
  ok    check_symbol_names.py
  ok    All:  36.96% fuzzy, 30.40% matched, 13.39% linked (13029 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FD924.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fd924
```

Baseline on this tree before any edit (HEAD `288dea4e match: carve-801fec64`): `./tools/decomp_build.sh`
-> `All:  36.96% fuzzy, 30.40% matched, 13.39% linked (13028 / 28465 functions)`,
`matched_functions 13028`, `complete_units 810`, `sha1sum build/G2ME01/main.dol` =
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.  After: **13029** matched, `complete_units 811`,
`total_units 2135 -> 2136`, `total_functions` still **28465**, the same DOL sha1.
`build/report.json` has the unit at `fuzzy_match_percent 100.0`, `total_code 116`,
`matched_code 116`, `total_functions 1`, `matched_functions 1`, `metadata.complete true`; the one
function is `fn_801FD924`, `0x801FD924`, `size 116`, `100.0`.

The judge above was re-run on the **final** tree, after the last edits (comment-only, in the new
source and in `PortLinkStubs.cpp`), and printed the same verdict and the same counts; the DOL sha1
and the unit's 116/116 did not move.  The stub comment's "called from three places" was one of
those corrections: retail calls `fn_801FD6F0` from **eight** sites
(`objdump -d build/G2ME01/main.elf | grep 'bl.*801fd6f0'`: the three 0x74-byte destructors at
0x801FD6B0, 0x801FD958 and 0x801FDB1C, plus five inside dtk's `auto_03_801FDC88_text` range).

## What was done

Wrote the one unsourced function at `.text 0x801FD924..0x801FD998` (0x74 = 116 bytes) as
`src/MetroidPrime/ScriptObjects/Carve801FD924.cpp`, out of dtk's `auto_03_801FD924_text` gap.
A carve is four files, all four in this change:

- `configure.py` - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD924.cpp"),` (one line)
  between `ScriptObjects/Carve801FD8E0.c` and `ScriptObjects/Carve801FDAA4.c`.
- `config/G2ME01/splits.txt` - `MetroidPrime/ScriptObjects/Carve801FD924.cpp:` /
  `.text start:0x801FD924 end:0x801FD998`, same position; `total_functions` is still **28465**.
- `files.cmake` - `src/MetroidPrime/ScriptObjects/Carve801FD924.cpp`, same position.
- `src/MetroidPrime/ScriptObjects/Carve801FD924.cpp` - one definition, `fn_801FD924`.

Two more files, and neither is decorative - see "The port side" below:

- `src/MetroidPrime/PortLinkStubs.cpp` - `stub_198` (`fn_801FD924`) retired, `stub_227`
  (`fn_801FD6F0`) and `stub_data_6` (`lbl_803B7BF0`) added in its place, top numerals and the
  breakdown line re-derived (193 -> **194** symbols, 187 functions, 6 -> **7** data objects).
- The gate rewrote `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` (probe count 811 -> 812 and
  the state block); both were reverted afterwards so the change is the five files above.

## What the function is, and the three measurements that fix it

`fn_801FD924` is the **deleting destructor of the 0x24-byte script-object element** whose
`rstl::destroy` pair `src/MetroidPrime/ScriptObjects/Carve801FD8E0.c` (0x801FD8E0..0x801FD924,
`Matching`) already holds: its `fn_801FD900` is `li r4,-1` / `bl fn_801FD924`, so the claim
immediately below this one is this function's own caller.  Retail's 116 bytes, read this run with
`python3 tools/dol_read.py 0x801FD924 0x74`, are `9421fff0 7c0802a6 ... 38210010 4e800020`
(29 instructions) - the file's header carries the annotated listing.

Measured this run, byte for byte, against the two already-known relatives (`objdump -d` of
`build/G2ME01/main.elf`, which still held dtk's bytes at that address on the clean tree):

| relative | addr | size | differences from `fn_801FD924` |
|---|---|---|---|
| `fn_801FD67C` | 0x801FD67C | 0x74 | **one immediate**: `addi r0,r4,31740` where this one has `31728`, i.e. vtable `0x803B7BFC` vs `0x803B7BF0` |
| `fn_801FDAE8` | 0x801FDAE8 | 0x74 | the member offset (`addi r3,r30,24` vs `20`) **and** the vtable immediate (`31716` vs `31728`) |

All 29 instruction words match each relative except those; that is what let the body be written
without a matched sibling of the shape to copy.

**The element is 0x24 = 36 bytes and the two member offsets are retail's.**  The stride is measured
off retail's own walks that call `rstl::destroy` on this element: `fn_801FD890` (0x801FD890, 0x50,
unclaimed) at 0x801FD8B8 (`addi r31,r31,36`, `bl fn_801FD8E0` at 0x801FD8B4), and `fn_801FF7EC` in
the `Matching` `ScriptObjects/Carve801FF720.cpp`.  The layout inside the element, read off this
destructor's own bytes and off the class's copy constructor `fn_801FEAE0` (0x801FEAE0, 0x68,
unclaimed, disassembled this run):

- `+0x00` the vptr - `fn_801FEAE0` writes `lbl_803B7BCC` at +0x0 and then `lbl_803B7BF0` over it
  (`addi r0,r3,31728` at 0x801FEB10), which is where this destructor's constant comes from.
- `+0x04` `rstl::string mName`, 16 bytes (`mPtr`, `mCow`, `mSize`, the word-sized empty
  `rmemory_allocator`; `include/rstl/string.hpp:106-110`).  Sixteen, not twelve, is what puts the
  next member at +0x14 - the same measurement `docs/goal-notes/carve-801fdae8.md` records.
- `+0x14` a 16-byte member that `fn_801FD6F0` destroys (count at +4 of it, buffer at +0xC, read by
  `fn_801FD6F0`'s `lwz r0,4(r30)` / `lwz r5,12(r30)`); `fn_801FEAE0` copy-constructs the same
  offset (`addi r3,r30,20 / addi r4,r31,20 / bl fn_801FE8B8` at 0x801FEB20-0x801FEB28).

**The vtable is named, not written as its address.**  `extern "C" char lbl_803B7BF0[];` and
`self->mVTable = lbl_803B7BF0;` on a `void*` field is the spelling that puts
`R_PPC_ADDR16_HA` / `R_PPC_ADDR16_LO` into our object; the losing spellings (`= 0x803B7BF0`,
`= 0x803B7BF0u`) were measured on the sibling `fn_801FDAE8` in `docs/goal-notes/carve-801fdae8.md`
(same instruction words, green DOL, no matched function) and were not re-tried here - this unit
went straight to the winning spelling.  `lbl_803B7BF0` is `symbols.txt:18318`, `.data` `size:0xC`,
`{0, 0, &fn_801FF4B4}` (`objdump -s --start-address=0x803B7BF0 --stop-address=0x803B7BFC`), and its
accessor `fn_801FF4B4` is already ours, in the `Matching` `ScriptObjects/Carve801FF4A4.c`.

**The two dtor calls are spelled differently on purpose** (both measured in this tree's own units):
`self->mName.~basic_string();` is a *named* destructor call and produces retail's null test
(`addic. r0,r30,4 / beq`), while `fn_801FD6F0(&self->mMember, -1)` is a plain call on an address
with no test, which is what retail has.  The flag is a `short` (`extsh. r0,r31`).  The name after
the `~` must be the unqualified injected class name (`~basic_string()`, not `~rstl::string()`).

## Verification (all measured in this lane's tree, nothing recalled)

- The built DOL's 116 bytes at file offset 0x1FA724 are **byte-identical to the disc's** (python
  compare of `build/G2ME01/main.dol` against `orig/G2ME01/sys/main.dol`), and
  `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- Our object `build/G2ME01/obj/MetroidPrime/ScriptObjects/Carve801FD924.o`: `.text` 116, `data` 0,
  `bss` 0; every instruction matches the disc's except five relocation-carried words (the
  `lbl_803B7BF0` `@ha`/`@l` pair and the three `bl` sites).
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD924.cpp` -> `PASS -> kept as
  Matching`, `kept: 1 / 1   failed: 0   skipped: 0`.
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FD924.cpp` -> `.text claimed 116, ours
  116, retail 116, fits`; `no extra functions`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD924` -> `ok: 1
  unit(s) checked, none emits its functions out of retail order` (one function).
- `python3 tools/check_symbol_names.py` -> `checked 576 units; 0 declared names are missing`;
  `python3 tools/check_files_cmake.py` -> *every configured DOL object is either in files.cmake or
  excluded with a reason*; `python3 tools/check_raw_offsets.py` -> `ok: 170 raw-offset site(s) in
  74 file(s)` (unchanged - the new file has none).
- The gate's own report diff: `SPLIT main/auto_03_801FD924_text: 4 function(s) accounted for
  across 2 new unit(s) in main (exact count match - a split, not a loss)`; the baseline report's
  `main/auto_03_801FD924_text` (4 functions, 384 bytes) is now this unit (1 function, 116 bytes)
  plus `main/auto_03_801FD998_text` (3 functions, 268 bytes).

## The port side (the part the item's reason gets half wrong)

Both symbols this unit names are unclaimed, so the DOL takes them from dtk's own objects
(`auto_03_801FD67C_text.o` for `fn_801FD6F0`; `auto_07_803B7AE0_data.o`, `00000110 D
lbl_803B7BF0`, for the vtable) - but the port's link carries neither.  Measured both ways:

- with the unit in `files.cmake` and no new blocks: `python3 tools/link_gap.py --rebuild` prints
  `283  MISSING` and `gap grew: fn_801FD6F0 is not in port_link_gap_list.md` plus
  `gap grew: lbl_803B7BF0 is not in port_link_gap_list.md`;
- with `stub_227` and `stub_data_6` in place: `281  MISSING`, `ok: 281 MISSING symbol(s), all
  accounted for in port_link_gap_list.md` - the same 281 the tree carried before the unit existed;
- `./tools/link_check.sh --strict`: `unique undefined symbols 287` (baseline file: 287),
  `duplicate definitions 0`, `0 compile error(s)`, `STRICT PASS - regression gate: 287 undefined
  against a baseline of 287 (no growth)`.

**Correction to the item's reason, measured here.**  The item says "`stub_198` retires, `stub_200`
already exists, and a data stub for `lbl_803B7BF0` would be the only new cost".  On this tree
`stub_200` is `fn_801FEE88` (the 0x24-byte element's copy constructor, added for
`Carve801FEE40.c`) and **`fn_801FD6F0` had no stub at all** - `grep -n 'asm("fn_801FD6F0"'
src/MetroidPrime/PortLinkStubs.cpp` was empty before this change.  So the carve costs **two** new
stand-ins (`stub_227`, `stub_data_6`) against **one** retirement (`stub_198`), and the totals move
193 -> 194 with the function count flat at 187.  The item is right that the vtable data stub is a
new cost; it is wrong that the function stub was already paid for.

## Not done, deliberately

- `fn_801FD6F0` (0x801FD6F0, 0x84) is **not** claimed here: it is the +0x14 member's own deleting
  destructor, it is a separate unclaimed function in front of this claim, and it has its own queued
  item (`carve-801fd6f0`).  This claim stops at its own 0x74 bytes.
- `fn_801FD67C` (0x801FD67C, 0x74) is the twin and is also already queued (`carve-801fd67c`);
  nothing here reopens it.
- `fn_801FD998` (0x801FD998, 0x84) starts exactly where this claim ends and stays retail's - see
  the `NEW:` line.
- Note for the next reader: `src/MetroidPrime/ScriptObjects/Carve801FD8E0.c:86` declares
  `extern void fn_801FD924(void* self, int flag);` while this unit defines it with `short flag`.
  Both are C-linkage `fn_801FD924`, the call site passes the literal `-1`, and the DOL is
  byte-identical - do not "fix" the declaration, and do not move the definition.

NEW: carve-801fd998 | match | MetroidPrime/ScriptObjects/Carve801FD998 | `fn_801FD998` 0x801FD998 0x84 is the 0x2C-byte element's member destructor and is `fn_801FD6F0`'s body with one constant changed: 33 instruction words each, differing in exactly three (`mulli r0,r0,44` for `mulli r0,r0,20` at +0x30, and the two `bl Free__7CMemoryFPCv` displacements at +0x54/+0x64); the shape is the `Matching` `fn_801FBCAC` in `ScriptObjects/Carve801FBC58.c:150-165` (count at +4, buffer at +0xC, mulli by the stride, `bl` to the per-element helper, `Free(buffer)`, `flag > 0` then `Free(self)`), it is the next contiguous unclaimed function after `fn_801FD924`'s claim ends at 0x801FD998, and its only new port cost is one stand-in for its callee `fn_801FDA1C` (0x801FDA1C, 0x38, unclaimed) - nothing retires

## Notes on the notes file

Driver-assigned path `../wt-mp2-goal/build/goal/notes/carve-801fd924.md`, worktree
`../wt-mp2-goal-L10` (branch `goal/lane-10`).  Written to the driver's path because that is where
the driver reads notes from and where `carve-801fdae8.md` (the item that seeded this one) lives;
nothing in `wt-mp2-goal` other than this file was touched.  I did not commit.

No `WALL:`, no `STALE:`.  `tools/flip_test.sh` and `tools/unit_fit.sh` take the unit as
`configure.py` writes it (`MetroidPrime/ScriptObjects/Carve801FD924.cpp`) - passing `src/...`
prints `SKIP ... not listed in configure.py` and reads like a broken carve.  `tools/carve_diff.sh`
takes its retail side from `build/G2ME01/main.elf`, which holds *our* object once the unit is in
the link, so it is only usable before the flip; the comparison in this note was made against the
disc instead.
