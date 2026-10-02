# carve-80229eac

**Kind:** `match` **Target:** `MetroidPrime/ScriptLoader/Carve80229EAC` **Result: PASS.**

`./tools/goal_check.sh build/goal/item.json` printed, verbatim:

```
goal_check: item carve-80229eac (match) target=MetroidPrime/ScriptLoader/Carve80229EAC
goal_check: baseline /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal-L13/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13036 -> 13037   linked 6140 -> 6141
  ok    check_symbol_names.py
  ok    All:  36.97% fuzzy, 30.41% matched, 13.40% linked (13037 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve80229EAC.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-80229eac
```

Baseline matched on this lane was **13036**; the item's own `reason` said the same thing about the
mid-unit split, the slot and the host block, and every one of those three came out as it said.

## What I did - the carve is four files

- **`src/MetroidPrime/ScriptLoader/Carve80229EAC.c`** (new, plain C, 95 lines, 1 function):
  `void fn_80229EAC(void* loader) { lbl_80419590 = loader; }`, with `extern void* lbl_80419590;`
  and the `#ifndef __MWERKS__` host definition at the end (the slot is claimed by no unit of ours -
  see below).
- **`configure.py:849`** - `Object(Matching, "MetroidPrime/ScriptLoader/Carve80229EAC.c"),`
  between `Carve80229BBC.c` (848) and `IngPuddle.cpp` (now 850), i.e. in address order.
- **`config/G2ME01/splits.txt:1703-1704`** -
  `MetroidPrime/ScriptLoader/Carve80229EAC.c:` / `.text start:0x80229EAC end:0x80229EB4`,
  between `Carve80229BBC.c` (ends 0x80229BC4, 1700-1701) and `IngPuddle.cpp` (starts
  0x80229EB4, 1706). Nothing else claims those 8 bytes and the claim spans no gap.
- **`files.cmake:616`** - `src/MetroidPrime/ScriptLoader/Carve80229EAC.c`, after
  `Carve80229BBC.c` (615), before `Carve8022A3F4.c` (617).

Nothing else changed. No `symbols.txt` rename is needed (the retail name `fn_80229EAC` is
reproduced verbatim by a `.c` unit, which is why module 25's `bl fn_80229EAC` still resolves), no
other unit's comment was made false - `IngPuddle.cpp:6-8` is about the *next* setter, 0x80229EE0,
which is still unclaimed - and the module-25 comment at `configure.py:1797` ("both loaders are
unnamed DOL setters ... so no `symbols.txt` rename and no DOL change") stays true.

## What the function is, measured

`build/G2ME01/asm/MetroidPrime/ScriptLoader/Carve80229EAC.s:9-12` - dtk's listing for this claim,
generated from the DOL after the split - is the whole unit:

```
# .text:0x0 | 0x80229EAC | size: 0x8
.fn fn_80229EAC, global
/* 80229EAC 00226CAC  90 6D 98 10 */ stw r3, lbl_80419590@sda21(r0)
/* 80229EB0 00226CB0  4E 80 00 20 */ blr
.endfn fn_80229EAC
```

`symbols.txt:9827` `fn_80229EAC = .text:0x80229EAC; // type:function size:0x8 align:4`;
`symbols.txt:20768` `lbl_80419590 = .sbss:0x80419590; // type:object size:0x8 data:4byte`. The
shape is the family shape, byte-identical to the landed `fn_80227530` / `fn_80227538`
(`Carve80227530.c`), `fn_80232834` and `fn_80232868`: store the argument into a loader pointer's
`.sbss` slot and return, never reading it back, so only the address is needed.

**Callers.** One module only, module 25 (`config/G2ME01/config.yml:137-138`,
`files/RelProd/GeomBlobV2.rel`, sha1 `6aac53ef251d4fc41f0b07133dc710c7e0f567cc`, re-`sha1sum`ed
against `orig/` and unchanged), in
`build/G2ME01/GeomBlobV2/asm/MetroidPrime/ScriptObjects/CGeomBlobV2Rel.s`:
`RELExit` (0x2414) `li r3, 0x0` on line 27 then `bl fn_80229EAC` on line 29; `fn_25_2460` (0x2460)
`lis r3, lbl_25_bss_0@ha` (line 55) / `stwu r0, lbl_25_bss_0@l(r3)` (line 58) then the call on
line 59 with r3 = `&lbl_25_bss_0`, the module's own 4-byte loader record. So the DOL slot holds a
*pointer to* a loader slot - which is what `CGeomBlobV2Rel.cpp:104` (already landed) already says.

**The slot is not ours.** `lbl_80419590` sits in the `.sbss` gap between `gLoader_RsfAudio`
(`RsfAudio.cpp` ends 0x80419590) and `gLoader_IngPuddle` (`IngPuddle.cpp` starts 0x80419598), so
the matching build takes it from dtk's `auto_10_80419590_sbss.o`. The unit therefore claims
`.text` only, and the host block is what keeps the port's flat link from losing the symbol - the
same shape as `Carve80227530.c`.

## Verification, all measured on this tree this run

- `./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80229EAC.c` - `PASS -> kept as Matching`,
  `kept: 1 / 1 failed: 0 skipped: 0`.
- `./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve80229EAC.c` -
  `.text claimed 8 ours 8 retail 8 fits`, `no extra functions`.
- `sha1sum build/G2ME01/main.dol` - `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged.
- `./tools/decomp_build.sh` - `All: 36.97% fuzzy, 30.41% matched, 13.40% linked (13037 / 28465
  functions)`; **`total_functions` is 28465**, unchanged after the `splits.txt` edit. Against the
  judge baseline: `matched_functions` 13036 -> 13037, `matched_code` 1987248 -> 1987256,
  `complete_units` 814 -> 815, `complete_code` 875772 -> 875780, `total_units` 2142 -> 2143 - one
  new unit and no extra auto unit, which is note 1 below.
- `build/report.json`: `main/MetroidPrime/ScriptLoader/Carve80229EAC` = 8/8 bytes, 1/1 functions,
  complete. Neighbour `main/MetroidPrime/ScriptLoader/IngPuddle` unchanged at 44/44 bytes, 1/1.
- `./tools/carve_diff.sh 80229EAC 8 build/G2ME01/obj/MetroidPrime/ScriptLoader/Carve80229EAC.o` -
  `retail: 2 instructions, 8 bytes` / `ours: 2 instructions, 8 bytes`, one difference:
  `stw r3,-26608(r13)` vs `stw r3,0(0)`, i.e. the linker's `R_PPC_EMB_SDA21 lbl_80419590`.
  `build/binutils/powerpc-eabi-objdump -r -j .text` on the object shows exactly that one
  relocation and no other; `powerpc-eabi-nm` shows `T fn_80229EAC` and `U lbl_80419590` - the name
  stays unmangled, which is what module 25's two `bl fn_80229EAC` resolve against.
- `python3 tools/check_symbol_names.py` - `checked 576 units; 0 declared names are missing from
  their object`.
- `./tools/probe_sources.sh` - `816 files, 0 failed, 0 errors; link: LINKED (287 undefined,
  0 duplicates)`. 287 is the judge's own baseline `build/goal/judge/undef.base.count` and
  `build/probe-logs/status.txt:582` shows `src/MetroidPrime/ScriptLoader/Carve80229EAC.c 0` - the
  file compiles and the host block holds the undefined count flat, as intended.
- `gate.sh` rewrote `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` as a side effect of
  `goal_check.sh` - the same effect `docs/goal-notes/carve-80232834.md` note 4 records;
  `git checkout --` on exactly those two files after the judge left the tree at the four files of
  the change.

## Notes for the next run

1. **A carve at the *end* of an auto unit keeps the unit's name; only its size changes.** The
   auto unit was `auto_03_80229BC4_text.s` = `# 0x80229BC4..0x80229EB4 | size: 0x2F0`, **nine**
   functions; after the split the same file is `# 0x80229BC4..0x80229EAC | size: 0x2E8` with
   **eight**, and `build/report.json` lists it under the *same* name with 744 bytes / 8 functions
   (from 752 / 9) and no `matched_functions` key at all - it is `NonMatching` and
   `auto_generated: true`. No new auto unit appeared. That is the
   mirror of `build/goal/notes/carve-8023289c.md` note 1, where the carve was at the front/middle
   of its auto unit and the remainder came back under a new name - an auto unit is named by its
   start address, so the name survives only when the start does. Either way it is one `splits.txt`
   line and nothing else.
2. **Cite the *claim's* listing, not the pre-split auto listing.** After the split dtk emits
   `build/G2ME01/asm/MetroidPrime/ScriptLoader/Carve80229EAC.s` - the retail listing for the
   range we claim, with the retail symbol name on it - and it is regenerated from the DOL, so it
   cannot go stale under the claim. The pre-split `auto_*_text.s` is rewritten by the split and
   its line numbers move (this item's seed cited `:233-237`, which no longer exist), so a comment
   that points at it is stale the moment the carve lands.
3. **Grep scope trap.** `build/G2ME01/*/asm/` matches the **86 module** asm dirs only; the DOL's
   own listings are one level up in `build/G2ME01/asm/`. A lane that searches only the former and
   concludes "nothing references this symbol anywhere" has measured the modules, not the DOL -
   both paths have to be named, and `src/` for our own units. With all three,
   `lbl_80419590`'s only reader anywhere is none: `src/` has two comments
   (`CGeomBlobV2Rel.cpp:31,104`), the DOL's listings have the store here plus dtk's
   `auto_10_80419590_sbss.s` definition, and all 86 modules' listings have nothing.
4. Nothing is claimed about the other seven functions of the re-split auto unit
   (`fn_80229BC4` 0x64 ... `fn_80229E38` 0x74), which stay retail.

## NEW:

NEW: carve-80229ee0 | match | MetroidPrime/ScriptLoader/Carve80229EE0 | carve `fn_80229EE0` (`.text` 0x80229EE0..0x80229EE8, 8 bytes, `symbols.txt:9829`, the first `.fn` of dtk's `auto_03_80229EE0_text.s` `# 0x80229EE0..0x80229F90 | size: 0xB0`) - `stw r3, gLoader_IngPuddle@sda21(r0)` / `blr`, byte-identical to this item's carve. `IngPuddle.cpp` claims `.text` 0x80229EB4..0x80229EE0 (ends exactly here), `.sbss` 0x80419598..0x804195A0, and both *defines* `gLoader_IngPuddle` (line 16) and reads it back (line 19), so the carve takes it `extern` and claims `.text` only: no host block, and the port's undefined count provably cannot move. Callers are module 32 (`config/G2ME01/config.yml:171-175`, `files/RelProd/IngPuddle.rel`, sha1 `312b87acb1dea81e5c03fccd6b87e68366f17a6f`) in `build/G2ME01/IngPuddle/asm/MetroidPrime/ScriptObjects/CIngPuddleRel.s`: `RELExit` `li r3, 0x0` line 33 then `bl` line 35, and `fn_32_78` `stwu r0, lbl_32_bss_0@l(r3)` line 62 then `bl` line 63 (argument `&lbl_32_bss_0`). It is the *front* case of note 1 above (the remainder reappears under a new name), and it needs `IngPuddle.cpp:6-8`'s "deliberately NOT claimed" sentence corrected in the same change, as `AtomicBeta.cpp` and `MysteryFlyer.cpp` were for their setters.
