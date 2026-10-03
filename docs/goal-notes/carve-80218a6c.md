# carve-80218a6c — `match` on `MetroidPrime/ScriptLoader/Carve80218A6C`

**Landed: one 8-byte loader setter, `fn_80218A6C` at 0x80218A6C..0x80218A74, as a new `Matching`
unit. `flip_test.sh` PASS and kept the unit; the judge PASS.**

## What I did

Carved `fn_80218A6C` out of dtk's `main/auto_03_80218A6C_text` into
`src/MetroidPrime/ScriptLoader/Carve80218A6C.c`, the four-file carve, in address order:

| file | change |
| --- | --- |
| `config/G2ME01/splits.txt` | new `MetroidPrime/ScriptLoader/Carve80218A6C.c` block, `.text 0x80218A6C..0x80218A74`, between the `MediumIng.cpp` and `MinorIng.cpp` blocks (`splits.txt:1650-1651`) |
| `configure.py:858` | `Object(Matching, "MetroidPrime/ScriptLoader/Carve80218A6C.c")`, one line, between `MediumIng.cpp` and `MinorIng.cpp` |
| `files.cmake:871-874` | the source, with a three-line comment, next to the other ScriptLoader carves |
| `src/MetroidPrime/ScriptLoader/Carve80218A6C.c` | the unit |

Plus one stale-claim fix, the same correction `Carve802188E4.c` made to `DarkSamus.cpp`:
`src/MetroidPrime/ScriptLoader/MediumIng.cpp:6-9` said the setter was "deliberately NOT claimed …
must stay in dtk's auto unit". It now names the carve. The carve preserves the name, which is
the part that sentence was really about.

## What I measured

The claim is exactly the gap and nothing else. `symbols.txt:9476` is
`fn_80218A6C = .text:0x80218A6C; // type:function size:0x8 align:4`; the nearest claims below and
above are `MediumIng.cpp` `.text 0x80218A40..0x80218A6C` and `MinorIng.cpp`
`.text 0x80218A74..0x80218AA0`, so 8 bytes is the whole gap and the function fills it.

The body, from `build/G2ME01/asm/auto_03_80218A6C_text.s`:

```
fn_80218A6C    0x80218A6C  0x8    stw r3, gLoader_MediumIng@sda21(r0)
                               blr
```

The slot `gLoader_MediumIng` is `.sbss 0x804193F8`, `size:0x8 data:4byte`
(`symbols.txt:20712`), claimed and defined by `MediumIng.cpp` (`:12-17`), so this unit claims
`.text` only and takes the pointer as `extern`. No `PortLinkStubs.cpp` duplicate exists:
`grep -rn "fn_80218A6C" src/` returns only the carve and `CMediumIngRel.cpp`'s declaration.

**The name is the module's import, which is why this is a `.c`.**
`strings build/G2ME01/MediumIng/MediumIng.plf | grep 80218A6C` → `fn_80218A6C`.

**The argument, read off module 41** (`config/G2ME01/config.yml:287-290`, sha1
`4ff29124bdf74564c3d33e6ae5948071479d9e61`, matching `orig/G2ME01/files/RelProd/MediumIng.rel`),
from `build/G2ME01/MediumIng/asm/MetroidPrime/ScriptObjects/CMediumIngRel.s`:

- `RELExit` (`.text` 0xDC): `li r3, 0x0` (line 115), `bl fn_80218A6C` (line 117) — a null.
- `fn_41_120` (`.text` 0x120, the registration `RELMain` calls at line 129):
  `lis r4, fn_41_150@ha` / `lis r3, lbl_41_bss_10@ha` (lines 140-141),
  `addi r0, r4, fn_41_150@l` / `stwu r0, lbl_41_bss_10@l(r3)` (lines 143-144) — a store
  *through* the slot address, which writes the module's own record and leaves r3 holding its
  address — then `bl fn_80218A6C` (line 145).

So the argument is `&lbl_41_bss_10`, and `auto_05_00000000_bss.s` gives it `size:0x4`: **one**
word, unlike SpacePirate's and DarkSamus's 0x1C-byte `*_FuncPtrs` records. That module's single
`FScriptLoader`, which `CMediumIngRel.cpp:149` already writes as `fn_80218A6C(&lbl_41_bss_10)`
against the `:134` declaration.

## Verification (all run in this tree)

```
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (pinned hash, matches)
./tools/decomp_build.sh         All: 37.68% fuzzy, 31.11% matched, 13.98% linked (13566 / 28465)
python3 tools/check_symbol_names.py    checked 608 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve80218A6C
                                    ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve80218A6C.c
                                    .text claimed 8  ours 8  retail 8  fits; no extra functions
./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80218A6C.c
                                    PASS  -> kept as Matching   (kept 1/1, failed 0, skipped 0)
./tools/goal_check.sh build/goal/item.json
    ok  no judge-owned path touched
    ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
    ok  counts: matched 13565 -> 13566   linked 6613 -> 6614
    ok  flip_test ... PASS, Object(Matching) in configure.py
    goal_check: PASS carve-80218a6c
```

`total_functions` is **28465**, unchanged, as required after a `splits.txt` edit.
`build/report.json` has `main/MetroidPrime/ScriptLoader/Carve80218A6C` at **1 / 1** matched.
`probe_sources.sh` went 966 → 967 files with 0 failures (the file header in `HANDOFF.md` was
rewritten with that number), which is the direct evidence that the new `.c` compiles **and
links** in the host port build.

## Two things measured that are worth the next lane's time

**1. `carve_diff.sh` says `NOT byte-exact` on every loader-setter carve, and it is a false
negative.** `./tools/carve_diff.sh 0x80218A6C 0x8 build/G2ME01/obj/MetroidPrime/ScriptLoader/Carve80218A6C.o`:

```
retail: 2 instructions, 8 bytes
ours  : 2 instructions, 8 bytes
  +0   retail: 80218a6c stw r3,-27016(r13)  ours: 00000000 stw r3,0(0)
differing instructions: 1
NOT byte-exact
```

The displacement is `-27016 = 0xFFF9 6990`, i.e. **`sda21`**, which the linker fills in for a
cross-object reference; the unlinked object has 0 there and objdump prints `(0)`. This is not a
defect in the unit — I ran the same command on the two already-landed siblings and they report
identically:

```
0x802188E4 (DarkSamus,  Matching)   retail stw r3,-27056(r13)   ours stw r3,0(0)   NOT byte-exact
0x80232834 (FogOverlay, Matching)   retail stw r3,-26440(r13)   ours stw r3,0(0)   NOT byte-exact
```

Both have `main.dol` at the pinned sha1. So for this family the acceptance test is `flip_test.sh`
plus the DOL hash, and `carve_diff.sh`'s verdict carries no information. Do not spend a run
chasing it. (A same-object slot reference would show a real displacement, so the useful rule is
narrower than "ignore the tool": ignore it when the store target is `extern`.)

**2. `fn_802188B0`'s importing module is `CommandoPirate`, not `CommandPirate`** — the setter that
stores into `gLoader_CommandPirate` (`.sbss 0x804193C8`, `symbols.txt:20706`) is imported by
`build/G2ME01/CommandoPirate/CommandoPirate.plf`. `CommandPirate.plf` does not contain the name.
Retail reused the one slot for both modules, or the names are off by one module; either way the
caller is not where the slot's name suggests, and a lane writing that header from the slot name
would cite the wrong `.plf`.

## Next: four more setters of exactly this shape, all unclaimed, all 8 bytes

Each is the gap between two already-claimed units, one function, and `+1` matched each. Verified
from `symbols.txt` (all `size:0x8`), the four `auto_03_*_text.s` listings, and
`config/G2ME01/splits.txt` (none is inside or bounding any existing `.text` claim):

| address | `symbols.txt` | body | slot (claimed by) | importing module |
| --- | --- | --- | --- | --- |
| 0x802188B0 | 9463 | `stw r3, gLoader_CommandPirate@sda21(r0)` / `blr` | 0x804193C8 (`CommandPirate.cpp`) | `CommandoPirate` — see above |
| 0x80218A04 | 9472 | `stw r3, gLoader_FlyingPirate@sda21(r0)` / `blr` | 0x804193E8 (`FlyingPirate.cpp`) | `FlyingPirate` |
| 0x80218AA0 | 9478 | `stw r3, gLoader_MinorIng@sda21(r0)` / `blr` | 0x80419400 (`MinorIng.cpp`) | `MinorIng` |
| 0x80218AD4 | 9480 | `stw r3, gLoader_ElitePirate@sda21(r0)` / `blr` | 0x80419408 (`ElitePirate.cpp`) | `ElitePirate` |

`docs/research/rel_loaders.md`'s table indexes the `Load*` thunks, not these setters, so it needs
no row and I did not touch it.

NEW: carve-80218a04 | match | MetroidPrime/ScriptLoader/Carve80218A04 | the FlyingPirate module's 8-byte loader setter, `.text 0x80218A04..0x80218A0C`, `symbols.txt:9472`, only `.fn` of `auto_03_80218A04_text.s`, unclaimed between `FlyingPirate.cpp` (ends 0x80218A04) and `Grenchler.cpp` (starts 0x80218A0C); `stw r3, gLoader_FlyingPirate@sda21(r0)` / `blr`, slot `.sbss 0x804193E8` (`symbols.txt:20710`) defined by `FlyingPirate.cpp`; copy `Carve80218A6C.c` and ignore `carve_diff.sh`'s NOT byte-exact
NEW: carve-80218aa0 | match | MetroidPrime/ScriptLoader/Carve80218AA0 | the MinorIng module's 8-byte loader setter, `.text 0x80218AA0..0x80218AA8`, `symbols.txt:9478`, only `.fn` of `auto_03_80218AA0_text.s`, unclaimed between `MinorIng.cpp` (ends 0x80218AA0) and `ElitePirate.cpp` (starts 0x80218AA8); `stw r3, gLoader_MinorIng@sda21(r0)` / `blr`, slot `.sbss 0x80419400` (`symbols.txt:20713`) defined by `MinorIng.cpp`; copy `Carve80218A6C.c` and ignore `carve_diff.sh`'s NOT byte-exact
NEW: carve-80218ad4 | match | MetroidPrime/ScriptLoader/Carve80218AD4 | the ElitePirate module's 8-byte loader setter, `.text 0x80218AD4..0x80218ADC`, `symbols.txt:9480`, only `.fn` of `auto_03_80218AD4_text.s`, unclaimed between `ElitePirate.cpp` (ends 0x80218AD4) and `Blogg.cpp` (starts 0x80218ADC); `stw r3, gLoader_ElitePirate@sda21(r0)` / `blr`, slot `.sbss 0x80419408` (`symbols.txt:20714`) defined by `ElitePirate.cpp`; copy `Carve80218A6C.c` and ignore `carve_diff.sh`'s NOT byte-exact
NEW: carve-802188b0 | match | MetroidPrime/ScriptLoader/Carve802188B0 | an 8-byte loader setter, `.text 0x802188B0..0x802188B8`, `symbols.txt:9463`, only `.fn` of `auto_03_802188B0_text.s`, unclaimed between `CommandPirate.cpp` (ends 0x802188B0) and `DarkSamus.cpp` (starts 0x802188B8); `stw r3, gLoader_CommandPirate@sda21(r0)` / `blr`, slot `.sbss 0x804193C8` (`symbols.txt:20706`) defined by `CommandPirate.cpp` - note the importing module is **CommandoPirate**, per `strings build/G2ME01/CommandoPirate/CommandoPirate.plf | grep -x fn_802188B0`, so read the call sites there and not in `CommandPirate.plf`