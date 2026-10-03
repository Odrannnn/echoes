# fn_45_2BBC (match) - MysteryFlyer/MetroidPrime/ScriptObjects/CMysteryFlyerRelTail2

**Result: the unit exists, is `Matching`, and reproduces retail byte for byte.**
`tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS fn_45_2BBC`.

## What the item was

`fn_45_2BBC` (0x2BBC, 0x3C bytes) is the module's out-of-line
`rstl::optional_object<CAABox>` converting constructor, called by `fn_45_10` (0x10) in the module's
head. It was inside dtk's `MysteryFlyer/auto_00_00000170_text` (59 functions, 0 matched). The target
unit named in `item.json` did not exist, so this run carved it: four files, all four present in the
diff.

```
config/G2ME01/rels/MysteryFlyer/splits.txt   +3    MysteryFlyer/MetroidPrime/ScriptObjects/CMysteryFlyerRelTail2.cpp: .text 0x2BBC..0x2BF8
configure.py                                 +18   Object(Matching, "MysteryFlyer/MetroidPrime/.../CMysteryFlyerRelTail2.cpp", source="MetroidPrime/.../CMysteryFlyerRelTail2.cpp", mw_version="GC/2.7") in Rel("MysteryFlyer", ...)
files.cmake                                  +5    src/MetroidPrime/ScriptObjects/CMysteryFlyerRelTail2.cpp
src/MetroidPrime/ScriptObjects/CMysteryFlyerRelTail2.cpp   (new)
```

`.text 0x2BBC..0x2BF8` is one whole function and ends on the real boundary at 0x2BF8
(`fn_45_2BF8`), so no claim spans an unclaimed gap; `0x170..0x2BBC` and everything from 0x2BF8 up
stay unclaimed and dtk fills them from retail. One contiguous range, one file, as the recipe
requires.

## Measured

| check | result |
| --- | --- |
| `build/report.json`, the new unit | `MysteryFlyer/MysteryFlyer/MetroidPrime/ScriptObjects/CMysteryFlyerRelTail2`, 100.00% fuzzy, **1/1 matched**, `complete_units: 1` |
| `auto_00_00000170_text` | 59 -> 58 functions (the one moved into the new unit) |
| `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` |
| all 86 RELs vs `config.yml` | 86 checked, **0 mismatches** (`MysteryFlyer.rel` included) |
| `tools/unit_fit.sh MysteryFlyer/MetroidPrime/ScriptObjects/CMysteryFlyerRelTail2.cpp` | `.text claimed 60 ours 60 retail 60 fits`; no extra functions |
| `tools/check_symbol_names.py` | checked 585 units, 0 declared names missing |
| `tools/check_files_cmake.py` | every configured DOL object in files.cmake or excluded with a reason; 0 dead sources |
| `tools/check_module_wiring.py` | clean |
| `tools/flip_test.sh MysteryFlyer/MetroidPrime/ScriptObjects/CMysteryFlyerRelTail2.cpp` | **PASS**, `Object(Matching)` in configure.py |
| `tools/goal_check.sh build/goal/item.json` | **PASS** - counts `matched 13460 -> 13461`, `linked 6508 -> 6509` |

Our `.text` and the module's `obj/MetroidPrime/ScriptObjects/CMysteryFlyerRelTail2.o` are the same
60 bytes as retail's, which is what makes the module hash hold:
`38 00 00 01 | 80 A4 00 00 | 98 03 00 18 | 80 04 00 04 | 90 A3 00 00 | 80 A4 00 08 | 90 03 00 04 |
80 04 00 0C | 90 A3 00 08 | 80 A4 00 10 | 90 03 00 0C | 80 04 00 14 | 90 A3 00 10 | 90 03 00 14 |
4E 80 00 20`.

## The body, and the one thing that was hard: the compiler version

`rstl::optional_object`'s layout is mirrored in a local `COptionalAabox` (`CAABox m_value; bool
m_valid;`) because `CMysteryFlyerRel.cpp` must keep *declaring* `fn_45_2BBC` by that name and cannot
define it, and because its header records that instantiating the real template in that unit emits a
trailing pool and breaks the module's hash. `sizeof(CAABox) == 0x18`, so the flag lands at +0x18 -
retail's `stb r0, 0x18(r3)`. The body is two statements:

```cpp
out->m_valid = true;
out->m_value = box;
```

**The statement order is not what decides the bytes; the compiler version is.** Measured with the
module's own cflags, one change at a time:

| spelling | version | result |
| --- | --- | --- |
| `m_valid` first | GC/1.3, 1.3.2 | 15 instrs, `li r0,1` then `stb` in slot 1 - **not retail** |
| `m_valid` first | GC/2.0, 2.5, 2.6, **2.7** | 15 instrs, `li; lwz; stb;` - **retail, byte for byte** |
| `m_valid` first | GC/3.0a5.2 | all six loads then all seven stores - not retail |
| `m_value` first | GC/1.3.2 | the `stb` in the last slot, and the copy is memberwise (two `CVector3f`, four loads into r6/r5/r4) instead of retail's two alternating temporaries |

So `mw_version="GC/2.7"`, the per-object override `CLumiteRelTail.cpp` and `CSandBossRelTail.cpp`
already use. **This is a scheduling difference between two builds of the compiler family, not a
missing statement: no spelling of the two statements reaches 1.3.2's order.** The same class of thing
`configure.py` records for `CGameOptions.cpp`.

Spellings tried and rejected, so the next run does not repeat them:

- a POD stand-in for the value (`float m_min[3]; float m_max[3];`) - **no change** to the bytes, so
  the 24-byte copy is not the version-sensitive part; only where the flag store lands is.
- `memcpy(&out->m_value, &box, sizeof(CAABox))` - MWCC emits a **call** to `memcpy`, with a frame:
  14 and 11 instructions at the module's default. Not usable.
- a placement-new copy (`COptionalAabox(const CAABox&) : m_value(box), m_valid(true)`) - the copy
  **constructor** goes through `lfs`/`stfs` and a `cmplwi r3,0 / beqlr` null guard: 17 instructions
  with `stb` last. Not retail.
- `new (out) CAABox(box)` alone (the `rstl::construct<CAABox>` spelling) - same `lfs`/`stfs` copy.
  `CMysteryFlyerRel.cpp`'s header is right that the head cannot instantiate the template.

## Two judge defects hit on the way, and the one workaround

Both are pre-existing and both are recorded here because they will bite the next REL `match` item.

1. **`tools/goal_check.sh` resolves a `match` target by string in `configure.py`.** The queue names a
   REL unit the way `build/report.json` does - `<Module>/<path>` - while `Rel(...)` lists module-
   relative `Object(...)` paths. First run:
   `FAIL match target MysteryFlyer/MetroidPrime/ScriptObjects/CMysteryFlyerRelTail2 has no
   Object(...) entry in configure.py`. Fixed the way `CIngBoostBallGuardian3790.cpp`, `-388C.cpp`
   and `-10B90.cpp` do it: the unit name in `splits.txt` and `configure.py` carries the `MysteryFlyer/`
   prefix and `source=` keeps the file at `src/MetroidPrime/ScriptObjects/...`. Consequence worth
   knowing: `report.json` then names the unit `MysteryFlyer/MysteryFlyer/MetroidPrime/...`
   (double prefix) - same as those three landed units.
2. **`tools/flip_test.sh`'s `unit_info` guesses a source root by counting parentheses** from the last
   `MusyX(` in `configure.py` up to the entry, and an entry inside a `Rel(...)` list reads one open
   paren too many, so it looks for the source under `extern/musyx/src`. It reported FAIL for the
   already-landed `MetroidPrime/ScriptObjects/CLumiteRelTail.cpp` too, so this is not about this unit.
   The workaround lane 4 already landed is **one stray `)` in a comment inside the `Rel` list**,
   documented in place in `configure.py` above the entry. I used it, with the same comment. The real
   fix belongs in `unit_info` (`Rel(...)` is not `MusyX(...)`); `tools/` is not mine to edit.

Note for the driver: `target_rose` (the PARTIAL path) cannot work for a **newly carved** unit at all -
it requires exactly one unit with the target's name in `report.base.json`, and a carve has none at
the head. So a REL `match` item on a brand-new unit has to pass the flip outright; there is no
partial fallback. This one does.

## Left alone, deliberately

- **`docs/HANDOFF.md` / `docs/RUNNING_THE_DECOMP.md`**: `gate.sh` rewrote their derived counts when I
  ran the judge; both were reverted with `git checkout --` so the diff is only the four carve files.
  The driver rewrites them from the tree anyway.
- **`config/G2ME01/config.yml`**: untouched. No `force_active:` entry was needed - `fn_45_2BBC` is
  not in `ldscript.lcf`'s FORCEACTIVE list, but `CMysteryFlyerRel.o`'s `bl fn_45_2BBC` holds the
  reference, so nothing is dead-stripped (the 0x3C is in the linked `.rel`, and the hash proves it).
- **`config/G2ME01/rels/MysteryFlyer/symbols.txt`**: no rename needed; the range's name is already
  `fn_45_2BBC`, `type:function`, `size:0x3C`.
- **No `NEW:` line.** The rest of module 45 is already covered by the queued
  `progress-twin-rel-mysteryflyer`, so filing a new item would duplicate it.