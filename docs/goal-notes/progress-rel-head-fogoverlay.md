# progress-rel-head-fogoverlay - FogOverlay (module 23) head as a Matching unit

**Item**: `kind: progress`, `target: module:FogOverlay`. Landed in full: the module's
`matched_functions` went **0 -> 5** and the unit is `Matching` with the module's sha1 holding.

## What landed

A new REL head unit, in the arrangement the module recipe prescribes: one contiguous `.text`
range, one file, only what the object reproduces, everything else left to dtk.

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/CFogOverlayRel.cpp` | new, 5 functions (the unit) |
| `config/G2ME01/rels/FogOverlay/splits.txt` | claim `.text 0x00000000..0x00000100` |
| `config/G2ME01/rels/FogOverlay/symbols.txt` | `fn_23_8C`/`fn_23_B0` -> `RELExit`/`RELMain`, `scope:global` |
| `configure.py` | `Rel("FogOverlay", [Object(Matching, .../CFogOverlayRel.cpp)])` |

`config.yml` untouched; the module still hashes `1b3676a0af38a6792c3a14d932c8248d1f622d6a`.

### The five claimed functions, from `config/G2ME01/rels/FogOverlay/symbols.txt`

| addr | symbol | bytes | what it is |
| --- | --- | --- | --- |
| 0x000 | `fn_23_0` | 0x60 | the class's deleting destructor |
| 0x060 | `fn_23_60` | 0x2C | virtual dispatch, vtable slot 0x38 |
| 0x08C | `RELExit` | 0x24 | `li r3,0 / bl fn_80232834` |
| 0x0B0 | `RELMain` | 0x20 | `bl fn_23_D0` |
| 0x0D0 | `fn_23_D0` | 0x30 | `lbl_23_bss_0 = fn_23_100 ; fn_80232834(&lbl_23_bss_0)` |

First neighbour left retail is `fn_23_100` (0x100, 0x3E4), the module's own entity loader; the
16 functions from there to `fn_23_117C` are the class's methods (Draw, AcceptScriptMsg, the
constructor chain). They need the CActor/CPatterned hierarchy this tree does not model.

## The one measurement worth keeping: this head has no accessor block

The recipe's first step is "copy `CAtomicAlphaRel.cpp`", and most landed heads do open with the
REL loader generator's thirteen short accessors - Krocuss, MysteryFlyer, ElitePirate, AtomicAlpha
all do. **FogOverlay does not.** It opens with a 0x60-byte deleting destructor, so the claim is
five functions where those are eighteen, and no body is copied from a sibling. Only `fn_23_60`
(the 0x38 dispatch) is shared with them, and the thirteen-virtual stand-in class reproduces it.
The reason is visible in `.data:0x0`: `lbl_23_data_0` is 0x7C bytes = two leading zero words plus
29 slots, i.e. the plain `CActor` table, not one of the `CAi`/generator tables the other heads
carry an extra accessor for. So: **read the first function of a module's `.text` before planning
its head; it decides five functions or eighteen.** (The same lesson is already recorded in
`CBloggRel.cpp` for a module whose head starts with a `CDamageVulnerability` destructor.)

## No spelling had to be discovered

Every body is one a sibling already reproduces, or one `src/MetroidPrime/CStateManager.cpp`
measures:

- `fn_23_60` - the `fn_45_D0` / `fn_32_8` shape. `mwcceppc` only reaches for `r12` on its own
  virtual-dispatch path, and it lays virtuals out the way retail's vtable is (two leading words
  then one per virtual), so **thirteen** virtuals put the called one at 0x38. Loading the vtable
  by hand compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)` and lands at 99%. Slot 12's
  return type is `CHealthInfo*` because `.data:0x0` names entry 0x38 `HealthInfo__6CActorFv`; the
  call discards it, so it does not affect the bytes.
- `fn_23_0` - MWCC's deleting destructor, the leaf `CStateManager.cpp` spells out. Three details
  are load-bearing and all three are measured there: the flag parameter is a **`short`** (`int`
  gives `cmpwi r31,0` where retail has `extsh. r0,r31`), the return type is a **pointer** (a
  `void` leaf loses the trailing `mr r3,r30`), and the base destructor is called with a literal
  `0`, which is what puts `li r4, 0` between the vtable `lis` and the `addi`.
- `fn_23_D0` - the `fn_45_140` / `fn_7_D8` shape: `lbl_23_bss_0 = fn_23_100; fn_80232834(&lbl_23_bss_0);`
  stores the loader with `stwu` so the store writes the slot and leaves `r3` holding its address
  for the setter call.
- `RELExit` / `RELMain` - the family shape.

**100.00% matched, 5/5, on the first build.** Nothing was tried and abandoned.

## The two names that were needed, and the one that was not

- `fn_23_8C` -> `RELExit` and `fn_23_B0` -> `RELMain`, **with `scope:global`**. dtk's own
  `_epilog`/`_prolog` (0x109C / 0x10C0) are force-active, are left unclaimed here, and take their
  second `bl` through these two. Unnamed, the module grows 48 bytes of relocations; named without
  `scope:global` (the AtomicAlpha case), the same. This is `tools/wire_rel_setup.py`'s rule, and
  the module's own `symbols.txt` carried neither name.
- `fn_23_D0` and `fn_23_0`/`fn_23_60` keep their dtk names; no rename needed, so none was made.
- **No `symbols.txt` rename and no DOL change for the setter.** It is the plain DOL symbol
  `fn_80232834` (`0x80232834`, two instructions, `stw r3, gLoader_FogOverlay; blr`,
  immediately after `LoadFogOverlay__...` at 0x80232808, which is 0x2C bytes and so ends exactly
  there). `src/MetroidPrime/ScriptLoader/FogOverlay.cpp` is that thunk and already records why
  the setter stays unclaimed in the DOL: REL modules import it by its retail name.

## Vtable: referenced, never defined

`lbl_23_data_0` (`.data:0x0`, 0x7C bytes) is the class vtable and is **not claimed and not
defined here**. A class with a defined virtual emits a `__vt__` into `.data` and the module's
sha1 moves; the same arrangement as `CIngSnatchingSwarmGenAccessors.cpp`. It is in the module's
`ldscript.lcf` FORCEACTIVE list, and so are `fn_23_0` and `fn_23_60`, and `.data:0x0` stores both
- so **nothing in this claim is a dead-stripping hazard** and no `force_active:` entry is needed.
`tools/audit_rel_claim.py FogOverlay` reports `preplf 21 text symbols, plf 21, 0 dropped by
-strip_partial` and `5/5 functions` in the claim.

## Verification (all measured, none recalled)

```
./tools/decomp_build.sh FogOverlay/MetroidPrime/ScriptObjects/CFogOverlayRel
  -> FogOverlay/MetroidPrime/ScriptObjects/CFogOverlayRel: 100.00% fuzzy, 100.00% matched (5 / 5 functions)
  -> CHECK config/G2ME01/build.sha1: 87 files OK
  -> All: 31.17% fuzzy, 23.45% matched, 11.78% linked (10132 / 28465 functions)

python3 tools/check_decl_order.py --unit CFogOverlayRel   -> ok, none emits out of retail order
python3 tools/audit_rel_claim.py FogOverlay                -> ok, 0x0..0x100 5/5; 0 problems
python3 tools/check_symbol_names.py                       -> 505 units, 0 missing names
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CFogOverlayRel.cpp
  -> .text claimed 256 ours 256 retail 256 fits; no extra functions
python3 tools/check_files_cmake.py                        -> every configured object accounted for
sha1sum build/G2ME01/main.dol                             -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
```

Counts, from `build/report.json` against `build/goal/judge/report.base.json`:

| | base | now |
| --- | --- | --- |
| `module:FogOverlay` matched_functions | 0 | **5** (of 21 in the module) |
| project matched_functions | 10127 | **10132** |
| linked (objdiff `complete`) | 4917 | **4922** |
| total_functions | 28465 | **28465** (unchanged) |

`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS progress-rel-head-fogoverlay`**
(all seven checks ok, `target rose: module:FogOverlay: 0 -> 5 / 21 functions`).

Not in `files.cmake`, and that is the measured reason, not an oversight: the unit calls
`fn_23_100`, `__dt__6CActorFv` and `fn_80232834`, none of which the port can link, so listing it
grows the undefined count. It also defines RELMain/RELExit, which collide in a flat link.
`check_files_cmake.py` counts it under "module entry point" and passes.

## NEW: tools/flip_test.sh cannot locate the source of ANY REL unit

```
$ ./tools/flip_test.sh MetroidPrime/ScriptObjects/CFogOverlayRel.cpp
    no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CFogOverlayRel.cpp)
    - configure.py would link the retail object and this would pass while proving nothing
  FAIL  -> reverted
```

**Not caused by this change, and it is not specific to this unit.** `unit_info` in
`tools/flip_test.sh` decides a unit's source root by finding the last `MusyX(` before the entry
and comparing paren counts, and it counts parens inside `configure.py`'s **comments**. A comment
with one unmatched `(` (e.g. configure.py:1513 `Tryclops' head, .text 0x0..0x178: the two
second-vtable entries, `fn_81_10` (the`) leaves the running depth at 1 for the rest of the file,
so every later entry is misread as a MusyX unit. Verified against an **unmodified HEAD**
`configure.py` - the count is 87 open / 86 closed at `CIngPuddleRel`, 102/101 at
`CMysteryFlyerRel`, 278/277 at `CScriptCoinRel`; all three are long-landed `Matching` units.

So `flip_test.sh` **cannot judge any REL unit**, and the item brief's "the acceptance test" is
`flip_test.sh` for DOL units only - for a REL module the acceptance test is the module's sha1,
which held (`87 files OK`, and the independent re-hash in `gate.sh` step 3). Two ways to fix it,
both one line in the same function, and neither is this item's to make:
strip comments before counting (as `check_files_cmake.py` already does for `files.cmake`), or
take the root from the enclosing `Rel(`/`DolphinLib(`/`MusyX(` call by name rather than by paren
balance.

## What is left in the module (not filed as `NEW:`)

`fn_23_100` (0x100, 0x3E4) is the generated `SLdrFogOverlay` entity loader: a
`LoadTypedefSLdrEditorProperties` walk with a `switch` over the `kFogOverlay` property tag
constants, then `new` + the `__ct__6CActorF9TUniqueId...` chain. The sixteen methods above it are
the class's own (Draw, AcceptScriptMsg, PreThink helpers, the constructor). All need the
CActor/CPatterned hierarchy this tree does not model - the same wall as `Metaree`'s tail and
`IngSpaceJumpGuardian`'s 125 methods - so they are a class of work, not one function, and a
`NEW:` line would cost a lane an hour to rediscover the wall. The `switch` in `fn_23_100` is the
one part that is arithmetic rather than hierarchy, if anyone ever wants it.

---

# Second run (lane 7, 2026-09-30) - re-measured on a clean tree, 0 -> 7

**The previous run's work was not in this tree.** `git status` was clean, `CFogOverlayRel.cpp`
did not exist, `config/G2ME01/rels/FogOverlay/splits.txt` had no claim and
`build/goal/judge/report.base.json` measured `module:FogOverlay` at **0** matched functions. So
this is not `STALE:` - the notes above were the recipe and this run re-derived the same head from
scratch and got the same answer, which is itself the useful result. Everything above about the
head is confirmed on this tree; the two spellings below are new.

## What landed

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/CFogOverlayRel.cpp` | new, 5 functions, `.text 0x0..0x100` |
| `src/MetroidPrime/ScriptObjects/CFogOverlayRelStubs.cpp` | new, 2 functions, `.text 0x624..0x62C` |
| `config/G2ME01/rels/FogOverlay/splits.txt` | two claims |
| `config/G2ME01/rels/FogOverlay/symbols.txt` | `fn_23_8C`/`fn_23_B0` -> `RELExit`/`RELMain`, `scope:global` |
| `configure.py` | `Rel("FogOverlay", [Object(Matching, ...Rel.cpp), Object(Matching, ...RelStubs.cpp)])` |
| `files.cmake` | `CFogOverlayRelStubs.cpp` only (see the two exclusions below) |

`config/G2ME01/config.yml` **untouched**; the module still hashes
`1b3676a0af38a6792c3a14d932c8248d1f622d6a` and is `cmp`-identical to the disc's copy.

## Measurement: `module:FogOverlay` 0 -> 7 of 21

| | base | now |
| --- | --- | --- |
| `module:FogOverlay` matched_functions | 0 | **7** (of 21 in the module) |
| project matched_functions | 10218 | **10225** |
| linked (objdiff `complete_code`) | 5004 | **5011** |
| total_functions | 28465 | **28465** (unchanged) |

`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS progress-rel-head-fogoverlay`**
(all seven checks ok, `target rose: module:FogOverlay: 0 -> 7 / 21 functions`).

Both units are at 100.00% fuzzy / 100.00% matched, on the first build each. Nothing was tried
and abandoned.

## The new range: `.text 0x624..0x62C`, the module's two empty virtual overrides

`fn_23_624` (0x624) and `fn_23_628` (0x628) are **each a single `blr`** in retail, and both are
vtable entries of `lbl_23_data_0` - `.data:0x0` stores `fn_23_628` at vtable offset 0x24 and
`fn_23_624` at 0x2C (`build/G2ME01/FogOverlay/asm/auto_04_00000000_data.s`).

Three things make this the cheapest kind of claim, and all three are worth checking for on any
module before planning its head:

1. **The bodies really are empty, so nothing is stubbed.** `void fn_23_624() {}` reproduces
   `blr` because that is what an override that does nothing compiles to. The reviewer rule is
   about stand-ins for unwritten work; these are the work. Each is named and commented as an
   empty override in the file's own header, and each is referenced by the module's own `.data`,
   so neither is a dead-stripping hazard.
2. **They are already in dtk's FORCEACTIVE set** (`build/G2ME01/FogOverlay/ldscript.lcf` lists
   `fn_23_624` and `fn_23_628`), so no `force_active:` entry in `config.yml` is needed and
   `config.yml` stays untouched. **Read the module's `ldscript.lcf` before claiming a small
   unreferenced-looking range** - it says which of the module's own functions survive
   `-strip_partial` without a config change.
3. **They are 4 bytes apart, so one contiguous range and one file cover both** - no
   link-order cycle, no carve. The arrangement is the recipe's: one contiguous range, one file,
   only what the object reproduces.

Against the previous run's "five functions" this is the second slice of the same module: 7 of 21
with two units, and the `main.dol` and all 86 module hashes held throughout.

## What I tried and reverted, with the measurement

### `fn_23_4E4` (0x4E4, 0x54) - reached 100%, then had to be given back

`fn_23_4E4` is `SLdrFogOverlay`'s deleting destructor: MWCC's shape with **no vptr store**
(`SLdrEditorProperties`, the struct it extends, is not polymorphic), calling the base destructor
with a literal `-1`:

```
00000500  41 82 00 1C  beq  <epilogue>      ; the `if (this)` guard
00000504  38 80 FF FF  li   r4,-0x1         ; the base destructor's flag
00000508  48 00 0A D1  bl   __dt__20SLdrEditorPropertiesFv
0000050C  7F E0 07 35  extsh. r0,r31        ; `if (flag > 0)`, the SHORT flag
```

I wrote it as a third unit (`CFogOverlayRelLdrDtor.cpp`, `void* fn_23_4E4(void*, short)`) and it
compiled to retail's bytes **first try, 100.00% matched, 1/1** - the `short` flag parameter, the
pointer return type and the literal `-1` are the same three load-bearing details
`CStateManager.cpp` records and the head's own `fn_23_0` uses. Two things blocked it:

1. **It is dead-stripped, and `config.yml`'s `force_active:` is the documented fix.** With the
   claim in `splits.txt` the module's sha1 broke, and `nm` on the plf showed why precisely:
   `fn_23_4E4` is present in the preplf at 0x4e4 and **absent from the plf**, with every
   function after it shifted down 0x54 (`fn_23_538` at 0x538 in the preplf, 0x4e4 in the plf) -
   the `.rel` came out 96 bytes short. Nothing in the module references it: retail's own caller,
   `fn_23_100`, calls the *base* destructor `__dt__20SLdrEditorPropertiesFv` directly and never
   the derived one. Adding `force_active: [fn_23_4E4]` to the module's `config.yml` entry (the
   Tweaks precedent, lines 419-429) fixed the strip and the hash held - **so the strip itself is
   not the blocker.**
2. **The blocker is `files.cmake`, and it is not this item's to fix.** `gate.sh` step
   `files.cmake` runs `tools/check_files_cmake.py`, which fails for any `Matching` object in
   `configure.py` that is neither listed in `files.cmake` nor in that tool's own `EXCLUDED`
   dict. Listing it is a **measured gate failure**:
   `probe_sources.sh` -> `link_check: 1 symbol(s) this change ADDED to the gap: NEW
   __dt__20SLdrEditorPropertiesFv`, taking the port's undefined count 250 -> 251, and the
   baseline is 250. `__dt__20SLdrEditorPropertiesFv` has no PC-side definition anywhere - it is
   not in `build/goal/judge/undef.base.txt` and `src/MetroidPrime/ScriptLoader/
   SLdrStructMembers.cpp:189` records that the port deliberately never defines it - so a host link
   cannot bind it. The only other route is an `EXCLUDED` entry in `tools/`, which this item may
   not edit.

So the unit was reverted whole (file, `splits.txt` claim, `configure.py` entry and the
`config.yml` `force_active:`), because leaving it in fails the gate, and deleting the module's
`force_active:` while keeping the claim breaks the hash. **The finding for the next run: the
range is solved and reproducible - the only thing between it and 8 functions is one line in
`tools/check_files_cmake.py`'s `EXCLUDED` dict, in the same shape as the `CGameStateStreamCtor`
entry, with the reason measured above.** A `NEW:` is not filed for it: the target would be
`tools/check_files_cmake.py`, not a unit, module or symbol, and the prompt forbids a
`NEW:` whose work does not raise a count on its own.

### `files.cmake` - only the stubs are listed, and why

`CFogOverlayRel.cpp` is **not** in `files.cmake`: it defines `RELMain`/`RELExit`, which collide
in a flat link, and `check_files_cmake.py` counts that case separately ("39 further units are out
because they define a module entry point") rather than failing. `CFogOverlayRelStubs.cpp` **is**
listed - it defines no module entry point, it has no dependency at all (two `blr`s), and listing
it takes the port's link nowhere: `probe_sources.sh` measures `750 files, 0 failed, 0 errors;
LINKED (250 undefined, 0 duplicates)`, the baseline exactly. The reason is written into
`files.cmake` beside the line, not only here.

## Verification (all measured on this tree, none recalled)

```
./tools/decomp_build.sh -r FogOverlay/.../CFogOverlayRel FogOverlay/.../CFogOverlayRelStubs
  -> CFogOverlayRel:      100.00% fuzzy, 100.00% matched (5 / 5 functions)
  -> CFogOverlayRelStubs: 100.00% fuzzy, 100.00% matched (2 / 2 functions)
  -> CHECK config/G2ME01/build.sha1: 87 files OK
  -> All: 31.21% fuzzy, 23.50% matched, 11.82% linked (10225 / 28465 functions)

python3 -  # the check that means something, RUNNING_THE_DECOMP.md "The check that actually means something"
  -> 86/87 module hashes hold
cmp orig/G2ME01/files/RelProd/FogOverlay.rel build/G2ME01/FogOverlay/FogOverlay.rel
  -> byte-identical

python3 tools/check_decl_order.py --unit CFogOverlayRel
  -> ok: 2 unit(s) checked, none emits its functions out of retail order
python3 tools/audit_rel_claim.py FogOverlay
  -> ok CFogOverlayRel.cpp 0x00000000..0x00000100 5/5; ok CFogOverlayRelStubs.cpp
     0x00000624..0x0000062C 2/2; 0 claims with a problem
  -> FogOverlay: preplf 21 text symbols, plf 21, 0 dropped by -strip_partial
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CFogOverlayRel{,Stubs}.cpp
  -> .text claimed 256 ours 256 retail 256 fits / claimed 8 ours 8 retail 8 fits;
     no extra functions, both
python3 tools/check_symbol_names.py   -> 505 units, 0 declared names are missing
python3 tools/check_files_cmake.py    -> every configured DOL object is either in files.cmake
                                         or excluded with a reason   (exit 0)
./tools/probe_sources.sh              -> 750 files, 0 failed, 0 errors; LINKED (250 undefined,
                                         0 duplicates)
sha1sum build/G2ME01/main.dol          -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/goal_check.sh build/goal/item.json
  -> goal_check: PASS progress-rel-head-fogoverlay
```

`flip_test.sh` was not run and cannot be: the previous run's `NEW:` that it cannot locate the
source of any REL unit is **confirmed on this tree** - `unit_info` in `tools/flip_test.sh` picks
a unit's source root by paren-balancing `configure.py` including its comments, and
`configure.py:1513` (Tryclops' head comment, one unmatched `(`) leaves the depth at 1 for the
rest of the file, so `FogOverlay/MetroidPrime/ScriptObjects/CFogOverlayRel` is misread as a
`MusyX` unit and the script reports `no source file
(extern/musyx/src/MetroidPrime/ScriptObjects/CFogOverlayRel.cpp)` and refuses. The acceptance
test for a REL module is the module's sha1 against `config/G2ME01/config.yml`, which held both
here and in `gate.sh` step 3. `check_decl_order.py` is the in-tree substitute for the permutation
check and it passes on both units.

## Where the module stands now: 7 of 21 claimed, 14 left

Unclaimed and still retail, in address order:

- `fn_23_100` (0x100, 0x3E4) - the generated `SLdrFogOverlay` entity loader, a
  `LoadTypedefSLdrEditorProperties` walk with a `switch` over the `kFogOverlay` property tag
  constants, then `new` + the `__ct__6CActorF9TUniqueId...` chain. **The `switch` is the one
  part that is arithmetic rather than hierarchy**, and the constants are readable straight out of
  the disassembly (`cmpw` against `0xD22_0D22`... the 17 `lis`/`addi`/`subi` triples at 0x150-0x294
  are the property tags). Nothing after it is reachable, because it is 0x3E4 bytes and the next
  function boundary is 0x4E4.
- `fn_23_538` (0x538, 0xEC) - `SLdrFogOverlay`'s own default constructor: `SLdrEditorProperties`
  ctor, `CColor::Green`, 12 float stores and a `CColor` ctor. **Struct layout, not hierarchy** -
  it is a real candidate, and `include/MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp`
  already models the base. It is not FORCEACTIVE, so a claim needs the same `force_active:`
  treatment `fn_23_4E4` needed, and the same `files.cmake` question (`ReadFloat`/`__ct__6CColor`
  are defined; `__ct__9CVector3fFR12CInputStream` may not be - measure before promising).
- `fn_23_62C` (0x62C, 0x484), `fn_23_AB0` (0xAB0, 0xE4), `fn_23_B94` (0xB94, 0x60),
  `fn_23_BF4` (0xBF4, 0x168), `fn_23_D5C` (0xD5C, 0x25C), `fn_23_FB8` (0xFB8, 0x20),
  `fn_23_10E4`, `fn_23_1130` - the class's own methods (`Draw`, the two float helpers,
  `AcceptScriptMsg`, the constructor) and the module's `_epilog`/`_prolog` tail. All need the
  CActor/CPatterned hierarchy this tree does not model, the same wall as `Metaree`'s tail and
  `IngSpaceJumpGuardian`'s 125 methods. **No `NEW:`**: a class of work, not one function, and a
  `NEW:` line would cost a lane an hour to rediscover the wall.

The lesson from the previous run is confirmed and worth keeping: **read the first function of a
module's `.text` before planning its head, and read the module's `ldscript.lcf` before claiming
a small range.** FogOverlay's head is five functions where the family's is eighteen, and the
`ldscript.lcf` is what says the two 4-byte stubs are claimable with no config change at all.
