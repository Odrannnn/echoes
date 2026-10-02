# carve-801fd4b0 - `MetroidPrime/ScriptObjects/Carve801FD4B0` (kind `match`)

**Result: the unit is `Matching` and the flip passes.** `tools/goal_check.sh build/goal/item.json`
prints `goal_check: PASS carve-801fd4b0`, exit 0, run twice (once before the last comment-only
edits to `PortLinkStubs.cpp` and `Carve801FD4B0.cpp`, once after).

## The four-file carve

- `src/MetroidPrime/ScriptObjects/Carve801FD4B0.cpp` - new, `extern "C" void* fn_801FD4B0(...)`.
- `config/G2ME01/splits.txt:1365` - `MetroidPrime/ScriptObjects/Carve801FD4B0.cpp:` /
  `.text start:0x801FD4B0 end:0x801FD52C` (0x7C = 124 bytes), inserted between
  `Carve801FBC58.c` and `Carve801FD5E8.c`.
- `configure.py:771` - `Object(NonMatching, ...)` added, `flip_test.sh` promoted it to
  `Object(Matching, ...)` and kept it there.
- `files.cmake:578` - the source listed.
- Plus `src/MetroidPrime/PortLinkStubs.cpp` - four stand-ins for the unit's unclaimed callees
  (`stub_228`..`stub_231`), which is not part of the four-file rule but is required for the port
  link.

Claim boundaries, both verified against `symbols.txt` and dtk's own
`build/G2ME01/asm/auto_03_801FBD68_text.s` (`:1707`, `:1750`, `:1786`, `:1824`):

- below: `fn_801FD420` (0x801FD420, 0x90) ends exactly at 0x801FD4B0 and is unclaimed;
- above: `fn_801FD52C` (0x801FD52C, 0x84) starts exactly at 0x801FD52C and is unclaimed, and
  `fn_801FD5B0` (0x801FD5B0, 0x38) sits between it and the next claimed range
  (`Carve801FD5E8.c`, 0x801FD5E8) - so the claim could not extend past 0x801FD52C anyway.

`total_functions` in `build/report.json` is **28465** before and after, as it must be.

## Measurements

```
./tools/decomp_build.sh MetroidPrime/ScriptObjects/Carve801FD4B0.cpp
  All:  36.98% fuzzy, 30.42% matched, 13.41% linked (13047 / 28465 functions)
build/report.json  main/MetroidPrime/ScriptObjects/Carve801FD4B0
  fuzzy 100.0, total_code 124, matched_code 124, total_functions 1, matched_functions 1
build/gate-diff.log
  matched 13046 -> 13047   linked 6149 -> 6150   (+1 functions at 100%, 1 units newly linked)
  LINKED   main/MetroidPrime/ScriptObjects/Carve801FD4B0
  +100%    ... :: fn_801FD4B0
  SPLIT    main/auto_03_801FBD68_text: 3 function(s) moved into main/MetroidPrime/ScriptObjects/Carve801FD4B0,
           main/auto_03_801FD52C_text (exact count match - a split, not a loss)
./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FD4B0.cpp
  .text claimed 124  ours 124  retail 124  fits
  no extra functions: our object defines only what the retail unit object does
./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD4B0.cpp
  PASS  -> kept as Matching        kept: 1 / 1   failed: 0   skipped: 0
./tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FD4B0
  ok: 1 unit(s) checked, none emits its functions out of retail order
```

Port link, `python3 tools/link_gap.py --rebuild` in this tree:

```
with stub_228..stub_231 in place:   280  MISSING   ok: all accounted for in port_link_gap_list.md
with all four reverted away:        284  MISSING
  gap grew: fn_801FD52C is not in port_link_gap_list.md
  gap grew: fn_801FD7D4 is not in port_link_gap_list.md
  gap grew: fn_801FD998 is not in port_link_gap_list.md
  gap grew: fn_801FDB5C is not in port_link_gap_list.md
```

So the carve costs the port link nothing, and unlike the three sibling carves next door it needs
**no data stub**: `fn_801FD4B0` stores nothing into its receiver, so it names no vtable.

## The spelling, and what the item's `reason` got wrong

The body is the shape `Carve801FD924.cpp` / `Carve801FDAE8.cpp` / `Carve801FD67C.cpp` already match
100%, with three differences: four teardowns instead of one, no vptr store, and **no named
destructor call** - retail has no null test before any of the four `bl`s, so all four are plain
calls on `&self->xNN` (`fn_801FD6F0`'s case in those files, not `~basic_string()`'s). The fourth is
`fn_801FD52C(&self->x00, -1)` and `&self->x00 == self`, which is why retail emits `mr r3,r30` at
0x801FD4F4 rather than an `addi`.

**Correction worth keeping: the item's reason says `fn_801FDB5C` "is already ours
(`Carve801FDB5C.c`, `Matching`)". It is not.** `config/G2ME01/splits.txt` has
`Carve801FDB5C.c` claiming **0x801FDBE0..0x801FDC88** - three functions, `fn_801FDBE0`,
`fn_801FDC18`, `fn_801FDC68` - and that unit's own header says the claim *starts* at 0x801FDBE0
"rather than at 0x801FDB5C because the 132-byte function in front of it, `fn_801FDB5C`, is not
matched by any spelling measured so far". So the real cost of this carve is **four** stand-ins,
not two: `fn_801FDB5C` (0x801FDB5C, 0x84), `fn_801FD998` (0x801FD998, 0x84), `fn_801FD7D4`
(0x801FD7D4, 0x84) and `fn_801FD52C` (0x801FD52C, 0x84). `fn_801FD998` is also unclaimed, so the
reason's "is ours as of this item" is wrong too.

All four callees are the same function three times over with one `mulli` immediate changed - read
the count at +4 and the buffer at +0xC of the receiver, walk `count` elements of that stride
through an inner `fn_`, `Free__7CMemoryFPCv(x0c_buffer)`, then free their own receiver behind
`extsh. r0,r31 / ble`. Strides and inner walks, measured this run:

```
fn_801FDB5C  0x801FDB5C  0x84  mulli 48=0x30 @0x801FDB8C  bl fn_801FDBE0 @0x801FDBA8
fn_801FD998  0x801FD998  0x84  mulli 44=0x2C @0x801FD9C8  bl fn_801FDA1C @0x801FD9E4
fn_801FD7D4  0x801FD7D4  0x84  mulli 36=0x24 @0x801FD804  bl fn_801FD858 @0x801FD820
fn_801FD52C  0x801FD52C  0x84  mulli 36=0x24 @0x801FD55C  bl fn_801FD5B0 @0x801FD578
```

## Layout, and what is *not* asserted

This destructor addresses four 0x10-byte containers at +0x00, +0x10, +0x20 and +0x30, so
`SCarve801FD4B0Element` spells exactly those four and nothing more. **The element's own stride is
not measured by anything in the file and is not claimed** - nothing here reads a member's words,
and the only caller, `fn_801FD420`, hands it a pointer loaded from +4 (`lwz r3,4(r30)`) and passes
`li r4,1`, so it gives no stride either. The strides in the table above name four *other*
classes' element sizes (0x30 = `Carve801FDB5C.c`, 0x2C = `Carve801FDAA4.c`, 0x24 =
`Carve801FD924.cpp`, all `Matching`), which is why the file says so and stops there.

## Blocked

Nothing. One NEW: below, for the function immediately above this claim.

NEW: carve-801fd52c | match | MetroidPrime/ScriptObjects/Carve801FD52C | carve `fn_801FD52C` (0x801FD52C..0x801FD5B0, 0x84 = 132 bytes, `symbols.txt:8268`), the unclaimed function immediately above `Carve801FD4B0.cpp`'s claim and one of the four 0x84-byte container destructors - 33 instructions, `mulli r0,r0,36` at +0x30, hands the walk to `fn_801FD5B0` and frees the buffer then its own receiver; it needs `fn_801FD5B0` (0x801FD5B0, 0x38) as its only stand-in, and with `fn_801FD5B0` claimed too the unit spans 0x801FD52C..0x801FD5E8 straight up to the `Carve801FD5E8.c` claim