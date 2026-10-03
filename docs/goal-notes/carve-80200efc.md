# carve-80200efc

**STATUS: DONE — the unit is `Matching` at 100.00% (1/1), `flip_test` PASS, full judge PASS.**

## What I did

Wrote `src/MetroidPrime/ScriptLoader/Carve80200EFC.c` (one function) and placed it as a new
`Matching` unit. Four files, one change:

- `src/MetroidPrime/ScriptLoader/Carve80200EFC.c` (new) — `.text 0x80200EFC..0x80200F04`
- `config/G2ME01/splits.txt` — new entry between `Parasite.cpp` and `PillBug.cpp`
- `configure.py` — `Object(Matching, "MetroidPrime/ScriptLoader/Carve80200EFC.c")`, one line,
  in address order
- `files.cmake` — `src/MetroidPrime/ScriptLoader/Carve80200EFC.c` listed next to
  `Carve80200E3C.c`

The claim is exactly the queued range: `Parasite.cpp` ends at 0x80200EFC and `PillBug.cpp`
starts at 0x80200F04, so this run slots between two claimed units with no unclaimed gap on
either side. `total_functions` is still **28465** (measured on `build/report.json` before and
after).

## What the function is

`fn_80200EFC`, 8 bytes, two instructions (`build/G2ME01/asm/auto_03_80200EFC_text.s`):

```
80200efc: 90 6d 95 e8   stw  r3,gLoader_Parasite@sda21(r0)
80200f00: 4e 80 00 20   blr
```

A loader setter: store the argument into the `.sbss` loader slot and return. It is the
byte-shape twin of the already-matched `fn_80200E3C` in `Carve80200E3C.c`, so the body is the
twin's: `gLoader_Parasite = loader;`. **No spelling was tried** — the twin already compiles to
these bytes, so there was nothing to search for.

`gLoader_Parasite` is `.sbss 0x80419368`, `size:0x8` (`config/G2ME01/symbols.txt:20689`),
already defined by the `Matching` unit `Parasite.cpp`, which reads all three loaders through
it. `Parasite.cpp`'s own header reserves these bytes for a separate unit and says why: "REL
modules import it by its retail name, so it cannot be renamed and must stay in dtk's auto
unit." This file is that unit, and it takes the slot as `extern` and claims `.text` only.

The argument type comes from module 47 (`CParasiteRel.cpp`, .text 0x0..0x148, 17/17 matched):
`RELExit` at 0x0BC passes `li r3,0`, and `fn_47_100` at 0x100 calls
`fn_80200EFC(&lbl_47_bss_50)`. The record is **0xC bytes = three `FScriptLoader`s**
(`build/G2ME01/Parasite/asm/auto_05_00000000_bss.s:54`, `size: 0xC`), so the struct is three
pointer-sized fields.

## Measured

| check | result |
| --- | --- |
| `./tools/decomp_build.sh MetroidPrime/ScriptLoader/Carve80200EFC` | `main/MetroidPrime/ScriptLoader/Carve80200EFC: 100.00% fuzzy, 100.00% matched (1 / 1 functions)`, `87 files OK` |
| `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged) |
| `./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80200EFC.c` | `PASS -> kept as Matching`, `kept: 1 / 1 failed: 0 skipped: 0` |
| `./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve80200EFC.c` | `.text claimed 8 ours 8 retail 8 fits`; no extra functions |
| `python3 tools/check_symbol_names.py` | `checked 603 units; 0 declared names are missing` |
| `python3 tools/check_decl_order.py --unit ...` | ok (trivially — one function cannot be permuted) |
| `./tools/goal_check.sh build/goal/item.json` | **`goal_check: PASS carve-80200efc`** |
| counts | `matched 13550 -> 13551   linked 6598 -> 6599`, `All:` did not fall |

`gate.sh` was green inside the judge, so the report diff found no function anywhere worse, all
86 RELs held, module wiring and docs claims held, and the port probe linked with 0 duplicates.

## Two things worth recording

**1. `tools/carve_diff.sh` cannot judge a carve whose only instruction is an `sda21` store, and
it reports the same false negative on the twin.** On this unit:

```
$ ./tools/carve_diff.sh 80200efc 8 build/G2ME01/src/MetroidPrime/ScriptLoader/Carve80200EFC.o
retail: 2 instructions, 8 bytes
ours  : 2 instructions, 8 bytes
  +0   retail: 80200efc stw r3,-27160(r13)  ours: 00000000 stw r3,0(0)
differing instructions: 1
NOT byte-exact
```

The `ours` bytes are `00 00 00 00` for the displacement because it is an unresolved r13
`sda21` relocation in a **relocatable** `.o`; `carve_diff.sh` disassembles the object, not the
linked DOL, so the linker's fill-in never happens. The **already-matched, judge-passed** twin
`Carve80200E3C` produces byte-identical output from the same command. Two instructions, right
length, right opcode, wrong displacement read — this is `docs/PROCESS_LESSONS.md`'s
"verification that cannot fail" in the opposite direction: an instrument that cannot pass.
`flip_test.sh` is the only thing here that decides, and the header comment in `carve_diff.sh`
already says byte-exactness is the acceptance test, so it is worth knowing the tool cannot
see it for this shape. `flip_test.sh` and the DOL sha1 both did.

**2. Listing this file in `files.cmake` costs the port nothing, and that was checked before
adding it.** `fn_80200EFC` is not among the 285 names in
`build/goal/judge/undef.base.txt`, and no port source references it (`CParasiteRel.cpp`
declares it `extern "C"` and is deliberately *not* in the flat host link). So the unit is a
DOL-only definition and the port's undefined count is unmoved; `gate.sh`'s `port probe` and
`port link dups` steps both stayed green.

Nothing is blocked and there is no `NEW:` item — the range the item named is the whole range
and all of it is claimed.

Not committed, per the brief. The judge rewrote `docs/HANDOFF.md` and
`docs/RUNNING_THE_DECOMP.md` itself via `MP_GATE_DOCS_WRITE=1`; those are derived-count rewrites
the driver owns and they are not part of this change.