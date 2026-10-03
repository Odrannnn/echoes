# carve-8022d5a8 — `MetroidPrime/ScriptLoader/Carve8022D5A8`

## What I did

Carved the 8 bytes at `.text 0x8022D5A8..0x8022D5B0` out of dtk's unclaimed
`main/auto_03_8022D5A8_text` as its own `Matching` unit, all four files in one change:

| file | the change |
| --- | --- |
| `src/MetroidPrime/ScriptLoader/Carve8022D5A8.c` | new, 82 lines, the body (below) |
| `config/G2ME01/splits.txt` | new block, `.text start:0x8022D5A8 end:0x8022D5B0`, between `MetareeSwarm.cpp` (ends 0x8022D5A8) and `Carve8022D758.c` (starts 0x8022D758) |
| `configure.py` | `Object(Matching, "MetroidPrime/ScriptLoader/Carve8022D5A8.c"),` on one line, in address order between `MetareeSwarm.cpp` and `Carve8022D758.c` |
| `files.cmake` | `    src/MetroidPrime/ScriptLoader/Carve8022D5A8.c` in the sorted Carve block, between `Carve8022A3F4.c` and `Carve8022D758.c` |

The one extra edit: `src/MetroidPrime/ScriptLoader/MetareeSwarm.cpp` lines 6-10 said the
0x8022D5A8 setter "must stay in dtk's auto unit", which this change makes false. Rewritten to
the wording `ScriptLoader/SpacePirate.cpp` already uses for the same situation ("is its own
unit, `Carve80200E3C.c`: REL modules import it by its retail name, so it stays `fn_80200E3C`
verbatim..."). Comment only, 6 lines.

The body, exactly the twin's body with this pair's names:

```c
struct SMetareeLoaderSlot {
  void* loader;            /* FScriptLoader, at +0 */
  unsigned int padding;    /* at +4; never written by this function */
};
extern struct SMetareeLoaderSlot* gLoader_MetareeSwarm;
void fn_8022D5A8(struct SMetareeLoaderSlot* loader) { gLoader_MetareeSwarm = loader; }
```

`.sbss` is **not** claimed: `MetroidPrime/ScriptLoader/MetareeSwarm.cpp` already claims and
defines `gLoader_MetareeSwarm` at 0x804195E0, so this unit takes it as `extern`, which is the
arrangement `Carve80200E3C.c` / `SpacePirate.cpp` already use. No `.sbss` claim was needed, and
no `PortLinkStubs.cpp` duplicate existed (`fn_8022D5A8` appears nowhere in that file) - measured,
not assumed; the gate's `port link` line reports 0 duplicates.

## What it is, and how I know

The twin is `fn_80200E3C` (`src/MetroidPrime/ScriptLoader/Carve80200E3C.c`), measured `Matching`
1/1 in `build/report.json` before this change. Retail's two words differ only in the symbol
name: 0x8022D5A8 is `90 6D 98 60 / 4E 80 00 20`, 0x80200E3C is `90 6D 95 D8 / 4E 80 00 20`
(`stw r3, gLoader_X@sda21(r0)` / `blr`).

The argument is read off module 43's own listing
(`build/G2ME01/MetareeSwarm/asm/MetroidPrime/ScriptObjects/CMetareeSwarmRel.s`), not assumed:

* `RELExit` at module 0x64, 0x24 B: `li r3, 0` then `bl fn_8022D5A8`.
* `fn_43_A8` at 0xA8, 0x30 B: `lis r4, fn_43_D8@ha ; lis r3, lbl_43_bss_20@ha ; addi r0,r4,fn_43_D8@l ;
  stwu r0, lbl_43_bss_20@l(r3) ; bl fn_8022D5A8` - so the argument is the **address** of the
  module's own four-byte slot, not a loader.

That is why what is stored is a pointer: `MetareeSwarm.cpp`'s reader `LoadMetareeSwarm` at
0x8022D57C emits `lwz r6, gLoader_MetareeSwarm@sda21(r0)` / `lwz r12, 0x0(r6)` / `mtctr r12` /
`bctrl` (measured in `build/G2ME01/asm/MetroidPrime/ScriptLoader/MetareeSwarm.s`), i.e. it loads
the pointer this setter stored and then the loader out of it.

Slot: `config/G2ME01/symbols.txt:20778` `gLoader_MetareeSwarm = .sbss:0x804195E0; //
type:object size:0x8 data:4byte`.

## Measured result

`./tools/goal_check.sh build/goal/item.json` → **PASS** (run twice, both from the final tree):

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13589 -> 13590   linked 6637 -> 6638
  ok    check_symbol_names.py
  ok    All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13590 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve8022D5A8.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8022d5a8
```

* `./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve8022D5A8.c` → `PASS -> kept as Matching`,
  `kept: 1 / 1 failed: 0 skipped: 0`, and the `Object(Matching, ...)` entry is present on one
  line.
* `build/report.json`: `main/MetroidPrime/ScriptLoader/Carve8022D5A8` **1 / 1, complete=true**.
  `matched` 13589 → 13590, `linked` 6637 → 6638, **`total_functions` 28465 → 28465** (unchanged
  after the `splits.txt` edit, as required).
* `sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, the pinned hash,
  i.e. the DOL still reproduces retail **with this unit's own object in the link**.
* Our object's emitted `.text`, from `build/G2ME01/asm/MetroidPrime/ScriptLoader/Carve8022D5A8.s`,
  is the retail two instructions byte for byte (`90 6D 98 60`, `4E 80 00 20`).
* `./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve8022D5A8.c` → `.text claimed 8 ours 8
  retail 8 fits` / `no extra functions`.
* `python3 tools/check_symbol_names.py` → 0 missing names (gate step ok).
* No `asm` added; the diff touches only the four carve files plus a comment in
  `MetareeSwarm.cpp`. `python3 tools/check_docs_claims.py` and
  `python3 tools/check_files_cmake.py` both pass as gate steps.

The gate's per-function diff line is worth reading, because "SPLIT" is the one thing here that
could have been a loss:

```
per-function diff             SPLIT   main/auto_03_8022D5A8_text: 5 function(s) accounted for
                                         across 2 new unit(s) in main (exact count match - a split, not a loss)
```

That is the arithmetic of this carve: 1 function into `Carve8022D5A8` + 4 left behind in
`auto_03_8022D5B0_text` (the auto unit is renamed by its new first function, and
`fn_8022D5B0`/`fn_8022D610`/`fn_8022D724`/`fn_8022D738` are still retail) = 5. Nothing went
missing.

## One measured tool limitation, worth not spending another lane's time on

`tools/carve_diff.sh` **cannot** judge this family, and its "NOT byte-exact" here is not a
defect. It reads raw bytes out of the `.o`, and the `stw r3, gLoader_X@sda21(r0)` displacement
word is a **linker-filled relocation**, so in the object it is still zero:

```
$ ./tools/carve_diff.sh 8022D5A8 8 build/G2ME01/obj/MetroidPrime/ScriptLoader/Carve8022D5A8.o
retail: 2 instructions, 8 bytes
ours  : 2 instructions, 8 bytes
  +0   retail: 8022d5a8 stw r3,-26528(r13)  ours: 00000000 stw r3,0(0)
differing instructions: 1
NOT byte-exact
```

The **already-`Matching`** twin prints the identical thing, which is what rules out a defect in
my body:

```
$ ./tools/carve_diff.sh 80200E3C 8 build/G2ME01/obj/MetroidPrime/ScriptLoader/Carve80200E3C.o
  +0   retail: 80200e3c stw r3,-27176(r13)  ours: 00000000 stw r3,0(0)
differing instructions: 1
NOT byte-exact
```

So for any carve whose body is a single `@sda21` store, `carve_diff.sh` is a **check that
cannot fail**, and the authority is objdiff's per-function match plus the DOL sha1 - both
green here. (For carves with real data displacements it is still useful.) The item brief tells
lanes to check bytes with `carve_diff.sh`; that advice is right for most carves and misleading
for this shape. Read it as "instruction count and mnemonic shape", and let `flip_test.sh` decide.

## The rest of the auto range stays retail

`main/auto_03_8022D5B0_text` is 4 functions (`fn_8022D5B0` 0x60, `fn_8022D610` 0x114,
`fn_8022D724` 0x14, `fn_8022D738` 0x20), 0x8022D5B0..0x8022D758. None is a twin candidate:
they are `CIOWin`'s destructor pair, a `CGraphics`-touching `OnMessage` dispatch, and two small
flag helpers, so they belong to classes this tree does not have. I claimed exactly
0x8022D5A8..0x8022D5B0 and nothing else.

## No blockers, no `NEW:` lines

Nothing was blocked, no wall was hit (the twin spelling was byte-exact on the first try, which
is what the seed predicted), and I have no follow-up worth a lane. `docs/HANDOFF.md` and
`docs/RUNNING_THE_DECOMP.md` were rewritten by `gate.sh`'s own `MP_GATE_DOCS_WRITE=1` state-block
sync (derived counts only, +1 matched / +1 linked / probe 990 → 991); per the brief I reverted
those two files, and the judge PASSes from the clean-docs tree.