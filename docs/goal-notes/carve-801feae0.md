# carve-801feae0 — `fn_801FEAE0`, the 0x24-byte script-object element's copy constructor

**Item:** `match`, target `MetroidPrime/ScriptObjects/Carve801FEAE0`, worked in `../wt-mp2-goal-L8`
on 2026-10-02. **Result: the unit is `Matching` 1/1 at 100.00%, `flip_test.sh` PASSES, and
`tools/goal_check.sh build/goal/item.json` prints `goal_check: PASS carve-801feae0`** (DOL still
hashes to `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs hold, the port's undefined count
did not move).

## What the function is, measured

`fn_801FEAE0` (0x801FEAE0, 0x68 = 104 B, `symbols.txt:8313`) is the copy constructor of the
0x24-byte element that the `Matching` `ScriptObjects/Carve801FEA98.c` constructs. Its 26
instructions were read out of the disc this run with `python3 tools/dol_read.py 0x801FEAE0 0x68`
and cross-checked against dtk's own `build/G2ME01/asm/auto_03_801FEAE0_text.s:8-35`:

```
stwu r1,-16 / mflr r0 / lis r5,lbl_803B7BCC@ha / stw r0,0x14(r1) / addi r0,r5,lbl_803B7BCC@l
stw r31,0xc(r1) / mr r31,r4 / addi r4,r31,4 / stw r30,0x8(r1) / mr r30,r3
lis r3,lbl_803B7BF0@ha / stw r0,0(r30) / addi r0,r3,lbl_803B7BF0@l / addi r3,r30,4
stw r0,0(r30) / bl __ct__Q24rstl66basic_string<c,...>FRC... / addi r3,r30,0x14
addi r4,r31,0x14 / bl fn_801FE8B8 / lwz r0,0x14(r1) / mr r3,r30 / lwz r31 / lwz r30 / mtlr
addi r1,r1,16 / blr
```

Three things say **constructor**, and are why it is spelled as one: the members are copy-constructed
in place (a bare `addi r3,r30,4` with **no null test** — `rstl::construct` / placement `new` costs an
`addic.` + `beq`); the two vptr stores come **before** the member inits, which is where a
constructor's base-class constructors run and not where a hand-written body would; and the receiver
comes back in `r3` (`mr r3,r30`), mwcceppc's constructor epilogue.

**The layout is not guessed — the class's deleting destructor is already `Matching`.**
`src/MetroidPrime/ScriptObjects/Carve801FD924.cpp` (0x801FD924..0x801FD998) restores this same class's
own vtable (`lbl_803B7BF0`, the second store here) into +0x0, destroys the `rstl::string` at +0x04
and calls `fn_801FD6F0` on the member at +0x14 — the member this constructor hands to `fn_801FE8B8`.
Its header records the layout word for word. 0x24 = 36 bytes total, which is what the two `Matching`
`rstl::construct` claims on either side measured off retail's own copy walks.

## The recipe: a two-level base chain, and the three spellings that failed first

This run's finding, and the reusable part: **each vptr store is a base class's constructor, and both
stores have to land at offset +0x0.** mwcceppc will not give two base classes the same offset, so the
obvious spellings put the second store at +0x4. Four spellings compiled with `tools/probe_cc.sh` and
compared with `tools/bytescmp.py`:

| spelling | result |
| --- | --- |
| non-empty base storing `lbl_803B7BCC` + **second empty base** storing `lbl_803B7BF0` | 25 of 26; the empty base got offset 0x4 |
| **empty** base writing `lbl_803B7BCC` through `this` + a `void*` member in the mem-init list | same failure, and `mName` pushed off +0x4 |
| non-empty base + `void*` member in the mem-init list | same two failures, plus `addi r4,r31,8` instead of `+4` |
| **`SCarve801FEAE0Base` → `SCarve801FEAE0Self` → `SCarve801FEAE0Element`, a two-level chain** | **26 of 26, 104 of 104 bytes** |

A single-inheritance chain gives every level offset 0x0, so both stores come out `stw r0,0(r30)`; and
each base's constructor is emitted before the derived class's mem-init list, which is what puts both
vptr stores in front of the string and the member. **This is the same recipe
`carve-801fecac`'s note predicted, and it generalises to `fn_801FEE88`.**

The member's copy constructor is a **one-line thunk** to `fn_801FE8B8`, because retail's is:
0x801FEB28 is a `bl` with nothing read out of the member before it and nothing written back after.
mwcceppc inlines the thunk, so the object's only call is retail's `bl` with retail's relocation.
Declaring the member's copy constructor out of line instead emits a `bl` under the *mangled member
name*, and objdiff pairs relocations by name.

## The config change, and the rename it needs

A constructor's symbol is mangled and retail's name here is the placeholder `fn_801FEAE0`, so
`config/G2ME01/symbols.txt:8313` is renamed to the name mwcceppc emits — **read out of the object with
`powerpc-eabi-nm`, not guessed**:

```
__ct__21SCarve801FEAE0ElementFRC21SCarve801FEAE0Element
```

The class name is the carve's own placeholder, after the `SCarve801FD924Element` naming
`Carve801FD924.cpp` uses; retail names no such class either. `Carve801FEA98.c`'s declaration and call
carry the same name — that reference is what proves the definition in the DOL link. Without the
rename the DOL is still byte-exact but objdiff pairs nothing and the unit counts 0.

## The four files of the carve, plus the caller and the port

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/Carve801FEAE0.cpp` | **new**, 262 lines: header (the 26 instructions, the layout and where it comes from, the three proofs it is a constructor, the four spellings and their scores, the rename, the port half) + the extern decls, the member with its thunk, the two-level base chain, the constructor, and the `#ifndef __MWERKS__` port entry point |
| `config/G2ME01/splits.txt:1404-1405` | `.text start:0x801FEAE0 end:0x801FEB48` (0x68), between `Carve801FEA98.c` and `Carve801FEC64.c`. `total_functions` is **28465** before and after (measured in `build/report.json`) |
| `configure.py:784` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEAE0.cpp")` |
| `files.cmake:591` | `src/MetroidPrime/ScriptObjects/Carve801FEAE0.cpp` |
| `config/G2ME01/symbols.txt:8313` | the rename above |
| `src/MetroidPrime/ScriptObjects/Carve801FEA98.c` | the callee's declaration and call renamed; the three comments that called the callee unclaimed and its matching "impossible" annotated **superseded**, each naming what was wrong (see below) |
| `src/MetroidPrime/PortLinkStubs.cpp` | `stub_225` (`fn_801FEAE0`) **retired** — this unit defines the symbol for the port's link too, through its `#ifndef __MWERKS__` half exporting retail's mangled name; `stub_232` (`fn_801FE8B8`) added for the new body's callee; `stub_data_9` (`lbl_803B7BCC`) for its base vtable (`lbl_803B7BF0` is `stub_data_6`, already there); header count clause updated to functions 191 → 193, data 9 → 10, total 200 → 203 |

The item's stated payoff held: **`stub_225` is retired, not moved.** The `#ifndef __MWERKS__` half
defines the same name as a plain `extern "C"` function, because the host compiler mangles the
constructor the Itanium way (`_ZN21SCarve801FEAE0ElementC1ERKS_`) while the port's call site is a C
file that can only call an unmangled name.

## The previous blockers' reason was wrong, and that is the reusable part

Three places in this tree said matching these 0x68 bytes "needs the `.data` vtable pair as well as the
`basic_string` copy constructor and `fn_801FE8B8`'s body, none of them claimed"
(`Carve801FEC64.c`, `Carve801FEA98.c`, `stub_225`'s own paragraph). Three errors in one sentence, the
same ones `carve-801fecac` found:

- **the string copy constructor is claimed and `Matching`** — `rstl/rstl_strings.cpp`,
  `symbols.txt:13852`'s `__ct__...FRC...` at 0x802FF134;
- **the vtables are dtk's `.data`; a `Matching` unit needs the *relocations*, not the objects** —
  `extern "C" char lbl_803B7BCC[];` and taking its address is enough;
- **`fn_801FE8B8` is a callee, not a dependency of the claim** — a `bl` to a symbol dtk already
  defines (`auto_03_801FDC88_text.o`, `powerpc-eabi-nm` shows `T fn_801FE8B8`) needs no body anywhere;
  only the *port's* flat link needs a stand-in, one stub line.

**A `Matching` unit needs its callees' symbols, not their bodies.** Every "claiming X would only move
the gap one function along" paragraph in this tree is about the *port*, and none of them is a reason
the *DOL* cannot claim X.

## Verification, every number from this run

| check | result |
| --- | --- |
| `build/report.json` | `main/MetroidPrime/ScriptObjects/Carve801FEAE0` = **1 / 1 functions, 100.00%**, `__ct__21SCarve801FEAE0ElementFRC21SCarve801FEAE0Element`, 104 B; `Carve801FEA98` still 2 / 2 at 100.00% after the rename |
| counts | `matched` 13076 → **13077**, `total_functions` 28465 → **28465**, units 2157 → 2158 |
| `tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FEAE0.cpp` | **PASS → kept as Matching** (DOL sha1 + all 86 RELs) |
| `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` |
| `python3 tools/bytescmp.py` (final.o, against the **disc**) | 26 of 26 instructions, 104 B vs 104 B; the 6 differing words are all relocated fields (4 `lis`/`addi` immediates, 2 `bl` displacements) |
| `python3 tools/check_symbol_names.py` | `checked 581 units; 0 declared names are missing from their object` |
| `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FEAE0` | ok (1 unit, none out of retail order) |
| `tools/unit_fit.sh` | 120 claimed / 200 ours / 120 retail, over by 80 = the weak COMDAT `__dt__Q24rstl66basic_string<...>Fv`, which mwldeppc drops (`CUnknown90.o` carries the same shape and is `Matching`) — `flip_test` is the verdict, and it passes |
| `tools/link_check.sh` | `compile errors 0`, `unique undefined symbols 285` (baseline 285, unmoved), `duplicate definitions 0` |
| `python3 tools/link_gap.py` | `279 MISSING symbol(s), all accounted for in port_link_gap_list.md` |
| `./tools/goal_check.sh build/goal/item.json` | **PASS** (`build/goal/check-gate.log`: `GATE PASS 3cfdd6d1+9 changed`) |

## NEW: item this run measured and can hand on

Same recipe, one different vtable. The 0x24-byte element's third copy constructor, at the far end of
the same neighbourhood; its bytes differ from `fn_801FEAE0`'s in a single relocated immediate, so
this item's source is the recipe with that one symbol changed.

NEW: carve-801fee88 | match | MetroidPrime/ScriptObjects/Carve801FEE88 | fn_801FEE88 (0x801FEE88, 0x68 = 104 B, the 0x24-byte element copy constructor that stores `lbl_803B7BCC` then `lbl_803B7BFC`) - the recipe in `Carve801FEAE0.cpp` with `lbl_803B7BFC` named instead of `lbl_803B7BF0`, so it needs only the same symbols.txt rename (class `SCarve801FEE88Element`, measure the emitted name with `powerpc-eabi-nm`) and no new port stub at all: `fn_801FEE88`'s stand-in and `lbl_803B7BFC` (`stub_data_8`) are both already in `PortLinkStubs.cpp`, and retiring that stand-in leaves the port's undefined count unmoved. `Carve801FEE40.c:48-57` already documents its 26 instructions

## Notes for the next lane

- **A `Matching` carve needs callees' symbols, not bodies** — the sentence to re-read above.
- **Two vptr stores at the same offset need a base *chain*, not two bases.** mwcceppc gives a second
  base class its own slot; only single inheritance keeps both levels at +0.
- **The rename is load-bearing for the count, not for the hash** — with the placeholder name kept,
  `main.dol` would still be byte-exact and objdiff would count 0.
- **`tools/unit_fit.sh` over-counts by 80 on any unit with an `rstl::string` member**: the weak
  COMDAT `__dt__...Fv` that mwldeppc drops. Not a blocker.
