# carve-80218bfc - `MetroidPrime/ScriptLoader/Carve80218BFC.c`

**Result: the unit is `Matching` and verified by `tools/flip_test.sh`.**
`./tools/goal_check.sh build/goal/item.json` exits 0 (`goal_check: PASS carve-80218bfc`),
with `matched 13571 -> 13572` and `linked 6619 -> 6620`.

## What I did

The item's four files, all in this one change, as a carve is required to be:

1. `src/MetroidPrime/ScriptLoader/Carve80218BFC.c` - new, 89 lines: the 8-byte body
   `stw r3, gLoader_Lumite; blr` (`void fn_80218BFC(SLumiteLoader loader)` at line 89) plus the
   header comment in the style of `src/Dolphin/Carve8038A7DC.c` and the sibling carves
   `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` / `Carve802188E4.c`.
2. `configure.py:870` - `Object(Matching, "MetroidPrime/ScriptLoader/Carve80218BFC.c"),` on one
   line, in address order between `Lumite.cpp` (line 869) and `Shrieker.cpp` (line 871).
3. `config/G2ME01/splits.txt:1689-1690` - `.text start:0x80218BFC end:0x80218C04`, in address
   order between `Lumite.cpp` (ends 0x80218BFC) and `Shrieker.cpp` (starts 0x80218C04).
   **`.text` only**: the `.sbss` slot belongs to `Lumite.cpp`.
4. `files.cmake:894-897` - the entry with its comment, after `Carve802189D0.c` in address order.

Claimed exactly `0x80218BFC..0x80218C04` and nothing else. One function, so declaration order
cannot be wrong, but `python3 tools/check_decl_order.py --unit
MetroidPrime/ScriptLoader/Carve80218BFC.c` was run anyway: `0 unit(s) checked` (a one-function
unit is not what that tool looks at), clean.
`total_functions` is still **28465** after the `splits.txt` edit.
No `PortLinkStubs.cpp` duplicate exists (grepped `src/MetroidPrime/PortLinkStubs.cpp` for
`80218BFC`: no match). No `asm` was added, and no `docs/` file was edited by me - the
`docs/HANDOFF.md` / `docs/RUNNING_THE_DECOMP.md` edits in `git status` are the judge's own
derived-count rewrites from `goal_check.sh`.

## What I measured

- Retail bytes: `build/G2ME01/asm/auto_03_80218BFC_text.s` - `stw r3,
  gLoader_Lumite@sda21(r0)` / `blr`, 8 bytes, `fn_80218BFC`.
- `config/G2ME01/symbols.txt:9490` - `fn_80218BFC = .text:0x80218BFC; // type:function size:0x8
  align:4`. `symbols.txt:9491` is the next thing in the DOL, `LoadShrieker` at 0x80218C04.
- `strings build/G2ME01/Lumite/Lumite.plf | grep 80218BFC` -> `fn_80218BFC`, so the name is
  retail's and the unit must be `.c` (`-lang=c`), exactly as for the SpacePirate and DarkSamus
  carves. `src/MetroidPrime/ScriptObjects/CLumiteRel.cpp:144` already declares it `extern "C"`.
- Module call sites, `build/G2ME01/Lumite/asm/auto_00_00000000_text.s`: `RELExit` at `.text:0x11C`
  materialises `li r3, 0x0` (line 138) and calls it (line 141); `fn_39_160` at `.text:0x160`
  stores `fn_39_190` at `lbl_39_bss_40` with `stwu r0, lbl_39_bss_40@l(r3)` (line 166) and hands
  **the slot's address** over (line 169). So the argument is a function pointer passed by value,
  which is why the store below writes one word and writes `value`, not the whole slot.
- The slot is `.sbss 0x80419428..0x80419430` (`symbols.txt:20718`, 8 bytes), owned and defined by
  `MetroidPrime/ScriptLoader/Lumite.cpp:16`, so this unit takes it `extern`.
- Our object is the twin's shape exactly (control run against `Carve802188E4.o`):
  `T fn_80218BFC` / `U gLoader_Lumite` and one `R_PPC_EMB_SDA21 gLoader_Lumite` at offset 0.
- Report entry after the build: `main/MetroidPrime/ScriptLoader/Carve80218BFC`,
  `fuzzy_match_percent 100.0`, `matched_functions 1 / 1`, `complete_units 1`.
- Gates: DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
  `check_symbol_names.py` 0 missing names over 609 units; `unit_fit.sh` ".text claimed 8 ours 8
  retail 8 fits / no extra functions"; `All: 37.69% fuzzy, 31.12% matched, 13.99% linked (13572 /
  28465 functions)`; `gate.sh` green inside the judge (which also checks all 86 RELs, the report
  diff, wiring, docs claims and the port probe). Probe count rose 972 -> **973** files, 0
  failures, because the port compiles this file too and it adds no undefined symbol.

## Three notes for the next lane

**`tools/carve_diff.sh` cannot decide this family, and its NOT byte-exact is not evidence.**
It disassembles the *unlinked* object, where the `R_PPC_EMB_SDA21` relocation is still `0(0)`,
while retail reads as `stw r3,-26968(r13)`. On this unit it prints `NOT byte-exact, differing
instructions: 1` - and it prints exactly the same for the already-**Matching** `Carve802188E4.o`
(`stw r3,-27056(r13)` vs `stw r3,0(0)`), which I ran as a control. What decides a carve is
`flip_test.sh` plus objdiff's 100%, not this tool. Recorded again because every lane in this
family will run it and read the verdict.

**`src/MetroidPrime/ScriptLoader/Lumite.cpp:6-8` is now wrong and this carve supersedes it.** It
reads "The 8-byte setter at 0x80218BFC is deliberately NOT claimed: REL modules import it by its
retail name, so it cannot be renamed and must stay in dtk's auto unit." The reason it gives for
not renaming is the same one that makes it *claimable*: an unmangled `.c` definition reproduces
the imported symbol exactly, and `fn_80218918`, `fn_802188E4` and now `fn_80218BFC` are all
`Matching`. `configure.py:1864` carries the same sentence for the Lumite REL. I did not edit
either, because a carve is four files and the reviewer rejects an unrelated fix; the fix belongs
in whichever commit next touches those files. (`NEW:` is not the form for a doc fix.)

**Still unclaimed, same shape, `stw r3, gLoader_...@sda21(r0) ; blr`, 8 bytes each.** 39 of them,
measured by diffing every `gLoader_*@sda21(r0)` in `build/G2ME01/asm/auto_*_text.s` against
`config/G2ME01/splits.txt`. Twelve sit alone in their own 8-byte `auto_03_*` unit, so they are
the same one-hour carve this was: 0x80218B68, 0x8021F9E4, 0x8021FA18, 0x8021FA4C, 0x80227AF8,
0x80229FF0, 0x8022A024, 0x8022EBC8, 0x8022EBFC, 0x8022EC30, 0x8022FFC4, 0x80235DCC. The rest
share an `auto_*` unit with behavioural code and are not one-item work. Two are filed below; the
rest are listed here so a seeding pass can queue them without re-measuring.

NEW: carve-80218b68 | match | MetroidPrime/ScriptLoader/Carve80218B68 | MetroidAlpha's 8-byte loader setter at 0x80218B68..0x80218B70, the same `stw r3, gLoader_MetroidAlpha; blr` as this carve; its own 8-byte auto unit, between two claimed units, and the module import is confirmed.
NEW: carve-80235dcc | match | MetroidPrime/ScriptLoader/Carve80235DCC | DarkSamusBattleStage's 8-byte loader setter at 0x80235DCC..0x80235DD4, the same shape as this carve; own 8-byte auto unit between two claimed units, import confirmed in DarkSamusBattleStage.plf.