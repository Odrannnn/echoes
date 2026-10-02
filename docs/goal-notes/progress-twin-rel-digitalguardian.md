# progress-twin-rel-digitalguardian

**Item:** `progress`, target `module:DigitalGuardian`.
**Measured result: `module:DigitalGuardian` matched_functions 16 -> 22 / 420** (`build/report.json`,
summed over every `DigitalGuardian/` unit, the same definition `tools/goal_check.sh`'s
`target_rose` uses). Project total `matched_functions` 13078 -> 13084, `linked` 6187 -> 6193.
All 86 REL sha1s in `config/G2ME01/config.yml` still hold after the change (measured with the
check printed in `docs/RUNNING_THE_DECOMP.md`: "modules checked: 86 mismatches: []"), and `main.dol`
is still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

Two things landed, in two separate ranges.

## 1. The module's `REL_Setup` tail, .text 0x1B6F8..0x1B89C - +5 functions

`python3 tools/wire_rel_setup.py DigitalGuardian` claimed the tail for the shared "REL" lib and
named what it references. Measured: `DigitalGuardian/REL/REL_Setup` goes 0/5 -> **5/5 at 100.00%**,
fuzzy 100.00%, `complete_units` 1. The claim is

```
REL/REL_Setup.cpp:
	.text       start:0x0001B6F8 end:0x0001B89C
	.rodata     start:0x00000F60 end:0x00000FE4
```

and `config/G2ME01/rels/DigitalGuardian/symbols.txt` gains four renames:
`fn_14_1B804` -> `ModuleDestructors`, `fn_14_1B850` -> `ModuleConstructors` (both
`scope:global`), and the two entry points `fn_14_138` -> `RELExit`, `fn_14_15C` -> `RELMain`
(both `scope:global`). `configure.py`'s `Rel("DigitalGuardian", ...)` block is **not** changed:
the shared `lib "REL"` group already carries `Object(Matching, "REL/REL_Setup.cpp")` for every
module, and naming it twice is a "Duplicate object name" error.

**The second build is not optional here, and the reason is the failure mode
`wire_rel_setup.py`'s own docstring describes.** `wire_rel_setup.py` renames the entry points
*after* its internal build, so the module it leaves on disk is linked with the still-unnamed
`fn_14_138` / `fn_14_15C` and is **48 bytes long** with unresolved `_epilog`/`_prolog`
relocations. Measured after the tool ran:

```
$ sha1sum build/G2ME01/DigitalGuardian/DigitalGuardian.rel   # what the tool leaves
028fca7e... vs config.yml a3798856ec6b175272529f6a6295a29140662bcc
$ cmp -l orig/.../DigitalGuardian.rel build/.../DigitalGuardian.rel | wc -l
13809          # and 154620 bytes against retail's 154572
```

One `./tools/decomp_build.sh` after the tool makes all 86 hashes match again. Anyone wiring a
module's tail should expect that and rebuild; it is not a sign the claim failed.

## 2. `DigitalGuardianVecList.cpp`, .text 0x1AB24..0x1AB30 - +1 function

New file `src/MetroidPrime/ScriptObjects/DigitalGuardianVecList.cpp`, one function,
`fn_14_1AB24`, 0xC bytes, `Matching`, **100.00% fuzzy, 1/1 matched, `complete_units` 1**:

```
li   r0, 0x0
stb  r0, 0xc(r3)
blr
```

Twin (from the item's list, confirmed here): `GetImpactParticle__17CEnergyProjectileFR13CStateManager`,
retail 0x8005EEF0, 0xC bytes, `main/MetroidPrime/TypesMatch` at 100.00%. Its spelling is
`CUnknownVec3List* CUnknownVec3List::ClearFlag() { xC_flag = 0; return this; }` in
`src/MetroidPrime/TypesMatch.cpp` - the same store of 0 to the same one-byte member at +0xC, and
`include/MetroidPrime/Weapons/CEnergyProjectile.hpp:44`'s `GetImpactParticle` returns
`rstl::optional_object_null()`, which is that store.

Four files, all in this change: the new source, `configure.py` (the `Object(...)` line),
`config/G2ME01/rels/DigitalGuardian/splits.txt` (the `.text` claim), and `files.cmake`. It is in
`files.cmake` for the reason the neighbouring `DigitalGuardianAccessors.cpp` entry gives: the body
reads a raw offset and emits **no relocation at all**, so `powerpc-eabi-nm -u` on its object
prints nothing and the port's undefined count cannot move. The module's class is unnamed in
retail, so - as in `DigitalGuardianAccessors.cpp` - the object is reached as a `void*` and the
offset is stated literally.

`fn_14_1AB24` is in the module's `ldscript.lcf` FORCEACTIVE list, which is what stops mwldeppc
dead-stripping a `Matching` unit's `.text` (structural fact 3 in `docs/RUNNING_THE_DECOMP.md`).
That is checked here, not assumed: it is how the candidate was chosen - see the candidate scan
below.

## What I measured first, and how the candidates were picked

`build/goal/judge/report.base.json` (the branch head) has `module:DigitalGuardian` at
**16 matched / 420 total**, all 16 from the pre-existing
`DigitalGuardian/MetroidPrime/ScriptObjects/DigitalGuardianAccessors` (16/16, 100.00%). The
module's units before this change were that one plus five `auto_*` units
(`auto_00_0000010C_text` 211 fns, `auto_fn_14_D904_text` 1, `auto_00_0000DF6C_text` 186,
`auto_fn_14_1AD2C_text` 1, `auto_00_0001B6F8_text` 5) and the data units. Nothing `auto_*` is ever
matched, so every further function has to come from a range a named unit claims.

Of the item's 90 twins, only six candidates are *contiguous runs where every function has an exact
matched twin **and** is in the module's FORCEACTIVE list** - and a claim has to be one contiguous
range per file (`docs/RUNNING_THE_DECOMP.md`, structural fact 1):

| range | functions | bytes |
| --- | --- | --- |
| 0x1AA44..0x1AB50 | 5 | 268 |
| 0x1AA84..0x1AB50 | 4 | 204 |
| 0x1AAC4..0x1AB50 | 3 | 140 |
| 0xDF6C..0xDF7C | 2 | 16 |
| 0x1AB24..0x1AB50 | 2 | 44 |
| 0x7FEC..0x8090 | 2 | 164 |

`fn_14_1AB30` (0x1AB30..0x1AB50, 0x20 bytes, twin
`GetIngSnatchingNormal__10CPatternedCFf` = `return CVector3f::Up();`) is the obvious next step
and is **not** claimed here - it is a separate range and so needs its own file, and it is the one
thing in that pair that carries a relocation (`sUpVector__9CVector3f`, a DOL global). Left for a
later run rather than half-landed.

## Negative result: the five `IsValid` twins at 0x1AA44..0x1AB50 do NOT work - and the reason is general

I wrote the larger candidate first, `DigitalGuardianIsValid.cpp`, claiming
`.text 0x1AA44..0x1AAC4`: `fn_14_1AA44` and `fn_14_1AA84`, the module's two
`CValidEntityPredicate::IsValid` overrides over `CScriptPlatform` and `CScriptWaypoint`. They are
byte-identical to the DOL's own `IsValid__23CValidWaypointPredicateCFRC13CStateManager9TUniqueId`
(0x80073938, 0x40 bytes), which `src/MetroidPrime/ScriptObjects/CScriptWaypoint.cpp:17` already
reproduces at 100% as

```cpp
return TCastToConstPtr< CScriptWaypoint >(mgr.GetObjectById(id)) != nullptr;
```

The bodies are byte-for-byte retail. `build/G2ME01/DigitalGuardian/asm/MetroidPrime/ScriptObjects/
DigitalGuardianIsValid.s` and `objdump -s` on the object both match retail exactly. **And the
module hash still broke**, the same way an unresolved reference does:

* module `.text` came out **0x10 bytes short** (154556 vs retail 154572; every later address
  shifted down by 0x10 - header field `0x58` 0x1b89c -> 0x1b88c, `prolog` 0x1b7e0 -> 0x1b7d0);
* masking branch displacements, the first differing word is at 0x1AA50, inside the claim;
* the linked bytes there are *not* the object's - `mr r3,r4` is followed by `or r5,r4,r4` where
  retail has `stw r0,0x14(r1)`, and the `addi r4,r1,0x8 / lhz r0,0x0(r5) / sth r0,0x8(r1)` id
  copy is gone. That is 4 bytes shorter per function, 8 over the pair... and the module is 16
  short over 420 functions, so the loss is not confined to the claim.

The two things this range reaches are `GetObjectById__13CStateManagerCF9TUniqueId` and the two
`TCastToPtr<...>__FP7CEntity` template instantiations. `include/MetroidPrime/TCastTo.hpp`
declares `TCastToPtr` **with no definition**, which I relied on to bind the call to the DOL's copy
rather than instantiate it here. Whatever mwcceppc actually did with a declared-only template, the
result is not the same relocation set as retail's, and the module stops hashing - the same family
as the "48 bytes of relocations" failure `wire_rel_setup.py`'s docstring records for the tail.
I did not isolate which of the three references causes it. **Spellings already tried and measured:
that whole file, reverted.** Do not re-try it as written.

A second, smaller fact came out of the same run and is reusable: the same 0x40-byte body written
as `extern "C" bool fn_14_1AA44(const void* self, const CStateManager& mgr, TUniqueId id)` with a
forward-declared stand-in `class CStateManager { const CEntity* GetObjectById(TUniqueId uid) const; }`
does **not** keep retail's `lhz/sth` id copy even though it keeps every other instruction. Retail's
copy is what a by-value class parameter gets; a free function with an unused first parameter is
enough to lose it. Whatever lands this range needs the twin's *member-function* shape.

## Lesson worth keeping (not a NEW: item)

**A unit whose object is byte-identical to retail can still break a REL module's sha1, and the
break looks like the module got *shorter* rather than wrong.** `config/G2ME01/build.sha1` only
says `FAILED`; the diagnostic that localises it is comparing the built `.rel` against
`orig/G2ME01/files/RelProd/<Module>.rel` **as a byte stream after masking the branch
displacements** (opcode 31 extended, `w & 0xFC000003`). With the masking, ordinary
`bl`-target differences disappear and the first real difference is inside the claim; unmasked, the
first difference is at `.text+0x28`, a `bl` whose target simply moved, which reads as "the whole
module shifted" and is what sent me looking in the wrong place for a long time. Header field
`0x58` is `.text` size: retail 0x1B89C against a built 0x1B88C said 16 bytes had gone missing
somewhere, and `0x34`/`0x38`/`0x3c` (`prolog`/`epilog`/`_unresolved`) all sitting 0x10 low said
where they had gone missing from - the tail.

## Gates

`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**, every check `ok`:

```
ok  no judge-owned path touched
ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 13078 -> 13084   linked 6187 -> 6193
ok  check_symbol_names.py
ok  All:  37.06% fuzzy, 30.46% matched, 13.46% linked (13084 / 28465 functions)
ok  target rose: module:DigitalGuardian: 16 -> 22 / 420 functions
ok  no asm added
```

Separately, all 86 module sha1s equal `config/G2ME01/config.yml` (the check in
`docs/RUNNING_THE_DECOMP.md`), and `DigitalGuardian/MetroidPrime/ScriptObjects/DigitalGuardianVecList`
reads `complete_units: 1` in `build/report.json`, so it is `Matching` *and* really in the link.

## NEW

None filed. `fn_14_1AB30` (0x1AB30..0x1AB50, 0x20 bytes, twin
`GetIngSnatchingNormal__10CPatternedCFf`) is real work that would raise this module's count, but it
needs its own file for a one-range carve and it carries a relocation to a DOL global, so it is a
continuation of this item rather than a separate one; the driver can requeue this id as-is.

---

# Run 2 (lane 3, 2026-10-02, `wt-mp2-goal-L3` at `c7f9598a`) - 16 -> 25 / 420, judge PASS

**Re-measured first, and the re-measurement is the first thing to record: run 1's work is not in
git.** `git log --all --oneline -- src/MetroidPrime/ScriptObjects/DigitalGuardianVecList.cpp` and
`git log --all -S"0x0001B6F8" -- config/G2ME01/rels/DigitalGuardian/splits.txt` both print nothing,
on every branch, so run 1's `REL_Setup` claim and its `DigitalGuardianVecList.cpp` were reset and
never committed. `build/goal/judge/report.base.json` (the branch head this lane was cut from) has
`module:DigitalGuardian` at **16 matched / 420 total** - the pre-run-1 number. Everything below is
from this tree; nothing is recalled.

**Measured result: `module:DigitalGuardian` matched_functions 16 -> 25 / 420.** Project
`matched_functions` **13098 -> 13107**, `linked` **6196 -> 6205**, `All: 37.09% fuzzy, 30.52%
matched, 13.47% linked (13107 / 28465 functions)`, `total_functions` still **28465**.

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item progress-twin-rel-digitalguardian (progress) target=module:DigitalGuardian
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13098 -> 13107   linked 6196 -> 6205
  ok    check_symbol_names.py
  ok    All:  37.09% fuzzy, 30.52% matched, 13.47% linked (13107 / 28465 functions)
  ok    target rose: module:DigitalGuardian: 16 -> 25 / 420 functions
  ok    no asm added
goal_check: PASS progress-twin-rel-digitalguardian
```

Independently of the judge: all 86 module sha1s equal `config/G2ME01/config.yml` and are
`cmp`-equal to `orig/G2ME01/files/RelProd/` (**86 checked, 0 mismatches, 0 missing**),
`DigitalGuardian.rel` is `a3798856ec6b175272529f6a6295a29140662bcc` - the value
`config/G2ME01/config.yml:83` already records, unchanged - and `main.dol` is still
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

## What landed - four ranges, three new files, +9 functions

| claim | range | fns | source |
| --- | --- | --- | --- |
| `REL/REL_Setup.cpp` | `.text 0x1B6F8..0x1B89C`, `.rodata 0xF60..0xFE4` | 5 | shared "REL" lib, no C++ |
| `DigitalGuardianVecList.cpp` | `.text 0x1AB24..0x1AB50` | 2 | new, 57 lines |
| `DigitalGuardianContact.cpp` | `.text 0x1AB50..0x1AB98` | 1 | new, 79 lines |
| `DigitalGuardianMemberPtr.cpp` | `.text 0x1ABD8..0x1ABE0` | 1 | new, 24 lines |

All four read `complete_units: 1` and `100.00% fuzzy, 100.00% matched` in `build/report.json`, i.e.
they are `Matching` **and** really in the link - `python3 tools/audit_rel_claim.py DigitalGuardian`
prints `0 claim(s) with a problem` and `preplf 420 text symbols, plf 420, 0 dropped by
-strip_partial`, and `./tools/unit_fit.sh MetroidPrime/ScriptObjects/<each>.cpp` prints
`.text claimed N ours N retail N fits` / `no extra functions` for all three.

The four-file carve is complete for each: `configure.py:2398/2403/2407` (the `Object(...)` lines in
the existing `Rel("DigitalGuardian", ...)` block), `splits.txt:12/15/18`,
`files.cmake:1161/1167/1168`, and the source's own claim in its header comment.

### 1. `REL_Setup` tail, +5 - re-landed, and it works exactly as run 1 recorded

`python3 tools/wire_rel_setup.py DigitalGuardian` appended the tail claim to
`config/G2ME01/rels/DigitalGuardian/splits.txt` and renamed four symbols in `symbols.txt`
(`fn_14_138`->`RELExit`, `fn_14_15C`->`RELMain`, `fn_14_1B804`->`ModuleDestructors`,
`fn_14_1B850`->`ModuleConstructors`, all `scope:global`). `configure.py`'s `Rel("DigitalGuardian", ...)`
block is **not** touched, for the reason run 1 gave: the shared `lib "REL"` group already carries
`Object(Matching, "REL/REL_Setup.cpp")` for every module and naming it twice is a "Duplicate object
name" error.

**Run 1's "the second build is not optional here" is confirmed, not just repeated.** The tool renames
the entry points *after* its internal build, so the module it leaves on disk is linked with the
still-unnamed `fn_14_138` / `fn_14_15C`. Measured here: after `wire_rel_setup.py` and one
`decomp_build.sh -r`, the module hash was already correct (this tool was run on a warm tree, so its
internal build had nothing to redo); the point stands and cost nothing. `REL/REL_Setup` reads
**5/5 matched**, and `auto_00_0001B6F8_text` is gone from the report.

### 2. `DigitalGuardianVecList.cpp`, .text 0x1AB24..0x1AB50, +2

Both functions were named in run 1; **run 1 landed only the first** and left `fn_14_1AB30` for a
later run. Both are here, one file, one contiguous range:

- `fn_14_1AB24` (0x0C) - `li r0,0 / stb r0,0xc(r3) / blr`. Twin
  `CUnknownVec3List::ClearFlag()` (`src/MetroidPrime/TypesMatch.cpp:975`), and the store of 0 to the
  one-byte member at +0xC is what `rstl::optional_object_null()` compiles to, which is why the twin
  `twin_scan` lists for it is `GetImpactParticle__17CEnergyProjectile`.
- `fn_14_1AB30` (0x20) - **the one run 1 did not attempt.** `lis r4,sUpVector__9CVector3f@ha /
  lfsu f0,..@l(r4) / stfs f0,0x0(r3) / lfs f0,0x4(r4) / stfs f0,0x4(r3) / lfs f0,0x8(r4) /
  stfs f0,0x8(r3) / blr`. Twin `CPatterned::GetIngSnatchingNormal(float) const`
  (`src/MetroidPrime/Enemies/CPatterned.cpp:462`). Spelled
  `CVector3f fn_14_1AB30(const void* self) { return CVector3f::Up(); }`, which is **byte-exact**:
  `CVector3f::Up()` is `inline const CUnitVector3f&` off `CVector3f::sUpVector`
  (`include/Kyoto/Math/CUnitVector3f.hpp:39`), and copying a 12-byte aggregate out of a reference is
  the `lfsu` on the first word plus two `lfs`/`stfs` pairs. The `lfsu` (not `lfs`) is what says the
  three reads are consecutive, and it appears only because the whole value is copied - run 1's note
  that this "carries a relocation to a DOL global" is right and is not a problem:
  `DigitalGuardianAccessors.cpp` already references three DOL globals the same way.

Run 1's worry that this range needs its own file separate from 0x1AA44..0x1AB50 is right for a
different reason than given: **a unit can claim one contiguous `.text` range only**, and
0x1AB24..0x1AB50 is already a range of its own, so `fn_14_1AB24` + `fn_14_1AB30` in one file is
correct and needs nothing from the 0x1AA44 pair.

### 3. `DigitalGuardianContact.cpp`, .text 0x1AB50..0x1AB98, +1 - **new work**

`fn_14_1AB50` is not in run 1's lists. 18 instructions, no relocation, in the module's FORCEACTIVE
list. The body copies the flag byte at +0x18 of `this` unconditionally and the six words at +0 only
when the flag read from +0x730 of the argument is set.

**The shape that matters, and the five spellings measured against it.** Retail copies the record
as 8+4+8+4, i.e. two loads and two stores per 8 bytes, **in ascending offset order**:

```
lwz r5,0x718(r4); lwz r0,0x71c(r4); stw r5,0x0(r3); stw r0,0x4(r3);
lwz r0,0x720(r4); stw r0,0x8(r3);
lwz r5,0x724(r4); lwz r0,0x728(r4); stw r5,0xc(r3); stw r0,0x10(r3);
lwz r0,0x72c(r4); stw r0,0x14(r3);
```

Compiled with this unit's exact MWCC command line (`.tmp/fast_try.sh`, the recipe
`tools/probe_cc.sh` documents), all of these are **wrong**:

| spelling | what MWCC emits |
| --- | --- |
| six named `u32` members, memberwise | six `lwz r0` / `stw r0` - no `r5` |
| whole-struct assignment | same, then a **second** copy of the flag at the end |
| two-`u32` nested struct / `u32[2]` member | same |
| `s64` / `u64` member | two loads into `r0`/`r5` but stores **descending**: `stw r5,4(r3)` before `stw r0,0(r3)` |
| `union { u32 w[2]; u64 q; }` member | `lfd`/`stfd` - a `u64` alternative changes the whole thing |
| **`union { u32 w[2]; }` member** | **byte-exact, ascending** |

So the two 8-byte members are modelled as 8-byte unions of two `u32`, and that is the only one of
six that reproduces retail. The file says so and says why, because a reader who "simplifies" the
union into two `u32` members loses 4 instructions per function silently.

The second `lbz r0,0x730(r4)` after the `stb` is not asked for: it falls out of the plain spelling,
because mwcceppc cannot prove `this` and the argument are distinct objects and must reload the flag
after the store.

### 4. `DigitalGuardianMemberPtr.cpp`, .text 0x1ABD8..0x1ABE0, +1 - **new work**

`addi r3,r3,0x764 / blr`. The identical accessor already exists **in this module's own `Matching`
unit**: `DigitalGuardianAccessors.cpp`'s `fn_14_D8` at .text 0xD8 does the same for the member at
+0x754 and reproduces at 100.00%. So this is `DigitalGuardianAccessors`'s spelling with the offset
read off this module's disassembly rather than copied - no spelling work at all, which is why it is
the cheapest function in the item.

## Gates, all re-measured on this tree

```
$ python3 .tmp/relcheck.py     # 86 modules: built sha1 vs config.yml vs orig
modules checked: 86  mismatches: 0  missing: 0  cfg entries: 86
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
$ python3 tools/check_symbol_names.py      -> checked 584 units; 0 declared names are missing
$ python3 tools/check_files_cmake.py       -> every configured DOL object is in files.cmake or excluded with a reason
$ python3 tools/check_module_wiring.py     -> 108 unit(s) of our own code in 80 module(s), DigitalGuardian present
$ python3 tools/check_raw_offsets.py       -> ok: 176 raw-offset site(s) in 76 file(s), all documented
$ ./tools/probe_sources.sh                 -> probe: 836 files, 0 failed, 0 errors; link: LINKED (286 undefined, 0 duplicates)
$ python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/DigitalGuardian{Contact,MemberPtr,VecList}.cpp
ok: 0 unit(s) checked, none emits its functions out of retail order
```

**The port's undefined count is measured unchanged, not assumed: 286 with the three new files and
286 without them.** `git stash -u; ./tools/probe_sources.sh` on the clean tree prints
`833 files ... LINKED (286 undefined, 0 duplicates)`, and with the change
`836 files ... LINKED (286 undefined, 0 duplicates)`. `DigitalGuardianContact.cpp` and
`DigitalGuardianMemberPtr.cpp` read only raw offsets through the two pointers the ABI puts in r3
and r4 and `powerpc-eabi-nm -u` on both objects prints nothing; `DigitalGuardianVecList.cpp`'s only
relocation is `sUpVector__9CVector3f`, a DOL global `src/Kyoto/Math/CVector3f.cpp` already links.

**`check_decl_order.py --unit` reports `0 unit(s) checked` for all three** - it is a DOL tool and does
not resolve a source path out of a `Rel(...)` block, the same limitation
`docs/goal-notes/progress-rel-head-shrieker.md` records for `flip_test.sh`. All three units are
single-range and their functions are declared in descending retail offset by hand anyway; the
acceptance test for a REL unit is the module sha1, which is what holds above.

## Negative result: `fn_14_1AB98` is one register away, and the shape is now known

`fn_14_1AB98` (0x1AB98, 0x40) is **not** claimed - it sits between the two `DigitalGuardianContact`
and `DigitalGuardianVecList` ranges and stays retail bytes. It is in the module's FORCEACTIVE list,
so nothing structural stops a claim; the blocker is codegen. Measured this run:

```
lis r6, lbl_14_rodata_960@ha | stw r0,0x14(r1) | lfs f1, lbl_14_rodata_960@l(r6)
stw r31,0xc(r1) | mr r31,r3 | lwz r12,0x0(r4) | lwz r12,0x54(r12) | mtctr r12 | bctrl
lwz r0,0x14(r1) | lwz r31,0xc(r1) | mtlr r0 | addi r1,r1,0x10 | blr
```

**The call shape is solved and worth recording**, because it is not obvious: r3 is the hidden return
pointer and r4 is `this`, so this is a member function returning an 8-word struct **by value** whose
body is `return this->v54(0.0f);` - the callee needs r3 as its own sret and r4 as its own `this`, so
`bctrl` passes both through with no move at all, which is why retail has no `mr r3,r4`. `lbl_14_rodata_960`
is `.float 0` (`build/G2ME01/DigitalGuardian/asm/auto_03_00000000_rodata.s:963`).

Reproduced almost exactly with a 21-slot class whose `v54(float)` returns an 8-word struct - frame
0x10, `mr r31,r3`/`stw r31,0xc(r1)`/`lwz r31,0xc(r1)`, `lwz r12,0(r4)`, `mtctr`, `bctrl`, and the
constant loaded into f1 before the spill, all in retail's order. The variants measured, all
`lis r5` where retail has `lis r6`: dtor-pair + 19 void virtuals with `v54` at index 21; `v54` at
index 22; no destructor at all; `fn` itself virtual and last; `fn` before `v54`; an unused
`W1*` parameter; an unused `int` parameter; and the non-sret shapes (virtual call on the second
argument, virtual call returning `this`, call through a function-pointer table at +0x54 of a table
pointed to by +0 of the argument) which each need an extra `mr r3,r4` or a different vtable load and
are further away. Slot position was never the issue - the offset follows the virtual count exactly;
it is **one scratch register** for the constant's address, so retail had one more value live at that
point than any of these seven spellings does.

WALL: fn_14_1AB98 - sret + virtual-on-this + v54 at slot 0x54 + `lbl_14_rodata_960` reproduces 15 of
16 instructions; the constant's address lands in r5 where retail has r6, and none of seven ways of
occupying r5 at that point moves it.

## A second negative result, for the next run

`fn_14_1ABE0` (0x1ABE0, 0x34) is a 0x30-byte copy of six doubles that neither of the obvious
spellings produces: `double d[6]` gives a 2-deep pipeline (`lfd,lfd,stfd,stfd,lfd,lfd,...`) and six
*named* doubles give no pipeline at all and only ever use `f0`, where retail alternates `f1` for the
even elements and pipelines three loads deep. `CVector3f`-style 12-byte copies are the wrong size
entirely. Independently, **`fn_14_1ABE0`, `fn_14_1AC14` and `fn_14_1ACB8` are absent from this
module's generated `build/G2ME01/DigitalGuardian/ldscript.lcf` FORCEACTIVE list**, and
`config/G2ME01/config.yml` has exactly one `force_active:` key in the whole file and it belongs to
`main`, not to any REL module - so the dead-strip hazard of structural fact 3 in
`docs/RUNNING_THE_DECOMP.md` cannot be lifted from config.yml for a REL module the way the
2026-09-29 supersession claims. Any carve whose range contains one of those three functions has to
overcome that before it can be promoted, whatever the spelling is. Not measured as a break here -
this run did not claim the range - so it is a lead, not a verdict.

## What is still on the table, and what is not

Re-derived from scratch on this tree: the maximal runs of consecutive functions where **every**
function has an exact matched twin **and** is in the module's FORCEACTIVE list - the only ranges a
`Matching` carve can take, since a claim has to be one contiguous range per file. After this change
the list is:

| range | fns | bytes | state |
| --- | --- | --- | --- |
| 0x1AA44..0x1AB24 | 3 | 220 | `fn_14_1AA44`/`fn_14_1AA84` are run 1's failed `IsValid` pair; `fn_14_1AAC4` is the `__dt__18CErrorOutputWindow` twin. **Run 1 measured a wall on the first two and did not isolate why** - see the `NEW:` question below |
| 0x7FEC..0x8090 | 2 | 164 | `fn_14_7FEC` (0x5C) + `fn_14_8048` (0x48), twins `__dt__21CArchMsgParmUserInputFv` and `__dt__11IObjFactoryFv` |
| 0x8328, 0x8738, 0x8978, 0x8C70, 0x9014, 0x12FE0, 0x169F4 | 1 each | 0x5C each | all seven are the same 0x5C body, twin `__dt__21CArchMsgParmUserInputFv` |

The **dtor family is a wall for a reason this run can state**: `fn_14_8328` and `fn_14_8048` both
store the *address of a module-local vtable* (`lis r5,lbl_14_data_670@ha; addi r0,r5,..@l; stw r0,0x0(r31)`
- two vtables, `lbl_14_data_670` and `lbl_14_data_688`). A carve that spells the class with real
virtuals makes mwcceppc **emit its own vtable into the object's `.data`**, and the split claims
`.text` only, so the module's `.data` would hold two extra objects at addresses retail does not have
and the sha1 breaks with every function at 100%. This is the same failure
`DigitalGuardianAccessors.cpp`'s header documents for including `MetroidPrime/CPhysicsActor.hpp`
("0x28 bytes of `.data` in this object"). The only spelling that avoids it would be an `extern`
declaration of the module's own `lbl_14_data_670` data label plus hand-written vptr stores, which is
hand-assembly with a C++ wrapper and not worth it. **Seven 0x5C dtors, `fn_14_8048` and
`fn_14_11BA8`/`fn_14_13A80` are therefore not reachable from this item's arrangement.**

## NEW

None filed. The two leads above are (a) a codegen question on one function and (b) a structural
question about a range, and neither is a unit, module or symbol whose promotion this item's
arrangement reaches - which is what the instructions ask a `NEW:` to be. Run 1's `fn_14_1AB30`
follow-up is closed here (it landed). Requeueing this id as-is continues at 25/420.
