# progress-unit-ianimreader — Kyoto/Animation/IAnimReader

`kind: progress`. Unit stays `NonMatching`; nothing in `configure.py`, `splits.txt` or `files.cmake`
was touched. **Matched functions 9 -> 11 of 12** (`tools/fast_try.sh Kyoto/Animation/IAnimReader`,
then `./tools/decomp_build.sh`).

## Baseline, re-measured first (not taken from `reason`)

`build/report.json` on the clean tree, `main/Kyoto/Animation/IAnimReader`: `total_functions 12`,
`matched_functions 9`, `fuzzy_match_percent 82.72251`, `matched_code 596/764`. The three functions
`reason` names were the three that failed, at the addresses below (dtk's `size:` field is
gap-to-next-symbol, so retail's real boundaries are the symbol addresses).

| retail | bytes | before |
| --- | --- | --- |
| `0x802B2770` `VGetAdvancementResults` | 48 | 75.00% |
| `0x802B27A0` `fn_802B27A0` | 32 | 0.00% (nothing emitted) |
| `0x802B27C0` `fn_802B27C0` | 88 | 0.00% (nothing emitted) |

Retail's shape (`./tools/dis.sh 0x802b2770 0x58`): `VGetAdvancementResults` is
`mr r4,r5` + `bl fn_802B27A0` + epilogue; `fn_802B27A0` is a frame-and-call wrapper;
`fn_802B27C0` (r3 = result slot, r4 = `&CCharAnimTime`) builds `mRemTime` from the argument,
then `mPosDelta = sZeroVector__9CVector3f` and `mRotDelta = sNoRotation__11CQuaternion`.
Prime 1 has the same three steps as
`CAdvancementResults::RemainderOnly` -> `CAdvancementResults(aTime)`
(prime-ref `src/Kyoto/Animation/IAnimReader.cpp:13`, `include/.../IAnimReader.hpp:28`), so the
donor for both is that pair.

## What changed

1. `src/Kyoto/Animation/IAnimReader.cpp` — added the missing out-of-line
   `extern "C" SAdvancementResults fn_802B27A0(const CCharAnimTime&)`, and
   `VGetAdvancementResults` now calls it instead of constructing the result itself.
2. `include/Kyoto/Animation/IAnimReader.hpp` — `SAdvancementResults(const CCharAnimTime&)`
   now initialises `mDeltas` in the member-init list (`: mRemTime(time),
   mDeltas(CVector3f::Zero(), CQuaternion::NoRotation())`) instead of assigning the two members
   in the body. Same initialisation, different spelling, and it is the spelling retail used.

Function declarations stay descending by retail offset (mwcceppc emits in reverse source order);
`python3 tools/check_decl_order.py --unit Kyoto/Animation/IAnimReader` ->
`none emits its functions out of retail order`. Every offset in the object now equals retail's:
`0x200 VGetAdvancementResults`, `0x230 fn_802B27A0`, `0x250 __ct__19SAdvancementResults`, `0x2a8
VSimplified`, `0x2b4 ~IAnimReader` (retail: `0x2770`, `0x27A0`, `0x27C0`, `0x2818`, `0x2824`).

## Per function

**`VGetAdvancementResults`, 75.00% -> 100%.** The missing bytes were retail's
`stw r31,12(r1)` / `mr r31,r3` around the call and the callee being `fn_802B27A0` rather than
the constructor. Once `RemainderOnly`'s wrapper exists and is called, mwcceppc emits exactly
retail's 12 instructions.

**`fn_802B27A0`, 0.00% -> 100%.** Retail's 8 instructions are what a struct-returning function
whose body is `return SAdvancementResults(time);` compiles to:
`stwu/mflr/stw/bl/lwz/mtlr/addi/blr`. Measured two spellings, both 32 bytes and identical
bytes:

| spelling | result |
| --- | --- |
| `static SAdvancementResults RemainderOnly(...)` in `SAdvancementResults` (Prime 1's name), emitted weak out-of-line | bytes correct, objdiff 75% -> ... **still 0%** - see below |
| `extern "C" SAdvancementResults fn_802B27A0(...)` in the .cpp | bytes correct **and** 100% |

**`fn_802B27C0`, 0.00% -> still 0%, but its bytes are now retail's.** Blocked by a naming
constraint, not by codegen - see the next section.

## The wall, measured: objdiff pairs a function with retail by symbol name only

After the ctor spelling change above, the 88 bytes our object emits at 0x250 are byte-identical
to retail's 0x802B27C0. Object bytes (`powerpc-eabi-objcopy -O binary --only-section=.text` at
0x250) vs `python3 tools/dol_read.py 0x802B27C0 0x58` — identical word for word, including the
instruction sequence and both relocations (`sNoRotation__11CQuaternion` at 0x804173C4,
`sZeroVector__9CVector3f` at 0x804174B0, which resolve to the same values). objdiff still scores
it 0.00%, because:

```
$ ./build/tools/objdiff-cli diff -p . -u main/Kyoto/Animation/IAnimReader --format json -o - fn_802B27C0
  left  fn_802B27A0                        addr 560 size 32 match 100.0
  left  fn_802B27C0                        addr 592 size 88 match None
  right fn_802B27A0                        addr 560 size 32 match 100.0
  right __ct__19SAdvancementResultsFRC13CCharAnimTime addr 592 size 88 match None
```

`match None` on both sides = **objdiff never paired them**, at identical addresses and identical
sizes. It pairs by name only. That is why the naming matters and it is the repo's existing
convention for a retail function dtk has no name for: `src/MetroidPrime/ScriptObjects/
CScriptWallCrawler.cpp:49` writes `extern "C" bool fn_83_68(void*)`.

The catch: retail's 88-byte body is only reachable through a **constructor's member-init list**
(a free function spelling emits a wrapper and calls something else instead - measured below), and
a constructor's symbol is always `__ct__19SAdvancementResultsFRC13CCharAnimTime`, which cannot be
spelled `fn_802B27C0`. `extern "C"` on a member function does not help: the standard ignores C
linkage on a class member.

### Spellings tried for the 88-byte body, with scores

| spelling | object | unit |
| --- | --- | --- |
| ctor body assigning `mDeltas.mPosDelta` / `mDeltas.mRotDelta` (the baseline) | 88 B but `lfsu`+3`lwz`/`stw` block copy, `addi` for `sZeroVector` | 9/12, 82.72% |
| ctor member-init list `: mRemTime(time), mDeltas(CVector3f::Zero(), CQuaternion::NoRotation())` | **88 B, retail's exact bytes** (`lfsu f0,0x74b0(r6)` + 2 `lfs`, then 4 `lfs`/`stfs`) | 11/12, 88.48%; `fn_802B27C0` unpaired |
| ctor defined out-of-line in the .cpp (strong `T` instead of weak `W`) | identical bytes, still unpaired - so weak-vs-strong is *not* the rule, name is | 11/12 |
| `extern "C" SAdvancementResults fn_802B27C0(t) { return SAdvancementResults(t, SAdvancementDeltas(Zero, NoRotation)); }` | 44 B wrapper + an out-of-line `__ct__(time, SAdvancementDeltas)`, layout shifts | 10/12, 86.91% |
| `extern "C" ... fn_802B27C0(t) { SAdvancementResults r; r.mRemTime = t; r.mDeltas = SAdvancementDeltas(...); return r; }` | 44 B + an emitted `__as__19SAdvancementResults`, layout shifts | 10/12, 86.91% |

The rule behind the table, reusable: **mwcceppc only emits retail's element-wise `lfs`/`stfs` per
field when the member is initialised in a constructor's init list; a plain assignment to the same
aggregate is a 16-byte block move (`lfsu` + `lwz`x3).** That one word of C++ is the whole
88-byte function.

Left as a measured wall rather than a `WALL:` line: the remaining gap is a symbol name, not
register allocation, and the bytes are already right.

## Gates (all run in this worktree, `MP_TOOLCHAIN_DIR=.../MetroidPrimePort`)

- `./tools/decomp_build.sh` -> `All: 33.56% fuzzy, 26.66% matched, 12.64% linked (11841 / 28465 functions)`
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
- `./tools/probe_sources.sh` -> `744 files, 0 failed, 0 errors; link: LINKED (324 undefined, 0 duplicates)`
- `python3 tools/check_symbol_names.py` -> `515 units; 0 declared names are missing`
- RELs: ninja runs `dtk shasum -c config/G2ME01/build.sha1` (DOL + 86 RELs) and passed;
  `tools/goal_check.sh` re-runs `tools/gate.sh`, which includes that and the independent
  re-hash against `config.yml`.
- `./tools/goal_check.sh build/goal/item.json` -> **PASS**:
  `matched 11839 -> 11841  linked 5727 -> 5727`, `target rose: main/Kyoto/Animation/IAnimReader: 9 -> 11 / 12`,
  `no asm added`, `no judge-owned path touched`.

Nothing anywhere got worse: DOL `matched_functions` 10291 -> 10293, game 10544 -> 10546, modules
and sdk unchanged - i.e. exactly this unit's two functions.

`./tools/unit_fit.sh Kyoto/Animation/IAnimReader.cpp` reports `.sdata`/`.sbss` unclaimed and one
"extra" symbol, the 88-byte **COMDAT weak copy** of `__ct__19SAdvancementResultsFRC13CCharAnimTime`,
which both linkers discard; `.text 764 = 764`. Only `flip_test.sh` could decide a flip, and this is
a `progress` item, so the unit was left `NonMatching`.

The header change touches only `SAdvancementResults`' one-argument constructor, and no other
translation unit constructs a `SAdvancementResults` from a `CCharAnimTime` alone (grep over
`include/` + `src/`), so no other unit's code moved - confirmed by the per-unit report diff in
`gate.sh` being clean.