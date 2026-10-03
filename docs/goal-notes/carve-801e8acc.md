# carve-801e8acc — `fn_801E8ACC` → new `Matching` unit `MetroidPrime/Cameras/Carve801E8ACC`

Carved the one seeded function out of the unclaimed dtk range `auto_03_801E8070_text`
(0x801E8070..0x801E8AEC) as its own plain-C `Matching` unit. The unit flips; the judge passes.

## What the range is

`config/G2ME01/symbols.txt:7881-7882`:

```
fn_801E8ACC = .text:0x801E8ACC; // type:function size:0x20 align:4
fn_801E8AEC = .text:0x801E8AEC; // type:function size:0x4 align:4
```

`build/G2ME01/asm/auto_03_801E8070_text.s:756-766` is the dtk disassembly of the 32 bytes the
item names — `bl AcceptScriptMsg__11CGameCameraFR13CStateManagerRC10CScriptMsg` and nothing else:

```
# .text:0xA5C | 0x801E8ACC | size: 0x20
.fn fn_801E8ACC, global
/* 801E8ACC 001E58CC  94 21 FF F0 */  stwu r1, -0x10(r1)
/* 801E8AD0 001E58D0  7C 08 02 A6 */  mflr r0
/* 801E8AD4 001E58D4  90 01 00 14 */  stw r0, 0x14(r1)
/* 801E8AD8 001E58D8  4B FC 83 E9 */  bl AcceptScriptMsg__11CGameCameraFR13CStateManagerRC10CScriptMsg
/* 801E8ADC 001E58DC  80 01 00 14 */  lwz r0, 0x14(r1)
/* 801E8AE0 001E58E0  7C 08 03 A6 */  mtlr r0
/* 801E8AE4 001E58E4  38 21 00 10 */  addi r1, r1, 0x10
/* 801E8AE8 001E58E8  4E 80 00 20 */  blr
```

There is **no `mr` anywhere in the range**, so r3, r4 and r5 reach the callee exactly as the caller
passed them: three arguments forwarded untouched, no load, no test, no return value.

## What it is, and how far the evidence goes

**Some `CGameCamera` subclass's `AcceptScriptMsg`, an override whose whole body is the call to the
base class's.** Both halves are measured:

- **The slot, from retail's own vtables.** `build/G2ME01/asm/auto_07_803B75A8_data.s:117-154` is
  `lbl_803B7708`, a 0x90-byte `.data` object (36 entries) in an unclaimed dtk data range, and
  **entry 6 of it is `fn_801E8ACC`**. Retail's own `__vt__11CGameCamera`
  (`build/G2ME01/asm/MetroidPrime/Cameras/CGameCamera.s:1573-1610`) is the same shape and its
  entry 6 is `AcceptScriptMsg__11CGameCameraFR13CStateManagerRC10CScriptMsg`. Counting the two
  nulls as entries 0 and 1, entries 2, 3, 4, 7, 8, 9, 10 and 12 of the two tables hold the **same
  names** (`__dt__`/`fn_801E95B4`, `TypesMatch`, `PreThink`, `SetActive__11CGameCameraFb`,
  `ClearFluidList__11CGameCameraFR13CStateManager`, `PreRender__6CActor`,
  `AddToRenderer__6CActor`, `CanRenderUnsorted__6CActor`) — that alignment is what pins the slot.
  Entry 5 differs (`Think__6CActorFfR13CStateManager` against `fn_801E8AF4`), i.e. the subclass
  overrides it, exactly as it overrides entry 6.
- **The class is NOT identified, and the file does not claim it is.** Retail names nothing in this
  vtable; `symbols.txt:18298` carries only
  `lbl_803B7708 = .data:0x803B7708; // type:object size:0x90`. Its destructor `fn_801E95B4` calls
  `__dt__11CGameCameraFv`, so the class derives from `CGameCamera` — that is as far as the
  evidence goes.

The callee is claimed already, and by a `Matching` unit: `MetroidPrime/Cameras/CGameCamera.cpp`
(`.text` 0x801B0500..0x801B1A18, `splits.txt:1206-1208`), so the matching build's link resolves the
`bl` out of that object. It is declared in the carve and never defined there.

## The twin — and a closer one than the item names

The item names `fn_80004438` (`src/MetroidPrime/Carve80004438.c`): the same eight instructions with
`bl fn_80004458` in place of the `bl`. Correct, and enough to justify the shape.

**A closer twin exists and is better evidence, because it is the same *kind* of function:**
`fn_8010EECC` (0x8010EECC, 0x20 = 32 bytes, `src/MetroidPrime/Carve8010EE5C.c:132`) is another
subclass's `AcceptScriptMsg` forwarding to its base's, and
`build/G2ME01/asm/MetroidPrime/Carve8010EE5C.s:41-50` is the identical instruction sequence —
including the absence of any register move — with
`bl AcceptScriptMsg__6CActorFR13CStateManagerRC10CScriptMsg` in place of this `bl`. Its source
declares all three parameters and forwards all three, which is the spelling reproduced here. A
two-parameter reading cannot compile to the same bytes: the callee takes `CStateManager&` and
`const CScriptMsg&`, and retail moved neither.

## The four files

| file | change |
| --- | --- |
| `src/MetroidPrime/Cameras/Carve801E8ACC.c` | new, 88 lines: header comment, the `extern` declaration, `void fn_801E8ACC(void*, void*, const void*)`, and a port-only stand-in for the callee |
| `config/G2ME01/splits.txt:1385-1386` | new entry `MetroidPrime/Cameras/Carve801E8ACC.c: .text start:0x801E8ACC end:0x801E8AEC`, between `Carve801E8028.c` and `Carve801E8AEC.c` (address order) |
| `configure.py:812` | `Object(Matching, "MetroidPrime/Cameras/Carve801E8ACC.c"),` on one line, immediately above the `Carve801E8AEC.c` entry |
| `files.cmake:826` | `src/MetroidPrime/Cameras/Carve801E8ACC.c` inserted above `src/MetroidPrime/ScriptObjects/Carve801E8AEC.c` |

Claim is **exactly** 0x801E8ACC..0x801E8AEC. Below it `fn_801E8874` (0x801E8874..0x801E8ACC) ends
where the claim begins and no unit claims it; above it `fn_801E8AEC` is already claimed by
`MetroidPrime/ScriptObjects/Carve801E8AEC.c`. The rest of `auto_03_801E8070_text` stays where the
seeder found it, so no claim spans a gap.

### Directory and the link-order question

The nearest claimed range below is `MetroidPrime/Cameras/Carve801E8028.c` (0x801E8028..0x801E8070),
the two-step `rstl::construct` chain for `CCameraShakerData`; above is the same camera
neighbourhood's `MetroidPrime/ScriptObjects/Carve801E8AEC.c`. The item's target says
`MetroidPrime/Cameras/` and that is where the file sits, so no judgement call was needed.

`docs/RUNNING_THE_DECOMP.md` §"The carve vein" records that a carve can create a `dtk dol split`
link-order cycle with a neighbouring pre-existing `Matching` unit. **This carve takes the tail of
the auto range** (0x801E8ACC..0x801E8AEC out of a range ending at 0x801E8AEC), so that range merely
shortens and is not split in two — the shape that produced the `CFrustumPlanes.cpp` cycle does not
apply. The build confirms it: the split and the link both succeeded on the first try.

### No `PortLinkStubs.cpp` duplicate

`grep -rn 'AcceptScriptMsg__11CGameCamera' src/ include/` returned nothing before this change, so
carve-vein gotcha 3 does not bite. The port-only stand-in for the unclaimed callee is kept **beside
the one reference that asks for it**, under `#ifndef __MWERKS__`, in the carve file itself — the
trade `src/MetroidPrime/Cameras/Carve801E8028.c:98-117` and
`src/MetroidPrime/Cameras/Carve801E7C14.c:120-141` make. `src/MetroidPrime/Cameras/CGameCamera.cpp`
is not in `files.cmake` (only `CGameCameraSetAspectRatio.cpp` and `CGameCameraGetPerspectiveMatrix.cpp`
are), so the host has no definition of it otherwise.

### Descending declaration order

One function, so descending order is vacuous —
`python3 tools/check_decl_order.py --unit MetroidPrime/Cameras/Carve801E8ACC.c` reports
`0 unit(s) checked, none emits its functions out of retail order`, i.e. the tool has nothing to
compare. Recorded so a later run does not read that "ok" as a check it performed; the `flip_test`
PASS below is what proves the `.text` is not permuted.

## Measured

```
$ ./tools/flip_test.sh MetroidPrime/Cameras/Carve801E8ACC.c
TEST MetroidPrime/Cameras/Carve801E8ACC.c
  PASS  -> kept as Matching

kept: 1 / 1   failed: 0   skipped: 0
```

`./tools/unit_fit.sh MetroidPrime/Cameras/Carve801E8ACC.c`: `.text claimed 32 ours 32 retail 32
fits`, `no extra functions`.

`build/report.json`, unit `main/MetroidPrime/Cameras/Carve801E8ACC`:
`fuzzy_match_percent 100.0`, `matched_code 32 / 32`, `matched_functions 1 / 1`,
`complete_units 1 / 1`, `total_functions 1`, `metadata.complete true`.

Whole-tree: **matched 13622 → 13623**, `linked` 6670 → 6671, `All:` 37.69% fuzzy / 31.13% matched
/ 14.00% linked (13623 / 28465 functions). `total_functions` stays **28465** — a function moved from
an unclaimed range into its own unit, so nothing was added or lost.

`sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, the pinned retail
hash, with this unit's object in the link.

`tools/carve_diff.sh` reports the one expected difference, the unlinked `bl` relocation
(`bl c <fn_801E8ACC+0xc>` against retail's resolved `bl 801b0ec0 <AcceptScriptMsg__...>`); the
other seven instructions are byte-identical and `flip_test` is what decides.

Port link (`build/probe-logs/link_check.log`): `unique undefined symbols 286`, `duplicate
definitions 0`, `STRICT PASS - regression gate: 286 undefined against a baseline of 287 (no
growth), 0 duplicate(s), 0 compile error(s), linker_ran=1`.

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item carve-801e8acc (match) target=MetroidPrime/Cameras/Carve801E8ACC
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13622 -> 13623   linked 6670 -> 6671
  ok    check_symbol_names.py
  ok    All:  37.69% fuzzy, 31.13% matched, 14.00% linked (13623 / 28465 functions)
  ok    flip_test MetroidPrime/Cameras/Carve801E8ACC.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801e8acc
```

`python3 tools/check_files_cmake.py`: every configured DOL object is either in `files.cmake` or
excluded with a reason; 0 on-disk sources in no manifest.

## Left alone deliberately

- `fn_801E8874` (0x801E8874..0x801E8ACC) stays in the dtk range — real work (the ball-camera
  position lerp), not a carve, and out of scope for a one-function item.
- The port-only stand-in drops every script message that reaches this override. That is announced
  in the source and unreachable anyway: the only thing that reaches it is `lbl_803B7708`, a dtk-only
  data object in an unclaimed range, so no instance of the class exists for the port to dispatch on.
- `AcceptScriptMsg__11CGameCameraFR13CStateManagerRC10CScriptMsg` itself is not claimed here; its own
  32 bytes belong to `MetroidPrime/Cameras/CGameCamera.cpp`, which already has them.

No blockers, no `WALL:`, no `STALE:`, no `NEW:`.