# carve-801ef730 — `MetroidPrime/Carve801EF730.cpp`, 2 functions, Matching

**Result: PASS.** `tools/goal_check.sh build/goal/item.json` printed
`goal_check: PASS carve-801ef730`, with `flip_test MetroidPrime/Carve801EF730.cpp: PASS,
Object(Matching) in configure.py`. `matched 13491 -> 13493`, `linked 6539 -> 6541`,
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail).

## What I did

Carved `.text 0x801EF730..0x801EF7B0` (0x80 = 128 bytes, 2 functions) out of dtk's unclaimed
`main/auto_03_801EF598_text` as a new `Matching` unit in all four places the carve rule requires,
and deleted a now-duplicate stand-in from the neighbouring unit.

- `src/MetroidPrime/Carve801EF730.cpp` — **new**, both bodies.
- `config/G2ME01/splits.txt:1334` — `MetroidPrime/Carve801EF730.cpp: .text start:0x801EF730
  end:0x801EF7B0`, placed between `MetroidPrime/CActorField25.cpp` (ends 0x801ECDCC) and
  `MetroidPrime/Carve801EF84C.cpp` (starts 0x801EF84C), i.e. address order.
- `configure.py:772` — `Object(Matching, "MetroidPrime/Carve801EF730.cpp"),` on **one line**,
  in the same place in address order.
- `files.cmake:772` — `    src/MetroidPrime/Carve801EF730.cpp`, in the carve block.
- `src/MetroidPrime/Carve801EF84C.cpp` — the `#ifndef __MWERKS__` empty stand-in for
  `fn_801EF730` **deleted** (it was at the end of the file), plus two header comments corrected
  (its "unit" block and its `fn_801EF730` declaration) because they said the symbol was unclaimed
  and defined only by that stand-in. That deletion is not cosmetic: with this carve listed in
  `files.cmake` the host link gets `fn_801EF730` from the new unit, so the stand-in would be a
  second definition, which is what `gate.sh`'s `port link dups` step exists to catch.

`total_functions` is still **28465** after the `splits.txt` edit (measured from
`build/report.json`: `measures.total_functions` 28465, and the per-unit sum is also 28465).

## Measured, not recalled

Retail bytes, read out of the disc rather than from our own `main.elf`:

```
$ python3 tools/dol_read.py 0x801EF730 0x80 orig/G2ME01/sys/main.dol
hex : 94 21 ff f0 7c 08 02 a6 90 01 00 14 93 e1 00 0c 7c 9f 23 78 93 c1 00 08 7c 7e 1b 79
      41 82 00 1c 38 80 ff ff 4b e1 4f f1 7f e0 07 35 40 81 00 0c 7f c3 f3 78 48 0d ec 25
      80 01 00 14 7f c3 f3 78 83 e1 00 0c 83 c1 00 08 7c 08 03 a6 38 21 00 10 4e 80 00 20
      94 21 ff f0 7c 08 02 a6 90 01 00 14 93 e1 00 0c 7c 7f 1b 78 48 00 00 19
      80 01 00 14 83 e1 00 0c 7c 08 03 a6 38 21 00 10 4e 80 00 20
```

Addresses and sizes from `config/G2ME01/symbols.txt:7975-7977`
(`fn_801EF730` 0x801EF730 size 0x54, `fn_801EF784` 0x801EF784 size 0x2C,
`fn_801EF7B0` 0x801EF7B0 size 0x9C). Instructions cross-checked against dtk's own dump
`build/G2ME01/asm/auto_03_801EF598_text.s:128-167`, which agrees instruction for instruction.

**The twins, compared against the disc, not against our build.** Each pair differs only in the
`bl` displacements:

| pair | twin | evidence |
| --- | --- | --- |
| `fn_801EF730` (0x54) | `fn_800045A0` (0x800045A0, 0x54), matched, `src/MetroidPrime/Carve800045A0.c:195` | same 21 words; `48 00 00 31` vs `4b e1 4f f1`, `48 2c 9d b5` vs `48 0d ec 25` |
| `fn_801EF784` (0x2C) | `GetIObjObjectFor__23TToken<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSkinnedModel>` (0x80031A00, 0x2C, `symbols.txt:931`) | same 11 words; `48 00 00 19` vs `48 00 00 01` |
| callee `fn_801EF7B0` (0x9C) | `GetNewDerivedObject__41TObjOwnerDerivedFromIObj<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSki` (0x80031A2C, 0x9C, `symbols.txt:932`) | same 0x9C body: `li r3,0x8 / bl __nw__FUlPCcPCc`, the three `stw` of the `__vt__4IObj` / `__vt__31CObjOwnerDerivedFromIObjUntyped` / `lbl_803B78D0` vtable, `stb r5,0x0(r31)`, `stw r4,0x4(r3)` |

Both twins sit in the already-matched `MetroidPrime/Factories/CCharacterFactory.cpp`, read out at
`build/G2ME01/asm/MetroidPrime/Factories/CCharacterFactory.s:2565-2578` and `:2579-2620`.

## What the bytes fix, and why the spelling is what it is

`fn_801EF730` is the MWCC deleting-destructor shape
`if (self) { fn_80004744(self, -1); if (flag > 0) Free(self); } return self;`. Two details come
from the bytes, not from taste: the flag is a **short** (`extsh. r0,r31` at 0x801EF758, not the
`cmpwi` an `int` would give) and `if (flag > 0)` sits **inside** `if (self)`, because the
receiver's `beq` (`41 82 00 1c`, target 0x801EF768) branches over the flag test to the epilogue.

**`fn_801EF784` is a class-returning function and the unit therefore had to be a `.cpp`, not the
`.c` the seed specified.** This is the one place I departed from the item's instruction, and it is
a real finding rather than a preference:

- Retail's 11 instructions are `stwu / mflr / stw r0 / stw r31,0xc(r1) / mr r31,r3 / bl / lwz r0 /
  lwz r31,0xc(r1) / mtlr / addi / blr`. The `stw r31` / `mr r31,r3` / `lwz r31` triple is the
  **hidden-sret convention**: the return type is the class `rstl::auto_ptr<T>` (8 bytes), so MWCC
  passes a return slot in r3, keeps it in the callee-saved r31 across the call, and moves the
  object argument to r4. `r3` is never read after the call, which is why the register is *only*
  saved and restored.
- Retail's own caller confirms the shape: `fn_801EF5FC` sets `addi r3,r1,0x8 / addi r4,r1,0x10`
  before its `bl fn_801EF784` at 0x801EF62C and reads `lwz r3,0xc(r1)` after, i.e. the result
  lands at `r1+8` and the caller takes its **+4** word. It also writes `stb r0,0x10(r1)` *before*
  the call, which is `obj.mHas = false` at the argument's +0. So the return is
  `{ bool mHas; T* mItem; }` — **8 bytes** (`include/rstl/auto_ptr.hpp:22-24`) — and it does not
  overlap the argument slot at `r1+0x10`.
- **A plain-C spelling cannot reproduce it, and the return type alone decides that.** Measured,
  compiling with the DOL's exact flags:
  - `void f(void*,void*)` / returning an 8-byte POD struct -> **8 instructions**, no `r31` at all.
  - returning a **12-byte** C struct -> retail's exact **11 instructions**.
  So plain C reaches the bytes only by lying about the return size: 12 bytes would overlap the
  argument slot in `fn_801EF5FC`, which retail does not. Only C++ has the class that MWCC returns
  through memory, so the unit is a `.cpp` with the definitions wrapped in `extern "C"` — which is
  what keeps the `fn_` names verbatim and unmangled, the seed's actual concern. That arrangement
  is already used for Matching carves of exactly this shape at
  `src/MetroidPrime/Carve801EF84C.cpp:99` and
  `src/MetroidPrime/Player/CGameStateBlockCopyCtor.cpp:34`, and
  `src/Kyoto/Animation/CAnimCharacterSet.cpp:124-139` already spells this very shape
  (`fn_8028EC50`, 0x2C, `mr r31,r3` included) with the same return type.
- `fn_801EF730` compiles to the same 21 instructions in C and in C++ — measured both ways — so
  nothing else in the unit depends on the choice.

**A closer twin than the seed named.** `fn_8028EC50` (0x8028EC50, 0x2C) is
`fn_801EF784`'s byte-shape twin *and* its codegen twin, because it is the same function for a
different `T`; the seed's `GetIObjObjectFor__23TToken<13CSkinnedModel>` is the same shape but only
reachable as a template instantiation. Both are noted in the header.

## Callers, measured

`grep -rn 'bl fn_801EF730\|bl fn_801EF784' build/G2ME01/asm/` gives three sites:

- 0x801EF62C — `fn_801EF784` from `fn_801EF5FC`, with the two stack addresses above.
- 0x801EF6DC — `fn_801EF730` from `fn_801EF6A0` (0x801EF6A0, 0x90) with `li r4,0x1`, the deleting
  flag; that caller sets `lbl_803B78D0` at the receiver's +0 and only calls when the +4 member is
  non-zero.
- 0x801EF880 — `fn_801EF730` from the **matched** `MetroidPrime/Carve801EF84C.cpp` (`.text` from
  0x801EF84C), which is why that file's stand-in had to go.

## The port-only stand-in, and the one gate it is for

`fn_801EF7B0` is claimed by nothing, so the DOL link takes it from dtk's own object of the
surrounding run. For the **host** link it is new: before this carve nothing in the port referenced
the symbol, only dtk's `auto_*` objects did. It gets an `#ifndef __MWERKS__` announced
stand-in returning an empty `rstl::auto_ptr`, the same trade
`src/MetroidPrime/Cameras/Carve801E7C14.c:119-140` makes for `__dt__17CCameraShakerDataFv` and
`src/MetroidPrime/Carve801EF84C.cpp` made for `fn_801EF730`. The guard is `__MWERKS__`, not
`TARGET_PC`, to match those files: the matching build must take the symbol from dtk's object, and
a second definition there would be a duplicate.

## Verification

```
$ ./tools/unit_fit.sh MetroidPrime/Carve801EF730.cpp
== MetroidPrime/Carve801EF730.cpp  (config/G2ME01/splits.txt)
   .text      claimed    128   ours    128   retail    128   fits
   no extra functions: our object defines only what the retail unit object does

$ ./tools/flip_test.sh MetroidPrime/Carve801EF730.cpp
  PASS  -> kept as Matching
kept: 1 / 1   failed: 0   skipped: 0

$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13491 -> 13493   linked 6539 -> 6541
  ok    check_symbol_names.py
  ok    All:  37.57% fuzzy, 31.00% matched, 13.87% linked (13493 / 28465 functions)
  ok    flip_test MetroidPrime/Carve801EF730.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801ef730
```

From `build/report.json` after the build: `main/MetroidPrime/Carve801EF730` is
`complete: true`, `matched_code_percent 100.0`, `matched_functions 2 / 2`;
`main/MetroidPrime/Carve801EF84C` is still `complete: true`, `100.0`, `3 / 3` — the stand-in
deletion did not disturb it. `main/auto_03_801EF598_text` is down to 3 functions from 5.

`tools/check_decl_order.py --unit MetroidPrime/Carve801EF730.cpp` and
`tools/check_files_cmake.py` both pass.

## Notes for the next run

- **`tools/carve_diff.sh` is wrong for any multi-function carve.** It reads the candidate object
  from offset 0 (`ours = read(OURS)`, and the `SYM` argument filters nothing — line 42 is
  `if SYM: ours = [i for i in ours if True]`). On a two-function unit it therefore compares
  `fn_801EF730` against `fn_801EF784`'s retail bytes and reports "NOT byte-exact" on a perfect
  object. `docs/RUNNING_THE_DECOMP.md` lists it as the way to "check bytes"; for a carve with more
  than one function it cannot be used as-is. Comparing per-symbol offsets works, and that is how
  both bodies here were confirmed: 21 and 11 instructions, every word equal apart from the `bl`
  relocations the linker fills.
- **A `.s`-free byte check has to read the disc, not `main.elf`.** `build/G2ME01/main.elf` is our
  link, so once this unit was configured the range already held *our* bytes and any comparison
  against it is circular. `tools/dol_read.py` on `orig/G2ME01/sys/main.dol` is the retail source
  (its own docstring says exactly this).
- **`-lang=c` reaches the sret eleven only by lying about the size.** Worth recording as a
  codegen rule: mwcceppc returns an 8-byte POD in r3:r4 and emits 8 instructions, a 12-byte struct
  through memory and emits 11, and a class with a user-declared default constructor through memory
  and emits 11. If a carve's twin is a class-returning function, the unit has to be a `.cpp`.

No `NEW:` line: this item did not stall, and the three findings above are lessons and a tooling
observation, not work whose success would raise a count.