# match-main-sda-float-constants

`kind: match`, `target: MetroidPrime/main`. Verdict **PARTIAL**: the flip is out of reach on the
`CErrorOutputWindow::__vt` link error attempt 2 recorded, the target rose **60 -> 62 / 99**
functions, and `tools/goal_check.sh build/goal/item.json` passed every other check. Diff is
`src/MetroidPrime/main.cpp` only, 77 insertions / 2 deletions, no asm, no deletion of real work.

## The item's premise is false, and finding that out is the main result

The item was filed on the claim that `CMain::CMain` and `TAverage.hpp` read `.sdata2`
`lbl_8041A3D8` / `lbl_8041A3DC` / `lbl_8041A3F0` (0.0f / 1.0f / 0.0) where retail reads `.sdata`
0x80417D98 / 0x80417D9C / 0x80417DB0, derived from `_SDA_BASE_` = 0x8041FD80 plus the
instructions' signed displacements. **That derivation is wrong, and so is every other one made
the same way.** I applied the change, measured it, and it moved `SetMaxSpeed` 99.25% -> 99.25%
and turned a passing relocation into a mismatching one. Reverted.

### There are two small-data bases, and `_SDA_BASE_` is not r2

Retail's `__init_registers` (0x80003464-0x80003470) loads **two** of them, 0x2640 apart:

```
3c 40 80 42   lis  r2,0x8042   /  60 42 23 c0   ori  r2,r2,0x23C0    ->  r2  = 0x804223C0
3d a0 80 41   lis  r13,0x8041  /  61 ad fd 80   ori  r13,r13,0xFD80  ->  r13 = 0x8041FD80
```

- **r2 = 0x804223C0** - the `.sdata2` window.
- **r13 = 0x8041FD80** - the `.sdata` / `.sbss` window. *This* is the value
  `powerpc-eabi-nm build/G2ME01/main.elf` reports for `_SDA_BASE_`, and the one `tools/sda.py`
  uses.

Both are 32-byte aligned, so the ABI allows either and nothing in the disassembly says which is
which. Four independent checks fix the assignment, and all four agree:

| check | r2 = 0x804223C0 | r13 = 0x8041FD80 |
|---|---|---|
| dtk's own naming of retail's relocations in `build/G2ME01/obj/MetroidPrime/main.o`: `lfs f1,-32744(r2)` -> `lbl_8041A3D8`, `lfs f0,-32740(r2)` -> `lbl_8041A3DC`, `lfd f2,-32720(r2)` -> `lbl_8041A3F0`, `lwz r0,-13760(r2)` -> `lbl_8041EE00` | every one lands on its named symbol | every one is out by 0x2640 |
| `dol_read.py 0x8041A3D0 0x60`: 0x8041A3D8 = 0x00000000, 0x8041A3DC = 0x3F800000 = 1.0f, 0x8041A3F0 = 0x0 as a double | the old comment's three values are exactly right | 0x80417D98 = 0x0000003B, 0x80417D9C = 0x00000008, 0x80417DB0 = a denormal |
| the ctor's ten `lwz r0,-32764(r13)` reach 0x80417D84, whose word is 0x000F4240 - the value `rstl::reserved_vector<uint,10>`'s one-argument fill is documented to use | n/a | **confirmed independently of dtk** |
| `dol_read.py 0x8041EE0`: 0x8041EE00 = 0x00008F00, and `fn_80009864` stores `0x8F00 * 28 / 8 * 4` = 0x7D000 = 512512 into the ARAM size, which is what the reader at 0x80007C2C wants | **confirmed independently of dtk** | n/a |

So: **`tools/sda.py` is wrong for every r2-relative displacement**, and it is wrong silently -
it prints `0x8041C7C0 (exact, .sdata2)` for `-13760`, and `.sdata2` 0x8041C7C0 really is
0x3F7D70A4 = 0.99f, so the output looks perfect. `lbl_80417D84` in the tree is *also* an r13
symbol, which is why it is right. `tools/` is the judge's, so nothing is changed here; the
r2/r13 split is now written into `src/MetroidPrime/main.cpp` at `fn_80009864` so the next reader
does not re-derive it.

**This is why the earlier round's `lbl_8041C7C0` for `fn_80009864` is wrong.** That fix was made
because `python3 tools/sda.py -13760` said so and a reviewer accepted it - but the function was
never in the tree afterwards, so nothing shipped. dtk names the site `lbl_8041EE00`.

## What `SetMaxSpeed`'s 99.25% actually was

Not a constant. The prologue and all twenty body instructions were already byte-identical to
retail; the three epilogue reloads were in the other order:

```
retail  80008a04: lwz r0,20(r1) ; lwz r31,12(r1) ; lwz r30,8(r1) ; mtlr r0
ours    00002180: lwz r31,12(r1); lwz r30,8(r1) ; lwz r0,20(r1) ; mtlr r0
```

A void function with no `mr r3,rN` in its epilogue leaves that order free (`~CMain`, which does
have one, puts the lr reload first and we already match it at 100%). **`const` on the parameter
is the whole fix**: `void CMain::SetMaxSpeed(const bool v)`. Twelve spellings measured, all with
the other 88 bytes unchanged - the table is in the source comment. `const bool v` and
`const bool fading = v;` both give 0 diff bytes; the other ten give 8, and two change the size.

`CMain::SetMaxSpeed__5CMainFb` **99.25% -> 100.00%** (96 B). The top-level `const` is not part of
the signature, so `CMain.hpp` is untouched.

## `fn_80009864` written, retail 0x80009864, 0x1C = 28 B (+1)

```
lwz   r0,-13760(r2) ; mulli r0,r0,28 ; srawi r0,r0,3 ; addze r0,r0 ; slwi r0,r0,2 ;
stw   r0,-28384(r13) ; blr
```

`(*(const int*)&lbl_8041EE00 * 28) / 8 * 4` is the only spelling of `v * 14` that emits all five
of those instructions; five measured (table in the source). Our object's two relocations are
`lbl_8041EE00` and `lbl_80418EA0`, which is what dtk names in retail's object. The unit had no
body for this symbol at all - it was one of the 39 unpaired ones - and the earlier acceptance of
`lbl_8041C7C0` never reached the tree, so nothing wrong shipped with it.

Also fixed: the `lbl_80418EA0` comment named `*(u32*)0x80415980`, which is in `.rodata` and is
not what the load reaches. It now names `lbl_8041EE00` and says so.

## What still stops the flip

Unchanged and not close - 37 of the 99 functions still have no body and `.text` is **SHORT by
7852** of the 17608 claimed (`tools/unit_fit.sh MetroidPrime/main.cpp`; 16 unclaimed extras, all
COMDAT, are pre-existing). `flip_test.sh` fails at link with

```
mwldeppc.exe Linker Error: multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
```

the same `splits.txt`/`.data` question attempt 2 filed, and
`python3 tools/check_decl_order.py --unit MetroidPrime/main` still reports the pre-existing
**would break on a flip** (our `.text` is in descending retail order). None of the three is this
item's to fix.

## Two traps in this tree that cost time here, for the next run

1. **`build/G2ME01/obj/MetroidPrime/main.o` is a stale copy of dtk's *retail* object, and
   `build.ninja`'s link rule feeds the DOL from `build/G2ME01/obj/*.o`.** So `./tools/decomp_build.sh`
   does not relink for a `NonMatching` unit, `sha1sum build/G2ME01/main.dol` is retail's
   whatever we write, and a green DOL hash says nothing about this unit. objdiff, by contrast,
   reads `build/G2ME01/src/MetroidPrime/main.o` (`objdiff.json`'s `base_path`), so the report is
   the real measurement - and it is up to date after a `./tools/decomp_build.sh main`.
2. **A scratch harness that rewrites `src/` and restores it from a backup taken before later
   edits will silently delete those later edits.** Mine did, once, and cost a rebuild cycle.
   Take the backup, or the restore, last.

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.07% fuzzy, 23.36% matched, 11.78% linked (10094 / 28465)
tools/goal_check.sh build/goal/item.json
                                       PARTIAL - gate ok, counts ok, names ok, target rose, no asm
                                       "matched 10092 -> 10094, linked 4918 -> 4918"
                                       "target rose: main/MetroidPrime/main: 60 -> 62 / 99 functions"
unit: main/MetroidPrime/main          60 -> 62 / 99, fuzzy 47.05% -> 47.22%, matched_code 7364 -> 7488
  SetMaxSpeed__5CMainFb               99.25% -> 100.00% (96 B)
  fn_80009864                         absent  -> 100.00% (28 B)
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (see trap 1)
python3 tools/check_symbol_names.py   504 units, 0 missing
python3 tools/check_raw_offsets.py    ok: 152 raw-offset site(s) in 61 file(s)
tools/unit_fit.sh MetroidPrime/main.cpp  .text SHORT by 7852; 16 extras, all COMDAT (pre-existing)
git status                            src/MetroidPrime/main.cpp only
```

No function anywhere got worse - the judge's report diff is `+2 functions at 100%` and nothing
fell. `docs/HANDOFF.md` is reverted after the judge; `gate.sh` rewrites it under
`MP_GATE_DOCS_WRITE=1` and the driver owns that file.

## For whoever picks up the r2/r13 split

`build/G2ME01/obj/*.o` is dtk's *retail* object for each unit and carries the correctly-resolved
relocation names, so it is the authority on which small-data address an instruction reaches -
**read it before `tools/sda.py`, not after.** A one-line fix to `tools/sda.py` (take the base
from the register named in the instruction) would retire this whole class of wrong constant; it
is the judge's file, so it is a `NEW:` for whoever owns tooling rather than for a lane.

NEW: match-main-tooling-sda-r2-base | match | MetroidPrime/main | `tools/sda.py` resolves r2-relative
displacements against r13's base 0x8041FD80 instead of r2's 0x804223C0 (retail `__init_registers`
0x80003464), so every `.sdata2` address it reports for an `lfs`/`lfd`/`lwz r2` is out by 0x2640 -
`fn_80009864`'s `-13760` is 0x8041EE00, not the 0x8041C7C0 an earlier review accepted; fixing the
tool and re-checking the tree's other r2-relative constants can change several units' meaning
without changing a single byte of output

NEW: match-main-getaveragevalue-f | match | MetroidPrime/main | `GetAverageValue<f>__FPCfi`
(0x80008B60, 200 B) is still unpaired and is real work, not transcription: 8x-unrolled `fadds`
sum, then `xoris r3,r4,32768` / `lis r0,17200` / two `stw` / `lfd f0,8(r1)` (the 2^52 double
trick) / `fsubs` / `fdivs` / `fmuls`, i.e. `sum / (count - c)` with `lfs f2,-32740(r2)` =
`lbl_8041A3DC` and `lfd f1,-32664(r2)` = `.sdata` 0x80417DE8; `include/Kyoto/TAverage.hpp`'s
`sum * (1.f / count)` is the wrong shape and it is a shared header, so it needs its own change
