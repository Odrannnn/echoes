# progress-rel-ingboostballguardian-10694

`kind: match`, `target: IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian10694`.
**Module 30 matched_functions 59 -> 60 (of 318)**, the new unit `Matching` at 1/1 / 100.00% fuzzy and
100.00% matched (`fn_30_10694`, 576 bytes), `All:` **13462 -> 13463 matched** and **6510 -> 6511
linked**, `All: 37.53% fuzzy, 30.97% matched, 13.83% linked (13463 / 28465 functions)`, DOL
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `IngBoostBallGuardian.rel`
`956265e8ccf3f489e9cb3a70ec22d0357ce17cca` matching `config/G2ME01/config.yml:222` and `cmp`-equal to
`orig/G2ME01/files/RelProd/`, `total_functions` still 28465, `probe 910 files, 0 failed` (link:
`LINKED`, 286 undefined, 0 duplicates), `check_symbol_names.py` 0 missing of 585 units,
`unit_fit.sh` `claimed 576 / ours 576 / retail 576, fits` with no extra functions,
`check_decl_order.py` `ok: 1 unit(s) checked, none emits its functions out of retail order`.
`./tools/flip_test.sh IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian10694.cpp`
-> **PASS** (kept as Matching) and `./tools/goal_check.sh build/goal/item.json` -> **PASS** (run
twice: once before and once after a comment-only correction in `configure.py` and the source).

## What I did

Claimed `.text 0x10694..0x108D4` of module 30 as its new unit: `fn_30_10694`, 0x240 = 576 bytes, the
whole-sub-object copy. All four parts of the carve in one change:

- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardian10694.cpp` (new, one function)
- `config/G2ME01/rels/IngBoostBallGuardian/splits.txt` - one `.text` entry, in address order between
  the 0xD230 and the 0x108D4 entries
- `configure.py` - one `Object(Matching, ..., source=..., mw_version="GC/2.7")` in the existing
  `Rel("IngBoostBallGuardian", ...)` block, on one line, right after the 108D4 entry
- `files.cmake` - the source path, with an empty host branch

## What the function is, measured

Sixty loads off r31 (`other`) and sixty stores into r30 (`self`), one `addi`/`addi`/`bl` pair for each
of the two records, and one `blr`. The two `R_PPC_REL24`s are at object offset 0x35B8 (to
`fn_30_10B90`) and 0x35D4 (to `fn_30_108D4`) of `build/G2ME01/IngBoostBallGuardian/obj/auto_00_0000D2E0_text.o`,
which is how this item became claimable at all. The head is **sixty members at 0x00..0xEF plus a word
at 0x138 and a float at 0x13C** - the load/store offset sets are equal (script-checked) and each
load's type (`lwz`/`lfs`) is its store's type, so nothing is narrower than a word and nothing is
packed. Two gaps: 0xF0..0x137 is the 0x48-byte record `fn_30_10B90` copies, and 0x140..0x29B is the
0x15C-byte record `fn_30_108D4` copies. So the sub-object this function writes is **0x29C bytes** and
its last store is +0x29B.

**The brief's register-allocation trap does not bite here, and the reason is measured, not assumed.**
`CIngBoostBallGuardian108D4.cpp` says to read the inline runs' stride off the disassembly before
spelling them. Done: over all 62 members the per-member type sequence
`W F F F F F F W W W W W W W W W W W W W F W F F F W W W W W F F F F F W F F F W F F F F F W F F F F
W W F F F W W W F F F` **repeats for no period from 1 to 30** (script, over every offset). So there
is no array-of-triples to model and the head is spelled flat, member by member - and the flat spelling
is byte-identical here, where at 87 members it was 13 bytes short. **The 108D4 note's conclusion holds
and its scope does not: model the repeating stride when the disassembly has one, and write it flat
when it does not.**

**`mw_version` is load-bearing again**, per-object, same reason as the two units it calls: retail keeps
two temporaries live and issues each load two instructions ahead of its store (`r0`/`r3` for the
words, `f0`/`f1` for the floats, and `r5` for the word at +0xDC that the first call would otherwise
clobber). Spellings tried, all one function, all compared as raw `.text` bytes against retail's 576:

| spelling | mw_version | object | byte-exact |
|---|---|---|---|
| 60 flat members + 2 calls | GC/2.7 | 1 fn, 576 B | **yes** |
| the same | GC/2.0, GC/2.5, GC/2.6 | 1 fn, 576 B | yes |
| the same | GC/1.3.2 (module default) | 1 fn, 576 B | no, **360 of 576 wrong** (plain pairs) |
| the same | GC/3.0a5 | 1 fn, 576 B | no, 429 wrong (emits `_savegpr_16`/`_restgpr_16`) |
| `self->rec0F0 = other.rec0F0` and `self->rec140 = other.rec140` | GC/2.7 | **2 fns**, 756 B (`__as__11RelRecord48FRC11RelRecord48` at 0x260) | no, 390 wrong |

The last row is the negative result worth keeping: the assignment spelling does not merely add an
extra function, it also collapses the head's schedule back to plain pairs, so it is worse in both
ways. Retail calls two *named* functions, so the two records are `extern "C"` calls here.

**The two record types are declared in this unit as well as in theirs, identically**, and that is
checked by measurement rather than by inspection: all three objects are byte-identical to retail, so
the three layouts agree by construction.

**`fn_30_1056C` is what identifies the shape**, and it is measured: it (0x1056C, 0x128) calls this
function at +0 (`R_PPC_REL24 fn_30_10694` at object offset 0x32A8), then initialises +0x29C and
beyond itself - so the module's whole class is larger than 0x29C and this is a base sub-object's copy
assignment, which is why the parameters are `self`/`other` and the return type is the pointer (the
epilogue's `mr r3,r30` at 0x35DC).

**No dead-strip hazard, and the count here needed care.** `fn_30_10694` is not in
`ldscript.lcf`'s FORCEACTIVE list; `powerpc-eabi-objdump -r` names it at module 0x10588 (in
`fn_30_1056C`) and at module 0x10C40 (in `fn_30_10C24`). The earlier reading of this run - "four
relocation records" - was wrong and is corrected in the source and in `configure.py`: **dtk's auto
units overlap**, so each of those two instructions is named by `auto_00_00000000_text.o` *as well as*
by the object that starts at its own address (`fn_30_EC6C` is in both `auto_00_00000000_text.o` and
`auto_00_0000D2E0_text.o`, byte for byte). Four records, two instructions, one caller each. No
`force_active:` entry, no `config.yml` change, no `symbols.txt` rename: the split claims `.text` only.

**The claim spans no unclaimed gap**: `fn_30_1056C` ends exactly at 0x10694, and `total_functions`
stays 28465 because retail's function boundaries do not move. The gate's per-function diff reports
`auto_00_0000D2E0_text: 1 function(s) moved into ...CIngBoostBallGuardian10694 (exact count match - a
split, not a loss)`.

## Measured results

`build/report.json` after the change (base from `build/goal/judge/report.base.json`):

    IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian10694   1 / 1  100.0  (fn_30_10694, 576 B)
    module total: 59 -> 60 / 318

`matched 13462 -> 13463, linked 6510 -> 6511`, **no function anywhere worse** (every unit and every
function in the report compared against the pre-change report: 0 worse, 1 new, 1 "gone" which is the
same function moved out of dtk's `auto_00_0000D2E0_text`), DOL sha1 and all 86 RELs unchanged.

## What is left, and where the next run should start

258 functions remain in module 30, all retail bytes in the gaps (all of them in `auto_*` text units).
The immediate neighbour in front of this one is the natural follow-on and is now claimable, because
both of its callees are declarable:

NEW: progress-rel-ingboostballguardian-1056c | match | IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian1056C | fn_30_1056C (.text 0x1056C..0x10694, size 0x128 = 296 bytes per config/G2ME01/rels/IngBoostBallGuardian/symbols.txt:255) is the caller of the unit this item landed - it calls fn_30_10694 at +0 (R_PPC_REL24 at object offset 0x32A8 of auto_00_0000D2E0_text.o, now a Matching unit) and then fn_30_AD4 at +0x29C (R_PPC_REL24 at 0x32B4; undefined in all three module-30 objects that reference it, recorded in that symbols.txt:27 as .text:0x00000AD4 size 0x5C - a plain extern declaration, and dtk's own objects already reference it so the module still links), then copies 28 members inline from +0x2CC to +0x320 whose measured type sequence is W F F F F H H H B W F F F F H H H B W F F F F H H H B B (so there is a near-0x1C stride from 0x2D0 to model, but the third repeat's last member is a byte where the first two have a word - resolve that before spelling, since the 108D4 lesson is that the stride question decides the spelling); mw_version will want GC/2.7 as here, the claim spans no gap (fn_30_10530 ends at 0x1056C), fn_30_1056C is named by dtk's own objects at module 0xECEC inside fn_30_EC6C so there is no dead-strip hazard, and it returns self the same way fn_30_10694 does