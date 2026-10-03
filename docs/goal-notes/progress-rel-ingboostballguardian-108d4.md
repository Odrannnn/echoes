# progress-rel-ingboostballguardian-108d4

`kind: match`, `target: IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian108D4`.
**Module 30 matched_functions 58 -> 59 (of 318)**, the new unit `Matching` at 1/1 / 100.00% fuzzy
and 100.00% matched, `All:` **13460 -> 13461 matched** and **6508 -> 6509 linked**,
`All: 37.52% fuzzy, 30.96% matched, 13.82% linked (13461 / 28465 functions)`, DOL
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `IngBoostBallGuardian.rel` `956265e8ccf3f489e9cb3a70ec22d0357ce17cca`
matching `config/G2ME01/build.sha1:31` and (unchanged) `config/G2ME01/config.yml:222`,
`cmp`-equal to `orig/G2ME01/files/RelProd/`, `total_functions` still 28465, `probe 908 files, 0 failed`,
`check_symbol_names.py` 0 missing of 585 units, `unit_fit.sh` `claimed 700 / ours 700 / retail 700,
fits` with no extra functions. `./tools/flip_test.sh IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian108D4.cpp`
-> **PASS** (kept as Matching) and `./tools/goal_check.sh build/goal/item.json` -> **PASS**.

## What I did

Claimed `.text 0x108D4..0x10B90` of module 30 as its new unit: `fn_30_108D4`, 0x2BC = 700 bytes,
the module's own copy of a 0x15C-byte record. All four parts of the carve in one change:

- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardian108D4.cpp` (new, one function)
- `config/G2ME01/rels/IngBoostBallGuardian/splits.txt` - one `.text` entry, placed in address
  order between the 0xD230 and the 0x10B90 entries
- `configure.py` - one `Object(Matching, ..., source=..., mw_version="GC/2.7")` in the existing
  `Rel("IngBoostBallGuardian", ...)` block, on one line
- `files.cmake` - the source path, with an empty host branch

## What the function is, measured

87 loads and 87 stores and one `blr`, in retail's schedule, and `powerpc-eabi-objdump -r
build/G2ME01/IngBoostBallGuardian/obj/auto_00_0000D2E0_text.o` has **no relocation anywhere in
0x35F4..0x38B0** (object offsets for the module's 0xD2E0-based object), so the bytes are the whole of
the claim and there is no callee to declare. The layout read off the instructions is confirmed by
script, not by eye: the per-member load type repeats `lfs, lwz, lbz` **exactly 29 times** at offsets
`12i, 12i+4, 12i+8` for i = 0..28, i.e. 0x00/0x04/0x08 .. 0x150/0x154/0x158, and the 87 store offsets
are the same 87 values in the same order. 0x15C bytes, no tail and no gaps.

**Who calls it, and where it sits.** `fn_30_10694` (0x10694, 0x240) is the whole-structure copy; it
copies +0x00..+0xEF inline, sets `r3 = r30+0xF0` / `r4 = r31+0xF0` and calls `fn_30_10B90`, copies
+0x138..+0x13F inline, then sets `r3 = r30+0x140` / `r4 = r31+0x140` and calls this function.
Measured on the same object's relocations: `R_PPC_REL24 fn_30_10B90` at object offset 0x35B8 and
`R_PPC_REL24 fn_30_108D4` at 0x35D4. So this record is a member of the 0x3FC-byte structure and is
reached through r3/r4 as a copy assignment on an lvalue, not through a hidden return pointer -
which is why the parameters are spelled `self`/`other`. No name: a copy assignment says what each
member is made of, and the 10B90 note's argument (these bytes occur nowhere in the DOL) applies.

## The brief's advice is wrong here, and the finding is the run

The brief handed over says to copy member-by-member because `*self = other` stops inlining. Both
halves of that are **superseded at this size, and measured**:

1. **`*self = other` at 87 members emits an out-of-line `__as__11RelRecord15CFRC11RelRecord15C`** -
   732 bytes in the object, two functions. So the brief is right about the whole-object spelling
   being unusable. (The 10B90 note had measured the *opposite* at 18 members: inlined, one function,
   byte-identical. The inlining threshold sits between 18 and 87 members, as that note said.)
2. **The 87-member member-by-member spelling does NOT give retail's bytes.** It compiles to the same
   700 bytes and the same 175 instructions, and **five of them are wrong**:

       insn  +85  +0x154  retail: lwz r5,0xac(r4)  flat: lwz r0,0xac(r4)
       insn  +87  +0x15C  retail: lbz r0,0xb0(r4)  flat: stw r0,0xac(r3)
       insn  +88  +0x160  retail: stw r5,0xac(r3)  flat: lbz r0,0xb0(r4)
       insn +101  +0x194  retail: stb r0,0xc8(r3)  flat: lfs f0,0xcc(r4)
       insn +102  +0x198  retail: lfs f0,0xcc(r4)  flat: stb r0,0xc8(r3)

   That is **13 bytes of 700**, and it is register allocation, not layout: the first three are the
   15th triple's word member (offset 0xAC = 12*14+4) and the last two are the 17th triple's byte
   member (offset 0xC8 = 12*16+8). Register census of the flat version: `lwz r5` 28 times plus
   `lwz r0` once, against retail's `lwz r5` 29 times.
3. **The spelling that is byte-exact is one assignment per 12-byte triple:**

       struct RelTriple { float f; int w; unsigned char b; };
       struct RelRecord15C { RelTriple t[29]; };
       void fn_30_108D4(RelRecord15C* self, const RelRecord15C& other) {
         self->t[0] = other.t[0];  ...  self->t[28] = other.t[28];
       }

   29 lines, one function, **700 bytes, byte-identical to retail** (compared instruction by
   instruction against the object extracted from `orig/G2ME01/files/RelProd/IngBoostBallGuardian.rel`).
   The nesting is what fixes the register choice: the three-member implicit assignment operator is
   inlined per element, so the allocator never sees 87 independent scalar copies to interleave.

   **The lesson, since the next run will hit the same thing**: on a member-by-member copy of this
   shape, model the record as the *array of the repeating element* that the disassembly's stride
   implies, and write one assignment per element. Spreading the members out flat is the thing that
   goes wrong, and it fails at 13-of-700 bytes - under any percentage-based check.

**Spellings tried, none but the last reaches 100%** (all under GC/2.7 unless noted; compared as raw
`.text` bytes, not percentages):

| spelling | object | byte-exact |
|---|---|---|
| 87 flat members, member-by-member | 1 fn, 700 B | no, 13 bytes differ |
| nested `RelTriple t[29]`, 87 flat member copies | 1 fn, 700 B | no, 13 bytes differ (identical to row 1) |
| nested, 29 `self->t[i] = other.t[i]` | 1 fn, 700 B | **yes** |
| flat, `*self = other` | 1 fn, 732 B, emits `__as__` | no |
| nested, `*self = other` | 1 fn, 48 B | no (copies only the array pointer) |
| flat, through a local `RelRecord15C t` | 1 fn, 780 B | no |
| flat, `bool` for the third member | 1 fn, 700 B | no, same 13 bytes |
| flat, `unsigned int` for the second member | 1 fn, 700 B | no, same 13 bytes |
| flat, 87 members in reverse order | 1 fn, 700 B | no, 271 bytes differ |

`RelTriple t[29]` assigned element-wise is the unique hit among these. **The 10B90 note's claim that
the record "is a POD struct" should be read as "a POD struct, possibly an array of one element"** -
its 18-member record has no repeating stride so the distinction does not arise there.

## mw_version, measured again on this source

GC/2.7 is load-bearing and the per-object override is required: at the module default **GC/1.3.2**
the same source gives plain load/store pairs and **512 of its 700 bytes are wrong**; GC/1.3.2r
behaves the same. GC/2.0, GC/2.0p1, GC/2.5, GC/2.6 and GC/2.7 are all byte-exact; GC/3.0a5 gives
844 bytes. Setting the version on the `Rel(...)` block instead would recompile the other fifteen
module-30 units.

## configure.py naming, inherited and re-measured

The object is named `IngBoostBallGuardian/<path>` with an explicit `source=`, because
`goal_check.sh` resolves a `match` target by searching configure.py for the queue's spelling, and
the queue names a REL unit the way `report.json` does. The price is the doubled prefix objdiff
prints for the unit. The entry sits after the 388C entry on purpose: `flip_test.sh`'s `unit_info`
counts parentheses from the last `MusyX(`, so a `Rel(...)` list entry reads one open paren too many
and is looked for under `extern/musyx/src`; the stray close paren at the end of the 388C comment
balances the span for this entry and every entry after it. Replicated by hand before building - all
four module-prefixed entries (388C, 10B90, 3790, 108D4) resolve to `src/...` and the files exist.
The comment above the new entry contains no unbalanced paren and no literal `MusyX(`.

## No dead-strip hazard, and that is measured

`fn_30_108D4` is not in `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but the
`R_PPC_REL24` at object offset 0x35D4 in `auto_00_0000D2E0_text.o` holds the reference, so dtk's own
object keeps this unit's `.text` in the link. No `force_active:` entry, no `config.yml` change and no
`symbols.txt` rename: the split claims `.text` only, and keeping the `fn_30_*` name matters because
dtk's objects name it that way. `powerpc-eabi-nm -n` on the built object gives `00000000 T
fn_30_108D4` and nothing else.

## Measured results

`build/report.json` for module 30 after the change (base from `build/goal/judge/report.base.json`):

    IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian108D4   1 / 1  100.0
    module total: 58 -> 59 / 318

`matched 13460 -> 13461, linked 6508 -> 6509`, **no unit anywhere worse** (checked every unit in the
report against the baseline), DOL sha1 and all 86 RELs unchanged. `unit_fit.sh` reports
`claimed 700 / ours 700 / retail 700` with no extra functions. `check_decl_order.py` reports
`ok: 0 unit(s) checked` - it does not see REL units, and with one function the descending-order
question does not arise anyway.

## What is left, and where the next run should start

259 functions remain in module 30, all retail bytes in the gaps (all of them in `auto_*` text units).
**The whole-structure copy `fn_30_10694` (0x10694..0x108D4, 0x240) is now claimable**: both of its
callees are units of their own, it has no relocation-free property (it *has* two `R_PPC_REL24`s, to
`fn_30_10B90` and `fn_30_108D4`, both already declared), and it is the same inline member-by-member
shape - copies +0x00..+0xEF, +0x138..+0x13F and the two calls in between. It must declare both
callees, and its `mw_version` has to be GC/2.7 (or whichever version its own body measures at). It
is 0x240 bytes of 100-ish inline copies, so the register-allocation trap above is a live risk there
too - expect to need the same "is there a repeating stride?" question answered before writing it.

NEW: progress-rel-ingboostballguardian-10694 | match | IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian10694 | fn_30_10694 (.text 0x10694..0x108D4, 0x240 = 576 bytes) is the whole-structure copy that calls the two now-claimed units (R_PPC_REL24 to fn_30_10B90 at object offset 0x35B8 and to fn_30_108D4 at 0x35D4 of auto_00_0000D2E0_text.o, so it must declare both): it copies +0x00..+0xEF inline, calls fn_30_10B90 at +0xF0, copies +0x138..+0x13F inline and calls fn_30_108D4 at +0x140 - the same inline member-by-member shape, so budget for the register-allocation trap measured this run (13 wrong bytes out of 700 for a flat member-by-member copy; the fix was to model the repeating 12-byte stride as an array of one element and assign element-wise) and read the inline runs' stride off the disassembly before spelling them
