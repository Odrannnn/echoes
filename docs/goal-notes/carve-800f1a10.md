# carve-800f1a10 — `MetroidPrime/Carve800F1A10`, one function, `Matching`

`kind: match`. Carved 0x800F1A10..0x800F1A18 (0x8 = 8 bytes, 1 function) out of the unsourced
dtk range `main/auto_03_800F15CC_text` and landed it as its own `Matching` unit. No twin: this
one is 2 instructions with no call target and no data address, so the bytes are the whole of
the specification.

## Result, measured

| | before | after |
| --- | --- | --- |
| `matched` | 12490 | **12491** (+1) |
| `linked` (unit `Matching` + has source) | 5868 | **5869** (+1) |
| `All:` | 35.31% fuzzy, 29.12% matched, 12.91% linked (12490 / 28465) | **35.31% fuzzy, 29.12% matched, 12.91% linked** (12491 / 28465) |

- `main/MetroidPrime/Carve800F1A10`: **100.00% fuzzy, 100.00% matched, 1 / 1 function**,
  `total_code 8`, `complete_code 8`, `complete_units 1`.
- `./tools/flip_test.sh MetroidPrime/Carve800F1A10.c` → `PASS  -> kept as Matching`,
  `kept: 1 / 1   failed: 0   skipped: 0`.
- `./tools/goal_check.sh build/goal/item.json` → `goal_check: PASS carve-800f1a10`
  (no judge-owned path touched; gate.sh green; counts 12490 → 12491 matched, 5868 → 5869
  linked; `check_symbol_names.py` ok; flip_test PASS).
- `sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged.
  All 86 RELs still `cmp`-equal (`87 files OK` at the `CHECK config/G2ME01/build.sha1` step).
- `tools/unit_fit.sh MetroidPrime/Carve800F1A10.c` → `.text claimed 8  ours 8  retail 8
  fits`, `no extra functions: our object defines only what the retail unit object does`.
- `tools/carve_diff.sh 800F1A10 8 build/G2ME01/obj/MetroidPrime/Carve800F1A10.o fn_800F1A10`
  → `retail: 2 instructions, 8 bytes` / `ours: 2 instructions, 8 bytes`,
  `differing instructions: 0`, **`BYTE-EXACT`**. (Byte-exactness is the only verdict
  `carve_diff` can give here, and the acceptance test is still `flip_test.sh`.)
- `python3 tools/check_symbol_names.py` → `checked 525 units; 0 declared names are missing`.
- `python3 tools/check_raw_offsets.py` → `ok: 167 raw-offset site(s) in 71 file(s)` —
  unchanged from the head (167 / 71), because the member is reached through the `void*` the
  caller passes in r3, not through a typed class.
- `python3 tools/check_files_cmake.py` → `files.cmake: 752 sources; configure.py declares 648
  DOL objects`, `every configured DOL object is either in files.cmake or excluded with a reason`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/Carve800F1A10.c` → `ok: 0 unit(s)
  checked, none emits its functions out of retail order` (one function, so the rule is
  vacuous; the file still states descending order in its header).
- `main/auto_03_800F15CC_text` went 4 → **3** functions (still 0 matched):
  3 + 1 = 4, so no function was lost. The split landed inside the existing unclaimed range
  without dtk needing a new `auto_*` unit, because the neighbouring claim below
  (`MetroidPrime/Carve800F1A18.c`, 0x800F1A18..0x800F1A24) was already in place.
- `total_functions` is still **28465** after the `splits.txt` edit (measured in
  `build/report.json`), as the carve rules require.

## The four files

| file | line | entry |
| --- | --- | --- |
| `src/MetroidPrime/Carve800F1A10.c` | 53 | `unsigned char fn_800F1A10(void* self) { return *(unsigned char*)((unsigned char*)self + 8); }` |
| `config/G2ME01/splits.txt` | 493-494 | `MetroidPrime/Carve800F1A10.c:` / `.text start:0x800F1A10 end:0x800F1A18` |
| `configure.py` | 670 | `Object(Matching, "MetroidPrime/Carve800F1A10.c"),` (one `Object` per line) |
| `files.cmake` | 441 | `    src/MetroidPrime/Carve800F1A10.c` |

All four are in address order: the entry sits between `Carve800F15C8.c` (0x800F15C8) and
`Carve800F1A18.c` (0x800F1A18) in each of the three manifests, and 0x800F1A10..0x800F1A18 is
contiguous and bounded on both sides by claimed ranges, so the claim spans no unclaimed gap.

`grep -rn "fn_800F1A10" src/ include/` returns nothing else, so there is no
`PortLinkStubs.cpp` duplicate to delete (this is the carve rule 3 failure mode; `gate.sh`'s
`port link dups` step is green, 0 duplicates).

## What the body is, and how it was decided

```
# .text:0x444 | 0x800F1A10 | size: 0x8
.fn fn_800F1A10, global
/* 800F1A10 */  88 63 00 08   lbz   r3, 0x8(r3)
/* 800F1A14 */  4E 80 00 20   blr
```

- The C is taken from the `MetroidPrime/Carve80193C84.c` shape, which is the same two-
  instruction accessor with a different load width: `+0x8` out of a `void*`, no class, no
  `#include`, nothing assumed beyond the pointer the caller passed in r3.
- **Why a byte.** Retail's load is `lbz`, so the returned member is a byte at `+0x8`, and the
  neighbours in the same neighbourhood write it with `stb`: `fn_800F1A24` at 0x800F1A68
  (`stb r0, 0x8(r31)`, `r31` its own `this`, `r0 = 1`), `fn_800F1A84` at 0x800F1C00
  (`stb r0, 0x8(r31)`, `r0 = 0`), and `__ct__6CBSDieFv` at 0x800F1C3C (`stb r0, 0x8(r3)`)
  in an object that is only 0xC bytes long. `bool` and `int` returns compile to the identical
  two instructions here — `lbz` already zero-extends into all of r3 — so the C says
  `unsigned char` to match the load retail performs rather than to claim a type the bytes
  cannot distinguish. The header says so explicitly.
- **No caller, no callee, no data.** `build/binutils/powerpc-eabi-objdump -d
  build/G2ME01/main.elf | grep 800f1a10` prints the definition and nothing else — the symbol
  occurs once, with no `bl` to it anywhere in the DOL. So the load displacement is the whole
  of the evidence for the member offset, which is also the whole of the body. That is what a
  carve is allowed to rest on, because the bytes are the result.
- **Not a virtual.** The `CBSAttack` vtable `lbl_803B3D60` (`.data` 0x803B3D60, dumped with
  `powerpc-eabi-objdump -s -j .data`) holds 800f0f84, 800f124c, 800f1254, 800f125c, 800f1264,
  800f126c, 800f0198, 800766cc, 800f1284, 800f127c, 800f16e0, 800f15cc, 800f15c8 — 800f1a10 is
  not in it. So this is a non-virtual member accessor and needs no vtable claim, which is why
  the unit claims `.text` and nothing else.
- **Directory.** `MetroidPrime/`, taken from the nearest claimed range: the claim immediately
  below is `MetroidPrime/Carve800F15C8.c` (0x800F15C8..0x800F15CC) and the nearest non-carve
  claim below that is `MetroidPrime/BodyState/CBodyStateInfo.cpp` (`.text`
  0x800EF908..0x800F128C), so this address is 0x784 bytes past the end of that unit.
  Note: the neighbouring `src/MetroidPrime/Carve800F1A18.c` header says this same region is
  "0x7DFC bytes into `MetroidPrime/CPhysicsActor.cpp`", which is wrong —
  `MetroidPrime/CPhysicsActor.cpp` claims 0x800E9C1C..0x800EBD40 and does not reach here.
  Correcting it there means editing another item's file, so it is left alone; it is a
  documentation fix, which is not a `NEW:` item.

## Nothing else

- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`, but
  that is `tools/goal_check.sh` rewriting the derived counts (12491 / 5869, probe 759) from
  the tree. I did not touch them; the driver discards those edits before judging.
- No `.s` / assembly was added. No initialisation was deleted. No other unit's `.text` moved:
  the only shared file this changes is `splits.txt`, and the new range is bounded by two
  already-claimed neighbours, so dtk re-splits only inside `auto_03_800F15CC_text`.

