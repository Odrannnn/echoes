# progress-rel-head-flyingpirate

FlyingPirate's (module 22) accessor block and entry path, `.text 0x470..0x5D4`, as one
`Matching` unit: `src/MetroidPrime/ScriptObjects/CFlyingPirateRel.cpp`, 16 functions.
Module 22 had **no `Rel(...)` block at all** before this, so its seven functions were the shared
`REL_Setup` and `global_destructor_chain` units and nothing of ours; it is now 23 of 163.

## The four files

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/CFlyingPirateRel.cpp` | new, 16 functions, descending by retail offset |
| `config/G2ME01/rels/FlyingPirate/splits.txt` | `MetroidPrime/ScriptObjects/CFlyingPirateRel.cpp: .text start:0x00000470 end:0x000005D4`, added first (address order) |
| `configure.py` | `Rel("FlyingPirate", [Object(Matching, "MetroidPrime/ScriptObjects/CFlyingPirateRel.cpp")])` at the end of the list, with the measured notes |
| `docs/research/raw_offsets.md` | **no change - see "not documented" below** |

Not in `files.cmake`, deliberately, as `CGrenchlerRel.cpp` / `CMysteryFlyerRel.cpp` /
`CDarkCommandoRel.cpp`: it calls `fn_22_5D4` and `fn_80218A04`, which the port cannot link.
`tools/check_files_cmake.py` accepts it as a `MODULE_ENTRY` source (33 such units, +1 here).

## Claimed range, and why it is not 0x0

```
0x470 fn_22_470  0x08  addi r3,r3,0x970
0x478 fn_22_478  0x08  li r3,1
0x480 fn_22_480  0x24  bit 5 of the byte at +0xbb8 -> .rodata:0x3cc (.float 5) / :0x3c8 (.float 50)
0x4A4 fn_22_4A4  0x3C  GetBoundingBox into a local, then fn_22_AA48(out, &box)
0x4E0 fn_22_4E0  0x10  lbl_8041AAB8 -> *((float*)(self + 0x448))
0x4F0 fn_22_4F0  0x08  lbz r3, 0x44f(r3)
0x4F8 fn_22_4F8  0x08  li r3,0
0x500 fn_22_500  0x08  li r3,0
0x508 fn_22_508  0x10  *id = kInvalidUniqueId
0x518 fn_22_518  0x0C  byte at +0x34c, bit 3
0x524 fn_22_524  0x08  addi r3,r3,0x754
0x52C fn_22_52C  0x08  li r3,1
0x534 fn_22_534  0x2C  virtual dispatch, vtable slot 0x38
0x560 RELExit    0x24  li r3,0 / bl fn_80218A04
0x584 RELMain    0x20  bl fn_22_5A4
0x5A4 fn_22_5A4  0x30  lbl_22_bss_58 = fn_22_5D4 ; fn_80218A04(&lbl_22_bss_58)
```

**The one structural difference from every other head in this family is that the claim starts at
0x470, not 0x0**: `fn_22_0` (0x164), `fn_22_164` (0x1E4), `fn_22_348` (0x94) and `fn_22_3DC` (0x94)
are 0x470 bytes of behavioural class code, not accessors, so they stay retail. The module's 163
text symbols now split **16 ours + 5 `REL_Setup` + 2 `global_destructor_chain` + 140 unclaimed**
(4 below 0x470, 136 from `fn_22_5D4` up - the entity loader and its 135 class methods, which need
the CActor/CPatterned hierarchy this tree does not model).

Eleven of the thirteen accessors are the family and transferred verbatim: `+0x448` float store,
the byte at +0x44f, the `kInvalidUniqueId` reset, the bit at +0x34c, the member address at +0x754,
the predicates, the vtable dispatch, and `fn_22_4A4` which is **instruction for instruction**
`CGrenchlerRel.cpp`'s `fn_27_8` (0x3C, same `stwu r1,-0x30` / `mr r31,r3` / GetBoundingBox /
`mr r3,r31` / out-of-line ctor shape) with `fn_22_AA48` for `fn_27_13C6C`. `fn_22_470` is the
member-address accessor at a **third** offset (+0x970; `fn_45_8` uses +0x818, `fn_27_0` +0x7c0).

## The one function that had to be discovered: `fn_22_480`

No sibling in the tree has it. dtk's own `asm/auto_00_00000000_text.s` spells the instruction
`extrwi. r0, r0, 1, 26`, which reads as bit 2, and **that is wrong**: the word is `54 00 DF FF` and
`powerpc-eabi-objdump -EB` on the retail `.rel` reads it as `rlwinm. r0, r0, 27, 31, 31`. This is
the trap `docs/RUNNING_THE_DECOMP.md` records twice ("do not reason about a bit out of the
mnemonic - reason about the encoding, and look for a `Matching` sibling").

Three spellings, all measured, in order:

| spelling | word | objdiff | module sha1 |
| --- | --- | --- | --- |
| `(self[0xBB8] & 4) != 0 ? A : B` | `rlwinm. r0, r0, 0, 29, 29` | 15/16, `fn_22_480` alone wrong | 2 bytes differ |
| `(self[0xBB8] & 32) != 0 ? A : B` | `rlwinm. r0, r0, 0, 26, 26` | 15/16 | 2 bytes differ |
| `((self[0xBB8] >> 5) & 1) != 0 ? A : B` | `rlwinm. r0, r0, 27, 31, 31` | **16/16** | **holds** |

The third is the family's own rule, already measured in this tree: mwcceppc emits
`SH = 32 - n`, `MB = ME = 31` for `(byte >> n) & 1` (`CScriptMetaree.cpp`'s `fn_42_36C` is the
`Matching` unit that proves it, `CMetareeSwarmRel.cpp`'s note has the same measurement). So `n = 5`,
**bit 5 of the byte**, mask 0x20. Both mask spellings also compile to a *dead* test - a bit above
the byte's top - so they would have been wrong as well as wrong-looking, which is why the shift
form is the one in the file.

## What was measured, and how

- `./tools/decomp_build.sh`: `All: 31.17% fuzzy, 23.46% matched, 11.78% linked (10143 / 28465)`.
- **All 86 REL hashes hold** (`config/G2ME01/config.yml` re-hash) and
  `build/G2ME01/FlyingPirate/FlyingPirate.rel` is `cmp`-equal to
  `orig/G2ME01/files/RelProd/FlyingPirate.rel`; `main.dol` still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `build/report.json`: `FlyingPirate/MetroidPrime/ScriptObjects/CFlyingPirateRel` **16/16, all
  functions 100.0%, `complete: True`**. Module sum **7 -> 23 of 163**. Project
  **matched 10127 -> 10143, linked 4917 -> 4933** (`report_diff.py`: `+16 functions at 100%,
  1 units newly linked`, and the only other line is a SPLIT line naming this unit).
- `tools/unit_fit.sh`: `.text claimed 356 ours 356 retail 356 fits`, `no extra functions`.
- `tools/audit_rel_claim.py FlyingPirate`: `ok ... 0x00000470..0x000005D4 16/16 functions`,
  `0 claim(s) with a problem`, and **`preplf 163 text symbols, plf 163, 0 dropped by
  -strip_partial`** - i.e. no dead-strip hazard, which the module's own `ldscript.lcf` predicts
  (it force-activates all thirteen of `fn_22_470`..`fn_22_534`; RELMain/RELExit are reached from
  `_prolog`/`_epilog` in the already-`Matching` shared `REL_Setup` unit, and `fn_22_5D4` /
  `fn_22_AA48` are held by `fn_22_5A4` / `fn_22_4A4`).
- `tools/check_decl_order.py --unit CFlyingPirateRel`: ok.
- `./tools/probe_sources.sh`: `749 files, 0 failed, 0 errors; link: LINKED (250 undefined,
  0 duplicates)` - the port's undefined count is unchanged at 250.
- `tools/goal_check.sh build/goal/item.json`: **PASS**.

## Two things a lane should know before spending time here

1. **`tools/flip_test.sh` cannot verify a REL module unit in this tree, and that is pre-existing.**
   It runs with the unit's configure.py path, finds `Object(Matching, ...)`, and then decides the
   source root with `k = s.rfind('MusyX(', 0, m.start())` plus a raw paren count over
   `s[k:m.start()]`. The last `MusyX(` in `configure.py` is at line 1292, so for **every** `Rel(`
   block after it the count is one too high (the block's own open `Rel(` is inside the window) and
   the unit is filed under `extern/musyx/src`. Measured here: `open 379 close 378` for
   `CFlyingPirateRel.cpp`, and the same for `CGrenchlerRel.cpp` (361/360),
   `CDarkCommandoRel.cpp` (347/346) and `EyeBallAccessors.cpp` (285/284) - all already landed.
   `flip_test.sh` therefore prints `no source file (extern/musyx/src/...)` and FAILs on all four.
   The REL acceptance test is the module's sha1 plus `unit_fit.sh` / `audit_rel_claim.py` /
   per-function scores, and all of those are green above. Fixing it is a `tools/` change, which
   this item may not make.
2. **`docs/research/raw_offsets.md` cannot document this file.** The five offsets it reaches
   (+0x970, +0x448, +0x44F, +0x34C, +0x754) all go through `static_cast< char* >` or a typed
   subscript, which `check_raw_offsets.py` does not key on, so it counts **0** sites - and the
   same tool fails a documented file whose count is 0 ("documented but has no raw offsets any
   more - delete the section"). The other module heads escape this only because each has a `+0x54`
   three-float copy, which this block has not got. So the debt is real and undocumented: the
   offsets and their blocker are in the source's own header instead. Worth a `NEW:` only if
   someone wants the checker taught the subscript form.
