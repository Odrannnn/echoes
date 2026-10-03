# progress-rel-ingboostballguardian-10b90

`kind: match`, `target: IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian10B90`.
**Module 30 matched_functions 54 -> 55 (of 318)**, the new unit `Matching` at 1/1 / 100.00% fuzzy
and 100.00% matched, `All:` **13456 -> 13457 matched** and **6504 -> 6505 linked**,
`All: 37.51% fuzzy, 30.94% matched, 13.81% linked (13457 / 28465 functions)`, DOL
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `IngBoostBallGuardian.rel` `956265e8ccf3f489e9cb3a70ec22d0357ce17cca`
matching `config/G2ME01/config.yml:222` and `cmp`-equal to `orig/G2ME01/files/RelProd/`,
`total_functions` still 28465, `probe 906 files, 0 failed`, `check_symbol_names.py` 0 missing of 585
units, `unit_fit.sh` `claimed 148 / ours 148 / retail 148, fits` with no extra functions.
`./tools/flip_test.sh IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian10B90.cpp`
-> **PASS** (kept as Matching) and `./tools/goal_check.sh build/goal/item.json` -> **PASS**.

## What I did

Claimed `.text 0x10B90..0x10C24` of module 30 as its new unit: `fn_30_10B90`, 0x94 = 148 bytes,
the module's own copy of a 0x48-byte record. All four parts of the carve in one change:

- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardian10B90.cpp` (new, one function)
- `config/G2ME01/rels/IngBoostBallGuardian/splits.txt` - one `.text` entry, placed in address
  order between the 0xD230 and the 0x11E44 entries
- `configure.py` - one `Object(Matching, ..., source=..., mw_version="GC/2.7")` in the existing
  `Rel("IngBoostBallGuardian", ...)` block, on one line, with the comment that says why
- `files.cmake` - the source path, with an empty host branch

## What the function is, measured

Eighteen loads and eighteen stores and one `blr`, in retail's two-deep schedule, and
`powerpc-eabi-objdump -r build/G2ME01/IngBoostBallGuardian/obj/auto_00_0000D2E0_text.o` has **no
relocation anywhere in 0x10B90..0x10C24**, so the bytes are the whole of the claim and there is no
callee to declare. The layout read off the instructions is six words at +0x00..+0x14, nine floats
at +0x18..+0x38, three words at +0x3C..+0x44 - eighteen four-byte members, 0x48 with no padding
and no tail. Every load is `lwz`/`lfs` and every store `stw`/`stfs`, so nothing is packed.

**Where it sits, and this is the part the brief did not say.** `fn_30_10694` (0x10694, 0x240) is
the whole-structure copy: it copies +0x00..+0xEF inline, then `addi r3,r30,0xF0` /
`addi r4,r31,0xF0` and calls `fn_30_10B90`, then copies +0x138..+0x13F inline and calls
`fn_30_108D4` at +0x140. So this record is **a member at +0xF0 of a 0x3FC-byte structure**, reached
through r3/r4 as an assignment on an lvalue rather than through a hidden return pointer - unlike
`fn_30_388C`, which `symbols.txt:2082` names as `__ct__11CHealthInfoFRC11CHealthInfo`. That is why
the parameters are spelled `self`/`other` and why the local type is called `RelRecord48` and
nothing worse.

**No name, and no search worth repeating.** Retail's 148 bytes occur **nowhere** in
`build/G2ME01/main.dol`: an exact 4-byte-aligned byte search returns no hit, and a
register-insensitive search (opcode class + rA + offset) over all 6.5M instructions returns no hit
either - the only offset-pattern matches are 16-instruction fragments inside the DOL's data
(`0x242FA0`, `0x1EE318`, `0x253B64`), not code. **A copy assignment says what each member is made
of, not what the record is called**; the 388C note's argument applies here with nothing to anchor
it, so the unit carries a local type named for its size.

## The finding that actually cost the run: `*self = other` does not compile at 18 members

The recipe the brief hands over - "model retail's layout as a POD struct and write the copy as one
whole-object assignment, then set `mw_version="GC/2.7"`" - **is wrong for a record this size, and
the failure is silent until `unit_fit.sh`.** With

    void fn_30_10B90(RelRecord48* self, const RelRecord48& other) { *self = other; }

`GC/2.7` stops inlining the implicit assignment operator and the object defines **two** functions:

    00000000 <fn_30_10B90>                              stwu/mflr/.../bl  (a 32-byte wrapper)
    00000020 <__as__11RelRecord48FRC11RelRecord48>       the 148 bytes, byte-identical to retail

So the bytes are right and the unit still cannot be `Matching`: retail's object defines one
function there and ours defines two, which is what `tools/unit_fit.sh` reports as an extra function
and what the goal brief warns makes a unit un-promotable however good it looks. The same
`./tools/decomp_build.sh` loop does not show it, and objdiff does not show it either.

**The size is the difference, not the spelling.** Measured on the same tree, same flags, same
`GC/2.7`: at **ten** members (`CIngBoostBallGuardian388C.cpp`, 0x54 bytes) `*self = other` inlines and
gives one byte-identical function; at **eighteen** it does not. Writing the eighteen
`self->wNN = other.wNN;` lines instead gives one function, and its 148 bytes are byte-identical to
retail (checked instruction by instruction against
`build/G2ME01/IngBoostBallGuardian/obj/auto_00_0000D2E0_text.o` at object offset 0x38B0). MWCC's
inline threshold is somewhere between ten and eighteen members here; the previous note's
"the copy is one line" advice is only true for the small records it was measured on, and the next
run on `fn_30_108D4` (87 members) should go straight to member-by-member.

`mw_version="GC/2.7"` is still load-bearing and still per-object, **measured again here on the
eighteen-member source**: at the module default `GC/1.3.2` the same file compiles to **148 bytes as
well** - one function, right length - but plain pairs, one live register and source order:

    lwz r0,0(r4) / stw r0,0(r3) / lwz r0,4(r4) / stw r0,4(r3) / ...

where retail keeps two live and issues each load two instructions ahead of its store. **Same byte
count, different bytes, and objdiff's fuzzy score is the only thing that would tell you**; the unit
would be 0x94 bytes of the wrong code. Setting the version on the `Rel(...)` block instead would
recompile the other eleven module-30 units.

## Two configure.py shapes carried over from the 388C note, both still needed

1. **The object is named `IngBoostBallGuardian/<path>` with an explicit `source=`.** `goal_check.sh`
   resolves a `match` target by searching `configure.py` for `Object(<state>, "<target>.cpp"`, and
   the queue names a REL unit the way `report.json` does, module prefix included. With the
   conventional `MetroidPrime/ScriptObjects/...` name the lookup finds nothing and the item fails
   with *"match target ... has no Object(...) entry in configure.py"*. `source=` is what
   `REL/global_destructor_chain.c` already uses; the source file itself stays in
   `src/MetroidPrime/ScriptObjects/`. The price is the doubled prefix objdiff prints for the unit
   (`IngBoostBallGuardian/IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian10B90`),
   and being outside `check_module_wiring.py`'s census (it only reads splits keys beginning
   `MetroidPrime/` or `REL/`) - its coverage comes from configure.py's own missing-source test and
   from flip_test, and both see it (verified).
2. **The stray `)` in the 388C entry's comment is load-bearing for this entry too, and the new
   comment must stay balanced and must not contain the literal anchor.** `flip_test.sh`'s `unit_info`
   counts parentheses from the last `MusyX(` up to the entry it is looking for, so an entry inside a
   `Rel(...)` list reads one open paren too many and is looked for under `extern/musyx/src`. The
   extra `)` at the end of the 388C comment balances the span, and every entry after it inherits the
   balance - **including the one added here.** Replicated by hand before building: all five module-30
   entries (`388C`, `10B90`, `D2xx`, `11E44`, `1464C`) resolve to `src/...` and the files exist. Two
   traps: a comment containing the literal text `MusyX(` moves `s.rfind('MusyX(', ...)`'s anchor and
   re-breaks the count (the 388C comment says "the last MusyX call" for that reason), and a comment
   with an unbalanced paren would push the next entry back under `extern/musyx/src`. The real fix
   belongs in `unit_info`, which should look for the enclosing `Rel(` and not treat a module-list
   entry as inside a MusyX block (not filed as `NEW:` - it is a tooling fix, not work that raises a
   count).

**No dead-strip hazard, and that is measured.** `fn_30_10B90` is not in
`build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but `powerpc-eabi-objdump -r`
on both `auto_00_00000000_text.o` and `auto_00_0000D2E0_text.o` shows an `R_PPC_REL24` naming it (at
0x10898), so dtk's own object holds the reference and this unit's `.text` survives the link.
`powerpc-eabi-nm -n` on the built object gives `00000000 T fn_30_10B90` and nothing else. No
`force_active:` entry, no `config/G2ME01/config.yml` change and no `symbols.txt` rename: the split
claims `.text` only, and keeping the `fn_30_*` name matters because dtk's objects name it that way.

## The claim is 0x10B90..0x10C24 and not the 0x108D4..0x10C24 run

`fn_30_108D4` in front of it is 0x2BC bytes of the same kind of copy (see NEW below) and
`fn_30_10C24` behind it is a different function, so neither is claimed here. The carve does split
dtk's `auto_00_0000D2E0_text.o` into `auto_00_0000D2E0_text` (0xD2E0..0x10B90) and
`auto_00_00010C24_text` (0x10C24..0x11CD8); the gate's per-function diff reports that as a split
with an exact count match, and `total_functions` stays 28465.

## Measured results

`build/report.json` for module 30 after the change (base from `build/goal/judge/report.base.json`):

    IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian10B90   1 / 1  100.0
    module total: 54 -> 55 / 318;   auto_* units: 264 -> 263 unmatched functions, still retail

`matched 13456 -> 13457, linked 6504 -> 6505`, **no function anywhere worse**, DOL sha1 and all 86
RELs unchanged. `unit_fit.sh` reports `claimed 148 / ours 148 / retail 148` with no extra functions.
`check_decl_order.py` reports `ok: 0 unit(s) checked` - it does not see REL units, and with one
function the descending-order question does not arise anyway.

## What is left, and where the next run should start

263 functions remain in module 30, all retail bytes in the gaps (all of them in `auto_*` text
units). The whole-structure copy `fn_30_10694` (0x10694..0x108D4, 0x240) is now claimable **only
together with both of its callees**: it is the same inline member-by-member shape, so if
`fn_30_108D4` lands, `fn_30_10694` is a mechanical follow-on that declares it and `fn_30_10B90`.

NEW: progress-rel-ingboostballguardian-108d4 | match | IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian108D4 | fn_30_108D4 (.text 0x108D4..0x10B90, 0x2BC = 700 bytes) is the same kind of relocation-free leaf copy: 174 loads and stores plus a blr, no entry in .rela.text anywhere in the range (measured on obj/auto_00_0000D2E0_text.o at object offset 0x35F4), copying 87 contiguous four-byte-or-narrow members 0x00..0x158 whose per-member load type repeats `float, int, unsigned char` exactly 29 times - so it is a POD struct of 29 float/int/unsigned char triples (stride 12) copied member-by-member under mw_version GC/2.7, NOT `*self = other`, which at 87 members certainly stops inlining and would emit an extra out-of-line __as__ function (measured this run at 18 members)