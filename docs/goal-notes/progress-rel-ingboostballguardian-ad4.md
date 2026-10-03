# progress-rel-ingboostballguardian-ad4

`kind: match`, `target: IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianAD4`.
**Module 30 matched_functions 63 -> 64 (of 319)**, the new unit `Matching` at 1/1 / 100.00% fuzzy,
100.00% matched code, `matched_code 92 / 92`, `complete_code 92` (`fn_30_AD4`), `All:`
**13465 -> 13466 matched** and **6513 -> 6514 linked**, `All: 37.54% fuzzy, 30.97% matched,
13.84% linked (13466 / 28465 functions)`, DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
`IngBoostBallGuardian.rel` `956265e8ccf3f489e9cb3a70ec22d0357ce17cca` matching
`config/G2ME01/config.yml` and (unchanged) `config/G2ME01/build.sha1:31`, `cmp`-equal to
`orig/G2ME01/files/RelProd/`, `total_functions` still 28465, `probe 913 files, 0 failed, 0 errors;
link: LINKED (286 undefined, 0 duplicates)`, `check_symbol_names.py` 0 missing of 585 units,
`unit_fit.sh` `claimed 92 / ours 92 / retail 92, fits` with no extra functions,
`check_decl_order.py --unit ...AD4` `ok: 1 unit(s) checked, none emits its functions out of retail
order` (one function, so the descending-order question does not arise).
`./tools/flip_test.sh IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianAD4.cpp`
-> **PASS** (kept as Matching) and `./tools/goal_check.sh build/goal/item.json` -> **PASS**.

Base figures are the judge's `build/goal/judge/report.base.json`, not the previous item's note.

## What I did

Claimed `.text 0xAD4..0xB30` of module 30 as its new unit: `fn_30_AD4`, 0x5C = 92 bytes, the copy
assignment of the 0x30-byte record at member offset +0x29C. All four parts of the carve in one
change:

- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianAD4.cpp` (new, one function)
- `config/G2ME01/rels/IngBoostBallGuardian/splits.txt` - one `.text` entry, placed in address order
  between the 0xA95C and the 0xB788 entries
- `configure.py` - one `Object(Matching, ..., source=..., mw_version="GC/2.7")` in the existing
  `Rel("IngBoostBallGuardian", ...)` block, on one line, right after the 1056C entry
- `files.cmake` - the source path, with an empty host branch

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`, but I did not
touch them: `tools/gate.sh` (inside `goal_check.sh`) rewrites the derived counts from the tree, and
it does so whether or not I revert them - I reverted them, re-ran `goal_check.sh`, got PASS and the
same two files modified again. Nothing else is in the diff.

## What the function is, measured

It is the **second callee of `fn_30_1056C`**, which is what makes it claimable: the unit the
previous item landed emits `R_PPC_REL24 fn_30_AD4` at **object offset 0x28** of
`build/G2ME01/src/IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian1056C.o`
(measured, `objdump -r`), i.e. the call for the member at +0x29C. Before this item, five of the
module's own objects referenced it and dtk filled it - `objdump -r` finds the relocation in
`auto_00_00000000_text.o` four times and in `auto_00_00000130_text.o`, `auto_00_0000D2E0_text.o`,
`auto_00_00010C24_text.o` and `auto_00_0001466C_text.o` once each, so the brief's "five objects" is
confirmed.

The body is three regions, read off `build/G2ME01/IngBoostBallGuardian/asm/auto_00_00000000_text.s`:

    li r5, 0x15 ; bl __copy        with r3 = r30 and r4 = r31 and no addi
    lwz r5,0x18(r31) ; addi r3,r30,0x20 ; lwz r0,0x1c(r31) ; addi r4,r31,0x20
    stw r5,0x18(r30) ; stw r0,0x1c(r30) ; bl fn_30_B84
    ... mr r3,r30

- **The head sub-record is 0x15 = 21 bytes, not 0x18.** `__copy`'s third argument is a *byte* count
  (`src/Runtime/CPlusLibPPC.cpp`: `void* __copy(char* to, char* from, size_t size)`), so +0x15..+0x17
  are padding this unit does not copy. `CIngBoostBallGuardian1056C.cpp` sizes the same record as
  `RelRecord30` with `unsigned char sub00[0x18]`; the two agree - 0x18 + 8 + 0x10 = 0x30.
- **It is at offset 0**, which is why `__copy` gets bare `r30`/`r31` with no `addi` - the same
  reasoning the 1056C entry gives for its `fn_30_10694` call.
- **The last 0x10 bytes are a callee, not inline loads**, so nothing here measures that layout.
  `RelSub10` is sized from `fn_30_B84`'s own disassembly (0xB84, 0x130): it reads +0x04 and +0x08,
  keeps both, and when either is nonzero allocates `+0x08 * 12` bytes into +0x0C before copying
  `+0x04 >> 2` twelve-byte `lfs`/`lwz`/`lbz` triples out of the source's +0x0C. Nothing at +0x00 is
  read. Only the size and the 4-byte alignment of that record are load-bearing here.
- **It returns `self`** - the `mr r3,r30` at 0xB18 - so the return type is the pointer.

## The padding is alignment, not a member, and both spellings were measured

| spelling of the head | object | bytes equal to retail's 92 |
|---|---|---|
| `unsigned char b[0x15] __attribute__((aligned(4)))` | 1 fn, 92 B | **92 (byte-exact)** |
| the same array with **no** attribute (the `int w18` behind it already pads to 0x18) | 1 fn, 92 B | **92 (byte-exact)** |
| `unsigned char b[0x15]; unsigned char pad15[3];` - the pad declared as a member | 1 fn, **108 B** | 49, first diff at 0x24 |
| the two word copies written +0x1C before +0x18 | 1 fn, 92 B | 88, first diff at 0x27 |

The declared-pad failure is **108 bytes against retail's 92**, the same number
`include/MetroidPrime/CDamageVulnerability.hpp` records for the same mistake: a declared pad is a
member, so the implicit assignment copies it and the compiler emits a second `__copy`. The
`aligned(4)` is kept even though the third row shows it is redundant as written, because it says so
rather than leaving the padding to the next member's alignment.

The two word copies are written in **declaration order** (+0x18 then +0x1C), which puts `+0x18` in
r5 and `+0x1C` in r0 as retail does. The reversed order - the trick `CIngBoostBallGuardian33B0.cpp`
needed at its own offsets - gives 88 of 92 here, so this is the other way round from that unit.

## mw_version, measured on this source

| mw_version | object | bytes equal to retail's 92 |
|---|---|---|
| GC/2.7 (the override used) | 1 fn, 92 B | **92 (byte-exact)** |
| GC/2.0, GC/2.0p1, GC/2.5, GC/2.6 | 1 fn, 92 B | 92 (byte-exact) |
| GC/1.3.2 (the module default) | 1 fn, 92 B | 82, first diff at 0x25 |
| GC/3.0a5 | 1 fn, **240 B** | 27 |

Load-bearing, with the same per-object override as the other five entries in this class: setting the
version on the `Rel(...)` block instead would recompile the other module-30 units.

## The carve's effect on the dtk auto units, measured

`auto_00_00000130_text` ran 0x130..0xAD4 with 32 functions; splitting at 0xAD4..0xB30 leaves it with
9 functions (2468 bytes) and makes dtk start a **new** `auto_00_0000B30_text` unit with the other 22
(3612 bytes). 9 + 22 + the 1 that moved into this unit = the 32 before, so it is a split and not a
loss, and `total_functions` stays 28465. All 86 RELs are unchanged and `87 files OK`.

## No dead-strip hazard, and that is measured

`fn_30_AAC` is not in `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but the
five relocations above are in dtk's own objects and `CIngBoostBallGuardian1056C.o` names it as well,
so a reference survives the carve (re-measured after the build: `objdump -r` still finds
`fn_30_AD4` four times in `auto_00_00000000_text.o` and once in each of `auto_00_00000130_text.o`,
`auto_00_0000D2E0_text.o`, `auto_00_00010C24_text.o` and `auto_00_0001466C_text.o`).

The two callees still resolve, and no new undefined symbol is added. **The carve makes dtk start a
new auto unit at 0xB30** (`auto_00_00000B30_text.o`, which `build.ninja:30129` puts in the module's
link, where `auto_00_00000130_text.o` had covered 0xB84 before), and that object defines
`fn_30_B84`; `__copy` was already an undefined REL symbol before the carve - `nm -u` found it in
`auto_00_00000130_text.o`, an object the module linked - so this unit adds none. Note that
`auto_00_00000000_text.o` is dtk's whole-module reference object for objdiff and is **not** in the
link (`build.ninja:30122` onwards lists `auto_00_00000130_text.o`, our object, then
`auto_00_00000B30_text.o`), so the copy of `fn_30_AD4` that object still carries is not a duplicate.
No `force_active:` entry, no `config.yml` change and no `symbols.txt` rename: the split claims
`.text` only, and keeping the `fn_30_*` name matters because dtk's objects call it that way.

**The claim spans no unclaimed gap**: `fn_30_A8C` is 0x20 bytes and ends exactly at 0xAAC, so
`fn_30_AAC` (0x28) runs 0xAAC..0xAD4 with nothing unclaimed between.

## What is left, and where the next run should start

Module 30 has 255 functions still unmatched (319 in the module, 64 matched), all retail bytes in the
gaps. The cheapest next claim
is the 40 bytes directly in front of this one, and it became claimable exactly because this item
landed the function it calls.

NEW: progress-rel-ingboostballguardian-aac | match | IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianAAC | fn_30_AAC (.text 0xAAC..0xAD4, size 0x28 = 40 bytes per config/G2ME01/rels/IngBoostBallGuardian/symbols.txt:26) is the function immediately in front of the unit this item lands, and the claim spans no gap because fn_30_A8C is 0x20 bytes and ends exactly at 0xAAC: its whole body is `stwu r1,-0x10(r1)` / `mflr r0` / `cmplwi r3,0` / `stw r0,0x14(r1)` / `beq` / `bl fn_30_F78` / the four-instruction epilogue / `blr`, i.e. a null-guarded one-argument call with r3 passed straight through, so it is spellable as `if (self) fn_30_F78(self);` with no address arithmetic and no return value. fn_30_F78 (.text 0xF78 size 0x74) is not claimed by any unit - there is a stale build/G2ME01/IngBoostBallGuardian/obj/IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianF78.o from an earlier attempt but no configure.py entry - so declare it extern "C"; nm finds it as T in auto_00_00000000_text.o and auto_00_00000B30_text.o so it still resolves. fn_30_AAC is named by an R_PPC_REL24 in auto_00_00000000_text.o (0xA98) and in auto_00_00000130_text.o (0x968), both of which keep the reference after the carve, so there is no dead-strip hazard; mw_version will want GC/2.7 like the rest of this class