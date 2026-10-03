# carve-80218ad4 - `MetroidPrime/ScriptLoader/Carve80218AD4.c`

**Result: the unit is `Matching` and verified by `tools/flip_test.sh`.**
`./tools/goal_check.sh build/goal/item.json` exits 0 (`goal_check: PASS carve-80218ad4`),
with `matched 13567 -> 13568` and `linked 6615 -> 6616`.

## What I did

The item's four files, all in this one change, as a carve is required to be:

1. `src/MetroidPrime/ScriptLoader/Carve80218AD4.c` - new, the 8-byte body
   `stw r3, gLoader_ElitePirate; blr` plus the header comment in the style of
   `src/Dolphin/Carve8038A7DC.c` / `src/MetroidPrime/ScriptLoader/Carve80218918.c`.
2. `configure.py:862` - `Object(Matching, "MetroidPrime/ScriptLoader/Carve80218AD4.c"),`
   on one line, between `ElitePirate.cpp` and `Blogg.cpp`.
3. `config/G2ME01/splits.txt:1664-1665` - `.text start:0x80218AD4 end:0x80218ADC`,
   in address order between `ElitePirate.cpp` (ends 0x80218AD4) and `Blogg.cpp`
   (starts 0x80218ADC). **`.text` only**: the `.sbss` slot belongs to `ElitePirate.cpp`.
4. `files.cmake:875-878` - the entry with its comment, alphabetically among the
   `ScriptLoader/Carve*.c` carves.

Claimed exactly `0x80218AD4..0x80218ADC` and nothing else. One function, so declaration
order cannot be wrong, but `python3 tools/check_decl_order.py --unit
MetroidPrime/ScriptLoader/Carve80218AD4.c` was run anyway: clean.
`total_functions` is still **28465** after the `splits.txt` edit.
No `PortLinkStubs.cpp` duplicate exists (grepped; the file defines no `fn_80218AD4`).
No `asm` was added.

## What I measured

- Retail bytes: `build/G2ME01/asm/auto_03_80218AD4_text.s` - `stw r3,
  gLoader_ElitePirate@sda21(r0)` / `blr`, 8 bytes.
- `config/G2ME01/symbols.txt:9480` - `fn_80218AD4 = .text:0x80218AD4; size:0x8 align:4`.
- `strings build/G2ME01/ElitePirate/ElitePirate.plf | grep 80218AD4` -> `fn_80218AD4`, so the
  name is retail's and the unit must be `.c` (`-lang=c`), exactly as for the Ing and
  DarkSamus carves.
- Module call sites, `build/G2ME01/ElitePirate/asm/auto_00_00000000_text.s`: `RELExit` at
  `.text:0x104` passes `li r3,0` (line 143); `fn_15_148` at `.text:0x148` stores
  `fn_15_178` with `stwu r0, lbl_15_bss_0@l(r3)` and hands **the slot's address** over
  (line 171). The record is 4 bytes -
  `config/G2ME01/rels/ElitePirate/symbols.txt:467` gives `lbl_15_bss_0 size:0x4`.
- The slot is `.sbss 0x80419408..0x80419410` (`symbols.txt:20714`, 8 bytes), owned and
  defined by `MetroidPrime/ScriptLoader/ElitePirate.cpp:16`, so this unit takes it `extern`.
- Report entry after the build: `main/MetroidPrime/ScriptLoader/Carve80218AD4`,
  `fuzzy_match_percent 100.0`, `matched_functions 1 / 1`, `complete_units 1`.
- Gates: DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
  `check_symbol_names.py` 0 missing names over 608 units; `unit_fit.sh` "fits, no extra
  functions"; `gate.sh` green inside the judge (which also checks all 86 RELs, the report
  diff, wiring, docs claims and the port probe). Probe count rose 968 -> **969** files, 0
  failures, because the port compiles this file too and it adds no undefined symbol.

## Two notes for the next lane

**`tools/carve_diff.sh` cannot decide this family, and its NOT byte-exact is not evidence.**
It disassembles the *unlinked* object, where every `R_PPC_EMB_SDA21` relocation is still
`0(0)`, while retail reads as `stw r3,-27000(r13)`. It prints
`NOT byte-exact, differing instructions: 1` for my unit **and for the three already-Matching
carves** (`Carve80218918`, `Carve80200E3C`, `Carve802188E4`) - I ran it on all four as a
control. What says the carve is right is that our object's relocations and symbols are
identical in shape to the Matching twin's:

```
ours:  *UND* gLoader_ElitePirate / F .text 00000008 fn_80218AD4 / R_PPC_EMB_SDA21 gLoader_ElitePirate
twin:  *UND* gLoader_Ings        / F .text 00000008 fn_80218918 / R_PPC_EMB_SDA21 gLoader_Ings
```

A note for whoever wrote it: the tool would decide this family if it resolved the relocation
(`objdump -r`, or compare only the opcode+operand registers and ignore the displacement).

**`ElitePirate.cpp`'s header comment was wrong and this carve supersedes it.** It reads "The
8-byte setter at 0x80218AD4 is deliberately NOT claimed: REL modules import it by its retail
name, so it cannot be renamed and must stay in dtk's auto unit." The reason it gives for not
renaming is the same one that makes it *claimable*: an unmangled `.c` definition reproduces
the imported symbol exactly. `fn_80218918` and `fn_802188E4` are the same case and are both
`Matching`. The comment in `src/MetroidPrime/ScriptLoader/ElitePirate.cpp:6-8` should be
corrected in place; I did not edit it, because a carve is four files and the driver discards
doc edits. `NEW:` is not the right form for a doc fix, so it is here.

**Still unclaimed, same shape, 8 bytes each** (every `stw r3, gLoader_...@sda21(r0) ; blr`
in `build/G2ME01/asm/`): 41 of them. The five beside this one - 0x802188B0 (CommandPirate),
0x80218A04 (FlyingPirate), 0x80218A38 (Grenchler), 0x80218A6C (MediumIng), 0x80218AA0
(MinorIng) - are already queued as `carve-802188b0`, `carve-80218a04`, `carve-80218a38`,
`carve-80218a6c`, `carve-80218aa0`. **0x802189D0 (SandBoss) is not in the queue**, which is
the one filed below.

NEW: carve-802189d0 | match | MetroidPrime/ScriptLoader/Carve802189D0 | SandBoss's 8-byte loader setter at 0x802189D0..0x802189D8, the same `stw r3, gLoader_SandBoss; blr` as this carve and not yet queued.