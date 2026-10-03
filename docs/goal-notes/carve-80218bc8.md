# carve-80218bc8 — `MetroidPrime/ScriptLoader/Carve80218BC8` (match)

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` exits 0 with every check `ok`;
`./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80218BC8.c` prints `PASS -> kept as
Matching`; `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and dtk
reports `87 files OK`. `report.json`: `main/MetroidPrime/ScriptLoader/Carve80218BC8` is
`complete: true`, **1/1** matched functions at 100.0%, `total_functions` still **28465**,
`matched` 13570 -> **13571**, `linked` 6618 -> **6619**. The gate's per-function diff prints
`SPLIT main/auto_03_80218BC8_text: 1 function(s) accounted for across 1 new unit(s) in main
(exact count match - a split, not a loss)`.

## What was claimed, and what it is

Claimed range **0x80218BC8..0x80218BD0** (`config/G2ME01/splits.txt`), 0x8 = 8 bytes, one
function (`symbols.txt:9488`: `fn_80218BC8 = .text:0x80218BC8; size:0x8 align:4`), which dtk
held in `build/G2ME01/asm/auto_03_80218BC8_text.s`:

```
# 0x80218BC8..0x80218BD0 | size: 0x8
.fn fn_80218BC8, global
/* 80218BC8 002159C8  90 6D 96 A0 */  stw r3, gLoader_GunTurretBase@sda21(r0)
/* 80218BCC 002159CC  4E 80 00 20 */  blr
```

**It is GunTurretBase's loader setter**, and `reason`'s twin was right: the byte-shape twin is
the matched `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
(`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`) - store the argument into the loader pointer's
`.sbss` slot and return. No spelling had to be tried; the family is already landed nine times
over, and the only difference between the members is the 16-bit `@sda21` displacement, because
the slots are 8 bytes apart. Measured with
`build/binutils/powerpc-eabi-objdump -d build/G2ME01/main.elf` against `_SDA_BASE_ = 0x8041FD80`
(`powerpc-eabi-nm`), all ten of them:

| function | slot | addr | word | disp |
| --- | --- | --- | --- | --- |
| `fn_80200E3C` | `gLoader_SpacePirate` | 0x80419358 | `90 6D 95 D8` | -0x6A28 |
| `fn_80200E70` | `gLoader_Kralee` | 0x80419360 | `90 6D 95 E0` | -0x6A20 |
| `fn_80200EFC` | `gLoader_Parasite` | 0x80419368 | `90 6D 95 E8` | -0x6A18 |
| `fn_80200F30` | `gLoader_PillBug` | 0x80419370 | `90 6D 95 F0` | -0x6A10 |
| `fn_80213CB8` | `gLoader_SporbBase` | 0x804193A8 | `90 6D 96 28` | -0x69D8 |
| `fn_8021887C` | `gLoader_Sandworm` | 0x804193C0 | `90 6D 96 40` | -0x69C0 |
| `fn_802188E4` | `gLoader_DarkSamus` | 0x804193D0 | `90 6D 96 50` | -0x69B0 |
| `fn_80218918` | `gLoader_Ings` | 0x804193D8 | `90 6D 96 58` | -0x69A8 |
| `fn_802189D0` | `gLoader_SandBoss` | 0x804193E0 | `90 6D 96 60` | -0x69A0 |
| **`fn_80218BC8`** | **`gLoader_GunTurretBase`** | **0x80419420** | **`90 6D 96 A0`** | **-0x6960** |

Every other byte of all ten is identical, including the `4E 80 00 20` `blr`.

## What the callers say the argument is (measured, not assumed)

Both callers are in module 28's own listing, `build/G2ME01/GunTurret/asm/
auto_00_00000404_text.s`, and neither calls it a setter by name:

* `RELExit` at `.text 0x430` (0x24 bytes) does `li r3, 0x0` then `bl fn_80218BC8` - the module
  tears its loaders down on the way out.
* `fn_28_474` at `.text 0x474` (0x3C bytes, reached from `RELMain` at 0x454) stores
  `fn_28_4B0` into `lbl_28_bss_60 + 0` and `fn_28_7B98` into `+4` and then calls it with `r3` =
  `&lbl_28_bss_60`. So the argument is that record's address and the record is **8 bytes, two
  `FScriptLoader`s**: `build/G2ME01/GunTurret/asm/auto_05_00000000_bss.s` reads
  `# .bss:0x60 | 0x60 | size: 0x8` / `.obj lbl_28_bss_60, global`, and that module holds four
  such 0x8-byte records (0x0, 0x20, 0x40, 0x60), three 0x18-byte ones and two 0xC-byte ones.
  `src/MetroidPrime/ScriptLoader/GunTurretBase.cpp` is that record in C++ -
  `SGunTurretBaseLoaders { FScriptLoader slot0; FScriptLoader slot1; }`, `LoadGunTurretBase`
  reading `+0` and `LoadGunTurretTop` reading `+4`. **Which** of `fn_28_4B0` / `fn_28_7B98` is
  which of the two is not settled here, so the source's field comments do not claim it.

The slot is `gLoader_GunTurretBase` at `.sbss 0x80419420` (`type:object size:0x8 data:4byte`,
`symbols.txt:20717`), which `GunTurretBase.cpp` - a `Matching` unit claiming `.text
0x80218B70..0x80218BC8` and `.sbss 0x80419420..0x80419428` - already defines
(`SLoaderSlot { SGunTurretBaseLoaders* value; unsigned int padding; }`) and already reads
through at `+0`. **Its own header had already reserved these eight bytes for a separate unit**:

> The 8-byte setter at 0x80218BC8 is deliberately NOT claimed: REL modules import it by its
> retail name, so it cannot be renamed and must stay in dtk's auto unit.

That is this file. So the unit claims `.text` only and takes the slot as `extern`.

## The four files (all in one change, each entry in address order)

* `config/G2ME01/splits.txt` - `MetroidPrime/ScriptLoader/Carve80218BC8.c: .text start:0x80218BC8
  end:0x80218BD0`, between `GunTurretBase.cpp` (ends 0x80218BC8) and `Lumite.cpp` (starts
  0x80218BD0). No unclaimed gap on either side, and the claim spans no gap.
* `configure.py` - `Object(Matching, "MetroidPrime/ScriptLoader/Carve80218BC8.c"),` on **one
  line**, between `GunTurretBase.cpp` and `Lumite.cpp`.
* `files.cmake` - `src/MetroidPrime/ScriptLoader/Carve80218BC8.c`, after
  `Carve802189D0.c` (0x802189D0..0x802189D8) and before `Carve802201F8.cpp` (0x802201F8), with
  the three-line comment the neighbouring carves carry.
* `src/MetroidPrime/ScriptLoader/Carve80218BC8.c` - the source, header comment in the style of
  `Carve80200E3C.c` / `src/Dolphin/Carve8038A7DC.c`.

Exactly this range, nothing else. **No `PortLinkStubs.cpp` duplicate**: `grep -rn 'fn_80218BC8'
src/ include/ files.cmake configure.py` matched only the new carve's own entry, and `gate.sh`'s
`port link gap` / `port link dups` steps pass. Listing a `.c` here adds a definition and so
cannot raise the port's undefined count (`undef.base.txt`'s 285 names is a floor).

## The three checks, and what each one actually proved

* `./tools/decomp_build.sh MetroidPrime/ScriptLoader/Carve80218BC8.c` - the split applied, dtk
  built the new object, `87 files OK`, DOL hash unchanged. **The link-order-cycle hazard did not
  fire here**: this run starts exactly where the pre-existing `Matching` `GunTurretBase.cpp`
  ends, which is the shape `RUNNING_THE_DECOMP.md` records as failing for `CFrustumPlanes.cpp`
  -> `auto_03_803029D8_text`. It did not, and the same shape is already landed twice in this
  directory (`Carve80200EFC.c` after `Parasite.cpp`, `Carve80200F30.c` after `PillBug.cpp`), so
  "a carve at a unit boundary always cycles" is too strong - the boundary alone is not the cause.
* `python3 tools/check_decl_order.py --unit 80218BC8` - `ok: 1 unit(s) checked, none emits its
  functions out of retail order`. **It cannot fire before the first build**, though: run against
  the tree as handed over it printed `0 unit(s) checked`, because the unit is not in
  `report.json` until a build has produced it. Worth knowing, and the reason the notes of
  `carve-8000432c` say the same thing.
* `./tools/carve_diff.sh 80218BC8 8 build/G2ME01/src/MetroidPrime/ScriptLoader/Carve80218BC8.o
  fn_80218BC8` - `retail: 2 instructions, 8 bytes` / `ours: 2 instructions, 8 bytes`, one
  differing instruction, and that difference is only the **unresolved `@sda21` displacement**:
  in the relocatable object our side reads `stw r3,0(0)` against retail's
  `stw r3,-26976(r13)`, which is the relocation mwldeppc fills. Do not read `NOT byte-exact`
  here as a mismatch; the linked bytes are the flip's subject.
* `./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve80218BC8.c` - `.text claimed 8, ours 8,
  retail 8, fits`; `no extra functions: our object defines only what the retail unit object
  does`. `nm` on our object: `00000000 T fn_80218BC8` and `U gLoader_GunTurretBase`, two symbols.
* `./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80218BC8.c` - `kept: 1 / 1`, and
  `configure.py` still reads `Object(Matching, ...)` on one line afterwards.

## Notes on the source

* `.c`, not `.cpp`: `symbols.txt` carries the `fn_<addr>` placeholder, and module 28 imports this
  function by that exact retail name, so the definition has to stay unmangled. Defining it here
  keeps `fn_80218BC8` in the DOL link, which is what module 28's two `bl fn_80218BC8` resolve
  against.
* The record is declared **above** the definition, not inside its parameter list: a struct named
  in a parameter list is scoped to that list, and the flat host build then rejects the definition
  as a conflicting type (the trap `Carve80200E3C.c` records).
* The slot is spelled `extern struct SGunTurretBaseLoaders* gLoader_GunTurretBase;` - i.e. as the
  4-byte pointer member of the 8-byte `SLoaderSlot` `GunTurretBase.cpp` defines, because that is
  the only word retail stores. MWCC does not encode a variable's type in its name, so this
  references the symbol itself. Same convention as `Carve80200E70.c` / `Carve80200F30.c`.
* One function, so the descending-declaration-order rule cannot be got wrong here (mwcceppc emits
  in reverse source order); the header says so, as the family's do.
* No `asm`, no assembly, no transcribed bytes.

## NEW

None. The item is done and the tree is left in exactly the state the judge passed: four files,
no commit, nothing outside `wt-mp2-goal-L5` touched.