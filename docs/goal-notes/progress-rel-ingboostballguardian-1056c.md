# progress-rel-ingboostballguardian-1056c

`kind: match`, `target: IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian1056C`.
**Module 30 matched_functions 60 -> 61 (of 318)**, the new unit `Matching` at 1/1 / 100.00% fuzzy,
100.00% matched and `matched_code 296 / 296`, `complete_code 296` (`fn_30_1056C`), `All:`
**13463 -> 13464 matched** and **6511 -> 6512 linked**, `All: 37.54% fuzzy, 30.97% matched,
13.84% linked (13464 / 28465 functions)`, DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
`IngBoostBallGuardian.rel` `956265e8ccf3f489e9cb3a70ec22d0357ce17cca` matching
`config/G2ME01/config.yml:222` and (unchanged) `config/G2ME01/build.sha1:31`, `cmp`-equal to
`orig/G2ME01/files/RelProd/`, `total_functions` still 28465, `probe 911 files, 0 failed, 0 errors;
link: LINKED (286 undefined, 0 duplicates)`, `check_symbol_names.py` 0 missing of 585 units,
`unit_fit.sh` `claimed 296 / ours 296 / retail 296, fits` with no extra functions,
`check_decl_order.py` `ok: 0 unit(s) checked, none emits its functions out of retail order` (it
does not see REL units; with one function the descending-order question does not arise).
`./tools/flip_test.sh IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian1056C.cpp`
-> **PASS** (kept as Matching) and `./tools/goal_check.sh build/goal/item.json` -> **PASS**.

## What I did

Claimed `.text 0x1056C..0x10694` of module 30 as its new unit: `fn_30_1056C`, 0x128 = 296 bytes, the
whole-class copy assignment. All four parts of the carve in one change:

- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardian1056C.cpp` (new, one function)
- `config/G2ME01/rels/IngBoostBallGuardian/splits.txt` - one `.text` entry, placed in address order
  between the 0xD230 and the 0x10694 entries
- `configure.py` - one `Object(Matching, ..., source=..., mw_version="GC/2.7")` in the existing
  `Rel("IngBoostBallGuardian", ...)` block, on one line, right after the 10694 entry
- `files.cmake` - the source path, with an empty host branch

## What the function is, measured

It is the **caller of the unit the previous item landed**, which is what makes it claimable:
`powerpc-eabi-objdump -r build/G2ME01/IngBoostBallGuardian/obj/auto_00_0000D2E0_text.o` gives
`R_PPC_REL24 fn_30_10694` at object offset 0x32A8 and `R_PPC_REL24 fn_30_AD4` at 0x32B4 (module
offsets 0x10588 and 0x10594, object offset + 0xD2E0 = module offset). `fn_30_10694` is a `Matching`
unit now, so the call is declarable; `fn_30_AD4` is `.text:0xAD4 size 0x5C`
(`config/G2ME01/rels/IngBoostBallGuardian/symbols.txt:27`) and is undefined in **five** of the
module's own objects - `auto_00_00000000_text.o` (four records, because dtk's head object overlaps
four call sites), `auto_00_00000130_text.o`, `auto_00_0000D2E0_text.o`, `auto_00_00010C24_text.o`
and `auto_00_0001466C_text.o` - so it stays a plain `extern "C"` declaration and the module still
links. **The brief's "all three module-30 objects" is corrected here to five**, measured by
`objdump -r` over every `obj/auto_*.o`.
Our object emits exactly those two relocations at exactly those offsets and nothing else
(`powerpc-eabi-nm -S` gives `00000000 00000128 T fn_30_1056C`, `U fn_30_10694`, `U fn_30_AD4`).

**The class is larger than the sub-object `fn_30_10694` copies** - 0x321 bytes, not 0x29C - and
this function is what shows it: it runs the base copy at +0, the 0x30-byte record's copy at +0x29C,
and then copies its own tail. So this is the whole class's copy assignment and `fn_30_10694` is its
base sub-object's; both are spelled `self`/`other` with a pointer return, the return fixed by the
`mr r3,r30` at 0x32BC (object offset 0x30), which MWCC hoists to just after the tail's first load
and ahead of both calls' clobber of r3.

**The tail's layout, script-checked rather than by eye.** 28 loads off r31 and 28 stores into r30;
the load offset set and the store offset set are equal and each load's type is its store's type at
the same offset, from +0x2CC to +0x320 with nothing in between that is not one of the 28. The
per-member type sequence is

    W F F F F H H H B | W F F F F H H H B | W F F F F H H H B B

i.e. **three elements of a 0x1C stride at +0x2CC, +0x2E8 and +0x304 - `int`, four `float`s, three
`unsigned short`s and one `unsigned char`, 27 bytes of members padded to the element's 4-byte
alignment - plus one trailing byte at +0x320**, which is where the fourth element's `int` would be
and is a `lbz`/`stb` instead. (Read off `build/G2ME01/src/IngBoostBallGuardian/MetroidPrime/
ScriptObjects/CIngBoostBallGuardian1056C.o`, which objdiff scores 296/296 matched against retail, so
the two agree by construction. The same numbers read off dtk's *pre-carve* auto object were the
spelling's starting point.)

**The brief's third-repeat puzzle is resolved: the stride is 0x1C and the last byte is a separate
member after the array.** `RelTriple28 t[3]; unsigned char flag;` is 0x55 bytes and puts `flag` at
+0x320 without a hand-inserted pad, because the 27 members of each element pad to 0x1C on their own.

## The 108D4 lesson holds, and its scope is wider than the note said

The 108D4 note's rule - *model the repeating stride as an array of one element and assign
element-wise* - applies here and reaches byte-exactness. But **the count was never the reason**: at
87 members the flat spelling was 13 bytes of 700 wrong, and at **28 members both spellings are
byte-exact** (measured, table below). What 108D4 actually fixed was the *nesting*: the flat
spelling hands the register allocator N independent scalar copies to interleave, and at N=87 it
interleaves two of them wrongly. At N=28 it does not. So the corrected rule is **"nest whenever the
disassembly shows a stride; how far the nesting pays is a size question, and the size is measured,
not assumed."**

**A trap the 108D4 note does not mention, and it is the one that costs the size here: an array
member inside the element compiles to a *block* copy of that member.** Declaring the element as

    struct RelTriple28 { int w; float f[4]; unsigned short h[3]; unsigned char b; };

and assigning `self->tail.t[i] = other.tail.t[i]` gives four `lwz`/`stw` for the floats and one
`lwz`/`stw` plus one `lhz`/`sth` for the halfwords - **272 bytes against retail's 296**, and the
halfword at +0x2E2 vanishes entirely. The nine members inside the element must be spelled out one at
a time; only the *elements* are an array. This is the same failure as 108D4's out-of-line `__as__`
in a different costume: the compiler is being helpful in a way retail is not.

**The same trap closes the other way in the flat spelling**: the two adjacent bytes at +0x31E and
+0x320 pack into +0x31E and +0x31F unless an explicit `unsigned char pad31F;` is declared between
them. The array spelling has no such question.

## Spellings tried, all compared as raw `.text` bytes against retail's 296

| spelling | mw_version | object | byte-exact |
|---|---|---|---|
| `RelTriple28 t[3]` + `flag`, three element assignments + the byte | GC/2.7 | 1 fn, 296 B | **yes** |
| the same | GC/2.0, 2.5, 2.6 | 1 fn, 296 B | yes |
| the same | GC/1.3.2 (module default) | 1 fn, 296 B | no, **54 of 74 instructions differ** (plain pairs, one live register, source order) |
| the same | GC/3.0a5 | 1 fn, 296 B | no, 69 differ |
| as above, but `float f[4]` + `unsigned short h[3]` **inside** the element | GC/2.7 | 1 fn, **272 B** | no, 24 bytes short (block copy of the array members) |
| flat 28 members, `b31E` and `flag320` adjacent, **no pad** | GC/2.7 | 1 fn, 296 B | no, 2 instructions differ - `flag` lands at +0x31F |
| flat 28 members with an explicit `unsigned char pad31F;` | GC/2.7 | 1 fn, 296 B | **yes** |

The first and the last rows both reach retail's bytes. The array spelling is the one kept, because
the 0x1C stride it encodes is the thing the disassembly shows, and because it derives the +0x320
byte's offset instead of asserting it.

## The two calls, and why the base one carries no address arithmetic

`&self->base` is at offset 0 of `RelRecord321`, so `fn_30_10694(&self->base, other.base)` compiles
to the bare `bl` at 0x32A8 with **no `addi` in front of it** - measured, not assumed, and it is why
the base sub-object is a first member rather than reached through a cast. `fn_30_AD4` is at +0x29C
and retail does emit `addi r3,r30,0x29C` / `addi r4,r31,0x29C` for it, so it gets the explicit
member address.

**`fn_30_AD4`'s record is sized, not laid out, and that is deliberate**: this function contributes
only the two `addi`s for it, so `RelRecord30` is declared with the shape `fn_30_AD4`'s own
disassembly shows (0x18 bytes its first callee copies, two inline words at +0x18/+0x1C, 0x10 bytes
its second callee copies) to get 0x30 bytes and 4-byte alignment, and no more. `fn_30_10694`'s
record types (`RelRecord48`, `RelRecord15C`, `RelRecord29C`) are repeated here identically to
`CIngBoostBallGuardian10694.cpp`, which is checked by measurement - both objects are byte-identical
to retail, so the two layouts agree by construction.

## mw_version, measured again on this source

GC/2.7 is load-bearing and the per-object override is required, for the same reason as the three
units it calls: at the module default **GC/1.3.2** the same source is still 296 bytes and 74
instructions with **54 of the instructions not retail's**. GC/2.0, GC/2.5 and GC/2.6 are
byte-exact; GC/3.0a5 differs in 69. Setting the version on the `Rel(...)` block instead would
recompile the other **eighteen** module-30 units (19 `Object(` entries in that block, counting this
one).

## configure.py naming and comment balance, replicated by hand

The object is named `IngBoostBallGuardian/<path>` with an explicit `source=`, because
`goal_check.sh` resolves a `match` target by searching configure.py for the queue's spelling, and
the queue names a REL unit the way `report.json` does. The entry sits after the 10694 entry because
`flip_test.sh`'s `unit_info` counts parentheses from the last `MusyX(`, so a `Rel(...)` list entry
reads one open paren too many and is looked for under `extern/musyx/src`; the stray close paren at
the end of the 388C comment balances the span for this entry and every entry after it. Checked by
hand before building: the comment block is paren-balanced (8 open, 8 close), contains no literal
`MusyX(`, and the source file exists.

## No dead-strip hazard, and that is measured

`fn_30_1056C` is not in `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but an
`R_PPC_REL24` names it at module 0xECEC, inside `fn_30_EC6C`, and that record appears in **both**
`auto_00_00000000_text.o` and `auto_00_0000D2E0_text.o` - one instruction named twice, because
dtk's auto units overlap, so the two records are one call site and not two. dtk's own objects
therefore hold the reference and this unit's `.text` survives the link. No `force_active:` entry, no
`config.yml` change and no `symbols.txt` rename: the split claims `.text` only, and keeping the
`fn_30_*` name matters because dtk's objects name it that way.

**The claim spans no unclaimed gap**: `fn_30_10530` is 0x3C bytes and ends exactly at 0x1056C, and
`fn_30_10694` behind it is the 10694 unit. `total_functions` stays 28465 because retail's function
boundaries do not move. The gate's per-function diff reports `auto_00_0000D2E0_text: 1 function(s)
moved into ...CIngBoostBallGuardian1056C (exact count match - a split, not a loss)`.

## Measured results

`build/report.json` after the change (base from `build/goal/judge/report.base.json`):

    IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian1056C   1 / 1  100.0  (fn_30_1056C, 296 B)
    module total: 60 -> 61 / 318

`matched 13463 -> 13464, linked 6511 -> 6512`, **no function anywhere worse** (every function in the
report compared against the pre-change report: 0 worse, 1 new, 1 "gone" which is the same function
moved out of dtk's `auto_00_0000D2E0_text`), DOL sha1 and all 86 RELs unchanged.

## What is left, and where the next run should start

257 functions remain in module 30, all retail bytes in the gaps (all of them in `auto_*` text units).
The natural follow-on is `fn_30_AD4` (0xAD4, 0x5C) - it is this item's second callee, it is the
0x30-byte record's copy assignment, and its own two callees plus its two inline words are all
visible in its disassembly, so it is claimable the same way `fn_30_1056C` was.

NEW: progress-rel-ingboostballguardian-ad4 | match | IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianAD4 | fn_30_AD4 (.text 0xAD4..0xB30, size 0x5C = 92 bytes per config/G2ME01/rels/IngBoostBallGuardian/symbols.txt:27) is the second callee of fn_30_1056C, the unit this item lands: R_PPC_REL24 fn_30_AD4 sits at object offset 0x32B4 of auto_00_0000D2E0_text.o, and it is undefined in five of the module's own objects (auto_00_00000000_text.o four times, plus auto_00_00000130/0000D2E0/00010C24/0001466C once each) so it is a plain extern declaration today. Its own body is a 0x18-byte sub-record copied by a first callee (called with `li r5,21` as a third argument), then two inline words at +0x18 and +0x1C, then a 0x10-byte sub-record copied by a second callee at +0x20, and it returns self (`mr r3,r30` at 0xB18). Declare both of its callees as extern "C" unless a NEW: claims them first; mw_version will want GC/2.7 like the rest of this class, the claim spans no gap - fn_30_AAC is 0x28 bytes and ends exactly at 0xAD4, and fn_30_AD4 is named by dtk's own objects at module 0x10594 inside the fn_30_1056C this item lands, so there is no dead-strip hazard
