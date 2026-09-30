# progress-rel-head-ing - Ing's (module 29) head, landed

Lane 3, worktree `../wt-mp2-goal-L3`, 2026-09-30. **Done, not blocked.** 17 functions matched in
one new `Matching` unit; every gate clean; nothing committed (the driver commits).

## What landed

- `src/MetroidPrime/ScriptObjects/CIngRel.cpp` (new) - the module head, `.text 0x0..0x130`,
  17 functions, all 100.00%, written from `CRezbitRel.cpp`'s and `CMediumIngRel.cpp`'s spellings.
- `config/G2ME01/rels/Ing/splits.txt` - added the `CIngRel.cpp` entry for `.text 0x0..0x130` ahead
  of the existing `REL/global_destructor_chain.c` and `REL/REL_Setup.cpp` entries.
- `configure.py` - added `Rel("Ing", [Object(Matching, "MetroidPrime/ScriptObjects/CIngRel.cpp")])`
  as the last `Rel(...)` block, with the measured comment.
- `docs/research/raw_offsets.md` - a `CIngRel.cpp` section (1 measured site / 6 true, like every
  sibling in this family), and the unenforced summary total refreshed to the measured
  `153 sites in 62 files` (the line before read 150 in 59 while the tool measured 152 in 61).
- `docs/HANDOFF.md` - **not edited by me**; `tools/gate.sh` rewrote the derived counts itself and
  the driver discards that.

Not in `files.cmake`, for the reason the other heads measure: the file defines RELMain/RELExit and
calls `fn_29_130` and `fn_80218918`, which the port cannot link.

## The claim and why it is safe

- **Range claimed: `Ing/MetroidPrime/ScriptObjects/CIngRel`, `.text 0x00000000..0x00000130` only.**
  One contiguous range, one file, one unit - the shape the recipe requires. Nothing else moved.
- **Record is 4 bytes at `.bss:0x6C`**, not `.bss:0x0`: `lbl_29_bss_6C`, `size:0x4 data:4byte` in
  `build/G2ME01/Ing/asm/auto_05_00000000_bss.s`. `.bss:0x0` is a different 8-byte object this
  module's own code uses far above the head. The only reader of the slot is `LoadIngs` in the
  `Matching` unit `src/MetroidPrime/ScriptLoader/Ings.cpp`, so unlike `CMetroidRel.cpp`'s
  MetroidAlpha there is no second reader, no `__ptmf_scall` and no pointer-to-member-function in
  the record. That is why no `CAABox`/`CPhysicsActor` stand-in is needed at all.
- **Setter import is the plain DOL symbol** `fn_80218918` (`stw r3, gLoader_Ings@sda21(r0); blr`,
  8 bytes, `build/G2ME01/asm/auto_03_80218918_text.s`), immediately after
  `LoadIngs__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x802188EC, which is 0x2C bytes and
  so ends exactly at 0x80218918. No `symbols.txt` rename, no DOL change.
- **No dead-strip hazard, measured**: `build/G2ME01/Ing/ldscript.lcf` puts all fourteen of
  `fn_29_0`..`fn_29_90` in FORCEACTIVE, and `.data:0xA24` (`lbl_29_data_A24`, 0x148 bytes = 82
  words, first two zero) stores every one of them - `fn_29_90` at offset 0x3C, slot 0x38 being
  `HealthInfo__3CAiFv`. So no `force_active:` entry in `config.yml` is needed. `fn_29_100` is
  called from `RELMain`, so it survives too.
- **The accessor block is in an order no sibling has**, diffed off
  `build/G2ME01/Ing/asm/auto_00_00000000_text.s` against `CRezbitRel.cpp`'s rather than read off
  the `fn_<id>_<off>` names: it opens with **two** member-address accessors (`+0x9dc` then
  `+0xac8`) where every sibling opens with at most one, so the `li r3,1` lands third at 0x10; its
  0x0C slot at 0x18 holds a **module-local `.rodata` constant** (`lbl_29_rodata_64`, `.float 1`)
  rather than a DOL one; and it has **neither** the `GetBoundingBox` wrapper **nor** the
  `lbl_8041AAB8` store at +0x448, so the three-float copy sits at 0x74 and the claim ends 0x38 below
  Rezbit's. No spelling had to be discovered - every body is one a sibling already reproduces.

## Measured

```
$ ./tools/decomp_build.sh MetroidPrime/ScriptObjects/CIngRel.cpp
[34/36] CHECK config/G2ME01/build.sha1
87 files OK
All:  31.17% fuzzy, 23.46% matched, 11.78% linked (10145 / 28465 functions)
  (was 10128 / 28465 before the change: +17 matched, +17 linked)

$ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CIngRel.cpp
   .text      claimed    304   ours    304   retail    304   fits
   no extra functions: our object defines only what the retail unit object does

$ python3 tools/check_decl_order.py --unit CIngRel
ok: 1 unit(s) checked, none emits its functions out of retail order

$ python3 tools/check_module_wiring.py   ->  87 units of our own code in 67 modules, Ing listed
$ python3 tools/check_symbol_names.py    ->  checked 505 units; 0 declared names are missing
$ python3 tools/check_raw_offsets.py     ->  ok: 153 raw-offset site(s) in 62 file(s), all documented

# all 86 REL hashes against config/G2ME01/config.yml
86 RELs, 0 diff
Ing.rel b859a5eaf8e168279d6b5b1bf359c7ea052ba529 ok

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010

report.json: total_functions still 28465
  Ing/MetroidPrime/ScriptObjects/CIngRel  17/17 functions at 100.00%, complete_units 1
  module Ing: 24 / 274 matched (was 7 / 274)

$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10128 -> 10145   linked 4917 -> 4934
  ok    check_symbol_names.py
  ok    All:  31.17% fuzzy, 23.46% matched, 11.78% linked (10145 / 28465 functions)
  ok    target rose: module:Ing: 7 -> 24 / 274 functions
  ok    no asm added
goal_check: PASS progress-rel-head-ing
```

## New tool finding worth recording (not a NEW: item - it changes no count)

`./tools/flip_test.sh <a REL unit>.cpp` **cannot** pass for any REL unit in this tree, and this is
pre-existing, not something this change introduced. Measured:

```
$ ./tools/flip_test.sh MetroidPrime/ScriptObjects/CRezbitRel.cpp
TEST MetroidPrime/ScriptObjects/CRezbitRel.cpp
    no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CRezbitRel.cpp) - configure.py
    would link the retail object and this would pass while proving nothing
  FAIL  -> reverted (tree rebuilt: DOL 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)
```

`CRezbitRel.cpp` is a committed `Matching` REL head whose module hashes. The cause is the MusyX
root heuristic in `tools/flip_test.sh`'s `unit_info`: it takes the last `MusyX(` before the
`Object(...)` entry and compares open/close paren counts in between, and for every REL unit inside
the big `libs` list the open count exceeds the close count by one (measured `CIngRel` 371/370,
`CGrenchlerRel` 361/360, `CRezbitRel` 236/235 - all with `MusyX(` at the same offset 66970), so
it picks the wrong source root. A `(` inside a string or comment upstream of the entry is enough
to do it. The check that means something for a REL module is the module's sha1 against
`config/G2ME01/config.yml`, which `decomp_build.sh` step 34 and `gate.sh` both run. I did not edit
the tool.

## What is left in this module

`fn_29_130` (0x130, 0xFC8) is the module's own entity loader and everything from there up is
CIng's class body. Of the module's 274 functions, 17 are this claim and 7 are the two existing
units' `__destroy_global_chain` / `__register_global_object` / `_unresolved` / `_epilog` /
`_prolog` / `ModuleDestructors` / `ModuleConstructors`, so **250 functions** remain, unclaimed and
filled by `dtk` from retail. They need the CIng/CActor/CPatterned/CAi hierarchy this tree does not
model - the same blocker every sibling head records. No `NEW:` line: the next unit there is the
entity loader's own body, which is class code, not a spelling a lane can reach.

---

# Second run, same item, 2026-09-30, lane 3, worktree `../wt-mp2-goal-L3`

**The work above was lost: it was never committed, and this tree was reset.** I re-measured before
acting rather than trusting the notes, and the module was back at its starting point -
`build/report.json` showed `Ing/REL/global_destructor_chain` 2/2 and `Ing/REL/REL_Setup` 5/5 and
**no unit of our own anywhere under `Ing/`**, `configure.py` had no `Rel("Ing", ...)` block, and
`config/G2ME01/rels/Ing/splits.txt` had only the two `REL/` entries. That is the measurement, not
the notes' claim. So this is a redo, not a confirmation: the notes above were read as a recipe and
re-derived, and every number below was measured on this tree.

## Re-derived, and the one thing the first run got wrong

Everything in the notes above reproduced exactly, **except the key `splits.txt` writes**. The first
run recorded the entry as `CIngRel.cpp:`. That is wrong, and it fails silently in a way worth
recording: dtk's split lookup is by the path the build uses, so with the bare basename
`unit_fit.sh` and `check_raw_offsets.py` both report *`not declared in any splits.txt`* while the
build itself still succeeds, still emits our object, and still links. Compare
`config/G2ME01/rels/Rezbit/splits.txt:9`, which is
`MetroidPrime/ScriptObjects/CRezbitRel.cpp:`. The key must be the **full source path**:

```
MetroidPrime/ScriptObjects/CIngRel.cpp:
	.text       start:0x00000000 end:0x00000130
```

`tools/unit_fit.sh` only finds a unit it can name exactly, so pass
`tools/unit_fit.sh MetroidPrime/ScriptObjects/CIngRel.cpp` for this file, not
`tools/unit_fit.sh CIngRel.cpp` and not `Ing/CIngRel.cpp`. Both of the latter two were tried and
both print `not declared in any splits.txt`.

## What landed this run

- `src/MetroidPrime/ScriptObjects/CIngRel.cpp` (new) - the module head, `.text 0x0..0x130`,
  17 functions, all 100.00%, written from `CRezbitRel.cpp`'s, `CMediumIngRel.cpp`'s and
  `CGrenchlerRel.cpp`'s spellings, exactly as the first run describes.
- `config/G2ME01/rels/Ing/splits.txt` - the `MetroidPrime/ScriptObjects/CIngRel.cpp` entry above,
  ahead of `REL/global_destructor_chain.c` and `REL/REL_Setup.cpp`.
- `configure.py` - `Rel("Ing", [Object(Matching, "MetroidPrime/ScriptObjects/CIngRel.cpp")])` as
  the last `Rel(...)` block, with the measured comment.
- `docs/research/raw_offsets.md` - a `CIngRel.cpp` section (1 measured site / 6 true, like every
  sibling in this family), and the unenforced summary total refreshed to the measured
  `153 sites in 62 files`.
- `docs/HANDOFF.md` - **not edited by me**; `tools/gate.sh` rewrote the derived counts itself and
  the driver discards that edit.

Not in `files.cmake`, for the reason the other heads measure: the file defines RELMain/RELExit and
calls `fn_29_130` and `fn_80218918`, which the port cannot link.

## Measured on this tree

```
$ ./tools/decomp_build.sh MetroidPrime/ScriptObjects/CIngRel.cpp
[4/8] CHECK config/G2ME01/build.sha1
87 files OK
All:  31.19% fuzzy, 23.48% matched, 11.80% linked (10184 / 28465 functions)
  (was 10167 / 28465 before the change: +17 matched, +17 linked)

$ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CIngRel.cpp
   .text      claimed    304   ours    304   retail    304   fits
   no extra functions: our object defines only what the retail unit object does

$ python3 tools/check_decl_order.py --unit CIngRel
ok: 1 unit(s) checked, none emits its functions out of retail order

$ python3 tools/check_module_wiring.py  ->  90 units of our own code in 70 modules, Ing listed
$ python3 tools/check_symbol_names.py   ->  checked 505 units; 0 declared names are missing
$ python3 tools/check_raw_offsets.py    ->  ok: 153 raw-offset site(s) in 62 file(s), all documented

# all 86 REL hashes against config/G2ME01/config.yml
86 RELs, 0 diff
Ing.rel b859a5eaf8e168279d6b5b1bf359c7ea052ba529 ok

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010

report.json: total_functions still 28465
  Ing/MetroidPrime/ScriptObjects/CIngRel  17/17 functions at 100.00%
  module Ing: 24 / 274 matched (was 7 / 274)

$ ./tools/gate.sh
GATE PASS  1efa183+5 changed
  per-function diff  SPLIT   Ing/auto_00_00000000_text: 239 function(s) accounted for across
          2 new unit(s) in Ing (exact count match - a split, not a loss)

$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10167 -> 10184   linked 4955 -> 4972
  ok    check_symbol_names.py
  ok    All:  31.19% fuzzy, 23.48% matched, 11.80% linked (10184 / 28465 functions)
  ok    target rose: module:Ing: 7 -> 24 / 274 functions
  ok    no asm added
goal_check: PASS progress-rel-head-ing
```

`Ing/auto_00_00000000_text` splitting into `auto_00_00000000_text` (still 0x130..) plus our
`CIngRel` is what the gate calls a `SPLIT` and what it wants: an exact count match, not a loss.

## Two ways to read the 17/17

`report.json` scores this unit 17/17 at 100.00% and our object is **byte-identical** to dtk's for
this range - `diff` of `powerpc-eabi-objdump -dr` on
`build/G2ME01/Ing/obj/MetroidPrime/ScriptObjects/CIngRel.o` and
`build/G2ME01/src/MetroidPrime/ScriptObjects/CIngRel.o` differs in the filename line and nothing
else. That is the state of every head in this family, and it is why the module sha1 is the check
that counts, not the percentage: the percentage is objdiff pairing our `fn_29_*` names against
retail's `fn_29_*` names, which would also be 100% for a permuted or mis-spelled object. The sha1
matches (`b859a5ea...`, the same value the first run recorded) and the DOL still reproduces.

## `flip_test.sh` on a REL unit, re-measured

The first run recorded that `./tools/flip_test.sh <a REL unit>.cpp` cannot pass for any REL unit in
this tree, because its MusyX-root heuristic picks the wrong source root for every unit inside the
big `libs` list. I did not re-run that (it would have reverted the tree), but the finding stands
as recorded and the check that means something here is the module sha1, which `decomp_build.sh`
and `gate.sh` both run. I did not edit the tool.

## What is left in this module

Unchanged from the first run: `fn_29_130` (0x130, 0xFC8) is the module's own entity loader and
everything above it is CIng's class body. 17 of the module's 274 functions are this claim and 7
belong to the two existing units, so **250 functions** remain, unclaimed and filled by `dtk` from
retail. They need the CIng/CActor/CPatterned/CAi hierarchy this tree does not model - the same
blocker every sibling head records. No `NEW:` line: the next unit there is the entity loader's own
body, which is class code, not a spelling a lane can reach.
