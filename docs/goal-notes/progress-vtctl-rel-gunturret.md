# progress-vtctl-rel-gunturret — module GunTurret (28), 5 new matched functions

**Result: PASS.** `module:GunTurret`'s summed `matched_functions` went **19 -> 24 / 170**, and the
project total went **13345 -> 13350** with `linked` **6393 -> 6398**. Two new `Matching` units, five
functions, both verified by `tools/flip_test.sh`, and the module's sha1 still equals
`config/G2ME01/config.yml`.

## What landed

Two carves, four files each (configure.py, splits.txt, files.cmake, the source's own claim in its
header comment):

| unit | claimed `.text` | functions |
| --- | --- | --- |
| `MetroidPrime/ScriptObjects/CGunTurretBaseForwarders.cpp` | `0x0008B58..0x0008B98` | `fn_28_8B58`, `fn_28_8B78` |
| `MetroidPrime/ScriptObjects/CGunTurretBaseTriggers.cpp` | `0x0009028..0x0009058` | `fn_28_9028`, `fn_28_9048`, `fn_28_9050` |

Both are the class's vtable slots, and all five bodies are bare forwarders or two-instruction byte
accessors:

- `fn_28_8B58` -> `CPatterned::Render`, `fn_28_8B78` -> `CPatterned::PreRender`,
  `fn_28_9028` -> `CPatterned::Attacked` - the `stwu/mflr/stw/bl/lwz/mtlr/addi/blr` shape with
  `this`/the manager/the trigger data already in r3/r4/r5 and nothing added to the result.
- `fn_28_9048` reads the byte at `+0x808`, `fn_28_9050` the byte at `+0x7E8`.

The class is `CGunTurretBase` by the module's own mangled `"TCastToPtr<14CGunTurretBase>__FP7CEntity"`
(`fn_28_856C`, `fn_28_90C4`), but no header here declares it, and **no class is declared in either
unit on purpose**: a class with virtuals makes mwcceppc emit a vtable into the object, and a
`.data` the unit does not claim moves the module. So the bodies are ordinary C++ over the real
`CPatterned*` and nothing is claimed about the derived layout.

## Measured

- `tools/flip_test.sh` on both units: `kept: 2 / 2`, `PASS`, `Object(Matching)` in place.
- `build/report.json`: both new units `2/2` and `3/3` functions matched, `matched_code_percent`
  100.0, `metadata.complete` true. Module total 24/170.
- `total_functions` **28465**, unchanged by the splits.txt edit (it is 28465 before and after).
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- All 86 REL sha1s against `config/G2ME01/config.yml`: **0 diffs** (checked before the change and
  after each build).
- `python3 tools/check_raw_offsets.py` -> `ok: 188 raw-offset site(s) in 81 file(s)`.
- `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS progress-vtctl-rel-gunturret`,
  every check `ok`.

## Three things that were not obvious and cost time

1. **`CStateManager&` mangles as `CFR13CStateManager` in this compiler.** `CPatterned::Attacked` is
   declared `bool Attacked(CStateManager&, const CTriggerData&) const` in
   `include/MetroidPrime/Enemies/CPatterned.hpp:196`, and the retail symbol is
   `Attacked__10CPatternedCFR13CStateManagerRC12CTriggerData`. Reading `CFR` in the mangling as
   "const reference" and spelling the parameter `const CStateManager&` is a compile error
   (`illegal implicit conversion from 'const CStateManager' to 'CStateManager &'`). Read the
   qualifier out of `config/G2ME01/symbols.txt` and the header, not out of the mangling.
2. **dtk's generated FORCEACTIVE already holds every vtable slot of a module**, so a unit claiming
   one is never dead-stripped. `build/G2ME01/GunTurret/ldscript.lcf` lists `fn_28_8B58`, `fn_28_8B78`,
   `fn_28_9028`, `fn_28_9048`, `fn_28_9050` among ~100 `fn_28_*` entries, and no `force_active:` list
   in `config/G2ME01/config.yml` was needed. The 8-byte-accessor trap in
   `docs/RUNNING_THE_DECOMP.md` applies only to functions nothing in data references.
3. **A REL carve can be inserted anywhere; dtk re-cuts the auto units around it.** Claiming
   `0x8B58..0x8B98` split `auto_00_00007AF4_text` (37 functions) into
   `auto_00_00007AF4_text` (14) + the new unit (2) + `auto_00_00008B98_text` (21), and claiming
   `0x9028..0x9058` re-cut the new `auto_00_00008B98_text` again. `total_functions` is unchanged and
   the module hash holds, because every byte outside a claim is still filled from retail.

## Left in the module, measured, for the next lane

Every one of these is an exact-matching **single- or double-function carve** of the same kind, each
needing its own unit (one contiguous range per unit):

| claim | functions | why it is easy |
| --- | --- | --- |
| `.text 0x0003E84..0x0003EA4` | `fn_28_3E84` | `Touch__10CPatternedFR6CActorR13CStateManager` forwarder |
| `.text 0x00005C1C..0x00005C3C` | `fn_28_5C1C` | `AnimOver` forwarder |
| `.text 0x00006098..0x000060C0` | `fn_28_6098`, `fn_28_60B8` | `Attacked` forwarder + `lbz` byte at `+0x7F2` |
| `.text 0x0000854C..0x0000856C` | `fn_28_854C` | `Touch` forwarder |
| `.text 0x00008F70..0x00008F90` | `fn_28_8F70` | `AnimOver` forwarder |
| `.text 0x00001C38..0x00001C40` | `fn_28_1C38` | `addi r3,r3,0x978 ; blr`, one address accessor |

That is **+7 functions** in six more files, all with no new reasoning. `.text 0x00007B54..0x00007B98`
(`fn_28_7B54` = `li r3,1; blr`, plus `fn_28_7B5C`, the `optional_object<CAABox> GetTouchBounds`) is
+2 more but needs the `rstl::optional_object` return-by-sret spelling to come out at
`GetBoundingBox` + `fn_28_9A68`.

**Not attempted, and why** - all four are invention rather than decompilation without a header for
`CGunTurretBase`: `fn_28_8F90`/`fn_28_8FDC` (`0x8F90..0x9028`) call the state machine's vtable slot
0x30 and compare the float it returns against the members at `+0x7EC`/`+0x7F0`; `fn_28_9058`
(`0x9058..0x90C4`) reads `lbl_28_data_4A0`/`lbl_28_data_53C` out of the module's own `.data` and calls
through two more vtables; the run at `0x1BC4..0x1E14` needs the `CAABox`/`CTransform4f` argument order
and an `rstl::string` destructor; `fn_28_1E14` is 0x3A4 bytes of register work. All four are in the
module's FORCEACTIVE list, so none is blocked for that reason - only for want of the class layout.

## Files

- new `src/MetroidPrime/ScriptObjects/CGunTurretBaseForwarders.cpp` (44 lines)
- new `src/MetroidPrime/ScriptObjects/CGunTurretBaseTriggers.cpp` (56 lines)
- `configure.py:2660` and `configure.py:2664` (in `GunTurret`'s `Rel(...)` at 2649-2667: two
  `Object(Matching, ...)` added)
- `config/G2ME01/rels/GunTurret/splits.txt:12-16` (two `.text` claims)
- `files.cmake:1189-1190` (two sources)
- `docs/research/raw_offsets.md` - **required by the gate**, not optional: `check_raw_offsets.py`
  fails any file with a raw offset and no `##` section. Added the section for
  `CGunTurretBaseTriggers.cpp` (2 sites, `+0x7E8`, `+0x808`) and re-derived the totals paragraph
  from the tool. **While doing that I found the paragraph was already wrong:** its bold line said
  185 sites in 79 files while the tool printed 186 in 80 - the bold number had been written from the
  previous item's sentence instead of from the tool, which is the exact failure the paragraph two
  lines below warns about. Both now read the same measured line, 188 in 81.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` are also dirty in this worktree: `goal_check.sh`
runs `gate.sh` with `MP_GATE_DOCS_WRITE=1`, so the gate rewrote their derived counts
(matched 13350, linked 6398, REL units 1815, 132 of our units in 80 modules, probe 863 files). I did
not edit them and the driver discards edits to them before judging.

**Not committed**, as instructed.