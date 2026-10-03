# carve-8028b8bc - `Collision/Carve8028B8BC` is `Matching`, 2/2, flip PASS

`kind: match`, `target: Collision/Carve8028B8BC`. **Done.** No blocker, no `NEW:` line.

## What landed

Carve of retail `.text` **0x8028B8BC..0x8028B914** (0x58 = 88 bytes, 2 functions) out of dtk's
unclaimed `auto_03_8028B780_text`, as a new `Matching` unit in four files:

| file | what |
| --- | --- |
| `src/Collision/Carve8028B8BC.c` | the source (new), header comment in the style of `src/Dolphin/Carve8038A7DC.c` / `src/Collision/Carve8028B728.c` |
| `configure.py:449` | `Object(Matching, "Collision/Carve8028B8BC.c"),` immediately after `Carve8028B728.c` |
| `config/G2ME01/splits.txt:2161-2162` | `Collision/Carve8028B8BC.c:` / `.text start:0x8028B8BC end:0x8028B914`, in address order |
| `files.cmake:871` | `src/Collision/Carve8028B8BC.c` immediately after `Carve8028B728.c` |

Plus one necessary supporting change: `src/MetroidPrime/PortLinkStubs.cpp` gained the announced
stand-in **`stub_8028b8bc_0` -> `fn_8028B914`** (the carve's only callee), next to the identical
`stub_8028b728_0`. Without it the port link's undefined count would have risen by one and
`link_check.sh --strict` would have failed the gate. It is an empty body, it is announced as one,
and it does **not** claim 0x8028B914 is decompiled.

The claim is exactly this range and nothing else, so it does not span an unclaimed gap: it sits
inside the 0x8028B780..0x8028BC64 hole, with `Collision/Carve8028B728.c` (0x8028B728..0x8028B780)
below it and `Kyoto/Basics/CStopwatch.cpp` (0x8028BC64..) above - no adjacency to an existing unit
boundary, so no `dtk dol split` link-order cycle.

## The two functions, and why the twins are twins

`config/G2ME01/symbols.txt:11393-11394`:

```
fn_8028B8BC = .text:0x8028B8BC; // type:function size:0x38
fn_8028B8F4 = .text:0x8028B8F4; // type:function size:0x20
```

**`fn_8028B8BC`** (0x38, 13 instructions) is
`push_back_unsafe__Q24rstl49vector<12SAreaSurface,Q24rstl17rmemory_allocator>FRC12SAreaSurface`
at 0x8005C148 (`build/G2ME01/asm/MetroidPrime/CGameArea.s:10436-10451`, `Matching`) instruction for
instruction - same `lwz r6,0x4(r3)` / `lwz r7,0xc(r3)` / `addi r5,r6,1` / `slwi r0,r6,5` /
`stw r5,0x4(r3)` / `add r3,r7,r0` / `bl`, and the template is `include/rstl/vector.hpp:95`.
Only the `bl` target differs. The element size `slwi r0, r6, 5` = 0x20 is **measured**, not assumed:
the other caller of `fn_8028B8F4`, the loop at 0x8028BA40 in `fn_8028BA18`, steps both cursors with
`addi r31,r31,0x20` / `addi r30,r30,0x20`. (The 0x50-byte sibling two carves down,
`Carve8028B728.c`'s `fn_8028B728`, is the same body with a different element size and a
`mulli` instead of the shift.)

**`fn_8028B8F4`** (0x20, 8 instructions) is a frame and one unconditional `bl fn_8028B914`, which is
`rstl::construct<T>` as `include/rstl/construct.hpp:73-75` spells it - so it is written forward to
its callee, exactly as `Carve8028B728.c`'s `fn_8028B760` is. The seed's own twin
`fn_80004438` (`src/MetroidPrime/Carve80004438.c`, `Matching`) is the same eight instructions with a
different `bl`; there it is `rstl::destroy<T>`, which is why the shape alone does not name the
template - the callee's own bytes decide, and they are `construct`.

**`fn_8028B914`** (0x4C, above the claim) is declared, never defined. What it does, read off its
bytes: return early if the destination is null, else copy bytes `+0x00`/`+0x01`, words `+0x08`/`+0x0C`
and the four floats `+0x10..+0x1C` - the fields `fn_8028B7FC` fills - skipping `+0x04`.

## Verification, measured

```
$ ./tools/decomp_build.sh Collision/Carve8028B8BC.c
[6/9] CHECK config/G2ME01/build.sha1
87 files OK
All:  37.56% fuzzy, 31.00% matched, 13.87% linked (13491 / 28465 functions)

$ ./tools/unit_fit.sh Collision/Carve8028B8BC.c
   .text      claimed     88   ours     88   retail     88   fits
   no extra functions: our object defines only what the retail unit object does

$ ./tools/flip_test.sh Collision/Carve8028B8BC.c
  PASS  -> kept as Matching
kept: 1 / 1   failed: 0   skipped: 0

$ ./tools/goal_check.sh build/goal/item.json
goal_check: item carve-8028b8bc (match) target=Collision/Carve8028B8BC
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13489 -> 13491   linked 6537 -> 6539
  ok    check_symbol_names.py
  ok    All:  37.56% fuzzy, 31.00% matched, 13.87% linked (13491 / 28465 functions)
  ok    flip_test Collision/Carve8028B8BC.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8028b8bc
```

`build/report.json`, the new unit: `total_code 88`, `matched_code 88`, `100.0%`,
`matched_functions 2 / total_functions 2`, `complete_units 1`, `complete: true`.

Other gates, quoted from what they printed:

- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
- `tools/probe_sources.sh` -> `probe: 925 files, 0 failed, 0 errors; link: LINKED
  (286 undefined, 0 duplicates)` - up one file from 924, undefined unmoved, so the new stub did its
  job and no duplicate appeared.
- `link_check: duplicate definitions 0`
- `total_functions` still **28465** after the `splits.txt` edit.
- `python3 tools/check_decl_order.py` -> `ok: 1194 unit(s) checked, 37 permuted, all 37 accounted
  for in decl_order.md` (the 37 are pre-existing and listed; the new unit is not among them).
  Note the tool ignores `--unit <name>` on this tree and always checks all units.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show in `git status` because `gate.sh` runs with
`MP_GATE_DOCS_WRITE=1` and rewrote their derived counts (matched 13489->13491, linked 6537->6539,
DOL units 11556->11558, probe 924->925). Those are the gate's own output, not mine; the driver
discards them.

## Notes worth keeping

- **The `asm("...")` in the new stub is the file's established shape, not new assembly.** The
  `no asm added` check is only run for `progress` items, and this is a `match` item that passed
  outright; but for the record, `extern "C" void stub_8028b8bc_0() asm("fn_8028B914");` is the same
  label form as `stub_8028b728_0` above it and as ~190 other lines in that file. It names retail's
  symbol; the body is C.
- **The header's stub counts were re-derived, and one inherited claim in that file is wrong.** The
  top block of `PortLinkStubs.cpp` has been claiming that the term
  `^extern "C" void stub_[A-Za-z0-9_]*\(\) asm\(` *misses* the `_0`-suffixed stubs. It does not -
  `[A-Za-z0-9_]*` matches `_` - so `stub_801e515c_0`, `stub_80004438_0` and `stub_8028b728_0` were
  already inside its 191 before this run. Measured on this tree after the edit:
  `grep -c 'asm("'` **202**, that term **192**, `^extern "C" char stub_data_` **10**. I recorded the
  real numbers and noted the correction in place rather than carrying the old prose forward.
- `check_decl_order.py --unit <yours>` reports `0 unit(s) checked` on this tree; the whole-tree run
  is what actually covers the new file. `python3 tools/check_files_cmake.py` is green with the new
  entry ("every configured DOL object is either in files.cmake or excluded with a reason").

## For the next run

Nothing is blocked here. The obvious next carves in this same hole, all still inside
`auto_03_8028B780_text`, each a self-contained run with its own twin to read:

- `fn_8028B780` (0x8028B780, 0x7C) - `rstl::construct_impl` for the 0x50-byte element; null-receiver
  early return, two bytes, two words, `CTransform4f` ctor at +0x10, three floats at
  +0x40/+0x44/+0x48. Its stand-in already exists (`stub_8028b728_0`), so this one needs none.
- `fn_8028B914` (0x8028B914, 0x4C) - this carve's callee; its stand-in `stub_8028b8bc_0` already
  exists, so a carve of it would retire that stub and lower the file's count again.
- `fn_8028B7FC` (0x8028B7FC, 0xC0) / `fn_8028B960` (0xB8) / `fn_8028BA18` (0x68) - the vector
  ctor, `reserve` and the 0x20-stride copy loop that already cites `fn_8028B8F4`.
- `fn_8028BA80` (0xB8) / `fn_8028BB38` (0x68) are the 0x50-byte twin pair of the same two shapes
  (`Carve8028B728.c` claims the `push_back_unsafe`/`construct` halves at 0x8028B728/0x8028B760).