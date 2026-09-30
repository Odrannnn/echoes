# progress-rel-head-splinter

Goal item `progress-rel-head-splinter` (`kind: progress`, `target: module:Splinter`), worktree
`../wt-mp2-goal-L5`, lane 5, 2026-09-30. **PASS** from `tools/goal_check.sh build/goal/item.json`
(run in this worktree, same script/baseline the driver uses).

## What I did

Decompiled the `Splinter` REL module's head (module 74) as a new `Matching` unit, the ordinary
arrangement from `docs/RUNNING_THE_DECOMP.md`'s "The recipe for decompiling a REL module" and
`AGENTS.md`'s one rule.

- New `src/MetroidPrime/ScriptObjects/CSplinterRel.cpp` - 14 functions, `.text 0x0..0x118`
  (280 bytes), `Matching`, **100.00% fuzzy, 14/14 functions**.
- `config/G2ME01/rels/Splinter/splits.txt` - one new contiguous claim
  `MetroidPrime/ScriptObjects/CSplinterRel.cpp: .text start:0x00000000 end:0x00000118`. Nothing
  else changed; `REL/global_destructor_chain.c` and `REL/REL_Setup.cpp` keep their existing claims.
  The range above `0x118` stays unclaimed, so dtk fills it from retail.
- `configure.py` - new `Rel("Splinter", [Object(Matching, ".../CSplinterRel.cpp")])`. The module
  had **no `Rel(...)` block at all** before this, so its only units were the shared `REL_Setup` and
  `global_destructor_chain`.
- `docs/research/raw_offsets.md` - new `## src/MetroidPrime/ScriptObjects/CSplinterRel.cpp
  (1 site)` section, required by `tools/check_raw_offsets.py` (a new file with a raw offset fails
  the gate without it).

Not added to `files.cmake`, for the reason every other head in this family gives: the file defines
RELMain/RELExit, which a flat host link cannot hold, and calls `fn_74_118` and `fn_80218C64`, which
the port cannot link. `tools/check_files_cmake.py` counts it in its "module entry point" line
(38 -> 39) and reports no problem; 0 dead files.

## What I measured

| measurement | before | after |
| --- | --- | --- |
| `module:Splinter` matched_functions | 7 | **21** of 278 |
| `matched_functions` (whole project) | 10216 | **10230** |
| `linked` (Matching units only) | 5004 | **5018** |
| REL units (HANDOFF state block) | 1498 | 1512 |
| units of our own code in modules | 92 in 72 | **93 in 73** |
| `main.dol` sha1 | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` | unchanged |
| Splinter module sha1 (`config.yml`) | `39061caefdda8fe33c8b89dc6ac61dbcae83bc82` | unchanged; all 86 verified |
| raw-offset sites | 155 in 64 | 156 in 65 |

- `tools/audit_rel_claim.py Splinter`: `ok ... 0x00000000..0x00000118  14/14 functions`,
  `0 claim(s) with a problem`, `preplf 278 text symbols, plf 278, 0 dropped by -strip_partial`.
- `tools/unit_fit.sh MetroidPrime/ScriptObjects/CSplinterRel.cpp`: `.text claimed 280 ours 280
  retail 280 fits`, **no extra functions**.
- `tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CSplinterRel`: ok, no unit emits
  its functions out of retail order.
- `cmp build/G2ME01/Splinter/Splinter.rel orig/G2ME01/files/RelProd/Splinter.rel`: identical.
- `tools/check_symbol_names.py`: clean.
- `tools/goal_check.sh` verdict lines: `ok gate.sh`, `ok counts: matched 10216 -> 10230 linked
  5004 -> 5018`, `ok All: 31.20% fuzzy, 23.49% matched, 11.82% linked (10230 / 28465 functions)`,
  `ok target rose: module:Splinter: 7 -> 21 / 278 functions`, `ok no asm added` ->
  `goal_check: PASS progress-rel-head-splinter`.
- `total_functions` still **28465** after the `splits.txt` edit (the carve rule's check).

## The head, and what made it different

Read off `build/G2ME01/Splinter/asm/auto_00_00000000_text.s` over `0x0..0x118`, not off the
`fn_<id>_<off>` names, which say nothing about which function is which:

```
0x000 fn_74_0   0x08  addi r3,r3,0x7c4                 member address, +0x7c4
0x008 fn_74_8   0x10  lbl_8041AAB8 -> *((float*)(r3+0x448))
0x018 fn_74_18  0x08  lbz r3, 0x44f(r3)
0x020 fn_74_20  0x08  li r3,0x0
0x028 fn_74_28  0x08  li r3,0x0
0x030 fn_74_30  0x10  *r3 = kInvalidUniqueId
0x040 fn_74_40  0x0C  byte at +0x34c, bit 3
0x04C fn_74_4C  0x08  addi r3,r3,0x754
0x054 fn_74_54  0x08  li r3,0x1
0x05C fn_74_5C  0x1C  three floats from self+0x54 -> *out
0x078 fn_74_78  0x2C  virtual dispatch, vtable slot 0x38
0x0A4 RELExit   0x24  li r3,0 / bl fn_80218C64
0x0C8 RELMain   0x20  bl fn_74_E8
0x0E8 fn_74_E8  0x30  lbl_74_bss_70 = fn_74_118 ; fn_80218C64(&lbl_74_bss_70)
```

**This is the shortest head in the family so far - 14 functions, where `CIngRel.cpp` is 17,
`CAtomicAlphaRel.cpp` 18, `CMinorIngRel.cpp` 15.** Three accessor kinds are absent, and copying a
sibling's spelling would have got two of them wrong:

- **No `optional_object<CAABox>` wrapper.** Ing's `fn_29_10` and AtomicAlpha's `fn_2_10` are that
  wrapper; Splinter has no such function, so nothing here names a `CAABox` and
  `Kyoto/Math/CAABox.hpp` is not included at all (which is also why this file has **no
  `#ifdef __MWERKS__`-guarded `CPhysicsActor` stand-in** - `CSplitterRel.cpp` needs one and this
  one does not).
- **No `lbl_8041B758` accessor, and no module-local `.rodata` constant either.** Ing spends the
  0x0C slot at 0x18 on `lbl_29_rodata_64`, MinorIng on the DOL `lbl_8041B758`. Here that slot is
  the `+0x34c` bit read at 0x40 and the next function is the `+0x754` member address at 0x4C, so
  `lbl_8041B758` is not referenced at all.
- **One leading member-address accessor** (`+0x7c4` at 0x0) where Ing, IngBoostBallGuardian and
  AtomicAlpha each open with two, so the `lbl_8041AAB8` float store is the *second* function at
  0x8, not the third.

Every body was one a sibling already reproduces at 100% (`CIngRel.cpp` or `CMinorIngRel.cpp`), so
**no spelling had to be discovered for this run** - which is why the unit is 100.00% first try.
The two bodies that read oddly are dtk's rendering and are spelled to match the bytes:
`fn_74_40`'s `54 03 EF FE` is `rlwinm r3, r0, 29, 31, 31` (bit 3), which dtk prints as
`extrwi r3, r0, 1, 28`; `fn_74_5C` interleaves its loads and stores, so it is subscript stores
rather than a `CVector3f` copy.

### The one place the family's usual shape does not hold, and the check it cost

`config/G2ME01/rels/Splinter/symbols.txt:545` gives the loader slot
`lbl_74_bss_70 = .bss:0x70; size:0x8 data:4byte` - **`size:0x8`, not the `size:0x4` that
`lbl_29_bss_6C` (Ing) and `lbl_44_bss_84` (MinorIng) have.** `RUNNING_THE_DECOMP.md` states the
rule for exactly this case under "Read the loader record's size off *both* of the DOL's readers":
read the `.bss` size first, and if it is not 4, grep the DOL for every reader of the `gLoader_*`
symbol before choosing a shape. I did, and the answer is that the record is a plain
`FScriptLoader`:

- **one** reader of `gLoader_Splinter` in the whole DOL:
  `LoadSplinter__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218C38, which reads
  **word 0 only** - `lwz r6, gLoader_Splinter@sda21(r0) / lwz r12, 0x0(r6) / mtctr r12 / bctrl`.
  No second reader, no `__ptmf_scall`, no CodeWarrior pointer-to-member-function (contrast
  `CMetroidRel.cpp`, whose record is 0x10 bytes for exactly that reason).
- the registration `fn_74_E8` stores **one word** (`stwu r0, lbl_74_bss_70@l(r3)`), not three
  copied out of `.data` the way `CSnakeWeedSwarmRel.cpp` and `CSplitterRelMain.cpp` do.

So the 0x8 is retail's 8-byte slot for a 4-byte pointer, not a bigger record, and the declaration
is `extern FScriptLoader lbl_74_bss_70;` under MWCC with a host definition. Had I taken the 0x8 at
face value and written a pmf, the module's sha1 would have broken on the extra two words.

The import is the **plain DOL symbol `fn_80218C64`** (`stw r3, gLoader_Splinter@sda21(r0); blr`,
`build/G2ME01/asm/auto_03_80218C64_text.s`, immediately after `LoadSplinter` at 0x80218C38 which
is 0x2C bytes and so ends exactly there). No `symbols.txt` rename and no DOL change were needed,
unlike `CAtomicAlphaRel.cpp` which has to spell the MWCC-mangled `SetLoader_AtomicAlpha` form.

### Dead-strip: measured, not assumed

`build/G2ME01/Splinter/ldscript.lcf` lists all eleven of `fn_74_0`..`fn_74_78` in its FORCEACTIVE
block, and `lbl_74_data_854` - the module's own 0x26C-byte CPatterned vtable - stores every one of
them (`fn_74_78` at entry 0x3C, above `HealthInfo__3CAiFv` at 0x38, then `fn_74_8`/`fn_74_54`/
`fn_74_5C`, `fn_74_18`, `fn_74_0`/`fn_74_20`, `fn_74_28`, `fn_74_30`/`fn_74_40`/`fn_74_4C`). So
each is referenced twice over and **no `force_active:` entry was needed in
`config/G2ME01/config.yml`** - the trap `CGeomBlobV2` hit. `fn_74_78` is reached through a real
member call on a thirteen-virtual stand-in (`CSplinterDispatch::Slot12`), which is what makes
mwcceppc emit `lwz r12, 0(r3)`; the hand-loaded-vtable spelling compiles to `lwz r3` and loses the
function (measured in `CIngPuddleRel.cpp`).

## One compile error, and what it was

`extern "C" const unsigned short kInvalidUniqueId;` in my first draft **collided** with
`include/MetroidPrime/TGameTypes.hpp:17`, which already declares it as `const TUniqueId`:

```
#     136: extern "C" const unsigned short kInvalidUniqueId;
#   Error:                                                 ^
#   identifier 'kInvalidUniqueId' redeclared
#   was declared as: 'const TUniqueId'
#   now declared as: 'const unsigned short'
```

`CIngRel.cpp` gets away without the declaration because it includes the same header. The fix was to
delete my redeclaration and keep the `TGameTypes.hpp` include. Worth knowing for the next head: the
`kInvalidUniqueId`, `lbl_8041AAB8` and `lbl_8041B758` externs are **not** interchangeable - the
first comes from the header, the other two are module/DOL externs that must be declared.

## Left unclaimed, and why

`fn_74_118` (0x118, 0x724) is the module's own entity loader and the 264 functions above it are
CSplinter's methods - behavioural class code needing the CActor/CPatterned/CAi hierarchy this tree
does not model, which is why every other head in this family leaves the same range to dtk. Its
`mr r19, r3 / mr r24, r4 / mr r18, r5` prologue is the `(CStateManager&, CInputStream&,
const CEntityInfo&)` loader signature, so it is declared and never defined here. The module's 278
text symbols therefore split **14 ours + 5 `REL_Setup` + 2 `global_destructor_chain` + 257
unclaimed**.

Nothing blocked me, so there is no `NEW:` line to file. The next thing worth spending an item on in
this module is `fn_74_118` and its methods, which need the CActor/CPatterned/CAi headers - that is
a header job, not a lane-sized decompilation, and it is the same blocker every head in this family
records.
