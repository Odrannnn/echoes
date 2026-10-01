# match-cpowerbeam — `MetroidPrime/Weapons/CPowerBeam` is `Matching`

`kind: match`, target `MetroidPrime/Weapons/CPowerBeam`. **PASS** (`./tools/goal_check.sh
build/goal/item.json`, this worktree, exit 0) — full result, not partial: the unit flipped and the
DOL still reproduces retail with our object in the link.

## Measured result

| | before | after |
| --- | --- | --- |
| unit `matched_functions` | 13 / 14 | **14 / 14** |
| unit `fuzzy_match_percent` | 96.54321 | **100.00** |
| `Fire` (the item's target function) | 54.10% | **100.00%** |
| `All: linked` | 5523 | **5537** |
| `All: matched` | 11408 | **11409** |

`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:

```
matched  11408 -> 11409   linked 5523 -> 5537   (+1 functions at 100%, 1 units newly linked)
  LINKED   main/MetroidPrime/Weapons/CPowerBeam
  +100%    main/MetroidPrime/Weapons/CPowerBeam :: Fire__10CPowerBeamFRC34TCachedToken<18CWeaponDescription>bfQ212CPlayerState12EChargeStageRC12CTransform4fR13CStateManager9TUniqueIdUiUsP9TUniqueIdP10CSfxHandleff
no regression
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` with
`Object(Matching, "MetroidPrime/Weapons/CPowerBeam.cpp")` in `configure.py`, and all 86 RELs `cmp`-equal.
`tools/probe_sources.sh` -> `751 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0
duplicates)`; `tools/gate.sh`'s own `link_check` -> `unchanged from baseline (250 undefined, 0
duplicates)`. `python3 tools/check_symbol_names.py` clean.

Files: `configure.py` (one `NonMatching` -> `Matching`), `src/MetroidPrime/Weapons/CPowerBeam.cpp`,
`src/MetroidPrime/mainHead.cpp`. No header, no `splits.txt`, no `files.cmake`, no `tools/`, no asm.
`docs/HANDOFF.md`'s state block is the only other change and that is `gate.sh` writing it, not me.

## Three defects, one file each

### 1. `Fire` was at 54.10% because the sentinel was declared non-`const`

This is the item's whole named target, and it was **one word**. `mainHead.cpp` defines
`extern const ushort lbl_8041E2E6 = 0xFFFF`, but `CPowerBeam.cpp` read it through a plain
`extern "C" ushort`, so mwcceppc ranked the `lhz r0,0(0) R_PPC_EMB_SDA21` as an ordinary mutable
global and sank it to its point of use:

```
  ours                                retail
  fmr     f31,f3                      fmr     f31,f3
  stfd    f30,80(r1)                  lhz     r0,0(0)  R_PPC_EMB_SDA21   <-- hoisted
  fmr     f30,f2                      stfd    f30,80(r1)
  stfd    f29,72(r1)                  fmr     f30,f2
  fmr     f29,f1                      cmplw   r11,r0                    <-- and its compare
  stmw    r22,32(r1)                  stfd    f29,72(r1)
  mr      r22,r3 .. mr r29,r10        fmr     f29,f1
  lhz     r0,0(0)  R_PPC_EMB_SDA21     stmw    r22,32(r1)
  cmplw   r11,r0                      mr      r22,r3 .. mr r29,r10
```

61 instructions either way, same frame, same registers; two instructions in the wrong slot, which
objdiff scores 54.10%. Adding `const` to the declaration gives the load the same rank retail's own
read gets and the 244-byte function matches instruction for instruction. **Measured, not guessed:**
this is the first spelling I tried, and `CPowerBeam::Fire` went 54.10% -> 100.00% on that one
`extern "C" const ushort lbl_8041E2E6;`. (The previous run's note listed the seven spellings it had
tried for the *body*; none of them could have worked, because the body was already right. The wall
was in the declaration's type, not in the control flow. Seven body spellings had been spent on a
scheduling problem that a `const` solved.)

### 2. `2.f` and `0.f` put a private copy in this unit's `.sdata2`, which pushed `.bss2` by 8

With `Fire` matched, `flip_test.sh` still failed: 43 checksums wrong, DOL 32 bytes long. `dtk dol
info` on both builds gave the whole of it:

```
retail  .sdata2 | 0x8041A3C0 | 0x54C0     ours  .sdata2 | 0x8041A3C0 | 0x54C8
retail  .bss2   | 0x8041F880 | 0x84      ours  .bss2   | 0x8041F888 | 0x84
```

Retail's `2.0f` and `0.0f` live in `.sdata2` at **0x8041D250 / 0x8041D254** — confirmed by
`objdump -r` on the retail object (`R_PPC_EMB_SDA21 lbl_8041D250` / `lbl_8041D254` on the three
`lfs` in the ctor, `ReInitVariables` and `UpdateGunFx`) and by `tools/dol_read.py 0x8041D240 0x40`
(`u32: ... 0x40000000 0x0 ...`). Written as `2.f` / `0.f` the *instructions* still match, but
mwcceppc emits a private copy of each into **this unit's own `.sdata2`**, and mwldeppc appends an
unclaimed `.sdata2` past the end of the section. Declaring and using the two retail globals empties
the section (`.sdata2` 8 -> 0) and the build then reproduces `.sdata2` and `.bss2` exactly.

The section sizes measure this directly, and they are cheap to re-measure:

| source form | unit `.sdata2` |
| --- | --- |
| `2.f` / `0.f` as literals (the tree before this change) | 8 |
| `mSmokeTimer = dt` instead of `2.f`, rest unchanged | 4 |
| `mSmokeTimer = 1.f` instead of `2.f`, rest unchanged | 8 |
| `mSmokeTimer > 1.f` instead of `> 0.f`, rest unchanged | 12 |
| both literals named as retail globals | **0** |

One entry per distinct float literal in the file, and the ctor's `mSmokeTimer(0.f)` is one of the
three `lfs` sites, so all three had to change. This is the same trick `lbl_8041E2E6` and
`lbl_8041D394` already use in this file, and the values are defined for the port in `mainHead.cpp`
next to `lbl_8041D248`.

### 3. The two `new CElementGen` sites named this unit's `@stringBase0`, which moved retail's

After (2), 86 RELs and `main.dol`'s sections were all right and only `main.dol` still differed:
16826 single-byte diffs, every one a low address byte `+8`. The first is `AddPaksAndFactories`:

```
retail  38 c3 ea b8   addi r6,r3,-5432   ->  0x803AEAC8   (@stringBase0)
ours    38 c3 ea c0   addi r6,r3,-5440   ->  0x803AEAB8   (the claim that ends where @stringBase0 starts)
```

Retail's own CPowerBeam object has **no** `.rodata`, but its two `operator new` relocations are
`R_PPC_ADDR16_HA/LO lbl_803AAB80` — one of the three `"?\?(?\?)"` literals sitting in the unclaimed
`.rodata` gap at 0x803AAB70. Ours were `R_PPC_ADDR16_HA/LO @stringBase0`, i.e. mwcceppc's own
per-translation-unit copy of the literal, because `CMemory.hpp`'s inline
`operator new(size_t)` passes `"??(??"` and nothing told it otherwise. That 7-byte private
`.rodata` is appended by mwldeppc *after* every claimed `.rodata` contribution, which pushes the
real `@stringBase0` 8 bytes later and rewrites a `lis`/`addi` pair in **every other unit** that
references the pool.

`Kyoto/Alloc/CMemory.hpp` already documents the fix for exactly this (`CMEMORY_NEW_FILE`), and
`src/MetroidPrime/Weapons/CPlasmaProjectile.cpp` is the worked example. Declaring
`extern "C" const char lbl_803AAB80[];` / `#define CMEMORY_NEW_FILE lbl_803AAB80` before every
include, plus writing the two `new CElementGen` sites as `rs_new` (which is what routes them through
the macro), takes the unit's `.rodata` to 0 and the relocations onto retail's own symbol.

Worth knowing for the next attempt: **`CMEMORY_NEW_FILE` on its own is not enough.** Setting it with
the two sites left as `new` left `.rodata` at 7 bytes and both relocations on `@stringBase0` — the
header's inline `operator new(size_t)` hard-codes its own `"??(??"` and the macro only redirects
`rs_new`.

## What is left in the unit, and what is not

`tools/unit_fit.sh MetroidPrime/Weapons/CPowerBeam.cpp` still reports `.text` 492 bytes over and 8
functions ours emits that the retail object does not define. **Both flip anyway** — they are COMDAT
weak copies (template/`optional_object`/`single_ptr` destructors plus 3 inline virtuals) that
mwldeppc discards, and the unit flips, which is the only thing that decides. `check_decl_order.py`
reports no out-of-order unit, and the flip's own DOL hash would have caught a permutation.

The `.data` claim is 96 bytes against our 92. The last four are trailing padding: retail's own
relocation table stops at offset 0x58, so retail's bytes 0x5c..0x60 are zeros and mwldeppc's
zero-fill reproduces them. Nothing to do.

The unit's `.sdata` is 40 bytes of unreferenced data (no relocation against it; the values read as
`3, 0, 1, 0, 0, 2, 0, 4, 1.0f, 0x13`) and is unclaimed in `splits.txt`. It does not reach the link —
`.sdata` in the DOL is byte-identical to retail with this unit flipped — so it is not a blocker and I
left it alone rather than go looking for what emits it.

## Generalisable, for the next lane

- **`const` on a `extern` global is a scheduling decision, not a type annotation.** mwcceppc gives a
  load from a `const` global a different rank than the same load from a mutable one, and the
  difference shows up as the load sitting in a different slot of the prologue. A unit that is at
  54% with *every instruction accounted for* is very likely to be a declaration's type, not the
  body's control flow.
- **An unclaimed input section is not free.** mwldeppc appends `.rodata` after the last claimed
  contribution and `.sdata2` past the end of the section, and both move things other units hold
  addresses to. A unit can be at 100% fuzzy with 14/14 matched and still not flip because of 7 bytes
  of private string literal. Compare `dtk dol info` on the failing build against retail first: it is
  one line per section and names the cause immediately.
- **Three of the five `float` constants in this file's neighbourhood were already retail globals**
  (`lbl_8041D248`, `lbl_8041D250`, `lbl_8041D254`), and retail's `.sdata2` at 0x8041D200..0x8041D288
  is a dense float table. Any unit here that writes `0.f`/`1.f`/`2.f` should first check
  `config/G2ME01/symbols.txt` for a `.sdata2` float near its own code, and prefer naming it.

## What I verified

- `./tools/goal_check.sh build/goal/item.json` -> `PASS match-cpowerbeam`, exit 0
  (`no judge-owned path touched`; `gate.sh` incl. DOL sha1, 86 RELs, per-function report diff,
  wiring, docs claims, port probe and port link gap; `matched 11408 -> 11409`, `linked 5523 -> 5537`;
  `check_symbol_names.py`; `All: 32.79% fuzzy, 25.54% matched, 12.03% linked (11409 / 28465
  functions)`; `flip_test MetroidPrime/Weapons/CPowerBeam.cpp: PASS, Object(Matching) in
  configure.py`).
- `./tools/flip_test.sh MetroidPrime/Weapons/CPowerBeam.cpp` -> `PASS -> kept as Matching`
  (`kept: 1 / 1   failed: 0   skipped: 0`).
- `./tools/probe_sources.sh` -> `751 files, 0 failed, 0 errors`.
- `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json` -> `no
  regression`, and the only entry is `Fire` at 100%.
- `git status --porcelain --untracked-files=all` -> `configure.py`,
  `src/MetroidPrime/Weapons/CPowerBeam.cpp`, `src/MetroidPrime/mainHead.cpp`, plus `docs/HANDOFF.md`
  (written by `gate.sh` under `MP_GATE_DOCS_WRITE=1`; the driver discards edits to that file anyway).
- No assembly was added; the scratch compiler-driver and the fourteen source variants I tried live
  in `.tmp/opencode/` (gitignored) and are not in the diff.
