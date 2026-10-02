# carve-801fd7d4 - `MetroidPrime/ScriptObjects/Carve801FD7D4` (+ `Carve801FD858`) -> `Matching`

**Kind:** `match` **Target:** `MetroidPrime/ScriptObjects/Carve801FD7D4` **Result: PASS**
(`goal_check: PASS carve-801fd7d4`, verbatim under "Verification").

Six files: the four of two carves plus `src/MetroidPrime/PortLinkStubs.cpp`, whose `stub_230` this
change retires. `gate.sh` rewrote `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` as a side
effect, as it does for every lane; `git checkout --` on exactly those two left the tree at the six
files below.

## What I did

- **`src/MetroidPrime/ScriptObjects/Carve801FD7D4.c`** (new, plain C, 1 function): `fn_801FD7D4`,
  `.text 0x801FD7D4..0x801FD858`, 0x84 = 132 bytes, 33 instructions. The body is
  `ScriptObjects/Carve801FD998.c`'s `Matching` `fn_801FD998` verbatim with `44 -> 36` and the callee
  `fn_801FDA1C -> fn_801FD858`, and **the callee is declared with pointer parameters**
  (`void fn_801FD858(const void*, const void*)`, so `&first`/`&last` go out in r3/r4) - the
  declaration that is the whole trick, measured by the seeder `carve-801fd998`'s third run and used
  again here unchanged.
- **`src/MetroidPrime/ScriptObjects/Carve801FD858.c`** (new, plain C, 2 functions): `fn_801FD890`
  (0x801FD890, 0x50) and `fn_801FD858` (0x801FD858, 0x38), `.text 0x801FD858..0x801FD8E0`,
  0x88 = 136 bytes, declared **descending by address** (fn_801FD890 first). `fn_801FD858` is the
  forwarder with **by-value** `It` parameters (it dereferences r3/r4 itself), `fn_801FD890` is the
  0x24-strided walk calling `fn_801FD8E0`, which `ScriptObjects/Carve801FD8E0.c` (Matching) already
  defines. The two units cannot be one TU: one symbol cannot be declared both ways, which is exactly
  what the item's `reason` predicted.
- **`configure.py:779-780`**, **`config/G2ME01/splits.txt:1386-1391`**, **`files.cmake:586-587`** -
  `Carve801FD7D4.c` and `Carve801FD858.c` between `Carve801FD67C.cpp` and `Carve801FD8E0.c` in all
  three, i.e. three of the four carve files for the two claims, the fourth being each source's own
  claim in its header. `total_functions` is still **28465** after the `splits.txt` edit.
- **`src/MetroidPrime/PortLinkStubs.cpp`** - `stub_230` (`fn_801FD7D4`) retired: the port's link
  takes that symbol from `Carve801FD7D4.c`'s object now, and keeping the stand-in would be a
  duplicate definition. The block comment above `stub_228`/`stub_231` is corrected (it listed
  `fn_801FD858` among the unclaimed inner walks and repeated the "claiming X only moves the gap
  along" argument this carve refutes), and the header numerals move.

## Re-measured on this tree (the `reason` was a starting point, not a measurement)

- `config/G2ME01/symbols.txt:8276` `fn_801FD7D4 = .text:0x801FD7D4 size:0x84`; `:8277`
  `fn_801FD858 size:0x38`; `:8278` `fn_801FD890 size:0x50`; `:8275` `fn_801FD774 size:0x60` (below);
  `:8279` `fn_801FD8E0 size:0x20` (above).
- **The bytes came out of the disc, not out of `build/G2ME01/main.elf`** (which holds our own once
  the units are in the link): `python3 tools/dol_read.py 0x801FD7D4 0x84`,
  `... 0x801FD858 0x38`, `... 0x801FD890 0x50`. Before the carve these were three `.fn` blocks of
  dtk's `build/G2ME01/asm/auto_03_801FD6F0_text.s`, the unit `build/report.json` still calls
  `main/auto_03_801FD6F0_text`.
- **Twins, compared word by word out of the disc this run** (33/14/20 words each):
  - `fn_801FD7D4` vs `fn_801FD998` (`Carve801FD998.c`, Matching): **30 of 33 identical**; the three
    that differ are `+0x30 mulli 36` against `mulli 44`, and the two `bl Free__7CMemoryFPCv` words
    at +0x54 / +0x64 whose displacements differ by **0x1C4** - exactly 0x801FD998 - 0x801FD7D4.
    The `bl` to the inner walk at +0x4C is the **same word** (`48000039`) in both, because each walk
    sits 0x84 past its own destructor.
  - `fn_801FD7D4` vs `fn_801FD6F0` (unclaimed): 30 of 33, `mulli 20`; vs `fn_801FDB5C` (unclaimed):
    30 of 33, `mulli 48`. Nothing else differs in any of the three - that is the whole family.
  - `fn_801FD858` vs `fn_801FDA1C` (`Carve801FDA1C.c`, Matching) and vs `fn_801FDBE0`
    (`Carve801FDB5C.c`, Matching): **14 of 14 identical in both**, `bl` included (`48000015`,
    0x801FD87C + 0x14 = 0x801FD890).
  - `fn_801FD890` vs `fn_801FD5E8` (`Carve801FD5E8.c`, Matching): **20 of 20 identical**, `bl`
    included; vs `fn_801FDA54`: 19 of 20 (stride `0x24` vs `0x2c`); vs `fn_801FDC18`: 19 of 20
    (`0x24` vs `0x30`).
- **The stride 36 = 0x24, measured twice and neither time off the `addi` it fixes**: this unit's own
  `addi r31,r31,0x24` at 0x801FD8B8, and the second retail caller of the forwarder,
  `fn_801FE97C` (0x801FE97C, 0xB0, **unclaimed** - no `splits.txt` entry covers it), whose
  `mulli r0,r30,0x24` at 0x801FE9F0 builds its bound before `bl fn_801FD858` at 0x801FEA08.
- **Callers, by scanning every `bl` in the disc's `.text`** (not by grepping our build):
  `fn_801FD7D4` has exactly one, 0x801FD4F0 inside `fn_801FD4B0` (`addi r3,r30,0x10` / `li r4,-1`,
  the third of that destructor's four teardowns; `Carve801FD4B0.cpp`, Matching) - so the flag
  arrives as MWCC's "destroy, do not free me afterwards", and this is what the port link asks for
  the symbol. `fn_801FD858` has two: 0x801FD820 (`fn_801FD7D4`) and 0x801FEA08 (`fn_801FE97C`).
  `fn_801FD890` has one, 0x801FD87C. `fn_801FD8E0` has two: 0x801FD8B4 (`fn_801FD890`) and
  0x801FF810 (`fn_801FF7EC`, in the Matching `Carve801FF720.cpp`), which is the second, independent
  measurement of the 0x24 element.
- **No gap is spanned.** Below, `fn_801FD774` (0x801FD774 + 0x60) ends exactly at 0x801FD7D4;
  `Carve801FD7D4.c` ends exactly at 0x801FD858 (`fn_801FD858` is at 0x801FD858, `:8277`), and
  `Carve801FD858.c` ends exactly at 0x801FD8E0, where the Matching `Carve801FD8E0.c` starts.
- **The split behaved as it does**: `main/auto_03_801FD6F0_text` keeps `fn_801FD6F0` and
  `fn_801FD774` (5 functions -> 2), and the two new units replace the other three.

## Verification

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
goal_check: item carve-801fd7d4 (match) target=MetroidPrime/ScriptObjects/Carve801FD7D4
goal_check: baseline .../wt-mp2-goal-L4/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13098 -> 13101   linked 6196 -> 6199
  ok    check_symbol_names.py
  ok    All:  37.08% fuzzy, 30.52% matched, 13.47% linked (13101 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FD7D4.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fd7d4
```

Measured directly as well:

- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD7D4.c` - `PASS -> kept as Matching`,
  `kept: 1 / 1  failed: 0  skipped: 0`. The same for `Carve801FD858.c` - **both** units flip, not
  only the item's target.
- `./tools/unit_fit.sh` on both: `Carve801FD7D4.c` `.text claimed 132 ours 132 retail 132 fits`,
  `Carve801FD858.c` `.text claimed 136 ours 136 retail 136 fits`, both with `no extra functions: our
  object defines only what the retail unit object does`.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; the build printed
  `87 files OK` and the `All:` line above. `gate.sh`'s independent re-hash of all 86 RELs against
  `config/G2ME01/config.yml` passed inside the judge.
- `build/report.json`: `matched_functions` 13098 -> **13101** (+3), `complete_units` 834 -> **836**,
  `total_units` 2159 -> 2161 (+2: the dtk auto unit is reduced, not removed), `total_functions`
  **28465** unmoved, `fuzzy_match_percent` 37.07795 -> 37.082047. New units:
  `main/MetroidPrime/ScriptObjects/Carve801FD7D4` 100.0, 132/132 bytes, **1/1** functions;
  `main/MetroidPrime/ScriptObjects/Carve801FD858` 100.0, 136/136 bytes, **2/2** functions.
- **Nothing anywhere got worse**: compared unit by unit against
  `build/goal/judge/report.base.json` on `matched_functions`, `matched_code` and `complete_code`,
  **no unit fell**, and no unit disappeared. The neighbours are unmoved and 100.0:
  `Carve801FD8E0` 68/68 2/2, `Carve801FD998` 132/132 1/1, `Carve801FDA1C` 136/136 2/2,
  `Carve801FD4B0` 124/124 1/1, `Carve801FDAA4` 68/68 2/2, `Carve801FD5E8` 80/80 1/1.
- **Byte comparison against the disc, per word.** `Carve801FD7D4.o` `.text` (file offset 0x40,
  0x84): **30 of 33 words identical**; the three that differ are the three `bl`s reading
  `48000001` where retail has the target, and `objdump -r -j .text` shows exactly those three
  relocations - `R_PPC_REL24 fn_801FD858`, `R_PPC_REL24 Free__7CMemoryFPCv` twice. `nm -S`:
  `T fn_801FD7D4` size 0x84, undefined only those two names, unmangled.
  `Carve801FD858.o` `.text` (0x40, 0x88): **32 of 34 identical**; the two that differ are the two
  `bl`s, with exactly two relocations - `R_PPC_REL24 fn_801FD890` at 0x24 and
  `R_PPC_REL24 fn_801FD8E0` at 0x5c. `nm -S` shows `T fn_801FD858` 0x38 at +0x00 then
  `T fn_801FD890` 0x50 at +0x38 - **ascending in the object, as it must be**, which is the check
  `flip_test.sh` exists for and it passed.
- `python3 tools/check_symbol_names.py` - `checked 584 units; 0 declared names are missing from
  their object`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD7D4` -
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- Port: `build/gate-probe.log` -> `probe: 835 files, 0 failed, 0 errors; link: LINKED (286 undefined,
  0 duplicates)`. **286 is this item's judge baseline** (`build/goal/judge/undef.base.count`), so
  this carve adds nothing to the port's undefined list. `build/gate-link.log` -> `279 MISSING`, and
  `ok: 279 MISSING symbol(s), all accounted for in port_link_gap_list.md` - i.e. **no
  `docs/research/port_link_gap*.md` edit was needed**; the gap set did not change.
  `build/gate-dups.log` -> `unique undefined symbols 286`, `duplicate definitions 0`, which is the
  check that says retiring `stub_230` was necessary rather than optional (with it back the same
  command reports a duplicate of `fn_801FD7D4`).
- `PortLinkStubs.cpp` counts after the retirement, by the terms its own header names:
  `grep -cE 'asm\("'` **200**, `grep -cE '^extern "C" void stub[^ ]*\(\) asm\("'` **190**,
  `^extern "C" char stub_data_` **10**, unmangled `grep -cE 'asm\("(fn_|lbl_)'` **40**. Before the
  change the same terms read **201 / 191 / 10 / 40**.
- The judge's run is on the final tree apart from the two doc reverts, which `git checkout --`
  restores to HEAD.

## Left alone on purpose, and one drift worth recording

- **`PortLinkStubs.cpp`'s header numerals were stale before this change, not because of it**: the
  line read `190 functions, 9 data objects` while the file held **191 / 10 / 201** (measured on
  branch head `c7f9598a`). Per-commit counts: `ee7aa68c` (`match: carve-8000447C`) added two
  function stubs without moving them and `9064a13f` (`match: carve-801feae0`) added a data stub
  likewise; `062db81a` and `c7f9598a` each moved the function numeral without correcting the rest.
  This change restates what the file measures (190 / 10 / 200) because it moves that line itself.
- **The file's own breakdown paragraph** (line ~78) still reads `86 REL loader, 64 game method, 40
  unmangled fn_/lbl_, 1 allocator, 9 vtable/typeinfo` = 200 with `Carve801FD4B0.cpp` as the last
  re-derivation. Its `9` vtable/typeinfo term is the data count, now **10**, and its residual
  `200 - 86 - 40 - 9 - 1 = 64` would now read 63. I did not rewrite that paragraph: this change
  moves the count line, and re-deriving a breakdown whose REL-loader term cannot be grepped is the
  next lane's job with its own measurement. **Recorded here so it is not rediscovered.**
- No `NEW:` line is filed. The neighbours that would (`carve-801fdb5c`, `carve-801fd52c`) were
  already filed by `docs/goal-notes/carve-801fd998.md`'s third run and `carve-801fd6f0` /
  `carve-801fdcac` are already in the queue; re-filing them would cost a lane an hour for nothing.
  What this run measured for them, so the next run does not have to:
  - **`carve-801fdb5c`** (0x801FDB5C, 0x84, stride 48, callee `fn_801FDBE0`, which
    `Carve801FDB5C.c` already defines and which is **14 of 14 words** the same shape as the
    `fn_801FD858` this item matched): this item's `Carve801FD7D4.c` body with `36 -> 48` and
    `fn_801FD858 -> fn_801FDBE0`, declared `void fn_801FDBE0(const void*, const void*)`. Retire
    `stub_228`; no new stand-in.
  - **`carve-801fd6f0`** (0x801FD6F0, 0x84, stride 20, callee `fn_801FD774`): the same body with
    `36 -> 20` and the callee `fn_801FD774`, but `fn_801FD774` (0x801FD774, 0x60 = 24 instructions)
    is **not** the 0x38/0x50 forwarder-plus-walk pair - it dereferences r3/r4 into its own frame
    like the forwarders do, but then walks `addi r30,r30,0x14` with `cmplwi r30,0 / beq` +
    `beq` before `bl 0x802FE9B8` (`~rstl::basic_string`) per element. Carving `fn_801FD6F0` alone
    therefore retires `stub_227` and needs one stand-in for `fn_801FD774`; net zero for the port.
  - **`fn_801FE97C`** (0x801FE97C, 0xB0, unclaimed) is the second retail caller of `fn_801FD858` and
    is a vector resize: it needs `fn_801FF720`'s claim boundary checked and `fn_801FEA2C`
    (0x801FEA2C, 0x6C, unclaimed), so it is not a cheap carve and is not filed.
---

# Run 2 (this one) - re-run on `goal/lane-4` at `3367405b`

**Result: PASS** again (`goal_check: PASS carve-801fd7d4`, verbatim under "Verification").

**Not `STALE:` - measured, not assumed.** Run 1's diff was not on this tree when I started: `git
status --porcelain` was empty, HEAD was `3367405b match: carve-801fd52c` (two commits past the
`c7f9598a` run 1 measured), and **neither** `src/MetroidPrime/ScriptObjects/Carve801FD7D4.c` nor
`Carve801FD858.c` existed, `grep -n Carve801FD configure.py` had no `7D4` or `858` line,
`config/G2ME01/splits.txt` had no claim between `Carve801FD67C.cpp` and `Carve801FD8E0.c`, and
`build/report.json` listed no such unit (`main/MetroidPrime/ScriptObjects/Carve801FD7D4` absent) with
`main/auto_03_801FD6F0_text` still holding **5** functions.  So the carve was redone from the tree,
not reapplied from run 1's notes.  The baseline also moved: this tree reads **13101 / 836 /
2159 / 37.080948** before the change where run 1 read 13098 / 834 before it, so run 1's "after"
numbers are this tree's "before" numbers - two other items landed in between.

## What I did

The same six files and the same shapes run 1 measured, because every one of run 1's assertions
re-measured identically on this tree (below).  Nothing in the recipe had to change.

- **`src/MetroidPrime/ScriptObjects/Carve801FD7D4.c`** (new, plain C, 1 function): `fn_801FD7D4`,
  `.text 0x801FD7D4..0x801FD858`, 0x84 = 132 bytes, 33 instructions.  The body is
  `ScriptObjects/Carve801FD998.c`'s `Matching` `fn_801FD998` verbatim with `44 -> 36` and the callee
  `fn_801FDA1C -> fn_801FD858`, and **the callee is declared with pointer parameters**
  (`void fn_801FD858(const void*, const void*)`, so `&first`/`&last` go out in r3/r4).
- **`src/MetroidPrime/ScriptObjects/Carve801FD858.c`** (new, plain C, 2 functions): `fn_801FD890`
  (0x801FD890, 0x50) and `fn_801FD858` (0x801FD858, 0x38), `.text 0x801FD858..0x801FD8E0`,
  0x88 = 136 bytes, declared **descending by address** (fn_801FD890 first).  `fn_801FD858` is the
  forwarder with **by-value** `It` parameters (it dereferences r3/r4 itself), `fn_801FD890` is the
  0x24-strided walk calling `fn_801FD8E0`, which `ScriptObjects/Carve801FD8E0.c` (Matching) already
  defines.  The two units cannot be one TU: one symbol cannot be declared both ways.
- **`configure.py:780-781`**, **`config/G2ME01/splits.txt:1389-1394`**,
  **`files.cmake:587-588`** - the two new units between `Carve801FD67C.cpp` and `Carve801FD8E0.c` in
  all three, i.e. three of the four carve files for the two claims, the fourth being each source's
  own claim in its header.  `total_functions` is still **28465** after the `splits.txt` edit.
- **`src/MetroidPrime/PortLinkStubs.cpp`** - `stub_230` (`fn_801FD7D4`) retired, its block comment
  replaced with a paragraph naming what now defines the symbol (was the only place in the file that
  listed `fn_801FD858` as an unclaimed inner walk, and it repeated the "claiming X only moves the gap
  along" argument this carve refutes), and the header count line moved 200/190 -> **199/189** with a
  `Before that,` clause for the superseded 200/190 reading, which is now stated rather than
  overwritten.

## Re-measured on this tree (nothing carried over from run 1 unverified)

- `config/G2ME01/symbols.txt:8276` `fn_801FD7D4 = .text:0x801FD7D4 size:0x84`; `:8277`
  `fn_801FD858 size:0x38`; `:8278` `fn_801FD890 size:0x50`; `:8275` `fn_801FD774 size:0x60`;
  `:8279` `fn_801FD8E0 size:0x20`; `:8283-8284` the `Carve801FDA1C.c` twin pair; `:8309`
  `fn_801FE97C size:0xB0`; `:8348` `fn_801FF7EC size:0x4C`.
- **The bytes came out of the disc, not out of `build/G2ME01/main.elf`**: `python3 tools/dol_read.py
  0x801FD7D4 0x84`, `... 0x801FD858 0x38`, `... 0x801FD890 0x50`, each piped through
  `build/binutils/powerpc-eabi-objdump -D -b binary -EB -m powerpc:common --adjust-vma=<addr>`.
  (`-EB` is load-bearing: without it binutils decodes the big-endian words as little-endian and every
  mnemonic is wrong - that is what made my first pass of this read look like nonsense.)  Before the
  carve these were the third, fourth and fifth `.fn` blocks of dtk's
  `build/G2ME01/asm/auto_03_801FD6F0_text.s`, the unit `build/report.json` still calls
  `main/auto_03_801FD6F0_text` - and it still does, now with **2** functions
  (`fn_801FD6F0`, `fn_801FD774`), 228 bytes.
- **Twins, compared word by word out of the disc this run** (33 words each).  Every difference is at
  +0x30 (the stride `mulli`) or +0x54/+0x64 (the two `bl Free__7CMemoryFPCv` displacements, which
  differ by exactly the distance between the two functions - 0x0b61-0x099d = **0x1C4** =
  0x801FD998 - 0x801FD7D4 against `fn_801FD998`):
  - `fn_801FD7D4` vs `fn_801FD998` (`Carve801FD998.c`, Matching): **30 of 33 identical**, `mulli 24`
    against `mulli 44` (`0x1c000024` / `0x1c00002c`).
  - vs `fn_801FD6F0` (unclaimed): **30 of 33**, `mulli 20`.
  - vs `fn_801FDB5C` (unclaimed): **30 of 33**, `mulli 48`.  That is the whole family.
  - `fn_801FD858` vs `fn_801FDA1C` and vs `fn_801FDBE0`: **14 of 14 identical in both**, `bl` word
    `48000015` included (0x801FD87C + 0x14 = 0x801FD890).  `fn_801FD890` vs `fn_801FD5E8`:
    **20 of 20**, `bl` word `4800002d` included.
- **Callers, by scanning every `bl` in the disc's whole `.text`** (opcode 18, `LI = (w>>2) & 0xFFFFFF`
  sign-extended, `AA = (w>>1)&1`; note `LI` is bits 6-29 in MSB numbering = `(w>>2)&0xFFFFFF`, not
  `w & 0x03FFFFFF`, which is the reading that silently finds zero hits):
  - `fn_801FD7D4`: **one** `bl`, 0x801FD4F0, inside `fn_801FD4B0` (0x801FD4B0 + 0x7C =
    0x801FD52C, the `Matching` `Carve801FD4B0.cpp`) - `addi r3,r30,0x10` / `li r4,-1`, the third of
    that destructor's four teardowns.  So the flag arrives as MWCC's "destroy, do not free me
    afterwards", and this is the symbol the port link asks the stand-in for.
  - `fn_801FD858`: **two**, 0x801FD820 (inside `fn_801FD7D4`) and 0x801FEA08 (inside `fn_801FE97C`,
    0x801FE97C + 0xB0 = 0x801FEA2C, still unclaimed), whose `mulli r0,r30,0x24` at 0x801FE9F0
    builds its bound before the call - the second, independent measurement of the 0x24 element.
  - `fn_801FD890`: **one**, 0x801FD87C.  `fn_801FD8E0`: **two**, 0x801FD8B4 (this walk) and
    0x801FF810, inside `fn_801FF7EC` (0x801FF7EC + 0x4C) of the `Matching`
    `src/MetroidPrime/ScriptObjects/Carve801FF720.cpp`, whose own carve fixes 0x24 from its bytes -
    the third measurement of the same element size, and the one this unit's header comment cites.
- **No gap is spanned.**  Below, `fn_801FD774` (0x801FD774 + 0x60) ends exactly at 0x801FD7D4 and is
  still unclaimed; `Carve801FD7D4.c` ends exactly at 0x801FD858 (`symbols.txt:8277` puts
  `fn_801FD858` at 0x801FD858), and `Carve801FD858.c` ends exactly at 0x801FD8E0, where the
  `Matching` `Carve801FD8E0.c` claim starts.  Both boundaries are function edges.

## Verification

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
goal_check: item carve-801fd7d4 (match) target=MetroidPrime/ScriptObjects/Carve801FD7D4
goal_check: baseline .../wt-mp2-goal-L4/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13101 -> 13104   linked 6199 -> 6202
  ok    check_symbol_names.py
  ok    All:  37.09% fuzzy, 30.52% matched, 13.47% linked (13104 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FD7D4.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fd7d4
```

Measured directly as well:

- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD7D4.c` - `PASS -> kept as Matching`,
  `kept: 1 / 1  failed: 0  skipped: 0`.  The same for `Carve801FD858.c` - **both** units flip, not
  only the item's target.
- `./tools/unit_fit.sh` on both: `Carve801FD7D4.c` `.text claimed 132 ours 132 retail 132 fits`,
  `Carve801FD858.c` `.text claimed 136 ours 136 retail 136 fits`, both with `no extra functions: our
  object defines only what the retail unit object does`.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; the build printed
  the `All:` line above.  `gate.sh`'s independent re-hash of all 86 RELs against
  `config/G2ME01/config.yml` passed inside the judge.
- `build/report.json`: `matched_functions` 13101 -> **13104** (+3), `complete_units` 836 -> **838**,
  `total_units` 2159 -> **2161** (+2: the dtk auto unit is reduced from 5 functions to 2, not
  removed), `total_functions` **28465** unmoved, `fuzzy_match_percent` 37.080948 -> **37.08505**,
  `complete_code` 880200 -> 880468, `matched_code` 1994428 -> 1994696, `complete_data` and
  `matched_data` **unmoved** at 246372 / 256731 (no data claim here).  New units:
  `main/MetroidPrime/ScriptObjects/Carve801FD7D4` 100.0, 132/132 bytes, **1/1** functions;
  `main/MetroidPrime/ScriptObjects/Carve801FD858` 100.0, 136/136 bytes, **2/2** functions.
- **Nothing anywhere got worse**: `gate.sh`'s report diff against
  `build/goal/judge/report.base.json` passed inside the judge, which is the per-unit check.  The
  neighbours are unmoved and still 100.0: `Carve801FD8E0` 68/68 2/2, `Carve801FD998` 132/132 1/1,
  `Carve801FDA1C` 136/136 2/2, `Carve801FD4B0` 124/124 1/1, `Carve801FDB5C` 168/168 3/3.
- **Byte comparison against the disc, per word**, with
  `powerpc-eabi-objcopy -O binary --only-section=.text`:
  - `Carve801FD7D4.o` `.text` (0x84, 33 words): **30 of 33 identical**; the three that differ are
    the three `bl`s reading `48000001` where retail has the target, and `objdump -r -j .text` shows
    exactly those three relocations - `R_PPC_REL24 fn_801FD858` at +0x4c,
    `R_PPC_REL24 Free__7CMemoryFPCv` at +0x54 and +0x64.  `nm -S`: `T fn_801FD7D4` size 0x84,
    undefined only `fn_801FD858` and `Free__7CMemoryFPCv`, unmangled.
  - `Carve801FD858.o` `.text` (0x88, 34 words): **32 of 34 identical**; the two that differ are the
    two `bl`s, with exactly two relocations - `R_PPC_REL24 fn_801FD890` at +0x24 and
    `R_PPC_REL24 fn_801FD8E0` at +0x5c.  `nm -S` shows `T fn_801FD858` 0x38 at +0x00 then
    `T fn_801FD890` 0x50 at +0x38 - **ascending in the object, as it must be**, which is the check
    `flip_test.sh` exists for and it passed.
- `python3 tools/check_symbol_names.py` - `checked 585 units; 0 declared names are missing from
  their object`.  Note what that 585 does and does not say: the tool only walks `^(\S+\.cpp):`
  entries in `splits.txt` (`tools/check_symbol_names.py:37`), so neither of these `.c` units is in
  its 585 at all.  Run 1's 584 and this run's 585 differ because the two commits that landed in
  between added two `.cpp` units, not because of this change.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD858` -
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.  Run 1 reported the same
  tool as matching `.cpp` names only and therefore silent about `Carve801FD998`; on this tree it
  takes any unit name and resolves the object under `build/G2ME01/src/`, with no `.cpp` filter -
  a bogus name answers `ok: 0 unit(s) checked` and mine answers `ok: 1 unit(s) checked` - so for
  `Carve801FD858.c`, the unit that actually has two functions, it is a real check here.
  `flip_test.sh` is still the one that decides.
- Port: `build/gate-probe.log` -> `probe: 837 files, 0 failed, 0 errors; link: LINKED (286 undefined,
  0 duplicates)`.  **286 is this item's judge baseline** (`build/goal/judge/undef.base.count`), so
  this carve adds nothing to the port's undefined list.  `build/gate-link.log` -> `279 MISSING`, and
  `ok: 279 MISSING symbol(s), all accounted for in port_link_gap_list.md` - i.e. **no
  `docs/research/port_link_gap*.md` edit was needed**; the gap set did not change.
  `build/gate-dups.log` -> `unique undefined symbols 286`, `duplicate definitions 0`, which is the
  check that says retiring `stub_230` was necessary rather than optional (with it back the same
  command reports a duplicate of `fn_801FD7D4`).
- `PortLinkStubs.cpp` counts after the retirement, by the terms its own header names:
  `grep -cE 'asm\("'` **199**, `grep -cE '^extern "C" void stub[^ ]*\(\) asm\("'` **189**,
  `^extern "C" char stub_data_` **10**, unmangled `grep -cE 'asm\("(fn_|lbl_)'` **39**.  Before the
  change the same terms read **200 / 190 / 10 / 40**, which is what the header now records as its
  `Before that,` clause.
- `gate.sh` rewrote `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` as a side effect, as it does
  for every lane; `git checkout --` on exactly those two left the tree at the six files below.  The
  judge's run is on the final tree apart from them.

## Left alone on purpose

- **Run 1's one recorded drift is gone on this tree**: the header line read `190 functions, 9 data
  objects` before this change while the file held 190 / 10 / 200 - it had been corrected by an
  intervening commit (`3367405b`), which is also why the `Before that,` clause I wrote names the
  200/190 reading rather than run 1's stale 190/9 one.
- **The file's own breakdown paragraph** (line ~85) still reads `86 REL loader, 64 game method, 40
  unmangled fn_/lbl_, 1 CodeWarrior-mangled rmemory_allocator::allocate, 9 vtable/typeinfo` = 200,
  which was already 10 off on the `9` before this change (the data count is 10).  With the retirement
  its terms read 199 total / 39 unmangled / 10 data, so the residual becomes
  `199 - 86 - 39 - 10 - 1` = 63, not 64.  I did not rewrite that paragraph: this change moves the
  count line above it, and re-deriving a breakdown whose REL-loader term cannot be grepped is the
  next lane's job with its own measurement.  **Recorded here so it is not rediscovered.**
- **No `NEW:` line is filed.**  Run 1 already established that `carve-801fdb5c` and
  `carve-801fd6f0` are filed or queued, and re-filing either would cost a lane an hour for nothing.
  Re-measured this run, cheaply, so the next run need not repeat it - and nothing in run 1's recipes
  for them changed:
  - **`carve-801fdb5c`** (0x801FDB5C, 0x84, stride 48, callee `fn_801FDBE0`, already defined by the
    `Matching` `Carve801FDB5C.c`): confirmed again at **30 of 33 words** against this item's
    `fn_801FD7D4`, the only differences being `mulli 0x30` and the two `Free` displacements.  The
    body is `src/MetroidPrime/ScriptObjects/Carve801FD7D4.c` with `36 -> 48` and
    `fn_801FD858 -> fn_801FDBE0`, the callee declared `void fn_801FDBE0(const void*, const void*)`.
    Retire `stub_228`; no new stand-in.
  - **`carve-801fd6f0`** (0x801FD6F0, 0x84, stride 20, callee `fn_801FD774`): confirmed again at
    **30 of 33 words** against `fn_801FD7D4`.  `fn_801FD774` (0x801FD774, 0x60 = 24 instructions,
    re-read out of the disc this run) is **not** the 0x38/0x50 forwarder-plus-walk pair - it
    dereferences r3/r4 into its own frame (`lwz r31,0(r4)` / `lwz r30,0(r3)`) like the forwarders
    do, but then walks `addi r30,r30,0x14` with `cmplwi r30,0 / beq` at 0x801FD79C and a second
    unconditional `b` at 0x801FD7A4 before `bl 0x802FE9B8` (`~rstl::basic_string`) per element, and
    the loop bound is `cmplw r30,r31` / `bne` at +0x40.  Carving `fn_801FD6F0` alone therefore
    retires `stub_227` and needs one stand-in for `fn_801FD774`; net zero for the port.
  - **`fn_801FE97C`** (0x801FE97C, 0xB0) is still the second retail caller of `fn_801FD858` and is
    still unclaimed.  It needs `fn_801FF720`'s claim boundary checked and `fn_801FEA2C`
    (0x801FEA2C, 0x6C, unclaimed), so it is not a cheap carve and is still not filed.

## One wrong claim found in a committed file, left alone on purpose

`src/MetroidPrime/ScriptObjects/Carve801FD998.c:62-64` (run 1's own unit, already `Matching` and
committed) says:

> `tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD998` matches `.cpp` names
> only, so it says nothing

**That is wrong, measured this run.**  `tools/check_decl_order.py` takes any unit name and resolves
`build/G2ME01/src/<name>.o` (`tools/check_decl_order.py:51`); it has no `.cpp` filter.  Measured:
`--unit MetroidPrime/ScriptObjects/NoSuchUnit` prints `ok: 0 unit(s) checked`, while
`--unit MetroidPrime/ScriptObjects/Carve801FD7D4` and
`--unit MetroidPrime/ScriptObjects/Carve801FD858` each print `ok: 1 unit(s) checked`.  The tool it
was confused with is `tools/check_symbol_names.py:37`, which really does match `^(\S+\.cpp):` only -
see the 585/584 note above.  So `check_decl_order.py` **can** check these `.c` units, and does.

I did not edit `Carve801FD998.c`: it is a `Matching` unit this item did not touch, the sentence is
one line of prose in its header, and the correct fix belongs to whoever next works in that
neighbourhood (its own wording, and its "a later second function" clause, need re-checking against
what the tool actually reports).  The same claim was inherited into the header of
`src/MetroidPrime/ScriptObjects/Carve801FD7D4.c` when I copied that unit's comment, and **is**
corrected there, because it is a line this change introduces and a reviewer is right to hold it to
what was measured.  Not filed as `NEW:` - it is a documentation fix and would cost a lane an hour
for nothing.  Recorded here so the next run in this neighbourhood does not rediscover it.
