# progress-main-ensureworldpakready - DONE: the function is written and 100.00%, three functions up on main

**Verdict up front.** `CMain::EnsureWorldPakReady` is written in `src/MetroidPrime/main.cpp` and
scores **100.00%** against retail. `main/MetroidPrime/main` goes **40/99 -> 43/99**, the tree total
goes **10059 -> 10062**, and `MP_GATE_DOCS_WRITE=1 ./tools/gate.sh build/goal/judge/report.base.json`
prints **GATE PASS** with `+3 functions at 100%, 0 units newly linked`. Nothing anywhere got worse.
The diff is one file, 49 insertions, 0 deletions: one function and the comment above it. No
`configure.py` / `splits.txt` / `files.cmake` / header change, no `asm`.

This is the item `progress-cmainasyncidle-ensureworldpakready` asked for, against the unit that
actually claims 0x80005698. Its section 2 was right about why that one was unjudgeable; its section
4 was **incomplete** - see the correction in section 5, which is the reusable part of this item.

## 1. The target, re-measured first

    $ python3 tools/check_decl_order.py --unit MetroidPrime/main   # before
    main/MetroidPrime/main    would break on a flip

    $ python3 -c "...build/report.json..."
    main/MetroidPrime/main   matched 40 / 99
    tree                     matched_functions 10059
    All: 30.99% fuzzy, 23.28% matched, 11.78% linked (10059 / 28465 functions)

`CMain::EnsureWorldPakReady` was declared (`include/MetroidPrime/CMain.hpp:84`) and called three
times (`CAutoMapper.cpp:411`, `CGameState.cpp:654`, `CGameState.cpp:1343`) and defined nowhere.
Retail's `main.o` has it at **+0x2E0, 208 bytes**, inside `main/MetroidPrime/main`
(`.text start:0x800053B8 end:0x80009880`, `config/G2ME01/splits.txt:34-37`) - so the target is a
unit in the baseline, which is what the previous item's target was not.

## 2. What the function does, from the object's own bytes

`build/binutils/powerpc-eabi-objdump -d -r build/G2ME01/obj/MetroidPrime/main.o`, +0x2E0..+0x3B0:

| reloc | what it pins |
|---|---|
| `gpResourceFactory` @+0x18 | hoist `CResFactory`; `+4` is the `CResLoader`, kept in r31 across the loop |
| `GetPakFile__10CResLoaderCFi` @+0x30 | outer loop over paks, index in r30, `cmpw`/`blt` so signed |
| `lbz 40(r3)` + `rlwinm. r0,r0,27,31,31` @+0x34 | `CPakFile::IsWorldPak()` - the byte is the bitfield cluster |
| `__ct__...vector<pair<string,SObjectTag>>` @+0x4C | **copy** `CPakFile`+0x58 (the name list) to `r1+8` |
| `mulli r0,r0,24` / `add` / `cmplw` / `bne` @+0x58..0x74 | the scan: pointer form, stride 24 |
| `lwz r0,20(r4)` @+0x64 | `SObjectTag::id` - +0x14 into a 24-byte pair, i.e. `it->second.id` |
| `li r29,1` @+0x2C, `li r29,0` @+0x70 | the flag: **not-yet-seen** semantics |
| `sub_80323554__8CPakFileFv` @+0x8C | id **absent** |
| `EnsureWorldPakReady__8CPakFileFv` @+0x98 | id **present** |
| `__dt__...vector<pair<string,SObjectTag>>` @+0xA4 | destroy the copy with `li r4,-1` |
| `GetPakCount__10CResLoaderCFv` @+0xB0 | outer loop bound |

Both tails are retail's own relocations against defined `CPakFile` members. Neither is a stand-in,
and nothing initialisation-like was dropped to gain the percent.

## 3. The three spellings, all measured on this tree with `./tools/fast_try.sh MetroidPrime/main`

Same algorithm every time; only the scan's loop form, the flag's placement and the flag's polarity
change. **The final spelling is 100.00%.**

| # | variant | function | unit |
|---|---|---|---|
| 1 | iterators, `notFound` declared **below** `CPakFile& file` | 75.00% | 42/99 |
| 2 | iterators, `notFound` declared **above** it | **95.58%** | 42/99 |
| 3 | **index** loop instead of iterators (rest as final) | 94.13% | 42/99 |
| 4 | iterators, `bool found = false` with the arms in natural order | 99.96% | 42/99 |
| 5 | **final**: iterators, `notFound` above, arms in that sense | **100.00%** | **43/99** |

Three independent findings, and only the third is the obvious one:

**1. The scan must be written with iterators** (row 3, -5.87 points). mwcceppc strength-reduces
`for (int j = 0; j < names.size(); ++j)` to a **counted** loop - I disassembled it, it emits
`mtctr r0 / cmpwi r0,0 / ble / ... / bdnz` - where retail's bytes are the pointer form
`mulli r0,r0,24 / add r3,r4,r0 / cmplw r4,r3 / bne`. `begin()`/`end()` produce retail's form.

**2. The flag's initialisation must precede the `GetPakFile` call** (row 1 -> row 2, **+20.58
points**, the whole item). This is the non-obvious one and it is worth the whole lane.
`bool notFound = true;` declared *after* `CPakFile& file = *resLoader.GetPakFile(i);` costs 24.4
points, and the entire cost is **register allocation**, not instruction selection:

* declared above, the flag's live range spans a call, so it must live in a callee-saved register.
  mwcceppc then produces retail's allocation exactly: `r27` = `id`, `r28` = pak, `r29` = flag,
  `r30` = index, `r31` = res loader, `stmw r27,28(r1)` in the prologue and `lmw r27,28(r1)` in the
  epilogue. 208 bytes, same instruction count, zero differences.
* declared below, mwcceppc sinks the `li` past the `bl` and past `lbz r0,40(r3)`, the live range
  then fits entirely between two calls, and the flag is allocated to a **scratch** register (r28)
  with the pak pointer in r29 - retail has those the other way round.

**It is the live range that picks the register, not the source order as such.** Two extra
consequences, both measured: the flag's register and the pak pointer's are a **pair** that swaps
together (fix one and the other follows), and the prologue's register-save form follows from the
allocation - mwcceppc emits `stmw r27,28(r1)` for a contiguous ascending saved pair and individual
`stw`s for a descending one (which is why retail's own `EnsureWorldPaksReady` at +0x274, saving
r31 then r30, has four `stw`s and matches at 100% today).

**3. The flag's polarity is the last single instruction** (row 4, -0.04). `bool found = false` with
the arms in their natural order differs in exactly `li r29,1` against `li r29,0` and back - one
instruction pair. Retail stores "not yet seen" and clears it on a match. Control flow identical
either way; mwcceppc does not normalise the register's sense.

At 100.00% the two objects are identical **instruction for instruction, including every relative
branch offset** - I compared all 52 instructions pairwise after normalising `b`/`bl`/`beq`/`bne`
targets to relative displacements: **0 mismatches**.

## 4. Gates, all of them, quoted

    $ ./tools/decomp_build.sh
      All: 31.00% fuzzy, 23.29% matched, 11.78% linked (728 / 2043 files)
      All: 31.00% fuzzy, 23.29% matched, 11.78% linked (10062 / 28465 functions)
    # baseline is 30.99% / 23.28% / 11.78% (10059) - the All: line rose

    $ sha1sum build/G2ME01/main.dol
    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010

    $ MP_GATE_DOCS_WRITE=1 ./tools/gate.sh build/goal/judge/report.base.json ; echo $?
    configure ok / ninja + build.sha1 ok / hashes vs config.yml ok / report ok /
    per-function diff  matched  10059 -> 10062   linked 4918 -> 4918
                        (+3 functions at 100%, 0 units newly linked) /
    module wiring ok / dol_read ok / docs claims ok / gs offsets ok / raw offsets ok /
    decl order ok / files.cmake ok / module order ok / port probe ok / port link gap ok /
    reach stubs not in a real build ok
    GATE PASS  26f20c3+2
    0

    $ ./tools/probe_sources.sh | tail -1
    probe: 750 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)

    $ python3 tools/check_symbol_names.py | tail -1
    checked 504 units; 0 declared names are missing from their object

The **250** is `build/goal/judge/undef.base.count`, unchanged: `main.cpp` is not in `files.cmake`,
this unit is `NonMatching`, and nothing here is what the port links. (The probe says 750 files, not
the 749 in the previous item's notes, because lane-2's head `26f20c3` is newer; the list comes from
`files.cmake` + `CMakeLists.txt`, neither of which I touched.)

`MP_GATE_DOCS_WRITE=1` is how the judge invokes the gate; it rewrites `docs/HANDOFF.md`'s state
block (+2/-2 lines). I reverted that file - the brief forbids editing it.

**Which three functions rose, from the two reports, and none fell** (per-function diff over all
2043 units / 28465 functions):

    UP  EnsureWorldPakReady__5CMainFUi                                          0.00 -> 100.00
    UP  __ct__...vector<pair<string,SObjectTag>>FRC...                            0.00 -> 100.00
    UP  __dt__...vector<pair<string,SObjectTag>>Fv                                0.00 -> 100.00
    DOWN  none.  ADDED  none.  REMOVED  none.

The two extras are the vector's copy constructor and destructor. This function is their **first
caller in the whole tree**; they were unpaired COMDATs at 0.00% because nothing instantiated them,
and writing the function that does is what pairs them. They are not credited by hand.

## 5. Corrections and negative results, for the next run

* **Correction to `progress-cmainasyncidle-ensureworldpakready` section 4.** It reports this exact
  function reaching 100.00% from two changes (iterators, then `notFound` with the arms swapped).
  Reproduced here, **the arms-swapped iterator spelling is 75.00%, not 100%** - rows 1 and 5 of the
  table differ by *only* the position of the `bool notFound = true;` declaration, and that
  declaration is worth 24.4 points. Its 69.12% / 74.98% / 100.00% ladder was measured with the
  flag below `CPakFile& file`, so its "the flag's polarity is the last single instruction" finding
  is true only *after* the placement is already right: from the 100% spelling, inverting the
  polarity costs 0.04. **Read the placement finding first.**
* **Correction to the same notes' section 5.** It says `check_decl_order.py` "reads `configure.py`
  and `splits.txt`, which this change does not touch", so decl order is unchanged. That is wrong:
  `tools/check_decl_order.py:45-54` reads `build/G2ME01/src/`, i.e. the **built object**, so decl
  order does move when source order does. I measured both trees: the verdict is `would break on a
  flip` before **and** after, and the only difference is that `EnsureWorldPakReady__5CMainFUi` now
  appears as a new row in the permutation (it was absent because the function did not exist). The
  permutation is **pre-existing** and this change does not worsen it.
* **`unit_fit.sh` residue, measured before and after** (`MetroidPrime/main.cpp`): **20 functions /
  1708 bytes -> 21 functions / 1804 bytes** of COMDATs we emit and retail's object does not. The
  one my change adds is
  `destroy<rstl::pointer_iterator<rstl::pair<rstl::string,SObjectTag>, rstl::vector<...>>>`,
  96 bytes, instantiated by `names.begin()` / `names.end()`. It is the harmless cause `unit_fit`
  describes - a weak template COMDAT neither linker keeps - and this unit is `.text` **SHORT by
  8844 bytes** and 43/99, so it cannot flip on this or any other count. Worth knowing for a future
  `flip` attempt on this unit: the residue is not zero and not going to be.
* **A `progress` item's target must already be a unit in the branch head's baseline** -
  `progress-cmainasyncidle-ensureworldpakready`'s section 2 finding, re-confirmed and now paid off.
  This item passes because `MetroidPrime/main` is in `build/goal/judge/report.base.json`.

## 6. `NEW:`

**None.** Everything found here either raised this item's count or is a codegen lesson for
section 3, which is why it is in this file. The remaining 56 functions of `MetroidPrime/main` are
ordinary `progress` work and naming them would only restate the current item's target.

## 7. For the next run on this unit

* `CMain::EnsureWorldPakReady` is **done at 100.00%**; do not re-try it. `CMain::EnsureWorldPaksReady`
  is at 100.00% too. `CMain::AddWorldPaks` is at 96.00% and its remaining gap is documented in the
  comment above it (frame size 0xA0 vs our 0x90, plus a `GetPakFile` return-type decision that is a
  header edit touching four other units - its own item, not a rider).
* The **flip** for `MetroidPrime/main` is out of reach and that is correct: `.text` is SHORT by
  8844 bytes (43 of 99 functions matched), `.sbss` is over by 13, `.ctors` is empty, the decl order
  is permuted, and 21 COMDATs are extra. A carve is the only route and it wants a matching split.