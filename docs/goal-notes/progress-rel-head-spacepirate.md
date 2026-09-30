# progress-rel-head-spacepirate

SpacePirate's (module 72) head and entry path, `.text 0x0..0x140`, as one `Matching` unit:
`src/MetroidPrime/ScriptObjects/CSpacePirateRel.cpp`, **14 functions, all 100.00%**. Module 72 had
no own-code unit at all before this - its seven functions were the shared `REL_Setup` and
`global_destructor_chain` units and nothing of ours. It is now **21 of 262**.

## The four files

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/CSpacePirateRel.cpp` | new, 14 functions, descending by retail offset |
| `config/G2ME01/rels/SpacePirate/splits.txt` | `MetroidPrime/ScriptObjects/CSpacePirateRel.cpp: .text start:0x00000000 end:0x00000140`, added first (address order) |
| `configure.py` | `Rel("SpacePirate", [Object(Matching, "MetroidPrime/ScriptObjects/CSpacePirateRel.cpp")])` at the end of the list, with the measured notes |
| `docs/research/raw_offsets.md` | new `## src/MetroidPrime/ScriptObjects/CSpacePirateRel.cpp (1 site)` section, and the "debt, measured" total refreshed to the tool's `155 raw-offset site(s) in 64 file(s)` |

Not in `files.cmake`, deliberately, as `CIngRel.cpp` / `CFlyingPirateRel.cpp`: it calls `fn_72_140`
and `fn_80200E3C`, which the port cannot link. `tools/check_files_cmake.py` accepts it as a
`MODULE_ENTRY` source (38 units are out for that reason now, +1 here).

## Claimed range

```
0x000 fn_72_0   0x08  li r3,1                     a predicate that is always true
0x008 fn_72_8   0x08  addi r3,r3,0x920            the address of the member at +0x920
0x010 fn_72_10  0x08  li r3,1                     a predicate that is always true
0x018 fn_72_18  0x0C  lhz r0,0xa94(r4) ; sth r0,0x0(r3)   a copy of the TUniqueId at +0xa94
0x024 fn_72_24  0x0C  lbl_72_rodata_B00           this module's own .rodata:0xB00, .float 50
0x030 fn_72_30  0x10  lbl_8041AAB8 -> *((float*)(self + 0x448))
0x040 fn_72_40  0x08  li r3,0                     a predicate that is always false
0x048 fn_72_48  0x0C  the byte at +0x34c, bit 3
0x054 fn_72_54  0x08  addi r3,r3,0x754            the address of the member at +0x754
0x05C fn_72_5C  0x08  li r3,1                     a predicate that is always true
0x064 fn_72_64  0x2C  virtual dispatch, vtable slot 0x38
0x090 RELExit   0x24  li r3,0 / bl fn_80200E3C
0x0B4 RELMain   0x20  bl fn_72_D4
0x0D4 fn_72_D4  0x6C  lbl_72_bss_24 = {fn_72_140, lbl_72_data_8C0, lbl_72_data_8CC}
```

**Unlike `FlyingPirate`, the claim starts at 0x0** - this module has no behavioural class code
above the head, so the whole head is claimed and nothing of ours is left unclaimed at the bottom.
Everything from `fn_72_140` (0x140, 0xACC) up stays retail: it is the module's own entity loader
and its class methods, which need the CActor/CPatterned hierarchy this tree does not model. The
module's 262 text symbols now split **14 ours + 5 `REL_Setup` + 2 `global_destructor_chain` + 241
unclaimed**.

**Nine of the eleven accessors are the family** and transferred verbatim from units already
`Matching` here: the `+0x448` float store (`fn_22_4E0`), the bit at +0x34c (`fn_29_4C`), the
`+0x754` member address (`fn_29_64`), the predicates, the vtable dispatch (`fn_29_90`). Which body
belongs to which offset was read off `build/G2ME01/SpacePirate/asm/auto_00_00000000_text.s`, not off
the `fn_<id>_<off>` names. Unlike `CIngRel.cpp` this head has **no `GetBoundingBox` wrapper, no
three-float copy and no `*id = kInvalidUniqueId` reset**, which is why it is eleven accessors where
Ing's is fourteen and why the claim ends at 0x140 rather than 0x130.

## The two accessors this module is worth a lane for

1. **`fn_72_8`, the member-address accessor at a fourth offset.** `+0x920`, where `fn_29_0` uses
   `+0x9dc`, `fn_22_470` `+0x970`, `fn_45_8` `+0x818` and `fn_27_0` `+0x7c0`. Same
   `static_cast< char* >(self) + off` spelling, nothing to work out.
2. **`fn_72_18`, a `TUniqueId` getter**, with no counterpart in any other head in the tree. That
   `+0xa94` holds a `TUniqueId` is measured three ways, not assumed: `fn_72_7970` loads it
   (`lhz r0, 0xa94(r3)`) and stores it through a pointer to call
   `GetObjectById__13CStateManagerCF9TUniqueId`; `fn_72_AF70` hands it the same way to
   `SetTarget__13CBoneTrackingF9TUniqueId`, and the same function compares it word for word against
   `kInvalidUniqueId`.

   **It is written with an explicit out-pointer, and that is the only spelling that can be right.**
   Retail does `lhz r0, 0xa94(r4)` / `sth r0, 0x0(r3)` / `blr` - `self` in r4 and a *store* through
   r3 - so the value comes back through memory. A two-byte struct returned in r3 would be one
   instruction (`lhz r3, 0xa94(r3)`), not three. So:

   ```cpp
   void fn_72_18(TUniqueId* out, const void* self) {
     *out = *reinterpret_cast< const TUniqueId* >(static_cast< const char* >(self) + 0xA94);
   }
   ```

   This is how the family spells its other struct-returning accessors (`fn_22_4A4`, `fn_29_74`).
   One build was spent on it: `static_cast` from `const char*` to `const TUniqueId*` is rejected by
   MWCC (`illegal explicit conversion`), so the cast has to be `reinterpret_cast`.

## `fn_72_D4` is `fn_71_70` again

The registration is **instruction for instruction** `CSnakeWeedSwarmRel.cpp`'s `fn_71_70` - same
register allocation (r9/r8/r7 then r5/r4/r0), same six `lwz` out of `.data`, same `stwu` of the
loader, same six stores, same single call - so the same spelling reproduces it at 100% with no
search at all. The record is **0x1C bytes, not the four most of this family uses**, because two of
its members are CodeWarrior pointer-to-member-functions: `.data:0x8C0` is
`0 / 0xFFFFFFFF / fn_72_E0D8` and `.data:0x8CC` is `0 / 0xFFFFFFFF / fn_72_E0AC`, and both are
copied word for word rather than assigned. The `.bss` dump is the cheap check that says so -
`lbl_72_bss_24` is `.bss:0x24, size:0x1C` where the four-byte family's slot is `size:0x4`.

The two member signatures are read off the two functions, both in this module: `fn_72_E0AC` takes
no argument, writes `kInvalidUniqueId` to `+0xa84` and clears a bit in `+0xb30`'s byte at `+0x118`;
`fn_72_E0D8` takes one 16-bit id **through a pointer** (`lhz r0, 0x0(r4)`, and r4 is never
reassigned before it), does the same when the old id is `kInvalidUniqueId`, and returns whether it
took it. Neither is called from this file, so only the 12-byte size matters to the codegen - but
the signature is the honest reading and it costs nothing.

## The two unclaimed callees, named by what they are

- **`fn_80200E3C`** is the DOL's 0x80200E3C: `stw r3,-0x6a28(r13); blr` (`tools/dis.sh`), and
  `LoadSpacePirate` at 0x80200E10 reads the same displacement (`lwz r6,-0x6a28(r13)`). So it stores
  the **address** of a loader record, which is why the registration hands it `&lbl_72_bss_24`.
  `src/MetroidPrime/ScriptLoader/SpacePirate.cpp` (a `Matching` unit) already records that this
  setter is deliberately not claimed in the DOL because REL modules import it by its retail name,
  so it stays in dtk's auto unit. Declared `extern "C"` under its retail name, as
  `CFlyingPirateRel.cpp` does for `fn_80218A04`: an alias would be a different symbol and the call
  would resolve to nothing.
- **`lbl_72_bss_24`** is `.bss:0x24`, `size:0x1C data:4byte`. This unit's split claims `.text`
  only, so dtk's `.bss` object has to define it, and a second definition under MWCC is what produced
  mwldeppc's internal linker error on ScriptPlayerProxy - hence `extern` under MWCC and a host
  definition. It is `lbl_72_bss_24` and **not** `.bss:0x0`: this module's `.bss:0x0` is a different
  object (`lbl_72_bss_0`, 0xC bytes), read and written far above the head.

**MWCC does not encode a variable's type in its name**, so `extern bool (CEntity::*
lbl_72_data_8C0)(const TUniqueId*);` really does reference `lbl_72_data_8C0` itself. Measured on
the object: `nm` lists exactly five undefined symbols - `fn_72_140`, `fn_80200E3C`, `lbl_72_bss_24`,
`lbl_72_data_8C0`, `lbl_72_data_8CC` - plus the two data constants `lbl_72_rodata_B00` and
`lbl_8041AAB8`, and nothing else.

## No dead-strip hazard, and that is measured

`build/G2ME01/SpacePirate/ldscript.lcf` lists all eleven accessors (`fn_72_0` .. `fn_72_64`) in
its FORCEACTIVE block. RELMain/RELExit are the module's entry points and are reached from
`_prolog`/`_epilog` in the shared `REL/REL_Setup.cpp` unit, which is already `Matching` here, and
`fn_72_D4` is called from the `RELMain` in this same unit. `tools/audit_rel_claim.py SpacePirate`
prints **`preplf 262 text symbols, plf 262, 0 dropped by -strip_partial`**, so nothing needs a
`force_active:` entry in `config/G2ME01/config.yml`.

## The vtable slot, measured rather than assumed

`fn_72_64` dispatches to vtable slot **0x38**, and `.data:0x8D8` (0x148 bytes) is CSpacePirate's
vtable: it starts `[0][0][fn_72_F10C][TypesMatch__12CSpacePirateCFi]` and stores `fn_72_64` at
**offset 0x3C** (word 15, counted from the two leading offset-to-top and RTTI words that are zero
in this REL). So 0x3C is the fourteenth virtual and 0x38 the thirteenth - CActor's
`HealthInfo__6CActorFv` / `GetHealthInfo__6CActorCFv` pair, which is the reading
`CSnakeWeedSwarmRel.cpp` records for the same slot in module 71. Thirteen virtuals on the
stand-in class put `Slot12` at 0x38; a by-hand vtable load would compile to `lwz r3,0(r3)` where
retail has `lwz r12,0(r3)`, so it has to be a member call.

## What was measured, and how

- `./tools/decomp_build.sh`: `All: 31.20% fuzzy, 23.49% matched, 11.81% linked (10215 / 28465)`.
- **All 86 REL hashes hold** (independent re-hash against `config/G2ME01/config.yml`), and
  `build/G2ME01/SpacePirate/SpacePirate.rel` is `cmp`-equal to
  `orig/G2ME01/files/RelProd/SpacePirate.rel`; `main.dol` is still
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `build/report.json`: `SpacePirate/MetroidPrime/ScriptObjects/CSpacePirateRel` **14/14, every
  function 100.0%**. Module sum **7 -> 21 of 262**. Project **matched 10201 -> 10215, linked
  4989 -> 5003** (`report_diff.py`: `+14 functions at 100%, 1 units newly linked`, `no
  regression`, and one SPLIT line naming this unit).
- `tools/unit_fit.sh MetroidPrime/ScriptObjects/CSpacePirateRel`: `.text claimed 320 ours 320
  retail 320 fits`, `no extra functions`.
- `tools/audit_rel_claim.py SpacePirate`: `ok ... 0x00000000..0x00000140 14/14 functions`, `0
  claim(s) with a problem`.
- `tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CSpacePirateRel`: ok.
- `./tools/probe_sources.sh`: `749 files, 0 failed, 0 errors; link: LINKED (250 undefined,
  0 duplicates)` - the port's undefined count is unchanged at 250.
- `tools/check_raw_offsets.py`: `155 raw-offset site(s) in 64 file(s), all documented`.
- `tools/goal_check.sh build/goal/item.json`: **PASS**.

## Two traps a lane should know before spending time here

1. **`tools/flip_test.sh` cannot verify a REL module unit in this tree, and that is pre-existing** -
   the `progress-rel-head-flyingpirate` notes record the cause and it is unchanged. Run on this
   unit it prints `no source file (extern/musyx/src/MetroidPrime/ScriptObjects/
   CSpacePirateRel.cpp)` and `FAIL -> reverted (tree rebuilt: DOL 6ef9b491...)`; `configure.py`
   was byte-identical before and after (md5 `9649e004d62dda5fa30c9277df91b7ba`), so nothing was
   left broken. The REL acceptance test is the module's sha1 plus `unit_fit.sh` /
   `audit_rel_claim.py` / the per-function scores, and all of those are green above. Fixing it is a
   `tools/` change, which this item may not make.
2. **`tools/check_decl_order.py` prints `0 unit(s) checked` for a bare class name.** Run with
   `--unit MetroidPrime/ScriptObjects/CSpacePirateRel` it checks one unit and prints `ok`. The
   bare name is in `CFlyingPirateRel.cpp`'s own header, so copying that line verbatim verifies
   nothing.

## Two notes on what I did *not* change

- `tools/gate.sh` runs `check_docs_claims.py --write`, which rewrites `docs/HANDOFF.md`'s state
  block (matched 10201 -> 10215, linked 4989 -> 5003, REL units 1483 -> 1497, "92 units of our
  own code in 72 modules" with `SpacePirate` added). I reverted that file after each run so my diff
  is only the four files above; the driver's own run rewrites it when it commits.
- `docs/research/raw_offsets.md`'s "The debt, measured" paragraph had been left with a duplicated,
  sentence-broken pair of total lines by two earlier lanes (the same drift it complains about). I
  replaced both with the single measured line, because I had to touch those numbers anyway.
  Separately, that total was already stale before this item: it read 153 in 62 while the tool
  measured 154 in 63.

NEW: none. The remaining 241 functions of module 72 are its entity loader and class methods, which
need the CActor/CPatterned hierarchy; that is the blocker every other head in this family records,
and it is not one item's worth of work.
---

# Run 2 (lane L4, `goal/lane-4` at 0674096) — the earlier run's work was NOT in this tree

**The first thing to check, and the reason this run exists at all: none of the change the section
above describes was on this tree.** Measured on the clean `goal/lane-4` HEAD `0674096`:

- `src/MetroidPrime/ScriptObjects/CSpacePirateRel.cpp` did not exist (`ls` on
  `src/MetroidPrime/ScriptObjects/`).
- `configure.py` had **no** `Rel("SpacePirate", ...)` block; the only `SpacePirate` mention was
  line 698, `Object(Matching, "MetroidPrime/ScriptLoader/SpacePirate.cpp")` — the DOL-side
  loader, not the head.
- `config/G2ME01/rels/SpacePirate/splits.txt` had only `REL/global_destructor_chain.c` and
  `REL/REL_Setup.cpp`, no `CSpacePirateRel.cpp` entry.
- `build/report.json` gave the module **5 of 8** units and no
  `SpacePirate/MetroidPrime/ScriptObjects/CSpacePirateRel` unit at all.

So the section above is a *recipe*, not a landed result, and the queue item was live. This run
rebuilt it from the recipe and **re-measured every claim on this tree** rather than inheriting it.
Every figure in the section above that mattered turned out correct; one turned out wrong and is
corrected below.

## Result

The same four files, same unit, same range, and the outcome reproduces exactly:

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/CSpacePirateRel.cpp` | new, 14 functions, descending by retail offset |
| `config/G2ME01/rels/SpacePirate/splits.txt` | `MetroidPrime/ScriptObjects/CSpacePirateRel.cpp: .text start:0x00000000 end:0x00000140`, added last (after the two `REL/` entries) |
| `configure.py` | `Rel("SpacePirate", [Object(Matching, "MetroidPrime/ScriptObjects/CSpacePirateRel.cpp")])` at the end of the list, with the measured notes |
| `docs/research/raw_offsets.md` | new `## src/MetroidPrime/ScriptObjects/CSpacePirateRel.cpp (1 site)` section, and the "debt, measured" total rewritten |

Not in `files.cmake`, deliberately, as `CIngRel.cpp` / `CFlyingPirateRel.cpp` / `CSplinterRel.cpp`:
it calls `fn_72_140` and `fn_80200E3C`, which the port cannot link.

## What I re-measured, and what changed

**Everything in the recipe held on this tree** — I re-read it rather than trusting it:

- `build/G2ME01/SpacePirate/asm/auto_00_00000000_text.s` 0x0..0x140 is instruction for
  instruction what the section transcribes, including `fn_72_18`'s `lhz r0, 0xa94(r4)` /
  `sth r0, 0x0(r3)` and `fn_72_D4`'s six `lwz` out of `.data` into r9/r8/r7 then r5/r4/r0.
- `auto_05_00000000_bss.s`: `lbl_72_bss_24` at 0x24, `size:0x1C`. `lbl_72_bss_0` is a **separate**
  0xC-byte object, so the record is `lbl_72_bss_24` and not `.bss:0x0`.
- `auto_03_00000000_rodata.s`: `lbl_72_rodata_B00` is `.float 50`, `size:0x4`.
- `auto_04_00000000_data.s`: `.data:0x8C0` = `0 / 0xFFFFFFFF / fn_72_E0D8`,
  `.data:0x8CC` = `0 / 0xFFFFFFFF / fn_72_E0AC`. Both confirmed 12-byte ptmfs.
- `fn_72_E0AC` / `fn_72_E0D8` bodies re-read: `+0xa84` = `kInvalidUniqueId`,
  `+0xb30` -> `+0x118` bit cleared, and `lhz r0, 0x0(r4)` with r4 never reassigned, so the id
  arrives **through a pointer** and the function returns whether it took it.
- `tools/dis.sh 0x80200E3C 0x8` -> `stw r3,-27176(r13); blr`, and `LoadSpacePirate` at 0x80200E10
  reads `-27176(r13)` too, so the setter stores the **address** of the record.
- `config/G2ME01/symbols.txt:8377` gives `fn_80200E3C` its **plain** retail name, while
  `symbols.txt:9532` renames the same role in module 71 to
  `SetLoader_SnakeWeedSwarm__FP24SSnakeWeedSwarm_FuncPtrs`. That difference is load-bearing and is
  why this file declares `fn_80200E3C` under `extern "C"` while `CSnakeWeedSwarmRel.cpp` can use
  a C++ declaration. Recorded in both files so the next reader does not "fix" it back.
- `+0xa94` is a TUniqueId, three ways: `fn_72_7970` (`0x7700`) and `fn_72_AF70` (`0xA754`) both
  `lhz r0, 0xa94(r3)` and `sth r0, 0x8(r1)` before
  `GetObjectById__13CStateManagerCF9TUniqueId`, and `fn_72_E0D8` compares `+0xa84` word for word
  against `kInvalidUniqueId`.
- `build/G2ME01/SpacePirate/ldscript.lcf` FORCEACTIVE lists all eleven `fn_72_0`..`fn_72_64`.

**One claim in the section above was wrong, and I corrected it in place** in both the source
header and `configure.py`: the vtable slot. `.data:0x8D8` is 0x148 bytes = **82 words**, and
parsing it out properly (rather than by eye, which is how the off-by-one got in) gives:

```
word  0  0x08D8  0x00000000     offset-to-top
word  1  0x08DC  0x00000000     RTTI
word  2  0x08E0  fn_72_F10C
word  3  0x08E4  TypesMatch__12CSpacePirateCFi
word 13  0x090C  fn_72_3A0C        <- 0x38, the slot fn_72_64 dispatches to
word 14  0x0910  HealthInfo__3CAiFv <- 0x40
word 15  0x0914  fn_72_64           <- 0x3C
```

So `fn_72_64` is at 0x3C = **word 15** and, not counting the two leading words as virtuals, it is
the **fourteenth** virtual — that part was right. But 0x38 is **word 13, the twelfth** virtual,
not the thirteenth, and it holds this module's own `fn_72_3A0C`, **not**
`HealthInfo__6CActorFv`; `HealthInfo__3CAiFv` is at 0x40. Still CActor's `HealthInfo` /
`GetHealthInfo` neighbourhood — one extra CPatterned virtual below `HealthInfo` compared with
module 71, where `CSnakeWeedSwarmRel.cpp` records the pair at 0x38 and 0x3C.

**Nothing about the code changes.** Only the *offset* of the callee slot is load-bearing, thirteen
virtuals on the stand-in class put `Slot12` at exactly 0x38, and the 100% + the module hash are
the proof. The lesson, which is general and cost me a rebuild: **read the word index out of the
vtable programmatically, not by counting lines in a `sed` range** — a `sed -n A,Bp` plus
`awk NR==k` double-offset is how the wrong neighbour got named.

## The `docs/research/raw_offsets.md` total, measured rather than appended to

`tools/check_raw_offsets.py --list` on this tree prints **`total: 157 raw-offset site(s) in 66
file(s)`**, and the 66 `##` headings in the doc now sum to exactly 157 (`grep -c '^## \`src/'` =
66). The section above quoted 155 in 64, which is what its own tree measured; this tree is two
heads further on (`CSplinterRel`, `CMinorIngRel` landed after it), and the doc's own summary line
had drifted to a **triplicated, sentence-broken** pair of paragraphs reading 153 in 62 — three
lanes each *appended* their measurement instead of replacing the paragraph. Before my change this
tree already measured 156 in 65, so the line was stale by 3 in 3 independently of me. I replaced
all of it with the single measured line and folded the three revision histories into the paragraph
below it rather than adding a fourth.

## Measured, and how

- `./tools/decomp_build.sh`: `All:  31.22% fuzzy, 23.52% matched, 11.82% linked (10250 / 28465
  functions)`.
- **All 86 RELs `cmp`-equal** to `orig/G2ME01/files/RelProd/` (looped, `86 cmp-equal, 0 differ`),
  and `tools/gate.sh` prints `hashes vs config.yml  ok`. `main.dol` is still
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `build/report.json`: `SpacePirate/MetroidPrime/ScriptObjects/CSpacePirateRel` **14/14, every
  function 100.0%**, `fuzzy_match_percent` 100.0, `complete_units` 1. Module **7 -> 21 of 262**.
  Project **matched 10236 -> 10250, linked 5018 -> 5032**.
- `tools/report_diff.py`: `+100%` for all fourteen named functions, `no regression`, and one
  `SPLIT SpacePirate/auto_00_00000000_text: 253 function(s) accounted for across 2 new unit(s)
  (exact count match - a split, not a loss)`.
- `tools/unit_fit.sh MetroidPrime/ScriptObjects/CSpacePirateRel`: `.text claimed 320 ours 320
  retail 320 fits`, `no extra functions`.
- `tools/audit_rel_claim.py SpacePirate`: `ok MetroidPrime/ScriptObjects/CSpacePirateRel.cpp
  0x00000000..0x00000140 14/14 functions`, `0 claim(s) with a problem`, and
  `preplf 262 text symbols, plf 262, 0 dropped by -strip_partial`.
- `tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CSpacePirateRel`: `ok: 1 unit(s)
  checked, none emits its functions out of retail order`.
- `./tools/probe_sources.sh`: `749 files, 0 failed, 0 errors; link: LINKED (250 undefined,
  0 duplicates)` — the port's undefined count is unchanged at 250, and the file is not in
  `files.cmake`.
- `tools/check_raw_offsets.py`: `157 raw-offset site(s) in 66 file(s), all documented`.
- `build/binutils/powerpc-eabi-nm -u` on the built object lists exactly seven undefined symbols:
  `fn_72_140`, `fn_80200E3C`, `lbl_72_bss_24`, `lbl_72_data_8C0`, `lbl_72_data_8CC` plus the two
  data constants `lbl_72_rodata_B00` and `lbl_8041AAB8`, and nothing else.
- `tools/goal_check.sh build/goal/item.json`: **PASS** (run twice — once before the vtable-comment
  correction and once after, since a comment edit in a `Matching` source is a no-op for the bytes
  and the second run is what makes that a measurement rather than an assumption).

## The two traps, re-measured on this tree

1. **`tools/flip_test.sh` still cannot verify a REL module unit here**, and I re-measured it rather
   than quoting the run above. Run on this unit it prints
   `no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CSpacePirateRel.cpp) -
   configure.py would link the retail object and this would pass while proving nothing`, then
   `FAIL -> reverted (tree rebuilt: DOL 6ef9b491...)` and `kept: 0 / 1 failed: 1`. I took
   `md5sum configure.py` on both sides of that run and they were the same, and the DOL rebuilt to
   the pinned hash, so nothing was left broken. (Do not read that md5 as a tree fingerprint: I
   edited `configure.py`'s vtable comment *after* the `flip_test` run, so the file's hash has
   since changed. What proves the tree is intact is the two `goal_check.sh` PASSes after it, the
   pinned DOL hash, and all 86 RELs `cmp`-equal.) The REL acceptance test is the module's sha1
   plus `unit_fit.sh` / `audit_rel_claim.py` / the per-function scores, and all of those are
   green above. Fixing `flip_test.sh` is a `tools/` change, which this item may not make.
2. **`tools/check_decl_order.py` prints `0 unit(s) checked` for a bare class name.** Run with
   `--unit MetroidPrime/ScriptObjects/CSpacePirateRel` it checks one unit and prints `ok`. The
   bare name appears in `CFlyingPirateRel.cpp`'s own header, so copying that line verbatim
   verifies nothing.

## Two notes on what I did *not* change

- `tools/gate.sh` runs `check_docs_claims.py --write`, which rewrites `docs/HANDOFF.md`'s state
  block (matched 10236 -> 10250, linked 5018 -> 5032, REL units 1512 -> 1526, and "94 units of our
  own code in 74 modules" with `SpacePirate` added to the list). I reverted that file after each
  `goal_check.sh` run so my diff is only the four files above; the driver's own run rewrites it
  when it commits.

NEW: none. The remaining 241 functions of module 72 are its entity loader and class methods, which
need the CActor/CPatterned hierarchy; that is the blocker every other head in this family records,
and it is not one item's worth of work.
