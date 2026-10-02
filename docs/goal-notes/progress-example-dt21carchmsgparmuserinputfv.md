# progress-example-dt21carchmsgparmuserinputfv

**Item:** `progress`, target `module:AIMannedTurret`; the reason names `fn_1_48C0` (retail
`.text 0x48C0..0x491C`, 0x5C = 92 bytes), the module's twin of the matched
`__dt__21CArchMsgParmUserInputFv` (`src/MetroidPrime/CArchMsgParmUserInput.cpp`, DOL 0x8001D7E0,
also 0x5C).

**Measured result: `module:AIMannedTurret` matched_functions 10 -> 11 / 95** (`build/report.json`,
per-function exact matches summed over every `AIMannedTurret/` unit, which is what
`goal_check.sh`'s `target_rose` reads). Project `matched_functions` 13155 -> 13156, `linked`
6245 -> 6246, `All: 37.14% fuzzy, 30.57% matched, 13.51% linked (13156 / 28465 functions)`.

The new unit `AIMannedTurret/MetroidPrime/ScriptObjects/AIMannedTurretDtor` reads **1/1,
100.00% fuzzy, `complete: true`** - Matching and really in the link. All 86 RELs still match
`config/G2ME01/config.yml`; `AIMannedTurret.rel` is `949b8c21caf1112b10d07748dbe8c32d3bd7efac`
and `cmp`-equal to `orig/G2ME01/files/RelProd/AIMannedTurret.rel`, and `main.dol` is still
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

## What landed - the carve, five files

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/AIMannedTurretDtor.cpp` | new, one function, `fn_1_48C0` |
| `configure.py` | `Object(Matching, "MetroidPrime/ScriptObjects/AIMannedTurretDtor.cpp")` in the existing `Rel("AIMannedTurret", ...)` block |
| `config/G2ME01/rels/AIMannedTurret/splits.txt` | `.text start:0x000048C0 end:0x0000491C` for that unit |
| `files.cmake` | the source, with the comment every module carve there carries |
| `config/G2ME01/config.yml` | `force_active: [fn_1_48C0]` on the module's entry - **not optional**, see below |

`fn_1_48C0` needed no rename in `symbols.txt`: the name is already what the object defines, so
dtk's name and ours are the same string. `tools/check_decl_order.py --unit` prints
`0 unit(s) checked` (it is a DOL tool and does not resolve a `Rel(...)` path - the same
limitation `docs/goal-notes/progress-rel-head-shrieker.md` records), and the unit is
single-function anyway.

The split moves 6 functions, not 1: dtk re-cuts `auto_00_00000018_text` into
`auto_00_00000018_text` (0x18..0x48C0, 78 functions) and `auto_00_0000491C_text`
(0x491C..0x4E08, 5 functions). The gate's per-function diff reports it as
`SPLIT ... (exact count match - a split, not a loss)` and nothing is WORSE or GONE.

## The declaration that produced the shape - copy this

The bytes are MWCC's deleting-destructor (`__dt`) convention for a class whose base subobject has
its own vptr: null-`this` guard, store the class's vtable, the **inlined base destructor's own
guard** (the second `beq` reuses the cr0 from `mr. r31,r3`), store the base's vtable, then
`dispose > 0` - `extsh.` says the parameter is a **`short`** - and `CMemory::Free(this)`:

```cpp
extern "C" char lbl_1_data_1E0[];
extern "C" char lbl_1_data_1EC[];
extern "C" void Free__7CMemoryFPCv(const void* ptr);

struct SAIMannedTurretVptr { void* mVptr; };

extern "C" void* fn_1_48C0(SAIMannedTurretVptr* self, short flag) {
  if (self) {
    self->mVptr = lbl_1_data_1EC;
    if (self) {
      self->mVptr = lbl_1_data_1E0;
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
```

Measured: this object is one `.text` section of exactly 0x5C bytes with exactly retail's five
relocations (`lbl_1_data_1EC` at +0x2/+0x6 `ADDR16_HA/LO`, `lbl_1_data_1E0` at +0x12/+0x16, and
`REL24 Free__7CMemoryFPCv` at +0x28), and no other section. The names come from
`config/G2ME01/rels/AIMannedTurret/symbols.txt` (`.data:0x1E0`/`.data:0x1EC`) and
`config/G2ME01/symbols.txt:12992` (`Free__7CMemoryFPCv = .text:0x802CE388`, `CMemory::Free`).

**The nested `if (self)` is load-bearing and the flat version does not work.** Written as two
stores in a row, MWCC hoists both address computations into r5/r6, drops the second guard and the
`mr r3,r31` before the call, and the body comes out **0x54 bytes** - measured, not guessed. The
indirectness is what buys the second branch.

## Why it is not written as a class - the measured wall (and where it ends)

`class B { public: virtual ~B() {} }; class C : public B { public: ~C(); }; C::~C() {}` compiles
to **these same 23 instructions**, but mwcceppc also emits both vtables into the object's `.data`
(0x18 bytes, two `R_PPC_ADDR32` relocations to the destructor). This module's
`lbl_1_data_1E0`/`lbl_1_data_1EC` are 12 zero bytes each with **no relocation at all** (measured:
`powerpc-eabi-objdump -s -j .data build/G2ME01/AIMannedTurret/obj/auto_04_00000000_data.o`, and
`-r` on the same object; the retail REL's own relocation table has no entry for either), and a
`.text`-only split does not claim the object's `.data`. So the class spelling cannot be made to
match this module, whatever the class is called. A pure-virtual variant was tried and is further
off: `virtual ~PB() = 0` makes the compiler emit a `bl __dt__2PBFv` into the base destructor and
the body grows a frame and two extra instructions.

**This is the same arrangement `src/MetroidPrime/Carve8000447C.cpp` already uses in the DOL** for
MWCC's deleting-destructor convention (`extern "C"`, `short flag`, `if (flag > 0)
Free__7CMemoryFPCv(self); return self;`), so it is a spelling the tree already accepts, not an
invention for this item.

## `force_active` is what actually lands a REL `Matching` unit - measured

The first build after the split gave a **wrong** module and the correct-looking number beside it,
which is worth recording because it is easy to be fooled by: `sha1sum
build/G2ME01/AIMannedTurret/AIMannedTurret.rel` read the correct `949b8c21...` because the REL had
not been rewritten yet by the ninja edge that owns it. Asking ninja for the `.rel` itself
(`ninja build/G2ME01/AIMannedTurret/AIMannedTurret.rel`) shows the truth: without the
`config.yml` entry, **`dtk shasum -c config/G2ME01/build.sha1` fails on this module alone**
(`86 files OK`, 1 mismatch), `.text` comes out `0x52EC` against retail's `0x5348` and the file
28828 bytes against retail's 28956 - our 0x5C function was dropped by `-strip_partial` although
the object is in the link and objdiff reports it at 100%. Adding `force_active: [fn_1_48C0]` to
the module's `config.yml` entry puts it in the generated `build/G2ME01/AIMannedTurret/ldscript.lcf`
FORCEACTIVE list and the module goes back to `949b8c21...`.

**Superseded claim:** `docs/goal-notes/progress-twin-rel-digitalguardian.md` (run 2) says the
module-level `force_active:` list "cannot be lifted from config.yml for a REL module the way the
2026-09-29 supersession claims". It can: this entry is `config/G2ME01/config.yml`'s third module
`force_active:` list (SandBoss and Tweaks are the others) and it is exactly what makes the sha1
hold. That note's other conclusion - that this 0x5C family is unreachable because a class
spelling emits vtables into `.data` - is true of the class spelling and false of this one.

## Why this is the worked answer for the other 220

`python3 tools/twin_scan.py --list` on this tree pairs **221 unmatched REL functions in 50
modules** with `src/MetroidPrime/CArchMsgParmUserInput.cpp` as their twin's source, and after
this change `scan()`'s `rel_example` for all 221 of them is this unit: measured with

```python
from twin_scan import scan
twins, rest, missing = scan('.')
hit = [t for t in twins if t['rel_example'] and t['rel_example'][1] == 'fn_1_48C0']
# 221, and ('AIMannedTurret/MetroidPrime/ScriptObjects/AIMannedTurretDtor', 'fn_1_48C0',
#          'src/MetroidPrime/ScriptObjects/AIMannedTurretDtor.cpp')
```

which is the "worked answer every later module item is pointed at" the item asked for, and
`fn_1_48C0` itself is no longer in the unmatched list. The recipe, for a `progress` item on any
of the 221:

1. read the function out of `build/G2ME01/<Module>/asm/auto_*_text.s` and take the two data
   addresses it stores (they are the module's own `lbl_<id>_data_<off>` vtables);
2. declare them `extern "C" char <name>[];`, declare `Free__7CMemoryFPCv`, write the nested-`if`
   body above with the two labels in retail's order and the `short` flag;
3. the four-file carve (`configure.py`, `config/G2ME01/rels/<Module>/splits.txt`, `files.cmake`,
   the source's own claim), `Matching`, one contiguous `.text` range;
4. check whether the symbol is already in the module's generated
   `build/G2ME01/<Module>/ldscript.lcf` FORCEACTIVE list. If it references data (a vtable slot)
   it already is - DigitalGuardian's `fn_14_8328`, `fn_14_8048`, `fn_14_7FEC` are, and its
   `lbl_14_data_670`/`lbl_14_data_688` are global in `obj/auto_04_00000000_data.o` (`nm` prints
   `D`), so the same body should land them with no config edit at all; if nothing references it,
   add the **module-level `force_active:`** entry as here;
5. verify with the module's sha1 against `config/G2ME01/config.yml` (`dtk shasum -c
   config/G2ME01/build.sha1` is the same check from the build side), and ask ninja for the
   `.rel` target, not the `.plf` - the `.plf` success says nothing about the module.

Not attempted here, and not a `NEW:` item: step 4's DigitalGuardian lead is inside that module's
own twin item, and the 220 other copies are what `goal_seed.py --only twin` already proposes.

## Gates

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: PASS progress-example-dt21carchmsgparmuserinputfv
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13155 -> 13156   linked 6245 -> 6246
  ok    check_symbol_names.py
  ok    All:  37.14% fuzzy, 30.57% matched, 13.51% linked (13156 / 28465 functions)
  ok    target rose: module:AIMannedTurret: 10 -> 11 / 95 functions
  ok    no asm added
```

and `GATE PASS bcafaea9+7 changed` on the same run:
`configure ok / ninja + build.sha1 ok / hashes vs config.yml ok / report ok / per-function diff
SPLIT / module wiring ok / dol_read ok / docs claims ok / gs offsets ok / raw offsets ok /
decl order ok / files.cmake ok / module order ok / port probe ok / port link gap ok / reach stubs
ok`. The port is untouched by construction: the file's body is inside `#ifdef __MWERKS__`, so the
host translation unit is empty and `lbl_1_data_1E0`/`lbl_1_data_1EC` never reach the port's link
gap. Measured on this tree, `tools/probe_sources.sh` prints `probe: 849 files, 0 failed, 0
errors; link: LINKED (286 undefined, 0 duplicates)` against the judge's baseline of 286, and
`tools/link_gap.py --rebuild` prints `ok: 279 MISSING symbol(s), all accounted for in
port_link_gap_list.md`. `check_raw_offsets.py` still prints `185 raw-offset site(s) in 79
file(s)` - the two stores are a one-member struct, not an offset.

## Config changes, as a list

`config/G2ME01/config.yml` only: under `modules:` -> the `files/RelProd/AIMannedTurret.rel`
entry, a `force_active:` list with one member, `fn_1_48C0`. `config/G2ME01/rels/AIMannedTurret/
splits.txt` gains one block. No other config or `tools/` file is touched.

## WALL / NEW

None. No `WALL:` line: the function landed on the first spelling that was tried with the nested
`if` (the class spelling and the flat spelling are recorded above as failed spellings, but they
were resolved inside this run). No `NEW:` item: the remaining 220 copies of this shape are inside
modules that already have twin items, and their target (a count that rises) is exactly what this
item's recipe unlocks rather than a new unit, module or symbol. The driver can requeue this id
as-is if it wants: the module still has 84 unmatched functions, and this unit's claim is one
range of its own that the next carve does not have to touch.
