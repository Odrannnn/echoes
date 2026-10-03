# carve-8022fff8 — `MetroidPrime/ScriptLoader/Carve8022FFF8`

## What I did

Carved the 8 bytes at `.text 0x8022FFF8..0x80230000` out of dtk's unclaimed
`main/auto_03_8022FFF8_text` as its own `Matching` unit — all four carve files in one change,
each entry in address order:

| file | the change |
| --- | --- |
| `src/MetroidPrime/ScriptLoader/Carve8022FFF8.c` | new, 91 lines, the body below |
| `config/G2ME01/splits.txt:2007` | new block, `.text start:0x8022FFF8 end:0x80230000`, between `PlantScarabSwarm.cpp` (ends 0x8022FFF8) and `SkyRipple.cpp` (starts 0x80232308) |
| `configure.py:993` | `Object(Matching, "MetroidPrime/ScriptLoader/Carve8022FFF8.c"),` on one line, in address order between the same two neighbours |
| `files.cmake:1000` | `src/MetroidPrime/ScriptLoader/Carve8022FFF8.c`, in address order in the ScriptLoader carve block, with a two-line comment in the style of the neighbouring entries |

I also corrected `src/MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp:6-10`, whose header said
the 8-byte setter at 0x8022FFF8 "is deliberately NOT claimed … must stay in dtk's auto unit".
That became false with this carve, and the same comment in `MetareeSwarm.cpp` was updated the
same way by `carve-8022d5a8`. Comment only — no token that affects codegen.

Nothing else in the tree was touched. No `asm`, no `PortLinkStubs.cpp` duplicate
(`grep -rn 8022FFF8 src/PortLinkStubs.cpp` is empty), no `tools/`, `build/goal/` or docs edit.
`gate.sh` rewrote the derived counts in `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md`
when it ran; I reverted both with `git checkout --` so my diff carries only the carve, and the
judge re-derives them itself anyway.

## What it is

`fn_8022FFF8` is PlantScarabSwarm's loader setter, the same 8-byte family as `fn_80200E3C`
(SpacePirate) and `fn_8022D5A8` (MetareeSwarm). Retail's eight bytes:

```
8022fff8  90 6D 98 98  stw r3, gLoader_PlantScarabSwarm@sda21(r0)
8022fffc  4E 80 00 20  blr
```

which is byte-for-byte the twin `fn_80200E3C` (`90 6D 95 D8 / 4E 80 00 20`) and the nearer
sibling `fn_8022D5A8` (`90 6D 98 60 / 4E 80 00 20`) apart from the global's address. So the
body is the twin's body with this pair's names:

```c
struct SPlantScarabLoaderSlot {
  void* loader;          /* FScriptLoader, at +0 */
  unsigned int padding;  /* at +4; never written by this function */
};

extern struct SPlantScarabLoaderSlot* gLoader_PlantScarabSwarm;

void fn_8022FFF8(struct SPlantScarabLoaderSlot* loader) { gLoader_PlantScarabSwarm = loader; }
```

The struct is declared **above** the prototype, not inside it: a struct named in a parameter
list is scoped to that list and the host build then rejects the definition as a conflicting
type (carried over from `Carve80200E3C.c:50-55` and `Carve8022D5A8.c:66-72`).

## What I measured (nothing recalled)

**The argument is a slot address, not a loader.** Both callers are in module 49's own listing
(`build/G2ME01/PlantScarabSwarm/asm/MetroidPrime/ScriptObjects/CPlantScarabSwarmRel.s`):

- `RELExit` (0x64, 0x24 B): `li r3, 0x0` / `bl fn_8022FFF8`
- `fn_49_A8` (0xA8, 0x30 B): `lis r4, fn_49_D8@ha ; lis r3, lbl_49_bss_20@ha ;
  addi r0, r4, fn_49_D8@l ; stwu r0, lbl_49_bss_20@l(r3) ; bl fn_8022FFF8`

so `r3` is `&lbl_49_bss_20`, and `lbl_49_bss_20` is `.bss:0x20 size:0x4 data:4byte`
(`config/G2ME01/rels/PlantScarabSwarm/symbols.txt:91`) — one function pointer.

**The store and the load address one slot.** `gLoader_PlantScarabSwarm` is `.sbss 0x80419618`,
`size:0x8 data:4byte` (`config/G2ME01/symbols.txt:20785`), claimed and defined by
`PlantScarabSwarm.cpp` (`.sbss 0x80419618..0x80419620`). Its reader `LoadPlantScarabSwarm` at
0x8022FFCC, from `main.elf`:

```
8022ffd8  80 CD 98 98  lwz   r6,-26472(r13)   <- the pointer just stored
8022ffdc  81 86 00 00  lwz   r12,0(r6)        <- and the loader out of it
8022ffe0  7D 89 03 A6  mtctr r12 / 8022ffe4 bctrl
```

The reader's displacement is the setter's — `-26472(r13)`, `98 98` in both — which is why the
record's first word is read at `+0` and why `fn_49_A8` hands over `&lbl_49_bss_20`.

**Verdicts.**

```
tools/carve_diff.sh 0x8022FFF8 0x8 build/G2ME01/obj/MetroidPrime/ScriptLoader/Carve8022FFF8.o
  retail: 2 instructions, 8 bytes
  ours  : 2 instructions, 8 bytes
  +0   retail: 8022fff8 stw r3,-26472(r13)  ours: 00000000 stw r3,0(0)
  differing instructions: 1
  NOT byte-exact
```

**`carve_diff.sh` is uninformative for this whole family, and that is worth knowing.** The
displacement is an `R_PPC_EMB_SDA21` **relocation** against the extern global, so an unlinked
`.o` reads `stw r3,0(0)` whatever the code is:

```
$ objdump -r -d build/G2ME01/obj/MetroidPrime/ScriptLoader/Carve8022FFF8.o
00000000 <fn_8022FFF8>:
   0:  90 60 00 00  stw r3,0(0)
               0: R_PPC_EMB_SDA21  gLoader_PlantScarabSwarm
   4:  4e 80 00 20  blr
```

The instruction encoding itself is already byte-exact (`90 60` + reloc; the linker supplies
`98 98`). The already-`Matching` twin `Carve8022D5A8` prints the identical `NOT byte-exact`,
so this verdict is a property of the family, not of this carve. `flip_test.sh` is the authority.

```
tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve8022FFF8.c
   .text claimed 8   ours 8   retail 8   fits
   no extra functions: our object defines only what the retail unit object does
```

```
tools/flip_test.sh MetroidPrime/ScriptLoader/Carve8022FFF8.c MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp
TEST MetroidPrime/ScriptLoader/Carve8022FFF8.c
  PASS  -> kept as Matching
TEST MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp
  PASS  -> kept as Matching
kept: 2 / 2   failed: 0   skipped: 0
```

I flipped the neighbour too because I edited its header; a Matching unit is worth
re-verifying when anything in it moves.

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item carve-8022fff8 (match) target=MetroidPrime/ScriptLoader/Carve8022FFF8
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13596 -> 13597   linked 6644 -> 6645
  ok    check_symbol_names.py
  ok    All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13597 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve8022FFF8.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8022fff8
```

`total_functions` is still **28465** after the `splits.txt` edit, and the DOL is
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` with our object in the link — the one rule.

`report.json` for the new unit: `100.00%` fuzzy, `matched_functions 1 / 1`,
`complete_units 1`, `total_code 8 / matched_code 8`.

## Lessons (not `NEW:` items — nothing here is unblocked work)

- **`carve_diff.sh` cannot verify a loader-setter carve.** Every member of this family stores
  through an extern `.sbss` global, so the displacement is a relocation and the pre-link object
  always differs from retail in exactly that word. For this family use `objdump -r` to confirm
  the relocation is `R_PPC_EMB_SDA21` against the right symbol, and let `flip_test.sh` decide.
- **`check_decl_order.py --unit` reports "0 unit(s) checked" for a one-function `.c`.** It has
  nothing to compare, which is the right answer — a single function cannot be emitted out of
  order — but it is worth knowing the check is vacuous there rather than assuming it ran.
- **A carve next to a pre-existing unit boundary linked fine here**, which is the arrangement
  `RUNNING_THE_DECOMP.md` warns about for `0x80302BAC` (a cycle with `CFrustumPlanes.cpp`).
  Proximity to a claimed neighbour is not by itself fatal; the cycle needs the neighbour to
  point back into the same auto range.

## What is left, and why it is not queued

The rest of `main/auto_03_8022FFF8_text` — 0x80230000..0x80232308, **15 functions** (16 before
this carve, measured by `grep -c '^\.fn '`) — stays retail. It is `CPortalTransition`'s and
`CScriptPortalTransition`'s own method bodies: camera-spline and filter-pass rendering, several
hundred bytes each, and three of them already carry retail names
(`TouchModels__17CPortalTransitionFv` at 0x80230A20,
`CreateTransition__23CScriptPortalTransitionCFR13CStateManager` at 0x80231B14, and the
destructors), so they need their own mangling and a `.cpp`. That is a `progress` item on
`MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp`'s neighbourhood, not a carve, and it is a
multi-hour class-layout job (`CActorLights`, `CLight`, `CToken`, `CGameSpline` layouts first).
No `NEW:` line filed: none of it is bounded work whose success raises a count on its own.
