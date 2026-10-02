# carve-801fd5e8 (match) — `MetroidPrime/ScriptObjects/Carve801FD5E8`

`fn_801FD5E8` (0x801FD5E8, 0x50 = 80 bytes, 20 instructions, previously unclaimed and sitting in
dtk's `auto_03_801FA3CC_text`) is now a `Matching` unit of its own:
`src/MetroidPrime/ScriptObjects/Carve801FD5E8.c`, claiming exactly `.text:0x801FD5E8..0x801FD638`.

## What the function is

`rstl::destroy_impl<It, It>` for one element type, written out as C. Retail names it only the
`fn_<addr>` placeholder, so the shape is read off a twin, not off its own code.

**Twin: `fn_801FDC18`** (0x801FDC18, 0x50 = 80 bytes, `src/MetroidPrime/ScriptObjects/Carve801FDB5C.c`,
already `Matching`). Measured this run by disassembling both ranges of `build/G2ME01/main.elf` and
comparing decoded words - 20 instructions against 20 instructions, **two differ**:

| retail (ours) | twin | meaning |
| --- | --- | --- |
| `0x801FD60C bl 0x801FD638` | `0x801FDC3C bl 0x801FDC68` | the callee |
| `0x801FD610 addi r31,r31,36` | `0x801FDC40 addi r31,r31,48` | the element stride |

The other eighteen words are identical, including both branch displacements (`b 0x801FD614` /
`bne 0x801FD608` vs `b 0x801FDC44` / `bne 0x801FDC38` - same +0x2C and +0x20), which is what
pins the shape rather than merely its length.

**The `It` arguments are load-bearing and are why this is a struct, not `char*`.** MWCC passes a
struct parameter by reference and copies it into the callee's frame: that is why the body
dereferences r3 and r4, keeps `r30 = r4` and reloads `lwz r0,0(r30)` each iteration, and loads the
begin cursor once into `r31`. The plain-`char*` spelling of the same loop is
`fn_801FF66C` (0x801FF66C, 0x4C, `src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp`, also
`Matching`) and it differs in **six** words, all of it the cursor never being spilled
(`mr r31,r3` / `mr r30,r4` instead of `lwz r31,0(r3)` / `stw r30,8(r1)`, and `cmplw r31,r30`
instead of the reload). So `struct SCarve801FD5E8Iterator { void* current; }` passed by value is
what produces these bytes; it is not decoration.

**Element size 0x24 = 36 bytes**, measured twice off this very callee: this loop's
`addi r31,r31,36` at 0x801FD610, and `fn_801FF66C`'s `addi r31,r31,36` at 0x801FF694, which calls
the same `fn_801FD638`. Nothing is asserted about the class - the receiver never appears in this
body, so the pointer is `void*`.

**No stand-in was needed.** `fn_801FD638` (0x801FD638, 0x20) is defined for real by
`src/MetroidPrime/ScriptObjects/Carve801FD638.c` (landed by item `carve-801fd638`), so the
`bl` at 0x801FD60C lands on a real definition. Nothing in `src/MetroidPrime/PortLinkStubs.cpp`
stands in for either symbol and no stub was added or removed - the port's undefined count did not
rise (`probe: 810 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)`).

## The carve, four files in one change

- `src/MetroidPrime/ScriptObjects/Carve801FD5E8.c` - new, the source
- `config/G2ME01/splits.txt:1219-1220` - `.text start:0x801FD5E8 end:0x801FD638`
- `configure.py:759` - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD5E8.c")`
- `files.cmake:581` - the path

Each is in address order (in front of the `Carve801FD638.c` entries). One `Object(...)` per line.
`fn_801FD5E8` is the unit's only function, so the descending-declaration rule cannot be violated,
but `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD5E8` says
`ok: 1 unit(s) checked, none emits its functions out of retail order` so a later second function
in this unit starts from a verified order.

The claim is exactly the one function: `0x801FD5E8 + 0x50 = 0x801FD638`, which is exactly where
`Carve801FD638.c`'s claim starts, so no gap is spanned and nothing is split. `fn_801FD5B0`
(0x801FD5B0, 0x38, the retail caller at 0x801FD5D4) in front and `fn_801FD67C` (0x801FD67C, 0x74,
the element's real destructor) behind stay retail's.

## Measurements

```
./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FD5E8.c
   .text      claimed     80   ours     80   retail     80   fits
   no extra functions: our object defines only what the retail unit object does

./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD5E8.c
  PASS  -> kept as Matching
kept: 1 / 1   failed: 0   skipped: 0

sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010          (unchanged)
87 files OK
```

`build/report.json`, the new unit:

```
main/MetroidPrime/ScriptObjects/Carve801FD5E8
  fuzzy 100.0, matched_code 80/80, total_functions 1, matched_functions 1,
  complete_code 80/80, complete_units 1, metadata.complete true
```

Counts, against the judge's baseline `build/goal/judge/report.base.json`:

| | baseline | now |
| --- | --- | --- |
| `matched_functions` | 12685 | **12686** |
| `linked` (complete units) | 6044 | **6045** |
| `total_functions` | 28465 | **28465** |
| DOL complete units | 632 | 633 |

`./tools/goal_check.sh build/goal/item.json`:

```
goal_check: item carve-801fd5e8 (match) target=MetroidPrime/ScriptObjects/Carve801FD5E8
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12685 -> 12686   linked 6044 -> 6045
  ok    check_symbol_names.py
  ok    All:  35.54% fuzzy, 29.39% matched, 13.06% linked (12686 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FD5E8.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fd5e8
```

Also `./tools/probe_sources.sh`: `probe: 810 files, 0 failed, 0 errors; link: LINKED (291
undefined, 0 duplicates)`.

## Notes for the next run

- `tools/gate.sh` (run by `goal_check.sh` with `MP_GATE_DOCS_WRITE=1`) rewrites `docs/HANDOFF.md`
  and `docs/RUNNING_THE_DECOMP.md` as a side effect. The brief says a lane must not touch them, so
  they were reverted with `git checkout` after the judge ran; the final diff is the four carve files
  plus nothing else. Expect the same churn from any lane that runs `goal_check.sh`.
- The claim next in front of this one, `fn_801FD5B0` (0x801FD5B0, 0x38 = 56 bytes, 14
  instructions), is the retail caller of this function: `lwz r5,0(r4)` / `lwz r0,0(r3)` into its
  own frame at +0x8 and +0xc, `addi r3,r1,12` / `addi r4,r1,8`, then `bl 0x801FD5E8`. That is the
  `destroy(It,It)` half of the pair - the shape `fn_801FDBE0` (0x801FDBE0, 0x38, 14 instructions,
  in the same `Matching` twin file) already has, and it calls only `fn_801FD5E8`, which is now
  defined for real. Not tried here: this item's claim is one function and adding a second would
  mean reshaping a landed unit. Its stride is the *pointer* stride of the same 0x24 vector, so the
  two addresses would be adjacent claims in the same directory, not one claim - a claim may not
  span the unclaimed `fn_801FD52C` in between.
- `fn_801FD67C` (0x801FD67C, 0x74) remains the element's real destructor and stays unclaimed; its
  bytes need the `.data` vtable `lbl_803B7BFC` plus `fn_801FD6F0` (0x84) and `fn_801FD774`.
  `PortLinkStubs.cpp` already carries `stub_197` for it for the port's flat link.