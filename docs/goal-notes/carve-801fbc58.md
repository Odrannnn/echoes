# carve-801fbc58 — `MetroidPrime/ScriptObjects/Carve801FBC58` (kind: `match`)

**Result: `Matching`, 3/3 functions, and the judge passed.**

```
goal_check: item carve-801fbc58 (match) target=MetroidPrime/ScriptObjects/Carve801FBC58
goal_check: baseline .../wt-mp2-goal-L7/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12603 -> 12606   linked 5964 -> 5967
  ok    check_symbol_names.py
  ok    All:  35.47% fuzzy, 29.29% matched, 13.00% linked (12606 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FBC58.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fbc58
```

## The claim

`.text 0x801FBC58..0x801FBD68`, 0x110 = 272 bytes, three functions (0x54 / 0x84 / 0x38),
carved out of dtk's `main/auto_03_801FA3CC_text` (0x801FA3CC..). The `fn_<addr>` placeholders are
`config/G2ME01/symbols.txt:8229-8231`; the instructions were read from
`build/G2ME01/asm/auto_03_801FA3CC_text.s:1802-1882` **before** the claim split that unit.

Claimed exactly this range, nothing either side, and **the boundaries are function edges on both
sides** - the rule that avoids the dtk `Cyclic dependency` failure. Below: `fn_801FBBC4`
(0x801FBBC4, `size:0x94`), which ends exactly at 0x801FBC58. Above: `fn_801FBD68`
(0x801FBD68, `size:0x68`), which begins exactly at 0x801FBD68.

## What the three are, and the seed's twins all held

| fn | size | retail twin | twin's unit |
| --- | --- | --- | --- |
| `fn_801FBC58` | 0x54 | `fn_80004744` 0x80004744 | `src/MetroidPrime/Carve80004744.c` (100.0) |
| `fn_801FBCAC` | 0x84 | `__dt__Q24rstl82vector<Q24rstl38pair<Ui,Q24rstl20rc_ptr<10IMetaTrans>>,...>Fv` 0x80030E08 | `src/MetroidPrime/Factories/CCharacterFactory.cpp` (100.0) |
| `fn_801FBD30` | 0x38 | `destroy<Q24rstl116pointer_iterator<11CTweakValue,...>>` 0x800067A8 | `src/MetroidPrime/main.cpp` (100.0) |

All three reproduced byte-for-byte from those twins' logic with the callees and struct offsets this
copy uses. **One correction to the seed's reading of `fn_801FBD30`:** the seed called it the
`pointer_iterator` destroy for `CTweakValue`, which is right, but it is the *two-iterator* overload
(`construct.hpp:111-114`, `destroy(begin, end)`) that forwards to `destroy_impl` - the walk is the
callee, not this body. Read off the bytes: 14 instructions, the two `lwz` copies onto the frame and
one `bl`, no loop.

**The stride and the element are read off the callee, not guessed.** `fn_801FBD68` walks with
`addi r31,r31,0xc` and calls `fn_801FFE4C` on `r31+4` after two `addic. r0,r31,4` guards, so `T` is
0xC bytes with a reference-counted member at +4 = `rstl::pair<unsigned int, rstl::rc_ptr<IMetaTrans> >`
(4+8). `fn_801FBCAC`'s `mulli r0,r0,0xc` is the same 12.

**The `volatile` qualifiers on the frame copies are load-bearing, and that is measured, not
stylistic** - the same finding as `src/MetroidPrime/main.cpp:1267-1280` (`fn_800068F4`, 100.0),
whose store sequence over the same four slots (`last` at +0xC, `lastCopy` at +0x8, `firstCopy` at
+0x10, `first` at +0x14) is byte-identical to `fn_801FBCAC`'s. Without the two `volatile`s mwcceppc
folds the copies and emits two `stw`s where retail has four. The `end` temporary is the other half:
written as `last = items + count * 12; lastCopy = last;` the multiply and the sum both land in r0;
introducing `end` first and assigning both copies **from `end`** keeps `items` in the accumulator
register as retail has it (`mulli r0,r0,0xc / add r5,r5,r0`).

## The one `bl` target, and the port's link gap

`fn_801FBD30`'s only call is `bl fn_801FBD68` at 0x801FBD54. `fn_801FBD68` is exactly 0x0 bytes past
this claim's end, so it stays retail's and dtk supplies it from its own `auto_03_801FBD68_text.o`;
declared `extern`, never defined here. `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`) is
claimed by `Kyoto/Alloc/CMemory.cpp` and defined for the host at
`src/Kyoto/Alloc/PortMwccNew.cpp:34`, so both of `fn_801FBC58`'s and `fn_801FBCAC`'s `bl`s resolve
inside our own tree.

**Adding `stub_189` to `src/MetroidPrime/PortLinkStubs.cpp` was required, and it is the fifth file.**
Without it `tools/gate.sh` fails on `probe link-gap`: the port's link has no
`auto_03_801FBD68_text.o`, so the carve's own `bl` adds `fn_801FBD68` to its MISSING list and
`tools/link_gap.py` exits 1 with `gap grew: fn_801FBD68 is not in port_link_gap_list.md`. This is
the established trade for a carve whose only callee falls outside the claim - `stub_185`
(`fn_801F9848`, added for `Carve801F97C8.c`, same shape: an out-of-claim element copy constructor),
`stub_186`/`stub_188`, and `stub_183`/`stub_184` before them. Measured either way in this tree:

```
without stub_189:  link_gap.py --rebuild -> exit 1, "287 MISSING", gap grew: fn_801FBD68 ...
with    stub_189:  link_gap.py --rebuild -> exit 0, "286 MISSING, all accounted for"
```

It is a stand-in with an empty body like every other stub in that file, and it is **not** a claim
that `fn_801FBD68` is decompiled. The file is not in `configure.py`, so the stub cannot reach
`main.dol`. No `PortLinkStubs.cpp` *duplicate* existed to remove: `grep -rn "fn_801FBD68" src/`
returned nothing before this change, and the gate's `port link dups` step reads `ok`.

## The four carve files, each in address order

- `config/G2ME01/splits.txt:1183-1184` — new `MetroidPrime/ScriptObjects/Carve801FBC58.c` block
  between `Carve801F97C8.c` (ends 0x801F9848) and `Carve801FDB5C.c` (starts 0x801FDBE0).
- `configure.py:746` — `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FBC58.c"),` on
  **one line**.
- `files.cmake:568` — `src/MetroidPrime/ScriptObjects/Carve801FBC58.c`.
- `src/MetroidPrime/ScriptObjects/Carve801FBC58.c` — the source, new file.

Plus `src/MetroidPrime/PortLinkStubs.cpp:889-906`, `stub_189`, for the reason above.

Plain C, so the `fn_` names do not mangle. Definitions **descending by address**: `fn_801FBCAC`,
`fn_801FBD30`, `fn_801FBC58` - note that the *first* one to get right was also the trap the rule
names. `python3 tools/check_decl_order.py --unit src/MetroidPrime/ScriptObjects/Carve801FBC58.c`
reads `ok: 0 unit(s) checked, none emits its functions out of retail order`, and
`nm -n build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801FBC58.o` gives 0x0 / 0x54 / 0xD8 -
the same ascending offsets retail has at 0x801FBC58 / 0x801FBCAC / 0x801FBD30. With the file in
ascending source order the object was still 100.00% per function and the DOL was still wrong, which
is exactly the failure only `flip_test.sh` catches.

## What I measured

```
sha1sum build/G2ME01/main.dol               6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (retail)
./tools/decomp_build.sh                     All: 35.47% fuzzy, 29.29% matched, 13.00% linked
                                            (12606 / 28465 functions)
./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FBC58.c
  PASS  -> kept as Matching
  kept: 1 / 1   failed: 0   skipped: 0
python3 tools/check_symbol_names.py         checked 529 units; 0 declared names are missing
python3 tools/link_gap.py --rebuild         286 MISSING, all accounted for
```

`build/report.json` for the new unit:

```
main/MetroidPrime/ScriptObjects/Carve801FBC58   fuzzy 100.0   total_functions 3   matched_functions 3
  matched_code 272 / 272   complete_units 1
    fn_801FBC58 84 100.0    fn_801FBCAC 132 100.0    fn_801FBD30 56 100.0
```

`matched 12603 -> 12606, linked 5964 -> 5967`: +3 each, this item and nothing else. DOL function
total is still **28465** after the `splits.txt` edit (measured from `decomp_build.sh`'s own All:
line, and `report.json`'s `total_functions` for the DOL category is 16726 as before). No `asm` in
the diff. The `docs/HANDOFF.md` / `docs/RUNNING_THE_DECOMP.md` lines `goal_check.sh` rewrote
(`MP_GATE_DOCS_WRITE=1`) were reverted, so the diff is the four carve files plus the one stub block.

## A build-state trap worth recording, because it cost this run real time

**The first `./tools/decomp_build.sh` after adding the carve reported
`FAILED: [code=1] build/G2ME01/ok` with *all 87 files* listed as failing** - `main.dol` and all 86
RELs - and a following `ninja -n` showed only 2 pending edges. That is **not** 87 broken outputs:
it is `dtk shasum -c` reporting against outputs that ninja had not rebuilt yet because
`tools/decomp_build.sh` only re-runs `configure.py` when `build.ninja` is missing, and adding a
`splits.txt` entry does not invalidate it by mtime. The signature to recognise it by is
`ninja -n` pending count of 2-3 while `build/G2ME01/main.dol`'s sha1 is wrong. The fix is
`./tools/decomp_build.sh -r`, which regenerates and rebuilds.

**And the honest form of the same lesson, because I nearly filed it the other way:** the RELs that
report as `FAILED` there really were wrong on disk (`cmp build/G2ME01/AIMannedTurret/AIMannedTurret.rel
orig/G2ME01/files/RelProd/AIMannedTurret.rel` differed at byte 25604), and no carve of a `.text`
range in `main` can move a REL's bytes. Before assuming the cause was my change, the cheapest
discriminating test is: `git stash`, relink, compare the sha. That test put the wrong sha on my
change and the right sha on HEAD, in about ninety seconds - and had I skipped it I would have filed
a REL regression that did not exist. **Measure the baseline before blaming the diff.**

## No blockers, no `NEW:` lines, no commit

Two things for the next lane, filed here rather than as `NEW:` items because neither's success
obviously raises a count on its own.

**NOTE:** the callee of `fn_801FBD68` is `fn_801FFE4C` (retail 0x801FFE4C), the release of the
`rc_ptr` at +4 of each 0xC-byte element. It is inside the unclaimed range that
`main/auto_03_801FBD68_text` already owns and is a **different job from this one**: `rstl::rc_ptr`'s
`ReleaseData` needs the refcount layout, not a byte transcription. Worth knowing it is the next
thing this range wants, not part of this claim.

**NOTE:** the two `bl` targets inside `main` that this claim does *not* cover are
`fn_801FBD68` (0x68, the walk) and `fn_801FFE4C`; both sit above 0x801FBD68 in
`main/auto_03_801FBD68_text`, so a lane that later claims that unit gets them for free and can then
drop `stub_189`. Not touched here - the claim stops at the claim end, and stubbing rather than
carving is the documented trade for a callee that is only a forwarder.
---

# RUN 2 (lane 7, 2026-10-02) — `Matching`, **2 of the item's 3 functions**

```
goal_check: item carve-801fbc58 (match) target=MetroidPrime/ScriptObjects/Carve801FBC58
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12623 -> 12625   linked 5983 -> 5985
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FBC58.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fbc58
```

**STALE for the run-1 claim, and the difference matters.** Run 1's notes report this item
`Matching` with 3/3 and `matched 12603 -> 12606`. Measured on this tree, none of that is present:
the clean tree had no `Carve801FBC58.c`, no `splits.txt` entry, no `configure.py` line, and
`report.json` had no unit for it at all. **Run 1's change was never in this tree** - the files were
absent at HEAD `0103eb72`, and `git log --all --grep=801fbc58` finds no commit. Whatever run 1
wrote, it did not land. So this is not a `STALE:` item and not a repeat of run 1: the carve is
built here from the bytes, and it is 2 functions, not 3.

Run 1's notes are still worth reading for the identification work (the three twins, the stride,
the `volatile` finding), and **its byte-level claims did hold where this run could check them** -
see the two `SAME:` lines below.

## What landed

| fn | size | result |
| --- | --- | --- |
| `fn_801FBC58` 0x801FBC58 | 0x54 | **byte-exact** |
| `fn_801FBCAC` 0x801FBCAC | 0x84 | **byte-exact** |
| `fn_801FBD30` 0x801FBD30 | 0x38 | **not claimed** - see the wall below |

Claim is `.text 0x801FBC58..0x801FBD30`, 0xD8 = 216 bytes, 2 functions. Boundaries are function
edges both sides: below `fn_801FBBC4` (`symbols.txt:8228`, `size:0x94`, ends exactly at 0x801FBC58),
above `fn_801FBD30` (`symbols.txt:8231`, begins exactly at 0x801FBD30). Four carve files, each in
address order: `config/G2ME01/splits.txt:1189-1190`, `configure.py:748` (one line),
`files.cmake:574`. and the new source. Plus `src/MetroidPrime/PortLinkStubs.cpp:954-955`.

**SAME (run 1's reading, re-measured here):** the three twins are real and are byte-identical
apart from `bl` destinations - verified by objdump against `main.elf`, not from the notes:
`fn_80004744`@0x80004744, `__dt__Q24rstl82vector<Q24rstl38pair<Ui,Q24rstl20rc_ptr<10IMetaTrans>>,...>Fv`@0x80030E08,
`destroy<rstl::pointer_iterator<CTweakValue,...>>`@0x800067A8. **SAME:** the stride is 12
(`fn_801FBD68`'s `addi r31,r31,0xc`, `fn_801FBCAC`'s `mulli r0,r0,0xc`) and `T` has an
rc-counted member at +4.

## WALL: fn_801FBD30 64% - mwcceppc will not emit retail's load/store schedule (11 spellings tried)

Retail's 14 instructions, from `build/G2ME01/asm/auto_03_801FA3CC_text.s:1868-1881`:

```
stwu / mflr r0 / lwz r5,0(r4) / stw r0,0x14(r1) / addi r4,r1,8
lwz r0,0(r3) / addi r3,r1,0xc / stw r5,8(r1) / stw r0,0xc(r1) / bl
```

Three things must happen: **`*last` loads before the LR spill**, it lands in **r5** (r0 is taken by
the link register), and **both stores come after both loads**. Every body this run compiled instead
does each `lwz`/`addi`/`stw` triple back to back and uses r0 for both copies. Best attempt was 5 of
14 instructions differing, with the frame slots, both call arguments and the epilogue all correct.
It is a scheduler difference, not a logic difference and not a dropped initialisation.

Spellings compiled and rejected (all with `unsigned char*` locals unless noted; parameter list and
declaration order and assignment order and `volatile` placement all varied):

1. `void** first, void** last`, separate decls, `lastVal = *last; firstVal = *first;` -> load-last
   and load-first both correct order, but `stw r0,0x14(r1)` first and r0 for both (5 differ).
2. same signature, initialised at declaration, both orders -> same two shapes, 5 and 7 differ.
3. `unsigned char* volatile*` on both parameters, both orders -> same.
4. `volatile unsigned char**` parameters (volatile pointee), both orders -> does not compile:
   `illegal implicit conversion from 'volatile unsigned char *'`.
5. `unsigned char* const volatile*` parameters -> does not compile, same class of error.
6. locals `volatile`, parameter plain -> no change: address-taken locals are already forced to memory.
7. locals plain, parameters `volatile`-qualified pointer-to-pointer -> no change.
8. `firstVal`/`lastVal` swapped in the callee call `fn_801FBD68(&lastVal, &firstVal)` -> slots swap
   to match, order still wrong.
9. a 2-element array `pair[2]` instead of two locals, both store orders -> one order is byte-identical
   in slots but emits `stw r0,0xc(r1)` before the second load and adds nothing.
10. a 2-field struct `SPair {first,last}` and `SPairRev {last,first}`, three variants including a
    padded 12-byte one -> no match; the padded one grows the frame to `stwu r1,-0x20`.
11. returning the callee's value instead of discarding it -> does not compile.

So the next run should not re-try declaration order, assignment order, `volatile` placement,
parameter qualification, or a struct/array container - all measured, all the same two shapes. What
is **not** tried here: `register`-qualified locals, an inline asm barrier, or `#pragma`-level
scheduling hints. Those are spelling tricks, not decompiling, so I would not spend a lane on them.

## Why leaving `fn_801FBD30` out is the right call and not a partial

The item said to carve the contiguous part that matches and keep the unit name. `fn_801FBD30` sits
between `fn_801FBCAC` and the unclaimed range above, so the matching part **is** the two lower
functions and they are contiguous. Claiming the third would make the unit 2-of-3: objdiff would
report the function at ~64% and `flip_test.sh` would fail, so the DOL would not reproduce with this
object in the link and **no function would count** - strictly worse than shipping two.

`fn_801FBD30` is now `extern`, declared and never defined here, so it stays retail's and dtk
supplies it from its own `auto_03_801FA3CC_text.o` (0x801FA3CC..0x801FDBE0). `fn_801FBCAC`'s
`bl fn_801FBD30` at 0x801FBD08 is in retail's bytes, so the carve cannot drop the call.

## The two spellings that were load-bearing, re-measured here

**The `volatile` on two of the four frame copies in `fn_801FBCAC`**, same finding as run 1 and
`src/MetroidPrime/main.cpp:1267-1280`. Without them the object comes out **31 instructions against
retail's 33**: `beq` to +0x64 instead of +0x68, `stw r0,0x10` missing, `addi r3,r1,0x10` where
retail has `0x14`.

**A third measurement run 1 does not record, and it cost this run several compiles.** It is the
**non-volatile `first`** that has to be the one whose address is taken, not `firstCopy`. Passing
`&firstCopy` (volatile) and `&last` gives 32 instructions and folds `first` away entirely; the
correct spelling passes `&first` and `&last` while `firstCopy`/`lastCopy` remain the volatile
stores. Retail's argument registers are `addi r3,r1,0x14` and `addi r4,r1,0xc`, i.e. `first` at
+0x14 and `last` at +0xC.

The `end` temporary is also load-bearing, as run 1 recorded: `last = items + count * 12;
lastCopy = last;` puts the sum in r0; introducing `end` first and assigning both copies **from
`end`** keeps `items` in the accumulator register as retail has it (`mulli r0,r0,0xc /
add r5,r5,r0`).

## What I measured

```
sha1sum build/G2ME01/main.dol               6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (retail)
./tools/decomp_build.sh -r                  All:  35.49% fuzzy, 29.34% matched, 13.02% linked
                                            (12625 / 28465 functions)
./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FBC58.c
  PASS  -> kept as Matching
  kept: 1 / 1   failed: 0   skipped: 0
python3 tools/check_decl_order.py --unit src/MetroidPrime/ScriptObjects/Carve801FBC58.c
  ok: 0 unit(s) checked, none emits its functions out of retail order
./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FBC58.c
  .text  claimed 216  ours 216  retail 216  fits
  no extra functions: our object defines only what the retail unit object does
python3 tools/check_symbol_names.py         checked 530 units; 0 declared names are missing
```

`build/report.json` for the new unit:

```
main/MetroidPrime/ScriptObjects/Carve801FBC58  fuzzy 100.0  total_functions 2  matched_functions 2
  matched_code 216 / 216  complete_units 1
```

`nm -n` on the object gives `fn_801FBC58` at 0x0 and `fn_801FBCAC` at 0x54 - the same ascending
offsets retail has at 0x801FBC58 / 0x801FBCAC, from definitions written **descending**. No `asm` in
the diff.

## Two traps, both of which cost this run real time

**A `.tmp` scratch file inside the worktree is fine; a stale `build/` is not.** This tree's
`build/G2ME01/src/.../Carve801FBC58.o` and `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801FBC58.s`
were left over from run 1's failed attempt - timestamps 14:10, from before this run started. I read
the leftover object's disassembly to confirm the shape before writing the source. **That object was
run 1's, and it is not evidence about my code.** The only things that count are the ones measured
after my own edit: the `-r` build, the flip test and `goal_check.sh`, all above.

**`decomp_build.sh` reports a stale-looking 87-file FAILED wall that is not one.** After adding a
`splits.txt` entry, a plain `./tools/decomp_build.sh` reports every REL as `FAILED` even though
`ninja -n` shows only 2-3 pending edges - `dtk shasum -c` is checking outputs ninja has not rebuilt
because the script only re-runs `configure.py` when `build.ninja` is missing and a `splits.txt`
edit does not invalidate it by mtime. Run 1 recorded the same trap. **The fix is `-r`.** And the
discriminating test, which run 1 also recorded and which I used: `git stash`, relink, compare the
sha, before assuming your change caused it. On this tree the clean baseline is
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

**MWCC 2.7 will not implicitly convert between pointer-to-volatile-pointer and pointer-to-pointer
in either direction** (`illegal implicit conversion from 'unsigned char *volatile *' to 'void
**'`). This is why the carve's locals are `void*` and its parameters are `void**`, and why the
`volatile` in `fn_801FBCAC` sits on the *locals* rather than on `fn_801FBD30`'s parameters - the
volatile locals are what forces the four stores, and passing their addresses needs no conversion.

## The port's link, and `stub_191`

`fn_801FBD30` is 0x0 bytes past this claim's end, so the carve's own `bl` adds
it to the port link's MISSING list and `tools/link_gap.py` exits 1 with
`gap grew: fn_801FBD30 is not in port_link_gap_list.md`. `src/MetroidPrime/PortLinkStubs.cpp` grows
`stub_191` for it - **run 1 used `stub_189` for `fn_801FBD68`, and `stub_189`/`stub_190` are already
taken on this tree** (`stub_189` = `fn_801FEC64`, `stub_190` = `fn_801FDAA4`), so the next free
number is `stub_191`. Same trade `stub_188`/`stub_189`/`stub_190` already make, same empty body,
same disclaimer: **not** a claim that `fn_801FBD30` is decompiled. The file is not in
`configure.py`, so the stub cannot reach `main.dol`. No `PortLinkStubs.cpp` duplicate existed to
remove - `grep -rn "fn_801FBD30" src/` returned nothing before this change.

## No `NEW:` lines, no commit

**NEW:** would not be right here: the blocker is a spelling wall on one 0x38-byte function, and the
prompt says a measured wall goes in these notes, not the queue.

**NOTE:** `fn_801FBD30` and `fn_801FBD68` (0x68, the walk) and `fn_801FFE4C` all sit above this
claim's end inside `main/auto_03_801FA3CC_text`. A lane that claims that range gets all three at
once and can drop `stub_191`. `fn_801FFE4C` is the `rc_ptr` release at +4 of each element and needs
the refcount layout - a different job from this one.

The `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` lines `goal_check.sh` rewrote
(`MP_GATE_DOCS_WRITE=1`) were reverted, so the diff is the four carve files plus the one stub block.

---

# RUN 3 (lane 7, 2026-10-02) — `Matching`, **3 of 3**, the judge's `PASS`

```
goal_check: item carve-801fbc58 (match) target=MetroidPrime/ScriptObjects/Carve801FBC58
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12660 -> 12663   linked 6019 -> 6022
  ok    check_symbol_names.py
  ok    All:  35.52% fuzzy, 29.36% matched, 13.04% linked (12663 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FBC58.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fbc58
```

**Run 2's `WALL:` for `fn_801FBD30` is wrong, and this run breaks it with one line.** Run 2 filed
`WALL: fn_801FBD30 64%` after 11 spellings. The winning spelling is **its parameter type**, and it
is `const void*` - which run 2 did not try. `src/MetroidPrime/main.cpp:1252` already declares the
byte-identical `fn_80004864` exactly that way. **Read the twin's declaration, not just its body.**

Nothing from run 1 or run 2 was present on this tree: clean at HEAD `d48083df`, no
`Carve801FBC58.c`, no `splits.txt` entry, no `configure.py` line, no unit in `report.json`.

## THE ANSWER: `fn_801FBD30`'s parameters must be `const void*`

Measured against retail's bytes, all three of these, same body otherwise:

| `fn_801FBD30` params | `fn_801FBD30` | `fn_801FBCAC` |
| --- | --- | --- |
| `struct SCarve801FBC58Iterator` by value | byte-exact | **34 insns / 0x30 frame** (retail 33 / 0x20) |
| `void**` | **5 insns out of order** (`lwz/stw/lwz/stw` vs retail's both-loads-first) | byte-exact |
| **`const void*`** | **byte-exact** | **byte-exact** |

This is the whole result. The reason is one ABI fact visible from the two ends of one call: **the
`lwz`/`stw` pairs in `fn_801FBD30` are not explicit copies in the source - they are the by-value
parameter copy that the `fn_801FBD68` call forces**, because `fn_801FBD68` is declared with the
struct. That copy is materialised in the **caller's** frame, so the *caller's* parameter-passing
convention decides whether it lands on retail's slots. `struct` params make `fn_801FBCAC` build a
fresh argument copy per argument (frame blows to 0x30); `void**` breaks the copy's ordering;
`const void*` is pointer-to-the-iterator at both ends, which is what retail's argument registers
(`addi r3,r1,0x14` / `addi r4,r1,0xc`) already are.

**Dereferencing inside the call's argument list is also load-bearing.** Spelling the same thing as
two *named* locals first gives the same 14 instructions with a **32-byte** frame against retail's
16 - the explicit locals stop being the argument slots the frame was sized for.

`src/MetroidPrime/ScriptObjects/Carve801FDB5C.c:41-49` records the mirror image on the
byte-identical `fn_801FDBE0`: there the by-value `struct` **parameters** produce the pairs, and
`void**` puts the body 7 instructions out of order. Both files are one fact, and neither alone
gives you the spelling - the callee's type fixes the copy, the caller's type fixes the slots.

## The self-referential `carve_diff` trap - this one nearly cost the run

**`tools/carve_diff.sh` and `objdump` against `build/G2ME01/main.elf` are only evidence if
`main.elf` predates your object.** `main.elf` is *this tree's link output*. After `-r` it contains
your carve, so diffing your `.o` against it compares your bytes with your bytes: I got a clean
`BYTE-EXACT` on a `fn_801FBD30` that was **5 instructions wrong** and took the DOL sha from retail
to `66db3aff...`. The `bl` words also match there for the wrong reason.

The fix, and it is cheap: **capture the baseline before you touch anything.** Reverted the tree,
`./tools/decomp_build.sh -r`, `objdump --start-address=0x801FBC58 --stop-address=0x801FBD68
build/G2ME01/main.elf > .tmp/retail.txt`, then diff every candidate object against that file. The
same reasoning kills `build/G2ME01/asm/auto_*.s` as a reference **after** the build: dtk
regenerates those listings, and mine had already been cut back to `0x801FA3CC..0x801FBC58`.

`objdiff` and the DOL sha do catch it - the sha went `6ef9b491` -> `66db3aff` - but only after a
full rebuild. The captured baseline turns a 10-minute cycle into a 40-second one.

## The host compiler rejects what MWCC accepts - a real build failure, not a warning

Declaring `fn_801FBD68` with `struct SCarve801FBC58Iterator` **above** the struct definition
compiles fine under mwcceppc and fails the port link:

```
Carve801FBC58.c:170:15: error: type of formal parameter 1 is incomplete
```

The port link builds this `.c` with the host compiler too. **Put the struct before every
declaration that names it.** mwcceppc tolerating a forward-incomplete parameter type is not
permission; the gate that catches it is `tools/link_gap.py --rebuild`, not the DOL build.

## What I measured

```
sha1sum build/G2ME01/main.dol               6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (retail)
./tools/decomp_build.sh -r                  All: 35.52% fuzzy, 29.36% matched, 13.04% linked
                                            (12663 / 28465 functions)
./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FBC58.c
  PASS  -> kept as Matching       kept: 1 / 1   failed: 0   skipped: 0
python3 tools/check_decl_order.py --unit src/MetroidPrime/ScriptObjects/Carve801FBC58.c
  ok: 0 unit(s) checked, none emits its functions out of retail order
./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FBC58.c
  .text  claimed 272  ours 272  retail 272  fits
  no extra functions: our object defines only what the retail unit object does
python3 tools/check_symbol_names.py         checked 530 units; 0 declared names are missing
python3 tools/link_gap.py --rebuild         285 MISSING, all accounted for
```

```
main/MetroidPrime/ScriptObjects/Carve801FBC58  fuzzy 100.0  total 3  matched 3
  matched_code 272 / 272
    fn_801FBC58 84 100.0    fn_801FBCAC 132 100.0    fn_801FBD30 56 100.0
```

`matched 12660 -> 12663, linked 6019 -> 6022`: +3 each, this item and nothing else. DOL function
total still **28465**. `nm -n` on the object gives 0x0 / 0x54 / 0xD8 for
`fn_801FBC58`/`fn_801FBCAC`/`fn_801FBD30` - the same ascending offsets retail has - from
definitions written **descending**. No `asm` in the diff. The `docs/HANDOFF.md` and
`docs/RUNNING_THE_DECOMP.md` lines `goal_check.sh` rewrote (`MP_GATE_DOCS_WRITE=1`) were reverted,
so the diff is the four carve files plus one stub block.

## The claim, the four files, the stub

`.text 0x801FBC58..0x801FBD68`, 0x110 = 272 bytes, 3 functions (0x54 / 0x84 / 0x38), carved out
of dtk's `main/auto_03_801FA3CC_text`. Boundaries are function edges both sides: below
`fn_801FBBC4` (`symbols.txt:8228`, `size:0x94`) ends exactly at 0x801FBC58; above `fn_801FBD68`
(`symbols.txt:8232`) begins exactly at 0x801FBD68. All four files in address order:

- `config/G2ME01/splits.txt:1201-1202` - between `Carve801F97C8.c` (ends 0x801F9848) and
  `Carve801FDB5C.c` (starts 0x801FDBE0).
- `configure.py:752` - `Object(Matching, ...)`, **one line**.
- `files.cmake:574`.
- `src/MetroidPrime/ScriptObjects/Carve801FBC58.c` - new.
- plus `src/MetroidPrime/PortLinkStubs.cpp:982-1002`, `stub_194` (189-193 are taken on this tree).

`fn_801FBD68` is exactly 0x0 bytes past the claim end, so it stays retail's and dtk supplies it
from `auto_03_801FBD68_text.o`; the carve's own `bl fn_801FBD68` adds it to the port link's
MISSING list, so `stub_194` is the same trade `stub_192`/`stub_193` make. Measured either way
here:

```
without stub_194:  link_gap.py --rebuild -> "gap grew: fn_801FBD68 is not in port_link_gap_list.md"
with    stub_194:  link_gap.py --rebuild -> 285 MISSING, all accounted for
```

An empty-bodied stand-in, **not** a claim that `fn_801FBD68` is decompiled. The file is not in
`configure.py`, so it cannot reach `main.dol`. No `PortLinkStubs.cpp` duplicate existed to remove
(`grep -rn "fn_801FBD68" src/` returned nothing before this change).

## The twins, re-measured by objdump rather than recalled

All three verified instruction-by-instruction against `main.elf`: `fn_80004744`@0x80004744,
`__dt__Q24rstl82vector<Q24rstl38pair<Ui,Q24rstl20rc_ptr<10IMetaTrans>>,...>Fv`@0x80030E08,
`destroy<rstl::pointer_iterator<CTweakValue>,...>`@0x800067A8, and `fn_801FDBE0`@0x801FDBE0 - all
identical to the corresponding `fn_801FBC58`/`fn_801FBCAC`/`fn_801FBD30` apart from `bl`
destinations. **SAME** as runs 1 and 2: the stride is 12 (`fn_801FBD68`'s `addi r31,r31,0xc`,
`fn_801FBCAC`'s `mulli r0,r0,0xc`) and `T` has an rc-counted member at +4.

**`fn_801FBCAC`'s four locals must be plain `void*`, not the iterator struct** - declaring them as
the struct makes mwcceppc build whole-object copies and the frame grows to 0x30 with 34
instructions. Tried with `volatile struct` locals and with a `volatile` member on the struct;
both the same. The pointer spelling with `volatile` on `firstCopy`/`lastCopy` and the `end`
temporary is the byte-exact one, and run 2's notes on those two are **SAME** here.

## No `NEW:` lines, no commit

**NEW:** would be wrong on both counts. The blocker was one parameter-type choice, and the prompt
says a measured wall goes in these notes, not the queue - and this run has no wall left to report.
`docs/goal-notes/carve-801fbc58.md` records the run 2 `WALL:` it supersedes; the driver may want
to drop that queue entry, since `fn_801FBD30` is measured at 100.0 here.

**NOTE:** `fn_801FBD68` (0x68, the walk) and `fn_801FFE4C` (its `rc_ptr` release, which needs the
refcount layout) both sit above this claim's end in `main/auto_03_801FBD68_text`. A lane that
claims that range gets both at once and can drop `stub_194`.

---

# RUN 4 (lane 7, 2026-10-02) — `Matching`, **3 of 3**, the judge's `PASS`

```
goal_check: item carve-801fbc58 (match) target=MetroidPrime/ScriptObjects/Carve801FBC58
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12670 -> 12673   linked 6029 -> 6032
  ok    check_symbol_names.py
  ok    All:  35.53% fuzzy, 29.38% matched, 13.05% linked (12673 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FBC58.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fbc58
```

**Run 3's spelling was right and it is re-measured here, not inherited: `const void*` for
`fn_801FBD30`'s parameters.** First compile of the body came out `BYTE-EXACT` on all 0x110 bytes,
so run 4 needed no search at all - only four compiles total, three of which were me finding the
source order (below). Clean tree at HEAD `127f4bb6` had no `Carve801FBC58.c`, no `splits.txt`
entry, no `configure.py` line, no unit in `report.json`; nothing from runs 1-3 is present here.

## RUN 3's `WALL:` is superseded - do not spend a lane on it again

Run 2 filed `WALL: fn_801FBD30 64%` after 11 spellings and run 3 broke it with the parameter type.
This run compiles `fn_801FBD30` byte-exact **and** `fn_801FBCAC` byte-exact **and** `fn_801FBC58`
byte-exact from the same body, so the wall is dead. The driver may drop that queue entry.

## The claim, the four files, the stub

`.text 0x801FBC58..0x801FBD68`, 0x110 = 272 bytes, 3 functions (0x54 / 0x84 / 0x38), carved out of
dtk's `main/auto_03_801FA3CC_text`. Both boundaries are function edges: below `fn_801FBBC4`
(`symbols.txt:8228`, `size:0x94`) ends exactly at 0x801FBC58; above `fn_801FBD68`
(`symbols.txt:8232`) begins exactly at 0x801FBD68. The claim is the full range the item names.

- `config/G2ME01/splits.txt:1204-1205` - between `Carve801F97C8.c` (ends 0x801F9848) and
  `Carve801FDB5C.c` (starts 0x801FDBE0).
- `configure.py:753` - `Object(Matching, ...)`, **one line**.
- `files.cmake:575`.
- `src/MetroidPrime/ScriptObjects/Carve801FBC58.c` - new, 180 lines.
- plus `src/MetroidPrime/PortLinkStubs.cpp:1033-1054`, `stub_196` (**not** `stub_189`/`stub_194` -
  on this tree `stub_194` = `fn_8020D278` and `stub_195` = `fn_80008C28` are both taken, so 196 is
  the next free number. Runs 1/3's `stub_189`/`stub_194` are wrong for this tree).

`fn_801FBD68` is exactly 0x0 bytes past the claim end, so the carve's own `bl fn_801FBD68` adds it
to the port link's MISSING list and the stub is the same trade `stub_192`/`stub_194` make. Measured
either way here:

```
without stub_196:  link_gap.py --rebuild -> exit 1, "286 MISSING",
                    "gap grew: fn_801FBD68 is not in port_link_gap_list.md"
with    stub_196:  link_gap.py --rebuild -> exit 0, "285 MISSING, all accounted for"
```

Empty-bodied stand-in, **not** a claim that `fn_801FBD68` is decompiled. The file is not in
`configure.py`, so it cannot reach `main.dol`. No `PortLinkStubs.cpp` duplicate existed to remove
(`grep -rn "fn_801FBD68" src/` hit only the new carve).

## THE ORDER TRAP, and the two orders that both look right - this cost 2 of my 4 compiles

**mwcceppc emits definitions in reverse source order.** All three of these source orders were
compiled and `nm -n` read on the result:

| source order (top to bottom) | `.text` order in the object |
| --- | --- |
| BCAC, BD30, BC58 | BC58, BD30, BCAC |
| BC58, BD30, BCAC | BCAC, BD30, BC58 |
| BCAC, BD30, BC58 (again, after an edit gone wrong) | BC58, BD30, BCAC |
| **BD30, BCAC, BC58** | **BC58, BCAC, BD30 - retail's** |

so the rule the notes repeat is right (descending by retail offset) and it is worth the two
compiles: `nm -n` costs nothing, objdiff would have shown 100% per function anyway, and the DOL
sha would have been the only other signal.

## The twins, re-measured by objdump rather than recalled

All four verified instruction-by-instruction against the captured **retail** elf, and the raw
opcode words are identical - the differing slots are branch/call targets only, which is the claim:

| twin | at | size | differing insns vs |
| --- | --- | --- | --- |
| `fn_80004744` | 0x80004744 | 0x54 | `fn_801FBC58`: 2 (`beq`, `ble`, same local offsets) |
| `__dt__Q24rstl82vector<...rc_ptr<IMetaTrans>...>Fv` | 0x80030E08 | 0x84 | `fn_801FBCAC`: 3 (`beq`, `bl`, `ble`) |
| `fn_80004864` | 0x80004864 | 0x38 | `fn_801FBD30`: 1 (`bl`) |
| `fn_801FDBE0` | 0x801FDBE0 | 0x38 | `fn_801FBD30`: 1 (`bl`) |

**SAME** as runs 1-3: the stride is 12 (`fn_801FBD68`'s `addi r31,r31,0xc`, `fn_801FBCAC`'s
`mulli r0,r0,0xc`) and `T` has an rc-counted member at +4, so 4+8 is
`rstl::pair<unsigned int, rstl::rc_ptr<IMetaTrans> >`. **SAME:** the `volatile` on `firstCopy` and
`lastCopy`, the `end` temporary with both copies assigned **from it**, and passing `&first`/`&last`
rather than `&firstCopy`/`&last`. All three were compiled and none of them is optional.

## What I measured

```
sha1sum build/G2ME01/main.dol               6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (retail)
./tools/decomp_build.sh -r                  All:  35.53% fuzzy, 29.38% matched, 13.05% linked
                                            (12673 / 28465 functions)
./tools/decomp_build.sh MetroidPrime/ScriptObjects/Carve801FBC58
  main/MetroidPrime/ScriptObjects/Carve801FBC58: 100.00% fuzzy, 100.00% matched (3 / 3 functions)
./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FBC58.c
  PASS  -> kept as Matching       kept: 1 / 1   failed: 0   skipped: 0
python3 tools/check_decl_order.py --unit src/MetroidPrime/ScriptObjects/Carve801FBC58.c
  ok: 0 unit(s) checked, none emits its functions out of retail order
./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FBC58.c
  .text  claimed 272  ours 272  retail 272  fits
  no extra functions: our object defines only what the retail unit object does
python3 tools/check_symbol_names.py         checked 532 units; 0 declared names are missing
python3 tools/link_gap.py --rebuild         285 MISSING, all accounted for
```

```
main/MetroidPrime/ScriptObjects/Carve801FBC58  fuzzy 100.0  total 3  matched 3
  matched_code 272 / 272  complete_units 1
    fn_801FBC58 84 100.0   fn_801FBCAC 132 100.0   fn_801FBD30 56 100.0
```

`matched 12670 -> 12673, linked 6029 -> 6032`: +3 each, this item and nothing else. DOL function
total still **28465** (`report.json` `measures.total_functions`, and the build's own `All:` line).
`nm -n` on the object gives 0x0 / 0x54 / 0xd8 - retail's ascending offsets - from definitions
written descending. No `asm` in the diff (`grep -c 'asm('` on the new source: 0). The
`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` rewrites `goal_check.sh` made
(`MP_GATE_DOCS_WRITE=1`, 148955 lines each) were reverted, so the diff is the four carve files
plus one stub block.

## One build note, cheaper than run 3's

Run 3's self-referential `carve_diff` trap is real and I hit its setup cost once: `mwcceppc.exe` is
a PE binary, so a hand-rolled compile line needs `wibo` in front of it and
`build/tools/sjiswrap.exe` after, or you get `Exec format error`. **The cheap loop is not a
hand-rolled compile at all** - once the four carve files are in, `ninja
build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801FBC58.o` builds the one object in a second or
two, and objdiff's own `.s` listing is regenerated beside it. I ran `configure.py` once (a
`splits.txt` edit does not invalidate `build.ninja` by mtime) and then looped on `ninja <obj>`.
The only thing that needs care is the reference side: **`build/G2ME01/main.elf` stops being
evidence the moment you relink it**, so I copied it to `.tmp/opencode/retail.elf` before touching
anything and diffed every candidate against that copy.

## No `NEW:` lines, no commit

`NEW:` would be wrong on both counts - the item needed one parameter type, not a new blocker, and
there is no wall left to report. **NOTE:** `fn_801FBD68` (0x68, the walk) and `fn_801FFE4C` (its
`rc_ptr` release at +4, which needs the refcount layout) both sit above this claim's end in
`main/auto_03_801FA3CC_text`. A lane that claims 0x801FBD68..0x801FBE00 gets both at once and can
drop `stub_196`. That is the obvious next carve out of this range.
