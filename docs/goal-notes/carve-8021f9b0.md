# carve-8021f9b0 - `MetroidPrime/ScriptLoader/Carve8021F9B0.c`

**Result: the unit is `Matching` and verified by `tools/flip_test.sh`.**
`./tools/goal_check.sh build/goal/item.json` exits 0 (`goal_check: PASS carve-8021f9b0`),
with `matched 13580 -> 13581` and `linked 6628 -> 6629`.

## What I did

The item's four files, all in this one change, as a carve is required to be:

1. `src/MetroidPrime/ScriptLoader/Carve8021F9B0.c` - new, 107 lines: the 8-byte body
   `stw r3, gLoader_DigitalGuardian@sda21(r0) ; blr`
   (`void fn_8021F9B0(struct SDigitalGuardianLoaders* loaders)` at line 105) plus
   the header comment in the style of `src/MetroidPrime/ScriptLoader/Carve80218BFC.c` and its
   twin `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`.
2. `configure.py:896` - `Object(Matching, "MetroidPrime/ScriptLoader/Carve8021F9B0.c"),` on one
   line, in address order between `DigitalGuardian.cpp` (line 895) and `Shredder.cpp` (897).
3. `config/G2ME01/splits.txt:1767-1768` - `.text start:0x8021F9B0 end:0x8021F9B8`, in address
   order between `DigitalGuardian.cpp` (claim at 1763-1765, ends 0x8021F9B0) and `Shredder.cpp`
   (starts 0x8021F9B8). **`.text` only**: the `.sbss` slot belongs to `DigitalGuardian.cpp`.
4. `files.cmake:930-933` - the entry with its comment, in address order after
   `Carve80218BFC.c` (0x80218BFC) and before `Carve802201F8.cpp` (0x802201F8).

Claimed exactly `0x8021F9B0..0x8021F9B8` and nothing else. One function, so declaration order
cannot be wrong, but `python3 tools/check_decl_order.py --unit
MetroidPrime/ScriptLoader/Carve8021F9B0.c` was run anyway: `ok: 0 unit(s) checked` (a
one-function unit is not what that tool looks at), clean. `total_functions` is still **28465**
after the `splits.txt` edit. No `PortLinkStubs.cpp` duplicate exists (grepped
`src/MetroidPrime/PortLinkStubs.cpp` for `8021F9B0`: no match). No `asm` was added, and the
`docs/HANDOFF.md` / `docs/RUNNING_THE_DECOMP.md` edits in `git status` are the judge's own
derived-count rewrites from `goal_check.sh` (probe 981 -> 982 files), not mine.

**One file outside the four, and it is the item's own claim, not an unrelated fix:**
`src/MetroidPrime/ScriptLoader/DigitalGuardian.cpp:9-13` said "The 8-byte setter at 0x8021F9B0 is
deliberately NOT claimed: REL modules import it by its retail name, so it cannot be renamed and
must stay in dtk's auto unit." That sentence is falsified by this carve, and the reason it gives
is the same one that makes the function claimable: an unmangled `.c` definition reproduces the
imported symbol exactly. Replaced with the wording `d682ebe7` (carve-80200e3c) used on the
identical sentence in `SpacePirate.cpp`. **The same stale sentence is in 19 other
`src/MetroidPrime/ScriptLoader/*.cpp` files** (`grep -rn "deliberately NOT claimed" src/`),
listed below; I left those alone, as the previous lane in this family did.

## What I measured

- Retail bytes: `build/G2ME01/asm/auto_03_8021F9B0_text.s` - `stw r3,
  gLoader_DigitalGuardian@sda21(r0)` / `blr`, 8 bytes, `fn_8021F9B0`, `90 6D 97 90` / `4E 80 00
  20`.
- `config/G2ME01/symbols.txt:9608` - `fn_8021F9B0 = .text:0x8021F9B0; // type:function size:0x8
  align:4`. `symbols.txt:9609` is the next thing in the DOL, `LoadShredder` at 0x8021F9B8.
- `strings build/G2ME01/DigitalGuardian/DigitalGuardian.plf | grep 8021F9B0` -> `fn_8021F9B0`,
  so the name is retail's and the unit must be `.c` (`-lang=c`), exactly as for the SpacePirate
  and Lumite carves.
- Module call sites, `build/G2ME01/DigitalGuardian/asm/auto_00_0000010C_text.s`: `RELExit` at
  `.text:0x138` materialises `li r3, 0x0` and calls it at `.text:0x148`; `fn_14_17C` at
  `.text:0x17C` (what `RELMain` calls at `.text:0x168`) stores `fn_14_1B8` at `lbl_14_bss_C+0`
  and `fn_14_DF7C` at `+4`, then calls it at `.text:0x1A4` with `r3` still the address
  `lis r3, lbl_14_bss_C@ha` materialised. So the argument is the **address of a two-loader
  record**, not a loader - which is why this carve is the first of the family whose parameter is
  a pointer to a struct rather than a function pointer.
- `lbl_14_bss_C` is `.bss:0xC` of size `0xC`
  (`build/G2ME01/DigitalGuardian/asm/auto_05_00000000_bss.s:30`), so the record is 12 bytes and
  setup fills its first two words.
- The DOL readers agree on which word is which: `DigitalGuardian.cpp:29` calls `slot0` and
  `:33` calls `slot1`; `docs/research/rel_loaders.md:147-148` records
  `LoadDigitalGuardianHead` at 0x8021F958 with `slot` 4 and `LoadDigitalGuardian` at 0x8021F984
  with `slot` 0.
- The slot is `.sbss 0x80419510..0x80419518` (`symbols.txt:20750`, 8 bytes), owned and defined by
  `MetroidPrime/ScriptLoader/DigitalGuardian.cpp:26` (`splits.txt:1763-1765`), so this unit takes
  it `extern`. Retail's displacement is `-26736(r13)`, and `0x804C5D40 - 26736 = 0x80419510`, so
  the store really is `value` at +0 and nothing else.
- Our object is the twin's shape exactly: `T fn_8021F9B0` / `U gLoader_DigitalGuardian`, one
  `R_PPC_EMB_SDA21 gLoader_DigitalGuardian` at offset 0, unlinked disasm `stw r3,0(0)` / `blr`.
- Build: DOL `matched_functions` 11642 -> 11643, DOL `complete_units` 718 -> 719, All
  `13581 / 28465 functions`, `complete_units` 1006 -> 1007.
- Gates: DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
  `check_symbol_names.py` 0 missing names over 609 units; `unit_fit.sh` ".text claimed 8 ours 8
  retail 8 fits / no extra functions"; `gate.sh` green inside the judge (which also checks all 86
  RELs, the report diff, wiring, docs claims and the port probe). Probe count rose 981 -> **982**
  files, 0 failures, because the port compiles this file too and it adds no undefined symbol.

## Notes for the next lane

**`tools/carve_diff.sh` still cannot decide this family, and its NOT byte-exact is not
evidence.** It takes `<retail_start> <retail_size> <ours.o> [symbol]`, not a unit name, and it
disassembles the *unlinked* object, where the `R_PPC_EMB_SDA21` relocation is still `0(0)`.
On this unit it prints `differing instructions: 1 / NOT byte-exact` - and prints exactly the same
for the already-**Matching** `Carve802188E4.o` (`stw r3,-27056(r13)` vs `stw r3,0(0)`), which I
ran as a control. Recorded a third time because every lane in this family runs it and reads the
verdict.

**The `"deliberately NOT claimed"` sentence is wrong wherever it appears in this family, and
carving one of these falsifies the copy in its own loader unit.** Nineteen files still carry it:
`OctopedeSegment.cpp:7`, `FlyerSwarmLoaderSet.cpp:13`, `SkyRippleLoaderSet.cpp:13`,
`RsfAudioLoaderSet.cpp:13`, `RubiksPuzzle.cpp:7`, `Tryclops.cpp:7`, `PuddleSpore.cpp:7`,
`WispTentacle.cpp:7`, `Lumite.cpp:7`, `EmperorIngStage3.cpp:7`, `ElitePirate.cpp:7`,
`ChozoGhost.cpp:7`, `CommandPirate.cpp:7`, `StoneToad.cpp:7`, `Rezbit.cpp:7`,
`SplitterMainChassis.cpp:10`, `SwampBossStage1.cpp:7`, `DarkSamusBattleStage.cpp:7`,
`IngBoostBallGuardian.cpp:7`. Not a `NEW:` item (it is a doc fix); each is one commit's worth of
a line each, and the lane that carves a given file should do it in the same change.

**Still unclaimed, same shape, `stw r3, gLoader_...@sda21(r0) ; blr`, 8 bytes each.** Measured
just now by scanning every `auto_03_*_text.s` in `build/G2ME01/asm/` for a single-`.fn` unit of
exactly 8 bytes with that instruction pair, and diffing each address against
`config/G2ME01/splits.txt`: **38 such units, 19 claimed (18 before this item), 19 unclaimed.**
17 of the unclaimed are filed as `NEW:` below; 0x802188B0 and 0x80235DCC were already filed by
earlier lanes and are not repeated. For every remaining one I also confirmed the importing
module with `grep -l "fn_<addr>" build/G2ME01/*/*.plf`, so `.c` is the right spelling for all of
them.

NEW: carve-8021f9e4 | match | MetroidPrime/ScriptLoader/Carve8021F9E4 | Shredder's 8-byte loader setter at 0x8021F9E4..0x8021F9EC, `stw r3, gLoader_Shredder; blr`, own 8-byte auto unit between `Shredder.cpp` and `FrontEndDataNetwork.cpp`; `Shredder.plf` imports `fn_8021F9E4`.
NEW: carve-8021fa18 | match | MetroidPrime/ScriptLoader/Carve8021FA18 | ScriptFrontEndDataNetwork's 8-byte loader setter at 0x8021FA18..0x8021FA20, `stw r3, gLoader_FrontEndDataNetwork; blr`, own 8-byte auto unit; `ScriptFrontEndDataNetwork.plf` imports `fn_8021FA18`.
NEW: carve-8021fa4c | match | MetroidPrime/ScriptLoader/Carve8021FA4C | StoneToad's 8-byte loader setter at 0x8021FA4C..0x8021FA54, `stw r3, gLoader_StoneToad; blr`, own 8-byte auto unit between `StoneToad.cpp` and `Coin.cpp`; `StoneToad.plf` imports `fn_8021FA4C`.
NEW: carve-80218c30 | match | MetroidPrime/ScriptLoader/Carve80218C30 | Shrieker's 8-byte loader setter at 0x80218C30..0x80218C38, `stw r3, gLoader_Shrieker; blr`, own 8-byte auto unit immediately after the `Carve80218BFC.c` claim; `Shrieker.plf` imports `fn_80218C30`.
NEW: carve-80218cf0 | match | MetroidPrime/ScriptLoader/Carve80218CF0 | Splitter's 8-byte loader setter at 0x80218CF0..0x80218CF8, `stw r3, gLoader_SplitterMainChassis; blr`, own 8-byte auto unit; `Splitter.plf` imports `fn_80218CF0`.
NEW: carve-80218d24 | match | MetroidPrime/ScriptLoader/Carve80218D24 | ChozoGhost's 8-byte loader setter at 0x80218D24..0x80218D2C, `stw r3, gLoader_ChozoGhost; blr`, own 8-byte auto unit; `ChozoGhost.plf` imports `fn_80218D24`.
NEW: carve-80218d58 | match | MetroidPrime/ScriptLoader/Carve80218D58 | Tryclops' 8-byte loader setter at 0x80218D58..0x80218D60, `stw r3, gLoader_Tryclops; blr`, own 8-byte auto unit; `Tryclops.plf` imports `fn_80218D58`.
NEW: carve-80218d8c | match | MetroidPrime/ScriptLoader/Carve80218D8C | WispTentacle's 8-byte loader setter at 0x80218D8C..0x80218D94, `stw r3, gLoader_WispTentacle; blr`, own 8-byte auto unit; `WispTentacle.plf` imports `fn_80218D8C`.
NEW: carve-80218dc0 | match | MetroidPrime/ScriptLoader/Carve80218DC0 | SpankWeed's 8-byte loader setter at 0x80218DC0..0x80218DC8, `stw r3, gLoader_SpankWeed; blr`, own 8-byte auto unit; `SpankWeed.plf` imports `fn_80218DC0`.
NEW: carve-80218df4 | match | MetroidPrime/ScriptLoader/Carve80218DF4 | DarkTrooper's 8-byte loader setter at 0x80218DF4..0x80218DFC, `stw r3, gLoader_DarkTrooper; blr`, own 8-byte auto unit; `DarkTrooper.plf` imports `fn_80218DF4`.
NEW: carve-80227af8 | match | MetroidPrime/ScriptLoader/Carve80227AF8 | Rezbit's 8-byte loader setter at 0x80227AF8..0x80227B00, `stw r3, gLoader_Rezbit; blr`, own 8-byte auto unit; `Rezbit.plf` imports `fn_80227AF8`.
NEW: carve-80229ff0 | match | MetroidPrime/ScriptLoader/Carve80229FF0 | ScriptStreamedMovie's 8-byte loader setter at 0x80229FF0..0x80229FF8, `stw r3, gLoader_StreamedMovie; blr`, own 8-byte auto unit; `ScriptStreamedMovie.plf` imports `fn_80229FF0`.
NEW: carve-8022a024 | match | MetroidPrime/ScriptLoader/Carve8022A024 | IngSpiderballGuardian's 8-byte loader setter at 0x8022A024..0x8022A02C, `stw r3, gLoader_IngSpiderBallGuardian; blr`, own 8-byte auto unit; `IngSpiderballGuardian.plf` imports `fn_8022A024`.
NEW: carve-8022ebc8 | match | MetroidPrime/ScriptLoader/Carve8022EBC8 | EmperorIngStage3's 8-byte loader setter at 0x8022EBC8..0x8022EBD0, `stw r3, gLoader_EmperorIngStage3; blr`, own 8-byte auto unit; `EmperorIngStage3.plf` imports `fn_8022EBC8`.
NEW: carve-8022ebfc | match | MetroidPrime/ScriptLoader/Carve8022EBFC | DestructibleBarrier's 8-byte loader setter at 0x8022EBFC..0x8022EC04, `stw r3, gLoader_DestructableBarrier; blr`, own 8-byte auto unit; `DestructibleBarrier.plf` imports `fn_8022EBFC`.
NEW: carve-8022ec30 | match | MetroidPrime/ScriptLoader/Carve8022EC30 | SwampBossStage2's 8-byte loader setter at 0x8022EC30..0x8022EC38, `stw r3, gLoader_SwampBossStage2; blr`, own 8-byte auto unit; `SwampBossStage2.plf` imports `fn_8022EC30`.
NEW: carve-8022ffc4 | match | MetroidPrime/ScriptLoader/Carve8022FFC4 | IngBoostBallGuardian's 8-byte loader setter at 0x8022FFC4..0x8022FFCC, `stw r3, gLoader_IngBoostBallGuardian; blr`, own 8-byte auto unit; `IngBoostBallGuardian.plf` imports `fn_8022FFC4`.
