# progress-rel-ripper-loadertrio — DONE, judge-ready

`module:Ripper`, lane 1 (`wt-mp2-goal-L1`), 2026-09-29, HEAD `9a9fbf5` on `goal/lane-1`. Not
committed, per the driver. `matched` 9458 -> 9460, `linked` 4780 -> 4782, `module:Ripper`
**19 -> 21 / 56**, `total_functions` 28465 unchanged, the module's `.rel` byte-identical.

## The item's own work was already in the tree before this run started

`reason` says ".text 0xD8..0x178 is four functions". Those four functions were claimed and matched
by a *different* item, `rel-head-accessor-family-gaps`, committed as `efcdbce` (21:21) — after this
item was queued (19:36) and before this run began. Measured on the tree as found:

```
$ git log --all --oneline --grep=ripper -i | head -3
efcdbce progress: rel-head-accessor-family-gaps
$ grep -c CRipperRelMain config/G2ME01/rels/Ripper/splits.txt configure.py
1 / 2
$ python3 -c "…sum matched_functions over units named Ripper/…"
build/report.base.json module:Ripper 19 / 56      <- the judge's own baseline, at HEAD
build/report.json      module:Ripper 19 / 56      <- unchanged: nothing to do
```

`Ripper/MetroidPrime/ScriptObjects/CRipperRelMain` was already `complete: true`, 4/4 at 100.00%
(`fn_54_D8`, `RELExit`, `RELMain`, `fn_54_148`), and `build/report.base.json` — the judge recorded
it from HEAD — already read 19. So **a lane that trusted the `reason` and stopped there would have
scored 19 -> 19 and failed the item.** Re-measuring first is the whole reason the prompt says to.

The queue also still carries the `NEW: progress-rel-ripper-loadertrio` line that
`progress-rel-extend-ripper` filed for this exact range. **It should be retired**: its work landed
as `efcdbce`, and this item is its re-run.

## What this run landed instead: the next reachable range, .text 0x514..0x55C

Three files plus a new source, the four files a carve is:

- `src/MetroidPrime/ScriptObjects/CRipperForwarders.cpp` — **new**, 2 functions, 72 bytes.
- `config/G2ME01/rels/Ripper/splits.txt` — a third claim, 0x514..0x55C.
- `configure.py` — the `Rel("Ripper", ...)` comment and the third `Object(...)`.
- `files.cmake` — one line; see "why this one is listed" below.
- `docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md` — machine-made, by
  `python3 tools/check_docs_claims.py --write` (state block 9458->9460, 4780->4782, REL units
  1373->1375, probe 735->736, module wiring 83->84). Nothing else in either file was touched; the
  driver rewrites these from the tree anyway.

The two functions are the only ones in the module's behavioural block that are more than a constant:

```
0x514  fn_54_514  0x20  stwu / mflr / stw r0,0x14 / bl fn_54_534 / lwz / mtlr / addi / blr
0x534  fn_54_534  0x28  stwu / mflr / cmplwi r3,0 / stw r0,0x14 / beq / bl fn_54_55C / lwz / mtlr / addi / blr
```

Both pass both arguments through untouched, so the bodies are `fn_54_534(p, q)` and
`if (p) fn_54_55C(p, q)` — no member offset, nothing to name, and nothing to fake. `fn_54_534`'s
`cmplwi r3,0` lands *between* the `mflr` and the `stw r0,0x14(r1)`, which is the schedule MWCC
produces from that source without being told to, so this is a spelling that matched rather than one
that was tuned to. Neither function is in CRipper's vtable (`.data:0x48`, `lbl_54_data_48`, 84
words, `build/G2ME01/Ripper/asm/auto_04_00000000_data.s`), which is why dtk names them for their
offsets and `config/G2ME01/rels/Ripper/symbols.txt` needs no rename. `fn_54_55C` is called by name
and defined nowhere, so dtk fills it from retail.

**Why this one is in `files.cmake` and `CRipperRelMain.cpp` is not.** The module-head files define
`RELMain`/`RELExit`, which collide in a flat host link. This one defines neither, but it *calls*
`fn_54_55C`, a symbol of the module that the port does not have — so its two definitions are inside
`#ifdef __MWERKS__`, the arrangement `KrocussAccessors.cpp` and `RipperAccessors.cpp` both use, and
on the host the file is an empty translation unit. Measured: the port's undefined count is
**254 -> 254**, 0 duplicates.

## Measurements

| check | result |
| --- | --- |
| `sha1sum build/G2ME01/Ripper/Ripper.rel` | `f3ab11c967c58f4483a4264fbeb1ba4a837e8719` = `config.yml:276` = HEAD |
| `cmp … orig/G2ME01/files/RelProd/Ripper.rel` | identical; `decomp_build.sh` prints `87 files OK` |
| `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` |
| `decomp_build.sh` | `All: 29.11% fuzzy, 21.25% matched, 11.37% linked (9460 / 28465 functions)`; `total_functions` 28465 |
| `module:Ripper` | `19 -> 21 / 56` (`build/report.base.json` -> `build/report.json`) |
| `Ripper/MetroidPrime/ScriptObjects/CRipperForwarders` | `2 / 2 functions`, 100.00% fuzzy, `total_code 72` |
| `./tools/unit_fit.sh MetroidPrime/ScriptObjects/CRipperForwarders.cpp` | `.text claimed 72 ours 72 retail 72 fits`; `no extra functions` |
| `python3 tools/audit_rel_claim.py Ripper` | `0x0..0xD8 15/15`, `0xD8..0x178 4/4`, `0x514..0x55C 2/2`; 0 claims with a problem; `preplf 56 text symbols, plf 56, 0 dropped` |
| `python3 tools/check_decl_order.py --unit Ripper/MetroidPrime/ScriptObjects/CRipperForwarders` | `ok` (sources are in descending retail order: `fn_54_534` then `fn_54_514`) |
| `python3 tools/check_symbol_names.py` | `checked 484 units; 0 declared names are missing` |
| `python3 tools/check_raw_offsets.py` | `150 raw-offset site(s) in 59 file(s)`, all documented — this file adds **none** (it has no offsets) |
| `python3 tools/check_files_cmake.py` | `every configured DOL object is either in files.cmake or excluded with a reason`; 0 dead |
| `python3 tools/check_module_wiring.py` | `84 unit(s) of our own code in 64 module(s)` (was 83) |
| `./tools/probe_sources.sh` | `736 files, 0 failed, 0 errors; LINKED (254 undefined, 0 duplicates)` |
| `./tools/link_check.sh --strict` | `STRICT PASS - 254 undefined against a baseline of 254 (no growth), 0 duplicates, 0 compile errors` |
| `./tools/gate.sh build/report.base.json` | **GATE PASS 9a9fbf5+6 changed**; the per-function diff line reads `SPLIT Ripper/auto_00_00000178_text: 29 function(s) moved into Ripper/MetroidPrime/ScriptObjects/CRipperForwarders, Ripper/auto_00_0000055C_text (exact count match - a split, not a loss)` |
| `python3 tools/check_docs_claims.py` | `docs claims agree with the tree` (after `--write`) |

`flip_test.sh` was not run, for the reason every landed REL unit in this module gives: the unit is
declared `Matching` and stays `Matching`, and `AGENTS.md` names the module sha1 against
`config/G2ME01/config.yml` plus the `cmp` as the acceptance test for a REL unit. Both hold, and
`gate.sh` checks all 86.

## The wall, measured: `fn_54_158C` (.text 0x158C, 0x3C) will not match

**This is the one function of the block that is not CRipper class code, and it is the obvious next
claim, and it does not match.** It is `rstl::optional_object<CAABox>`'s converting constructor, out
of line, called by `RipperAccessors.cpp`'s already-matched `fn_54_0`; the same fifteen instructions
are `fn_45_2BBC` in `MysteryFlyer` (0x2BBC) and `fn_81_4FEC` in `Tryclops` (0x4FEC), both unclaimed
there too. Retail:

```
li r0,0x1 / lwz r5,0(r4) / stb r0,0x18(r3) / lwz r0,4(r4) / stw r5,0(r3) / lwz r5,8(r4) /
stw r0,4(r3) / lwz r0,0xc(r4) / stw r5,8(r3) / lwz r5,0x10(r4) / stw r0,0xc(r3) /
lwz r0,0x14(r4) / stw r5,0x10(r3) / stw r0,0x14(r3) / blr
```

`uchar m_data[sizeof(T)]` followed by `bool m_valid ATTRIBUTE_ALIGN(4)` is 0x18 + 4 = 0x1C, and
the copy is six **word** moves. Five spellings, each built with `./tools/decomp_build.sh` and read
back with
`build/binutils/powerpc-eabi-objdump -d` — sizes are exact from the third on, and no two agree with
retail:

| spelling | object | verdict |
| --- | --- | --- |
| `new (out) rstl::optional_object<CAABox>(box)` | **0x44** | MWCC's placement-new null check (`cmplwi r3,0; beqlr`) plus `stb` 2nd, copy grouped `lwz,lwz,stw,stw` |
| same with a reference parameter `optional_object<CAABox>& out` | **0x44** | identical — a reference does not suppress the check |
| class inside `extern "C"` deriving from `optional_object`, ctor in-class | **empty** | MWCC drops it |
| same, ctor defined out of line | **0x3C** (+ a weak `__dt__Q24rstl24optional_object<6CAABox>Fv`, +0x3C) | body is the right size and the right instructions, but the ctor is **mangled** — `__ct__10fn_54_158CFRC6CAABox` — so it cannot be the symbol `fn_54_158C` |
| hand-written stand-in `struct { uchar m_data[sizeof(CAABox)]; bool m_valid; }`, body `*reinterpret_cast<CAABox*>(m_data) = box; m_valid = true;` | **0x3C** | 12 of 15 instruction slots differ from retail: `stb` lands *last*, the copy uses r6/r5 and clobbers r4 |
| same with `CAABox m_data;` as a typed member and a constructor | **0x44** | MWCC copies floats (`lfs`/`stfs`), which retail does not |

Two findings worth keeping, both of which the tree can now state as fact rather than as a guess:

1. **A free function cannot be retail's shape here.** Placement `new` costs 8 bytes of null check
   and a *reference* parameter does not remove it, while the ctor that does have retail's size is
   only reachable as a mangled member. There is no spelling found that both keeps the name
   `fn_54_158C` and drops the check.
2. **The last 8 bytes are scheduling.** The 0x3C spellings emit the same 15 instructions with the
   same registers in a different order: retail pipelines the copy (each store trails one load) and
   puts the flag store third; the two reachable schedules put it second or last.

WALL: fn_54_158C 0/1 functions matched - five spellings, three of them exactly 0x3C, and the
remaining difference is the order of the `stb` against a six-word copy; MWCC's placement `new` adds
a null check a reference parameter does not remove, and the ctor that has the right size is only
reachable mangled.

## The rest of the module, mapped for whoever picks it up

56 functions in `config/G2ME01/rels/Ripper/symbols.txt`, 21 ours after this change, **35 left**, and
the three groups are counted off the symbols file rather than estimated:

- **0x178..0x15C8 held 31 functions; 29 of them are still unclaimed** (`auto_00_00000178_text` is
  now 0x178..0x514 and 0x55C..0x15C8, 31 and 27 functions, with `fn_54_514`/`fn_54_534` ours). This
  is CRipper's own behaviour, and **the module has no `CRipper` class anywhere in this tree** —
  `find . -name 'CRipper*' -not -path './build/*'` returns only `CRipperRelMain.cpp` and
  `CRipperForwarders.cpp`. `fn_54_178` (0x178, 0x35C) is the entity loader: `stwu r1,-0x790(r1)` and
  an `SLdrEditorProperties` construction. What the block calls, from the module's own asm:
  `__ct__`/`__dt__` of `CToken`, `CAnimData`, `CModelData`, `SLdrActorParameters`,
  `SLdrEditorProperties`, `CBodyStateInfo`, `CBodyStateCmdMgr`, `CDamageVulnerability` and `CAi`;
  `CPatterned`'s `Patrol`, `PreThink`, `Think`, `SetupStateMachine`, `AcceptScriptMsg`,
  `KnockBack`; `CPhysicsActor`'s `Stop`; `CBodyController`'s `Activate`; `CActor`'s `SetActive`,
  `SetMuted`, `SetTransform`; `CStateManager`'s `AllocateUniqueId`, `ObjectById`,
  `DeleteObjectRequest`; and the `rstl` string refcount helpers in `fn_54_968`/`fn_54_A90`. The
  members are at offsets (0x1C, 0x28, 0x4C …) that nothing in the tree can name. Of the block, the
  only functions that are *not* member code are the forwarder pair this item took and
  `fn_54_158C` above.
- **`fn_54_15C8` (0x15C8, 0x50)**, its own auto unit: a `.ctors` static-initialiser thunk. It
  `lwzu`s `lbl_54_data_0` and copies 12 bytes into `lbl_54_data_C` and `lbl_54_data_28` — three
  `.data` relocations a unit would have to reproduce, not a body to translate.
- **`0x1618..0x17BC, 5 functions**, `auto_00_00001618_text`: `_unresolved` (0xC4), `_epilog`,
  `_prolog`, `fn_54_1724` and `fn_54_1770` (0x4C each, the dtor- and ctor-table runners). CRT and
  linker glue — `OSReport`, `OSGetStackPointer`, walks of `_ctors`/`_dtors` — and every REL module
  carries the same five. Not source.

**One more increment is sitting there and is deliberately not in this diff:** `fn_54_C18`
(0xC18, 0x8, `li r3,0; blr`) is a vtable entry — the second-to-last word of `lbl_54_data_48`,
between `RenderIngSnatchingTransition__10CPatternedFR13CStateManagerRC12CTransform4fRC11CModelFlags`
and `fn_54_B8C` — so it is a predicate that is always false, exactly like `fn_54_54` and `fn_54_5C`
in this module's already-landed accessor unit. `bool fn_54_C18(const void* self) { return false; }`
compiles to those two instructions. It was left out because a claim whose entire content is two
instructions is scaffolding, and one unit per file would mean a second source, a second splits
entry and a second `files.cmake` line for 8 bytes. Whoever wants it has the spelling.

No `NEW:` is filed. The remaining work is the block above, which needs a `CRipper` class and its
layout, and the `fn_54_158C` wall, which is characterised above. A `NEW:` costs a lane about an
hour and neither is an hour away from a count.
