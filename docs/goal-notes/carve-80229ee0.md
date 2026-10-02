# carve-80229ee0 — `MetroidPrime/ScriptLoader/Carve80229EE0.c` is `Matching` at 100.00%

**Item:** `kind: match`, `target: MetroidPrime/ScriptLoader/Carve80229EE0`.
**Result:** landed. `.text 0x80229EE0..0x80229EE8`, 0x8 = 8 bytes, 1 function, 2 instructions.
`tools/flip_test.sh` **PASS**; `tools/goal_check.sh build/goal/item.json` **PASS** on every step.

## What I did

Claimed `.text 0x80229EE0..0x80229EE8` — the leading 8 bytes of dtk's `auto_03_80229EE0_text`
run — as its own `Matching` unit, with `fn_80229EE0` written from the bytes dtk recorded for the
run in `build/G2ME01/asm/auto_03_80229EE0_text.s:9-10` before the claim existed.

Five files, one change:

- `src/MetroidPrime/ScriptLoader/Carve80229EE0.c` (new) — the body and the measured header comment
- `config/G2ME01/splits.txt:1722-1723` — `.text start:0x80229EE0 end:0x80229EE8`, between
  `IngPuddle.cpp` and `FlyerSwarm.cpp`
- `configure.py:855` — `Object(Matching, "MetroidPrime/ScriptLoader/Carve80229EE0.c")`
- `files.cmake:624` — `src/MetroidPrime/ScriptLoader/Carve80229EE0.c`
- `src/MetroidPrime/ScriptLoader/IngPuddle.cpp:6-10` — the "deliberately NOT claimed" sentence the
  item named, corrected in place

Nothing else. `docs/HANDOFF.md` / `docs/RUNNING_THE_DECOMP.md` in `git status` are the judge's own
derived-count rewrite, not my edit.

The body is one line:

```c
void fn_80229EE0(void* loader) { gLoader_IngPuddle = loader; }
```

## The body, and why the spelling was not a guess

**Shape.** `stw r3, gLoader_IngPuddle@sda21(r0)` / `blr` — the byte-shape twin of the matched
`fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (`stw r3,
gLoader_SpacePirate@sda21(r0)` / `blr`) and of the landed `fn_8023289C` in `Carve8023289C.c`. It
never reads back what it stored, so it needs the slot's address only, not its element type.

**What it is: IngPuddle's loader setter.** The reader that fixes the sense of the store is
`LoadIngPuddle` at 0x80229EB4, which `build/G2ME01/asm/MetroidPrime/ScriptLoader/IngPuddle.s:10-16`
shows as `lwz r6, gLoader_IngPuddle@sda21(r0)` / `lwz r12, 0x0(r6)` / `mtctr r12` / `bctrl`: it
loads the *address* of a loader slot, then dispatches through its first word. So the argument is a
loader slot, not a loader.

**Who calls it, and what the argument is.** Every call site is in a REL module that imports the DOL
function by its retail name, so the argument is read off the module's own listing. Module 32
(`config/G2ME01/config.yml:171-175`, `files/RelProd/IngPuddle.rel`, sha1
`312b87acb1dea81e5c03fccd6b87e68366f17a6f`); both callers are in
`build/G2ME01/IngPuddle/asm/MetroidPrime/ScriptObjects/CIngPuddleRel.s`, which is a `Matching` unit
of ours, so the listing is ours and needs no separate disassembly:

- `RELExit` (`.text` 0x34, 0x24 bytes) — `li r3, 0x0` on line 33, `bl fn_80229EE0` on line 35: the
  module tears its registration down on the way out, so the argument is `0`.
- `fn_32_78` (`.text` 0x78, 0x30 bytes) — `lis r4, fn_32_A8@ha` (58) / `lis r3, lbl_32_bss_0@ha`
  (59) / `addi r0, r4, fn_32_A8@l` (61) / `stwu r0, lbl_32_bss_0@l(r3)` (62), `bl fn_80229EE0` on
  line 63: r3 still holds the slot's address, so the argument is `&lbl_32_bss_0`.
  `config/G2ME01/rels/IngPuddle/symbols.txt:103` gives `lbl_32_bss_0 size:0x4`, so the DOL slot
  holds a *pointer to* a 4-byte loader slot — which is why `IngPuddle.cpp:19` reads it back as
  `(*gLoader_IngPuddle.value)(mgr, input, info)`.

**`extern`, not a second definition.** The slot is `gLoader_IngPuddle` at `.sbss 0x80419598`,
`size:0x8 data:4byte` (`symbols.txt:20769`), and `IngPuddle.cpp` already claims (split
`splits.txt:1718-1720`, `.sbss 0x80419598..0x804195A0`) *and* already defines it at line 16. So this
unit claims `.text` only and takes the pointer as `extern`: a second definition is a duplicate the
moment both objects are in the link. MWCC does not encode a variable's type in its name, so the
store lands on the same address whatever the type is spelled; `IngPuddle.cpp`'s own `SLoaderSlot`
says what the two words are.

**No host-only block**, unlike `Carve80227530.c` (which had to define `lbl_80419568` under
`#ifndef __MWERKS__` because its slot is claimed by no unit of ours). The item's claim that the
port's undefined count provably cannot move is not a prediction, it is the absence of any other
reference — and it is measured below.

**C, not C++, and the retail name.** Retail names this nothing; `symbols.txt:9829` carries the
`fn_<addr>` placeholder and the file reproduces it verbatim. A C++ spelling would mangle to
`_Z<len>fn_<addr>v` and objdiff would pair nothing, which is also why the unit is a `.c`. Source
order is descending by retail address — with one function the order cannot be wrong, and
`tools/flip_test.sh` is what actually covers it.

**The corrected sentence.** `IngPuddle.cpp:6-8` said the setter was "deliberately NOT claimed:
REL modules import it by its retail name, so it cannot be renamed and must stay in dtk's auto
unit". What a carve must preserve is the **name**, and reproducing the `fn_<addr>` symbol verbatim
in a `.c` file preserves it: the unmangled `fn_80229EE0` still lands in the DOL link, which is what
module 32's two `bl fn_80229EE0` resolve against. What a rename would break is the name; what this
carve changes is only who supplies the bytes. Same correction `Carve8023289C.c` records for
`AtomicBeta.cpp` and `Carve80232868.c` for `MysteryFlyer.cpp`.

## Measured

| check | result |
| --- | --- |
| `./tools/decomp_build.sh` | `All: 36.98% fuzzy, 30.42% matched, 13.41% linked (13046 / 28465 functions)` (baseline 13045) |
| `build/report.json` | `main/MetroidPrime/ScriptLoader/Carve80229EE0`: `fuzzy_match_percent` 100.0, `matched_functions` 1 / 1, `complete_units` 1, `.text` 8 bytes |
| `tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80229EE0.c` | `PASS -> kept as Matching` (1/1 kept, 0 failed, 0 skipped) |
| `tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve80229EE0.c` | `.text claimed 8 ours 8 retail 8 fits`; no extra functions |
| `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged) |
| `sha1sum orig/G2ME01/files/RelProd/IngPuddle.rel` | `312b87acb1dea81e5c03fccd6b87e68366f17a6f` (module 32, `config.yml:173`) |
| `./tools/probe_sources.sh` | `823 files, 0 failed, 0 errors; link: LINKED (286 undefined, 0 duplicates)` |
| `python3 tools/check_symbol_names.py` | `checked 578 units; 0 declared names are missing from their object` |
| `tools/goal_check.sh build/goal/item.json` | `PASS` — `counts: matched 13045 -> 13046  linked 6148 -> 6149` |
| `total_functions` after the `splits.txt` edit | **28465**, unchanged |

Byte-for-byte against retail (`powerpc-eabi-objdump -d` on `orig/G2ME01/main.dol` at
0x80229EE0: `90 6d 98 18  stw r3,-26600(r13)` / `4e 80 00 20  blr`; our object
`build/G2ME01/obj/MetroidPrime/ScriptLoader/Carve80229EE0.o`: `90 60 00 00 stw r3,0(0)` +
`R_PPC_EMB_SDA21 gLoader_IngPuddle`, then `4e 80 00 20 blr`). The only difference is the
displacement, which the relocation fills and which resolves to the same 0x80419598.

`tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve80229EE0.c` reports
"0 unit(s) checked" — it skips `.c` units. With one function the order cannot be wrong, and
`flip_test` is what actually covers it.

## The front case, confirmed

The item predicted the remainder would "reappear under a new name". Measured: after the build,
`main/auto_03_80229EE0_text` is gone and `main/auto_03_80229EE8_text` exists —
`# 0x80229EE8..0x80229F90 | size: 0xA8`, one function `fn_80229EE8`. `total_functions` did not
move, so the two functions were re-parented, not lost. The new auto unit needs no
`configure.py` entry. This is the same step `carve-8023289c` recorded at its front case, and
`carve-80045160` at its middle one.

## An observation for the driver (not a change)

`build/goal/judge/undef.base.count` says 286 and `build-port-link/link_summary.txt` says 286, but
the judge's `undef.base.txt` holds **284** names against the current run's 286. The two extra are
`mp_cswarmbasics` and `mp_cswarmbasics_exit`, both declared as `asm(...)` aliases in
`src/MetroidPrime/PortReachStubs.cpp:885,890` — a file that is **unmodified at HEAD** and listed in
`files.cmake` at HEAD. They cannot have come from this change: the only external reference my
object emits is `gLoader_IngPuddle` (`powerpc-eabi-nm -u` on the DOL object and on
`build-port-link/.../Carve80229EE0.c.o` both report just that one), and it resolves —
`grep -c gLoader_IngPuddle build-port-link/link_undefined.txt` is 0. The baseline *list* looks
recorded from a run in a different `PortReachStubs.cpp` state (the brief notes
`tools/goal_verify/boot-progress.sh` "restores the reach-stubs file it touches"). The count, which
is what the port gate compares, agrees, and `goal_check` passed. Worth knowing that
`undef.base.txt` and `undef.base.count` came from different runs.

## Nothing was blocked

The spelling needed no retry — the `Carve8023289C` / `Carve80200E3C` twins are byte-identical in
shape. No wall, no spelling list to record.

## Follow-ups (not fixed here, per the brief's "keep the diff to what the item needs")

- `src/MetroidPrime/ScriptObjects/CIngPuddleRel.cpp:30-32` still says of this setter: "the 8-byte
  setter is left unclaimed there because REL modules import it by its retail name". That is now
  false in the same way `IngPuddle.cpp`'s sentence was, and the `Carve8023289C` / `Carve80232868`
  commits left the analogous sentence in `AtomicBetaAccessors.cpp` and `CMysteryFlyerRel.cpp:47-49`
  stale too. A documentation fix, so no `NEW:` item for it.
- The remainder of the auto unit is now a whole unit in its own right and is plain C work; see the
  `NEW:` line below.

NEW: carve-80229ee8 | match | MetroidPrime/ScriptLoader/Carve80229EE8 | carve 0x80229EE8..0x80229F90 (0xA8 = 168 B, 42 instructions) out of the new auto unit `main/auto_03_80229EE8_text`: fn_80229EE8 is CIngPuddle's member-wise initialiser/copy - u32 at +0, four floats at +4/+8/+C/+10, u16 at +14/+16/+18, u8 at +1A, then `bl __ct__11CHealthInfoFRC11CHealthInfo` into +0x1C and `bl __ct__20CDamageVulnerabilityFRC20CDamageVulnerability` into +0x3C, ending `stw r31, 0xa8(r29)` with r31 = r7 (5th arg); bytes in `build/G2ME01/asm/auto_03_80229EE8_text.s`; both callees are already declared and host-defined in `src/` (`src/MetroidPrime/Carve8016FD4C.c:80,90` and `src/Kyoto/Alloc/PortMwccNew.cpp:88,98`), so it needs no `PortLinkStubs.cpp` entry and no `.sbss` claim - the neighbouring claims are `Carve80229EE0.c` (ends here) and `FlyerSwarm.cpp` (starts 0x80229F90); spelling not measured, and there is no CIngPuddle header in `include/`, so expect to write it as raw offsets the way the `Carve*.c` units do