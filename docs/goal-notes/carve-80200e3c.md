# carve-80200e3c

**Kind:** `match` **Target:** `MetroidPrime/ScriptLoader/Carve80200E3C` **Result: PASS.**

`tools/goal_check.sh build/goal/item.json` printed, verbatim:

```
goal_check: item carve-80200e3c (match) target=MetroidPrime/ScriptLoader/Carve80200E3C
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12486 -> 12487   linked 5867 -> 5868
  ok    check_symbol_names.py
  ok    All:  35.30% fuzzy, 29.11% matched, 12.91% linked (12487 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve80200E3C.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-80200e3c
```

## What I did

Four files, the carve shape, each entry in address order:

- **`src/MetroidPrime/ScriptLoader/Carve80200E3C.c`** (new, plain C, 1 function).
- **`configure.py:732`** - `Object(Matching, "MetroidPrime/ScriptLoader/Carve80200E3C.c"),`
  between `SpacePirate.cpp` (731) and `Kralee.cpp` (733), one line.
- **`config/G2ME01/splits.txt:1145-1146`** -
  `MetroidPrime/ScriptLoader/Carve80200E3C.c:` / `.text start:0x80200E3C end:0x80200E44`,
  between the `SpacePirate.cpp` entry (which ends at 0x80200E3C) and the `Kralee.cpp` entry
  (0x80200E44). Nothing else claimed; the range is exactly retail's.
- **`files.cmake:543`** - added after `Carve801FF4A4.c`, before `Carve80201418.c`.

Plus one comment fix that my change made false, in the same change:
**`src/MetroidPrime/ScriptLoader/SpacePirate.cpp:6-11`** said the 8-byte setter at 0x80200E3C
was "deliberately NOT claimed ... must stay in dtk's auto unit". It now records that the
setter is its own unit and why a carve keeps the name.

## What the function is, measured

`build/G2ME01/asm/auto_03_80200E3C_text.s` is the whole of it:

```
.fn fn_80200E3C, global
/* 80200E3C */ stw r3, gLoader_SpacePirate@sda21(r0)
/* 80200E40 */ blr
```

The twin named in the item (`SetLoader_WallWalker__FPPFR13CStateManagerR12CInputStreamRC11CEntityIn`,
`src/MetroidPrime/ScriptLoaderRel.cpp`) has the same two instructions, so the body is one
store. The argument is module 72's loader record: both callers are in
`build/G2ME01/SpacePirate/asm/MetroidPrime/ScriptObjects/CSpacePirateRel.s` - `RELExit` passes
`li r3,0`, and `fn_72_D4` passes `r3 = lbl_72_bss_24` after copying one `FScriptLoader` and two
CodeWarrior pointers-to-member-function there word for word. `auto_05_00000000_bss.s` gives
`lbl_72_bss_24 size:0x1C`, and `CSpacePirateRel.cpp` records the three signatures, so the
parameter is spelled `struct SSpacePirateFuncPtrs*`.

## Verification

- `./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80200E3C.c` - `PASS -> kept as
  Matching`. DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 87 files OK.
- `./tools/decomp_build.sh` - `total_functions` is **28465**, unchanged, and DOL matched
  functions went `10938 -> 10939` out of 16726.
- `build/report.json`, unit `main/MetroidPrime/ScriptLoader/Carve80200E3C`:
  `fuzzy_match_percent 100.0`, `matched_code "8"/"8"`, `matched_functions 1/1`,
  `complete_units 1`, `complete: true`.
- `./tools/carve_diff.sh 80200E3C 8 build/G2ME01/src/MetroidPrime/ScriptLoader/Carve80200E3C.o`
  - `retail: 2 instructions, 8 bytes` / `ours: 2 instructions, 8 bytes`; the single reported
  difference is `stw r3,-27176(r13)` vs `stw r3,0(0)`, i.e. the linker's
  `R_PPC_EMB_SDA21 gLoader_SpacePirate`, which is the whole point:
  `objdump -r` on the object shows exactly that one relocation and no other.
- `python3 tools/check_symbol_names.py` - `525 units; 0 declared names are missing`.
- `./tools/probe_sources.sh` - `758 files, 0 failed, 0 errors; LINKED (291 undefined,
  0 duplicates)`. No `PortLinkStubs.cpp` duplicate: `fn_80200E3C` appears in no other source.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve80200E3C.c` printed
  `0 unit(s) checked` - it does not match `.c` unit names. Moot for a one-function file, and
  the descending-order rule cannot be violated by it, but a `.c` carve gets no check from
  that tool.

## Notes for the next run

1. **The carve-vein doc's link-order cycle did not fire here, and the reason is worth
   recording.** The warning is that a carve which *starts exactly where a neighbouring
   pre-existing `Matching` unit's `.text` ends* fails `dtk dol split` with
   `Cyclic dependency ... <that unit> -> auto_*`. That is precisely this address:
   `SpacePirate.cpp` claims `0x80200E10..0x80200E3C` and the carve starts at 0x80200E3C. It
   linked cleanly. So the cycle is not simply "abutting a preceding claimed unit" - the
   difference from `CFrustumPlanes.cpp`/`0x80302BAC` may be that the abutting unit here also
   claims `.sbss` (so its object is in a different section's order), or that the cycle needs
   a *third* ordering constraint. Worth one line in the carve-vein section: **the warning is
   not "adjacency" but "adjacency plus whatever else the two units claim"; this pair has
   adjacency and links.** No `NEW:` line for it - it is a lesson, not work that raises a count.

2. **A loader setter can be carved after all.** `SpacePirate.cpp` (and `Kralee.cpp`,
   `Parasite.cpp`, `PillBug.cpp` and the rest of the family) each carry the comment "REL
   modules import it by its retail name, so it cannot be renamed and must stay in dtk's auto
   unit". That is right about the *name* and wrong as a conclusion. Module 72 imports it as
   `fn_80200E3C` (visible in its own listing, `bl fn_80200E3C` twice), and a `.c` definition
   under the `fn_` name emits exactly that unmangled symbol - so the import resolves against
   our object and the DOL sha1 holds. A C++ `SetLoader_SpacePirate` would have mangled and
   broken both. **The same carve is available for every other loader setter of this family**;
   this is exactly the shape `tools/goal_seed.py` emits, and it is what made this one cheap.
   (Not filed as `NEW:` either - the seeder already produces these items; this is a
   confirmation, not a new target.)

3. **`build/G2ME01/obj/auto_03_80200E3C_text.o` is left on disk** after the re-split. It is a
   stale artifact: `grep -c auto_03_80200E3C_text build.ninja` is `0`, so nothing links it and
   there is no duplicate definition. Only `git clean` in `build/` removes it.