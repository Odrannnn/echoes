# rel-head-accessor-family-gaps — DONE, judge PASS

`module:Ripper`, lane 2 (`wt-mp2-goal-L2`), 2026-09-29, HEAD `32721a9`. **Landed, not blocked.**
`matched` 9437 -> 9441, `linked` 4773 -> 4777, `module:Ripper` **15 -> 19 / 56**,
`total_functions` 28465 unchanged. Not committed, per the driver.

## The item's `reason` was stale — the work it described had already landed

`reason` says the function in front of Ripper's accessor claim "is a one-function extension of the
same kind", i.e. `fn_54_0`. **That is already in the tree**: `git log --all --oneline --grep=ripper -i`
finds `b5f02d5 progress: progress-rel-extend-ripper`, `config/G2ME01/rels/Ripper/splits.txt` reads
`start:0x00000000`, and `fn_54_0` sits at the end of `RipperAccessors.cpp`. So the "one-function
extension" this item was queued for is done, and re-filing it would have been a no-op.

The second half of the `reason` is **also superseded**: it says "`Ripper` is in the module table as
*blocked* (no CRipper, no CPatterned, no `include/MetroidPrime/Enemies/`)". Measured on this HEAD:
`include/MetroidPrime/Enemies/CPatterned.hpp` exists, `TypesMatch__7CRipperCFi` and
`PreThink__10CPatternedFfR13CStateManager` are in the module's own vtable dump, and `check_module_
wiring.py` counts Ripper among the modules linking our code. **The blocker the note warned about is
gone; the module is not blocked at the head.**

So this run did the next real increment, which is what the item's target still asks for
(`module:Ripper` matched count strictly up): the **loader trio plus the vtable entry above it**,
`.text 0xD8..0x178` — exactly the `NEW: progress-rel-ripper-loadertrio` line that
`progress-rel-extend-ripper`'s notes had filed. That `NEW:` is now spent and should be retired.

## What landed — 5 files

- `src/MetroidPrime/ScriptObjects/CRipperRelMain.cpp` — **new**, 4 functions, `0xD8..0x178`.
- `config/G2ME01/rels/Ripper/splits.txt` — a second claim, 0x2 lines.
- `config/G2ME01/rels/Ripper/symbols.txt` — the rename below, 2 lines.
- `configure.py` — the `Rel("Ripper", ...)` comment and the second `Object(...)`.
- `docs/HANDOFF.md` — the state block, via `python3 tools/check_docs_claims.py --write`.

**Not** in `files.cmake`, deliberately — see the note at the top of the new source. That is the
fourth file of a carve and here it is a *non*-edit, which is what `CMysteryFlyerRel.cpp` and
`CIngSnatchingSwarmRel.cpp` do; `check_files_cmake.py` counts these under "further units are out
because they define a module entry point (RELMain/RELExit), which collides in a flat link".

## Measured

```
./tools/decomp_build.sh   All: 29.09% fuzzy, 21.22% matched, 11.37% linked (9441 / 28465)
  Ripper/MetroidPrime/ScriptObjects/RipperAccessors  15/15  100.00%
  Ripper/MetroidPrime/ScriptObjects/CRipperRelMain    4/4   100.00%
sha1sum build/G2ME01/Ripper/Ripper.rel   f3ab11c967c58f4483a4264fbeb1ba4a837e8719  (== config.yml, == HEAD)
cmp      build/G2ME01/Ripper/Ripper.rel  orig/G2ME01/files/RelProd/Ripper.rel  -> identical
sha1sum  build/G2ME01/main.dol          6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (unchanged)
all 86 RELs cmp-equal to orig:  identical=86  differing=0
config.yml re-hash (86 modules, 0 differ)
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CRipperRelMain.cpp
   .text claimed 160  ours 160  retail 160  fits;  no extra functions
python3 tools/audit_rel_claim.py Ripper
   ok RipperAccessors.cpp  0x00000000..0x000000D8  15/15
   ok CRipperRelMain.cpp   0x000000D8..0x00000178   4/4
   0 claim(s) with a problem;  preplf 56 text symbols, plf 56, 0 dropped by -strip_partial
python3 tools/check_decl_order.py --unit Ripper/MetroidPrime/ScriptObjects/CRipperRelMain  ok
./tools/link_check.sh --strict  254 undefined against a baseline of 254 (no growth), 0 dups, 0 errors
./tools/gate.sh build/goal/judge/report.base.json   GATE PASS  32721a9+5 changed
./tools/goal_check.sh build/goal/item.json          goal_check: PASS
  ok target rose: module:Ripper: 15 -> 19 / 56 functions
  ok counts: matched 9437 -> 9441   linked 4773 -> 4777
  ok no asm added   ok no judge-owned path touched
```

`gate.sh`'s per-function diff line is the only structural change:
`SPLIT Ripper/auto_00_000000D8_text: 35 function(s) accounted for across 2 new unit(s) in Ripper
(exact count match - a split, not a loss)`. `total_functions` in the module's `symbols.txt` is 56
before and after, as it must be.

## The one thing that was NOT free: the module's `symbols.txt` rename

`fn_54_104` and `fn_54_128` are dtk's names for Ripper's RELExit and RELMain, and dtk does not know
they are those. **The first build failed at the REL step**, not at compile:

```
[2/8] LINK build/G2ME01/Ripper/Ripper.preplf
[3/8] LINK build/G2ME01/Ripper/Ripper.plf
[4/8] REL
Caused by:
    Failed to find symbol fn_54_104 in any module
```

`config/G2ME01/rels/Ripper/symbols.txt` is what dtk's `rel make` resolves against, so the source
had to agree with *it* or the link could not complete. The fix is the IngSnatchingSwarm-style rename
both prior `NEW:` lines predicted, and it is two lines:

```
- fn_54_104 = .text:0x00000104; // type:function size:0x24
- fn_54_128 = .text:0x00000128; // type:function size:0x20
+ RELExit    = .text:0x00000104; // type:function size:0x24 scope:global
+ RELMain    = .text:0x00000128; // type:function size:0x20 scope:global
```

`scope:global` is copied from the eleven modules that already carry the names (`MysteryFlyer`,
`IngSnatchingSwarm`, `BacteriaSwarm`, `Tryclops`, `MetareeSwarm`, `AtomicAlpha`, `FishCloud`,
`PlantScarabSwarm`, `SnakeWeedSwarm`, `IngPuddle`, `Splitter` — `grep -c 'RELMain\|RELExit'` is 2 in
each). **This is a fifth file in the carve, not a fourth** — a carve that claims a module entry is
`configure.py`, `splits.txt`, `symbols.txt`, `files.cmake`, and the source. The brief's "four files"
counts `files.cmake`; when the unit is *not* listed there, the rename moves into `symbols.txt`
instead. Both prior `NEW:` lines named this as the one unknown, and it was correct.

## The import name: long-mangled, and it is already in the DOL

`fn_54_104` and `fn_54_148` both call
`SetLoader_Ripper__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity`. Measured, not
assumed, in three places:

- `strings build/G2ME01/Ripper/Ripper.preplf | grep -i setloader` -> exactly that long name, and
  nothing else. This is what distinguishes Ripper from `BacteriaSwarm`/`Tryclops`, whose preplf
  import table carries the plain `fn_802…` name.
- `config/G2ME01/symbols.txt:9539` has
  `SetLoader_Ripper__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity = .text:0x8021BBD0;
  // type:function size:0x8` — the DOL's side of the import, already named.
- The module's dtk asm prints that name on both `bl` sites.

**So no `symbols.txt` rename is needed for the import and the DOL is untouched** — the name the
module wants is already what the DOL exports. Writing it out in full in an `extern "C"` block is
sufficient, which is the `CIngSnatchingSwarmRel.cpp` arrangement; the short spelling would compile,
link every object and then fail the REL step with `Failed to find symbol SetLoader_Ripper in any
module`.

## `fn_54_D8` is a vtable entry, and thirteen virtuals is measured, not guessed

`fn_54_D8` is `lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr r12 / bctrl` in a 0x2C body, and dtk puts
it in the module's FORCEACTIVE list. `build/G2ME01/Ripper/asm/auto_04_00000000_data.s` shows
`.data:0x48` (CRipper's vtable, 0x150 bytes) storing it at offset 0x3C, immediately after
`HealthInfo__3CAiFv` at 0x38. So it is a member call on slot 0x38, and the thirteen-virtual
stand-in class puts the last virtual there. That is the count `CMysteryFlyerRel.cpp` and
`CIngSnatchingSwarmRel.cpp` each measured, not a copy of a guess: loading the vtable by hand
compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, because mwcceppc only reaches for r12
on its own virtual-dispatch path.

**Our object is instruction for instruction retail's** (from
`powerpc-eabi-objdump -d -r build/G2ME01/Ripper/obj/.../CRipperRelMain.o`), including the `lis`/`addi`
pair and the `stwu r0,lbl_54_bss_0@l(r3)` that stores the slot and leaves r3 holding its address:

```
00000000 <fn_54_D8>:  stwu r1,-16(r1) / mflr r0 / stw r0,20(r1) / lwz r12,0(r3) / lwz r12,56(r12)
                      / mtctr r12 / bctrl / lwz r0,20(r1) / mtlr r0 / addi r1,r1,16 / blr
0000002c <RELExit>:   ... li r3,0 ... bl  R_PPC_REL24 SetLoader_Ripper__FP...
00000050 <RELMain>:   ... bl   R_PPC_REL24 fn_54_148
00000070 <fn_54_148>: lis r4,fn_54_178@ha / lis r3,lbl_54_bss_0@ha / stw r0,20(r1)
                      / addi r0,r4,fn_54_178@l / stwu r0,lbl_54_bss_0@l(r3) / bl SetLoader_Ripper__FP...
```

Symbol order in the built object is `fn_54_D8` (0x0), `RELExit` (0x2C), `RELMain` (0x50),
`fn_54_148` (0x70) — ascending, which is what the descending source order has to produce.

## Two things that are NOT problems, measured so the next lane does not re-open them

1. **`RELExit`, `RELMain` and `fn_54_148` are not in `build/G2ME01/Ripper/ldscript.lcf`'s FORCEACTIVE
   list; only `fn_54_D8` is.** This is not a dead-stripping hazard and does not need fixing.
   `MysteryFlyer`'s built ldscript has **zero** `RELMain`/`RELExit` entries either, and its module
   reproduces. RELMain/RELExit are the module's *exported* entry points, so the plf's export list
   roots them; `fn_54_148` is reachable from `RELMain`. The proof that nothing was stripped is the
   module sha1 and the `cmp` — `audit_rel_claim.py` still reports `preplf 56 text symbols, plf 56,
   0 dropped by -strip_partial`, the same 56 as before the change.
2. **`flip_test.sh` FAILs on this unit, and that is expected.** It printed
   `no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CRipperRelMain.cpp) - configure.py
   would link the retail object and this would pass while proving nothing`, then `-> reverted (tree
   rebuilt: DOL 6ef9b491...)`. `flip_test.sh` is **DOL-only**; a REL unit is not in its scope at
   all. `AGENTS.md` names the module's sha1 against `config/G2ME01/config.yml` plus the `cmp` as the
   acceptance test for a REL unit, and both were re-verified after the revert:
   `f3ab11c967c58f4483a4264fbeb1ba4a837e8719`, `IDENTICAL`, `module:Ripper 19 / 56`. **Do not run
   `flip_test.sh` on a REL unit** — it costs a full rebuild and reports a failure that means
   nothing. (It left the tree clean; re-check `git status` and the sha1s afterwards, which I did.)

## What is left on this module

37 of 56 functions unclaimed, and the wall is unchanged and now precisely located:
`fn_54_178` (0x178, **0x35C** bytes) is Ripper's own entity loader. It opens a `stwu r1,-0x790(r1)`
frame and its third act is `addi r26,r1,0x430; bl __ct__20SLdrEditorPropertiesFv`, so it is
CActor/CPatterned behavioural class code — the hierarchy this tree does not model. The 36 functions
above it (0x4D4..0x15C8) are the same. `fn_54_158C` (0x158C, 0x3C) is the one exception and is
already reachable: it is `optional_object<CAABox>`'s converting constructor, called out of line by
`fn_54_0`, so it can be taken as a single unclaimed neighbour with no new class needed. That is the
next item on this module, and it is a one-function carve of the *last* unit
(`auto_fn_54_15C8_text`, 1 function, currently 0 matched).

## NEW: none filed

`progress-rel-ripper-loadertrio`, filed by `progress-rel-extend-ripper`, **is this item's work** and
is now spent. Refiling it would be a duplicate for the driver to triage, and the driver should retire
it. The two `NEW:` lines about this module that remain accurate for the *next* run are already in the
notes above (the `fn_54_158C` carve) — but a one-function carve of a single `optional_object` ctor is
small enough to fold into whatever item touches Ripper next, and I have not filed it: the item brief
is explicit that a `NEW:` must name work whose success raises a count, and I have measured this one
only as "reachable", not as "spelled and scoring 100%". Filing an unmeasured item would be the
error the brief warns about.
