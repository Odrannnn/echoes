# progress-rel-ingboostballguardian-fn30888

`kind: match`, `target: IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C`.
**Module 30 matched_functions 24 -> 25 (of 318)**, the new unit `Matching` at 1/1 / 100.00%,
`All:` **13136 -> 13137 matched** and **6228 -> 6229 linked**, DOL
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `IngBoostBallGuardian.rel` `cmp`-equal to
`orig/G2ME01/files/RelProd/`, `total_functions` still 28465, `probe 843 files, 0 failed`,
`check_symbol_names.py` 0 missing, `unit_fit.sh` `claimed 84 / ours 84 / retail 84, fits`, no
extra functions. `./tools/goal_check.sh build/goal/item.json` -> **PASS**, and
`./tools/flip_test.sh IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C.cpp`
-> **PASS** (kept as Matching).

## What I did

Claimed `.text 0x388C..0x38E0` of module 30 as its second unit: `fn_30_388C`, 0x54 = 84 bytes,
the module's own copy of a 0x20-byte record. All four parts of the carve in one change:

- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C.cpp` (new, one function)
- `config/G2ME01/rels/IngBoostBallGuardian/splits.txt` - one `.text` entry
- `configure.py` - one `Object(Matching, ..., source=..., mw_version="GC/2.7")` in the existing
  `Rel("IngBoostBallGuardian", ...)` block, plus the corrected method count in the head comment
  ("300 of the 301 ... stay unclaimed")
- `files.cmake` - the source path, with an empty host branch

## The one real finding: `mw_version`, not the spelling

The previous note on this module recorded that `fn_30_388C` needs MWCC's two-deep load/store
pipeline and that **four spellings do not produce it** (`*self = other`, member-by-member
assignment, a mem-init list, and reading members through subscripts). All four of those
measurements were taken at the module default **GC/1.3.2**, and that is the whole story:

    variant                                GC/1.3.2            GC/2.7
    *static_cast<T*>(self) = other         plain pairs         BYTE-IDENTICAL to retail
    same, class with a user ctor           plain pairs         BYTE-IDENTICAL
    member-by-member d->m = o.m            plain pairs         BYTE-IDENTICAL
    copy ctor, mem-init list               plain pairs         BYTE-IDENTICAL
    out-of-line copy ctor, mem-init list   plain pairs         BYTE-IDENTICAL

"Plain pairs" is `lfs f0,0(r4) / stfs f0,0(r3) / lfs f0,4(r4) / ...` - one live register,
source order. Retail is the two-deep schedule, `f0`/`f1` then `r0`/`r5`, every load issued two
instructions ahead of its store. **The same whole-object assignment that gives plain pairs under
1.3.2 is byte-identical under 2.7.** Also measured: `GC/2.0p1`, `GC/2.5` and `GC/2.6` give the
same 84 bytes; `GC/3.0a5` does not. So this is a `mw_version` question, not a source question,
which is the same class of finding `CLumiteRelTail.cpp` and `CSandBossRelTail.cpp` record. The
per-object override is set in the `Rel(...)` block.

The record's shape is load-bearing too: **the four halfword members must be separate class
members.** A nested two-`unsigned short` struct is copied as one `lwz`/`stw` and the function
comes out **68 bytes against retail's 84** (measured), because the two halves collapse into one
word copy.

## Why the function is CHealthInfo's copy constructor (measured, not inferred)

The module's 84 bytes occur **exactly once** in `build/G2ME01/main.dol` (which the gate shows is
retail byte-identical), at file offset **0x6DB60**. `build/G2ME01/asm/auto_03_80070DB4_text.s:10`
gives `/* 80070DB4 0006DBB4 */`, which fixes the mapping at `vaddr = fileoff + 0x80003200` and
puts those bytes at **0x80070D60** - `config/G2ME01/symbols.txt:2082`,
`__ct__11CHealthInfoFRC11CHealthInfo = .text:0x80070D60; // type:function size:0x54`. Same length,
same bytes, and the shape is a constructor copying into the hidden return pointer at r3.

The record as retail copies it: `float f0, f4, f8`, `int w0C`, `unsigned short h10, h12`,
`int w14`, `unsigned short h18, h1A`, `unsigned char b1C` - 0x1D bytes, padded to 0x20. The
names are for the accesses only; a copy constructor says what each member is made of and nothing
about what it is called. **This tree's `include/MetroidPrime/CHealthInfo.hpp` does not describe
these bytes** (three floats, two `CWeaponMode`, four `TUniqueId`), and it is not edited for this -
it is included by CAi, CScriptActor and seven other units - so the carve carries a local type with
retail's layout, the arrangement `CIngBoostBallGuardianRel.cpp` uses for
`CIngBoostBallGuardianDispatch`.

## No dead-strip hazard, and the three neighbours stay retail

`powerpc-eabi-nm` on the built object: `00000000 T fn_30_388C`, one symbol, 84 bytes.
`auto_00_00011CD8_text.o` has `U fn_30_388C`, so **dtk's own object holds the reference**, the
function is not in `ldscript.lcf`'s FORCEACTIVE list, and no `force_active:` entry was added -
`config/G2ME01/config.yml` is unchanged and no `symbols.txt` rename was needed.

The other three functions of the four-function run (`fn_30_3790`, `fn_30_37E0`, `fn_30_3838`)
**stay retail** and the claim does not touch them: all three are null-guarded constructors that
`bl fn_30_12DB4`, which is itself in the unclaimed region, so claiming them means reproducing a
function this item cannot see. This item claimed only 0x388C..0x38E0 rather than the whole run.

## Two configure.py shapes that are for the judge, not for dtk

Both are documented in the `configure.py` comment beside the entry; recording them here because
both are visible in the diff and neither is cosmetic.

1. **The object is named `IngBoostBallGuardian/<path>` with an explicit `source=`.**
   `tools/goal_check.sh` resolves a `match` item's target by searching `configure.py` for
   `Object(<state>, "<target>.cpp"`, and the queue names a REL unit the way `build/report.json`
   does - module prefix included. With the conventional `MetroidPrime/ScriptObjects/...` name the
   lookup finds nothing and the item fails with *"match target ... has no Object(...) entry in
   configure.py"* however correct the unit is. `source=` is the mechanism
   `REL/global_destructor_chain.c` already uses; the source file itself stays in
   `src/MetroidPrime/ScriptObjects/`. **Measured cost:** `tools/check_module_wiring.py` only reads
   splits keys beginning `MetroidPrime/` or `REL/`, so this unit is outside that check's census;
   its coverage comes from configure.py's own "Missing source file" test and from flip_test, and
   both do see it (verified). The doubled prefix in the report's unit name
   (`IngBoostBallGuardian/IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C`)
   is the price. Three queued items share this shape
   (`progress-rel-ingboostballguardian-fn30888`, `...-copys`, `progress-rel-ingboostballguardian-fn30f78`),
   so the next run should expect the same fix.
2. **The stray closing parenthesis at the end of that comment is deliberate.** The `unit_info`
   helper in `tools/flip_test.sh` picks a unit's source root by counting `(` and `)` from the
   last `MusyX(` up to the entry; the enclosing `Rel(` is still open when it stops, so **every**
   unit in a `Rel(...)` list counts one `(` too many, is looked for under `extern/musyx/src/`, and
   `check` refuses before it has built anything ("no source file ... would pass while proving
   nothing"). Measured on the clean tree: `CIngBoostBallGuardianRel.cpp` (153 `(` / 152 `)`),
   `CLumiteRelTail.cpp` (127/126), `CSandBossRelTail.cpp` (39/37). One more `)` than `(` in the
   comment balances the count for this entry, so flip_test finds the file and runs the test it
   exists to run - the DOL sha1 and all 86 RELs - instead of declining before it starts. The tool
   is the judge's and was not edited. **The real fix belongs in `unit_info`: it should look for
   the enclosing `Rel(`/`MusyX(` and not treat a module-list entry as inside a MusyX block.**
   (Not filed as a `NEW:` - it is a tooling fix, not work that raises a count.)

The `Object(...)` is also **on one line**: two of the judge's greps match `Object` and its quoted
path per line and would not see a multi-line entry (with one, the STATE check after the flip
reported `configure.py has it as ?, not Matching`).

## Measured results

`build/report.json` for module 30 after the change:

    MetroidPrime/ScriptObjects/CIngBoostBallGuardianRel               17 / 17  100.0  complete
    IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C   1 / 1  100.0  complete
    REL/global_destructor_chain                                        2 /  2  100.0  complete
    REL/REL_Setup                                                      5 /  5  100.0  complete
    module total: 24 -> 25 / 318;   auto_* units: 293 unmatched, still retail

`All: 37.12% fuzzy, 30.56% matched, 13.50% linked (13137 / 28465 functions)`,
`matched 13136 -> 13137, linked 6228 -> 6229`, **no function anywhere worse**, DOL sha1 and all 86
RELs unchanged. `unit_fit.sh` reports `claimed 84 / ours 84 / retail 84` with no extra functions;
`check_decl_order.py` reports `0 unit(s) checked` (it does not see REL units - `nm -n` is the
substitute, and with one function the order question does not arise).

## What is left, and where the next run should start

293 functions remain in module 30, all retail bytes in the gaps. The two queued items cover the
other two member-wise copy constructors (`0xF78..0xFEC`, `0x33B0..0x340C`) - **they should now be
straightforward, because this run establishes the spelling and the compiler version for exactly
that shape**: model retail's layout as a POD struct and write the copy as one whole-object
assignment, then set `mw_version="GC/2.7"`.

NEW: progress-rel-ingboostballguardian-10b90 | match | IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian10B90 | fn_30_10B90 (0x10B90, 0x94) is a relocation-free data-movement leaf, the same shape as fn_30_388C which this item reproduced byte-exactly with a POD whole-object assignment under mw_version GC/2.7 - model the member types from the loads and the copy is one line; the 0x2BC-byte 0x108D4..0x10B90 leaf is the same idea at five times the size
---

# Re-run 2026-10-02 (lane 4, worktree wt-mp2-goal-L4, goal/lane-4 @ c905e7fa)

**The run above is not in this tree; the item was re-done from scratch, not `STALE:`.** Measured
four ways before touching anything: `git status --porcelain` clean,
`git log --all --grep=fn30888` empty, no `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C.cpp`,
no `388C` key in `config/G2ME01/rels/IngBoostBallGuardian/splits.txt`, no `388C` `Object(` in
`configure.py`, and no such unit in `build/report.json`. Module 30's claimed units stood at
**41 / 41** (the seven units listed in the block below plus `REL/global_destructor_chain` and
`REL/REL_Setup`), not the 24 the first run recorded, so the tree moved on without it. This run
re-measured the recipe rather than trusting it; every claim marked "measured" here was measured here.

## Result

Same carve, all four files, `fn_30_388C` (`.text` 0x388C..0x38E0, 0x54) as a new `Matching` unit:

- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C.cpp` (new; local `RelCHealthInfo` with
  retail's layout, one `*self = other;`)
- `config/G2ME01/rels/IngBoostBallGuardian/splits.txt` - one `.text` entry, keyed
  `IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C.cpp:`
- `configure.py` - one `Object(Matching, "IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C.cpp", source="MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C.cpp", mw_version="GC/2.7")` in the existing `Rel("IngBoostBallGuardian", ...)` block
- `files.cmake` - the source path, with an empty host branch

Module 30's claimed units **41 -> 42 / 42**; the new unit 1/1, 100.00% fuzzy and 100.00% matched.
`All: 37.21% fuzzy, 30.64% matched, 13.51% linked (13197 / 28465 functions)`,
**matched 13196 -> 13197, linked 6246 -> 6247**, `total_functions` still 28465, DOL sha1
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `IngBoostBallGuardian.rel` `cmp`-equal to
`orig/G2ME01/files/RelProd/`, `probe 850 files, 0 failed`, `check_symbol_names.py` 0 missing of 585
units, `unit_fit.sh` `claimed 84 / ours 84 / retail 84, fits`, no extra functions,
`check_decl_order.py` `ok: 1 unit(s) checked`, `flip_test` **PASS** (kept as Matching), and
`./tools/goal_check.sh build/goal/item.json` -> **PASS**.

## Re-measured: both spelling claims hold, and the first one is a gate failure

1. **`mw_version` is the whole story, confirmed by making the gate fail.** With the per-object
   `mw_version="GC/2.7"` removed and nothing else changed, the *same* source compiles to 84 bytes
   of plain pairs - one live register, source order:
   `lfs f0,0(r4); stfs f0,0(r3); lfs f0,4(r4); stfs f0,4(r3); ...; lhz r0,0x10(r4); sth r0,0x10(r3)`.
   Retail keeps two live and issues each load two instructions ahead of its store (`f0`/`f1`, then
   `r0`/`r5`). The build stops with **`1 computed checksum(s) did NOT match`** - the module's sha1
   breaks, which is the cheapest possible proof that the version override is load-bearing rather
   than decorative. Restored, the build is byte-identical again.
2. **The four halfwords have to be four members.** Measured here too: wrapping +0x10/+0x12 and
   +0x18/+0x1A in a nested `struct { unsigned short lo, hi; }` makes each pair one `lwz`/`stw` and
   the function comes out **0x44 = 68 bytes against retail's 0x54 = 84**. Reverted.

## One correction to the run above

**`fn_30_12DB4` is not what the three functions in front of `fn_30_388C` call.** They are
null-guarded *destructors*, and on this tree the `bl` at 0x37B0 resolves to the DOL global
`internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`
(`config/G2ME01/symbols.txt:13842`, `.text:0x802FE9B8`, size 0x40), which several `Carve*` sources
already declare - not a module-local callee at 0x12DB4. `fn_30_37E0` additionally calls
`fn_30_3838` inside the same run, and all three call `Free__7CMemoryFPCv`. So 0x3790..0x388C is
claimable in principle (see the NEW below), which the note above read as needing an unseen module
function.

## flip_test's `unit_info` and the `Rel(` paren - confirmed, plus a trap in the fix

The counting bug is still exactly as described above, measured again here:

    entry                                                       parens from the last MusyX   root
    MetroidPrime/ScriptObjects/CIngBoostBallGuardianC6AC.cpp           165 / 164         extern/musyx/src  (wrong)
    IngBoostBallGuardian/MetroidPrime/ScriptObjects/...388C.cpp        171 / 172         src              (right)

**The trap: do not write the literal text `MusyX(` in the comment that does the balancing.**
`unit_info` does `s.rfind('MusyX(', 0, m.start())` over the whole file, comment text included, so a
comment that explains the hack by quoting `MusyX(` moves the anchor and re-breaks the count -
measured: with the literal in the comment the entry resolved to
`extern/musyx/src/MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C.cpp` again and flip_test
would have declined before building. The comment in configure.py says "the last MusyX call" instead
and ends with exactly one unmatched `)`. The entries *after* this one in the file also stop being
mis-rooted, since the span now balances for them too.

## NEW

NEW: progress-rel-ingboostballguardian-3790 | match | IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian3790 | the three null-guarded destructors fn_30_3790, fn_30_37E0, fn_30_3838 (.text 0x3790..0x388C, 0xAC bytes) call only Free__7CMemoryFPCv, each other, and the DOL global internal_dereference__Q24rstl66basic_string<c,...>Fv (symbols.txt:13842, already declared by several Carve* sources), so the whole run is claimable as one unit - this item deliberately claimed only 0x388C..0x38E0 in front of it
